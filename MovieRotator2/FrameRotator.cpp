// Copyright 2013  Chris Pearce
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "stdafx.h"
#include "FrameRotator.h"

using std::wstring;

HRESULT
FrameRotator::Init()
{
  HRESULT hr;

  // Create WIC factory.
  hr = CoCreateInstance(CLSID_WICImagingFactory,
                        NULL,
                        CLSCTX_INPROC_SERVER,
                        IID_PPV_ARGS(&mWICFactory));
  ENSURE_SUCCESS(hr, hr);

  // Create a Direct2D factory.
  hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                         &mD2DFactory);
  ENSURE_SUCCESS(hr, hr);

  // Create a DirectWrite factory.
  hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                           __uuidof(IDWriteFactory),
                           reinterpret_cast<IUnknown**>(&mDWriteFactory));
  ENSURE_SUCCESS(hr, hr);

  return S_OK;
}

/* static */
HRESULT
FrameRotator::CreateBGRBitmap(UINT32 aWidth,
                              UINT32 aHeight,
                              IWICBitmap** aOutBitmap)
{
  ENSURE_TRUE(mWICFactory, E_POINTER);
  HRESULT hr;
  IWICBitmapPtr bitmap;
  hr = mWICFactory->CreateBitmap(aWidth, aHeight, GUID_WICPixelFormat32bppBGR, WICBitmapCacheOnLoad, &bitmap);
  ENSURE_SUCCESS(hr, hr);

  *aOutBitmap = bitmap.Detach();

  return S_OK;
}

/* static */
HRESULT
FrameRotator::CreateBitmapRenderTarget(IWICBitmap* aBitmap,
                                       ID2D1RenderTarget** aOutRenderTarget)
{
  HRESULT hr;

  ID2D1RenderTargetPtr renderTarget;
  hr = mD2DFactory->CreateWicBitmapRenderTarget(aBitmap,
                                                D2D1::RenderTargetProperties(),
                                                &renderTarget);
  ENSURE_SUCCESS(hr, hr);

  *aOutRenderTarget = renderTarget.Detach();

  return S_OK;
}

HRESULT
FrameRotator::RotateFrame(Rotation aRotation,
                          IMFSample* aSample,
                          const MFVideoArea* aPictureRegion,
                          LONG aInputStride,
                          LONG aOutputStride,
                          IMFSample** aOutRotated)
{
  ENSURE_TRUE(aRotation != ROTATE_0, E_FAIL);
  ENSURE_TRUE(aSample, E_FAIL);

  HRESULT hr;

  UINT32 picWidth = aPictureRegion->Area.cx;
  UINT32 picHeight = aPictureRegion->Area.cy;

  // Create a bitmap to store the rotated frame.
  UINT32 rotatedWidth = (aRotation == ROTATE_180) ? picWidth : picHeight;
  UINT32 rotatedHeight = (aRotation == ROTATE_180) ? picHeight : picWidth;

  IWICBitmapPtr bitmap;
  hr = CreateBGRBitmap(rotatedWidth, rotatedHeight, &bitmap);
  ENSURE_SUCCESS(hr, hr);

  // Create a bitmap render target to draw the rotated frame onto.
  ID2D1RenderTargetPtr renderTarget;
  hr = CreateBitmapRenderTarget(bitmap, &renderTarget);
  ENSURE_SUCCESS(hr, hr);

  // Load the frame into a D2D bitmap
  IMFMediaBufferPtr buffer;
  hr = aSample->ConvertToContiguousBuffer(&buffer);
  ENSURE_SUCCESS(hr, hr);

  BYTE* rawBitmapData = nullptr;
  DWORD rawBitmapLength = 0;
  hr = buffer->Lock(&rawBitmapData, NULL, &rawBitmapLength);
  ENSURE_SUCCESS(hr, hr);

  ID2D1BitmapPtr videoFrame;
  hr = renderTarget->CreateBitmap(D2D1::SizeU(picWidth, picHeight),
                                  rawBitmapData,
                                  aInputStride,
                                  D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                                                                           D2D1_ALPHA_MODE_IGNORE)),
                                  &videoFrame);
  buffer->Unlock();
  ENSURE_SUCCESS(hr, hr);

  // Draw the frame onto the bitmap render target, with rotation.
  renderTarget->BeginDraw();

  // Start with an identity transform.
  D2D1::Matrix3x2F mat = D2D1::Matrix3x2F::Identity();

  // If the image is bottom-up, flip around the x-axis.
  if (aInputStride < 0) {
    mat = D2D1::Matrix3x2F::Scale( D2D1::SizeF(1, -1), D2D1::Point2F(0, picHeight/2) );
  }

  D2D1::Matrix3x2F rotationMatrix;
  D2D1_POINT_2F rotatedCenter = D2D1::Point2F(rotatedWidth / 2, rotatedHeight / 2);
  D2D1MakeRotateMatrix(FLOAT(aRotation * 90), rotatedCenter, &rotationMatrix);
  mat = mat * rotationMatrix;
  renderTarget->SetTransform(mat);

  // Draw the frame on the render target such that its center *before*
  // rotation is the center *after* rotation. This means that the
  // rotation transform will rotate the frame around its centre.
  D2D1_POINT_2F frameCenter = D2D1::Point2F(picWidth / 2, picHeight / 2);
  FLOAT x = rotatedCenter.x - frameCenter.x;
  FLOAT y = rotatedCenter.y - frameCenter.y;
  renderTarget->DrawBitmap(videoFrame,
                           D2D1::RectF(x,
                                       y,
                                       x + picWidth,
                                       y + picHeight),
                           1.0,
                           D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);

  renderTarget->EndDraw();

  // Copy rotated bitmap into a IMFSample.
  IMFSamplePtr rotatedSample;

  hr = CloneSample(aSample, &rotatedSample);
  ENSURE_SUCCESS(hr, hr);

  // Create the rotated buffer.
  IMFMediaBufferPtr rotatedBuffer;
  hr = MFCreateMemoryBuffer(aOutputStride * rotatedHeight, &rotatedBuffer);
  ENSURE_SUCCESS(hr, hr);
  hr = rotatedBuffer->SetCurrentLength(aOutputStride * rotatedHeight);
  ENSURE_SUCCESS(hr, hr);

  IWICBitmapLockPtr lock;
  WICRect rect = { 0, 0, rotatedWidth, rotatedHeight };
  hr = bitmap->Lock(&rect, WICBitmapLockRead, &lock);
  ENSURE_SUCCESS(hr, hr);

  UINT stride;
  hr = lock->GetStride(&stride);
  ENSURE_SUCCESS(hr, hr);

  UINT rotatedBufferSize;
  BYTE* rotatePixels;
  hr = lock->GetDataPointer(&rotatedBufferSize, &rotatePixels);
  ENSURE_SUCCESS(hr, hr);

  // Lock the output buffer, the IMFSample's buffer.
  BYTE* output = nullptr;
  DWORD outMaxLength = 0;
  DWORD outCurrentLength = 0;
  hr = rotatedBuffer->Lock(&output, &outMaxLength, &outCurrentLength);
  if (FAILED(hr)) {
    DBGMSG(L"Failed to lock rotated buffer\n");
    return hr;
  }
  // Copy the rotated image.
  hr = MFCopyImage(output, aOutputStride, rotatePixels, stride, 4*rotatedWidth, rotatedHeight);
  ENSURE_SUCCESS(hr, hr);

  hr = rotatedBuffer->Unlock();
  ENSURE_SUCCESS(hr, hr);

  rotatedSample->AddBuffer(rotatedBuffer);
  *aOutRotated = rotatedSample.Detach();

  return S_OK;
}
