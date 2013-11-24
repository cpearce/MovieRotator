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
#include "D2DManager.h"
#include "resource.h"

#include <map>

using std::map;

static ID2D1DCRenderTarget* sRenderTarget = nullptr;
static ID2D1Factory* sD2DFactory = nullptr;
static IWICImagingFactory* sWICFactory = nullptr;
static ID2D1Bitmap* sOpenFileIcon = nullptr;
static ID2D1Bitmap* sRotateClockwiseIcon = nullptr;
static ID2D1Bitmap* sRotateAntiClockwiseIcon = nullptr;
static ID2D1Bitmap* sSaveRotationIcon = nullptr;
static IDWriteFactory* sDWriteFactory = nullptr;
static IDWriteTextFormat* sGuiTextFormat = nullptr;

static map<ResourceId, ID2D1Bitmap*> sResourceBitmaps;

// Because the CreateWindow function takes its size in pixels,
// obtain the system DPI and use it to scale the window size.
static FLOAT sDpiX = 0;
static FLOAT sDpiY = 0;

/* static */
HRESULT D2DManager::Init()
{
  HRESULT hr;

  hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                &sD2DFactory);
  ENSURE_SUCCESS(hr, hr);

  // Create WIC factory.
  hr = CoCreateInstance(CLSID_WICImagingFactory,
                        NULL,
                        CLSCTX_INPROC_SERVER,
                        IID_PPV_ARGS(&sWICFactory));
  ENSURE_SUCCESS(hr, hr);

  // The factory returns the current system DPI. This is also the value it will use
  // to create its own windows.
  sD2DFactory->GetDesktopDpi(&sDpiX, &sDpiY);

  DBGMSG(L"DPI = %f, %f\n", sDpiX, sDpiY);

  // Create a DirectWrite factory.
  hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                           __uuidof(IDWriteFactory),
                           reinterpret_cast<IUnknown**>(&sDWriteFactory));
  ENSURE_SUCCESS(hr, hr);

  // Create a DirectWrite text format object.
  hr = sDWriteFactory->CreateTextFormat(L"Verdana",
                                        NULL,
                                        DWRITE_FONT_WEIGHT_NORMAL,
                                        DWRITE_FONT_STYLE_NORMAL,
                                        DWRITE_FONT_STRETCH_NORMAL,
                                        13.0,
                                        L"", //locale
                                        &sGuiTextFormat);
  ENSURE_SUCCESS(hr, hr);

  // Center the text horizontally and vertically.
  hr = sGuiTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
  ENSURE_SUCCESS(hr, hr);

  hr = sGuiTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
  ENSURE_SUCCESS(hr, hr);

  hr = sGuiTextFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
  ENSURE_SUCCESS(hr, hr);

  IDWriteInlineObjectPtr inlineObject;
  hr = sDWriteFactory->CreateEllipsisTrimmingSign(sGuiTextFormat, &inlineObject);
  ENSURE_SUCCESS(hr, hr);

  DWRITE_TRIMMING trimming = {DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
  hr = sGuiTextFormat->SetTrimming(&trimming, inlineObject);
  ENSURE_SUCCESS(hr, hr);

  return S_OK;
}

static inline FLOAT
DpiConvert(UINT32 aPixelValue, FLOAT aDpi)
{
  return ceil(float(aPixelValue) * aDpi / 96.f);
}

/* static */
FLOAT
D2DManager::DpiScaleX(UINT32 aPixelValue)
{
  return DpiConvert(aPixelValue, sDpiX);
}

/* static */
FLOAT
D2DManager::DpiScaleY(UINT32 aPixelValue)
{
  return DpiConvert(aPixelValue, sDpiY);
}

/* static */
HRESULT
D2DManager::Shutdown()
{
  D2DManager::DiscardDeviceResources();
  SAFE_RELEASE(sDWriteFactory);
  SAFE_RELEASE(sD2DFactory);
  return S_OK;
}

static ID2D1SolidColorBrush* sLightSlateGrayBrush = nullptr;
static ID2D1SolidColorBrush* sBlackBrush = nullptr;

/* static */
ID2D1SolidColorBrush*
D2DManager::GetBlackBrush()
{
  return sBlackBrush;
}

/* static */
ID2D1SolidColorBrush*
D2DManager::GetGrayBrush()
{
  return sLightSlateGrayBrush;
}

/* static */
IDWriteTextFormat*
D2DManager::GetGuiTextFormat()
{
  return sGuiTextFormat;
}

static HRESULT
EnsureDeviceResources()
{
  ENSURE_TRUE(GetMainWindowHWND(), E_ABORT);

  if (sRenderTarget) {
    return S_OK;
  }

  HRESULT hr;
  RECT rc;
  GetClientRect(GetMainWindowHWND(), &rc);

  // Create a DC render target.
  D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
      D2D1_RENDER_TARGET_TYPE_DEFAULT,
      D2D1::PixelFormat(
          DXGI_FORMAT_B8G8R8A8_UNORM,
          D2D1_ALPHA_MODE_IGNORE),
      0,
      0,
      D2D1_RENDER_TARGET_USAGE_NONE,
      D2D1_FEATURE_LEVEL_DEFAULT
      );

  hr = sD2DFactory->CreateDCRenderTarget(&props, &sRenderTarget);
  ENSURE_SUCCESS(hr, hr);

  // Create a gray brush.
  hr = sRenderTarget->CreateSolidColorBrush(
    D2D1::ColorF(D2D1::ColorF::LightSlateGray),
    &sLightSlateGrayBrush);
  ENSURE_SUCCESS(hr, hr);

  // Create a blue brush.
  hr = sRenderTarget->CreateSolidColorBrush(
    D2D1::ColorF(D2D1::ColorF::Black),
    &sBlackBrush);
  ENSURE_SUCCESS(hr, hr);

  return S_OK;
}

/* static */
HRESULT
D2DManager::DiscardDeviceResources()
{
  // Discard all the loaded bitmaps.
  map<ResourceId,ID2D1Bitmap*>::iterator itr = sResourceBitmaps.begin();
  while (itr != sResourceBitmaps.end()) {
    ID2D1Bitmap* bitmap = itr->second;
    SAFE_RELEASE(bitmap);
    itr++;
  }
  sResourceBitmaps.clear();

  SAFE_RELEASE(sLightSlateGrayBrush);
  SAFE_RELEASE(sBlackBrush);
  SAFE_RELEASE(sRenderTarget);

  SAFE_RELEASE(sOpenFileIcon);
  SAFE_RELEASE(sRotateClockwiseIcon);
  SAFE_RELEASE(sRotateAntiClockwiseIcon);
  SAFE_RELEASE(sSaveRotationIcon);

  return S_OK;
}

static PAINTSTRUCT sPaintStruct;
static HDC sHDC;

/* static */
ID2D1DCRenderTarget*
D2DManager::BeginPaint()
{
  HRESULT hr = EnsureDeviceResources();
  ENSURE_SUCCESS(hr, nullptr);
  ENSURE_TRUE(GetMainWindowHWND(), nullptr);

  sHDC = ::BeginPaint(GetMainWindowHWND(), &sPaintStruct);
  //DBGMSG(L"PaintStruct HWND=0x%x rect={%d,%d,%d,%d}\n",
  //       GetMainWindowHWND(),
  //       sPaintStruct.rcPaint.left, sPaintStruct.rcPaint.top,
  //       sPaintStruct.rcPaint.right, sPaintStruct.rcPaint.bottom);

  sRenderTarget->BindDC(sPaintStruct.hdc, &sPaintStruct.rcPaint);
  sRenderTarget->BeginDraw();

  return sRenderTarget;
}

/* static */
void
D2DManager::EndPaint()
{
  ENSURE_TRUE(sRenderTarget,);
  if (sRenderTarget->EndDraw() == D2DERR_RECREATE_TARGET) {
    D2DManager::DiscardDeviceResources();
  }
  ::EndPaint(GetMainWindowHWND(), &sPaintStruct);
}

/* static */
void
D2DManager::Invalidate()
{
  InvalidateRect(GetMainWindowHWND(), NULL, FALSE);
}

//
// Creates a Direct2D bitmap from a resource in the
// application resource file.
//
static HRESULT
LoadResourceBitmap(ResourceId aResourceId,
                   PCWSTR aResourceType,
                   UINT destinationWidth,
                   UINT destinationHeight,
                   ID2D1Bitmap **ppBitmap)
{
  ENSURE_TRUE(sRenderTarget, E_POINTER);
  ENSURE_TRUE(ppBitmap, E_POINTER);

  IWICBitmapDecoderPtr pDecoder = NULL;
  IWICBitmapFrameDecodePtr pSource = NULL;
  IWICStreamPtr pStream = NULL;
  IWICFormatConverterPtr pConverter = NULL;
  IWICBitmapScalerPtr pScaler = NULL;

  HRSRC imageResHandle = NULL;
  HGLOBAL imageResDataHandle = NULL;
  void *pImageFile = NULL;
  DWORD imageFileSize = 0;

  // Locate the resource.
  imageResHandle = FindResourceW(HINST_THISCOMPONENT,
                                 MAKEINTRESOURCE(aResourceId),
                                 aResourceType);
  HRESULT hr = imageResHandle ? S_OK : E_FAIL;
  ENSURE_SUCCESS(hr, hr);

  // Load the resource.
  imageResDataHandle = LoadResource(HINST_THISCOMPONENT, imageResHandle);

  hr = imageResDataHandle ? S_OK : E_FAIL;
  ENSURE_SUCCESS(hr, hr);

  // Lock it to get a system memory pointer.
  pImageFile = LockResource(imageResDataHandle);

  hr = pImageFile ? S_OK : E_FAIL;
  ENSURE_SUCCESS(hr, hr);
  // Calculate the size.
  imageFileSize = SizeofResource(HINST_THISCOMPONENT, imageResHandle);

  hr = imageFileSize ? S_OK : E_FAIL;
  ENSURE_SUCCESS(hr, hr);
  // Create a WIC stream to map onto the memory.
  hr = sWICFactory->CreateStream(&pStream);
  ENSURE_SUCCESS(hr, hr);

  // Initialize the stream with the memory pointer and size.
  hr = pStream->InitializeFromMemory(reinterpret_cast<BYTE*>(pImageFile),
                                     imageFileSize);
  ENSURE_SUCCESS(hr, hr);

  // Create a decoder for the stream.
  hr = sWICFactory->CreateDecoderFromStream(
    pStream,
    NULL,
    WICDecodeMetadataCacheOnLoad,
    &pDecoder);
  ENSURE_SUCCESS(hr, hr);

  // Create the initial frame.
  hr = pDecoder->GetFrame(0, &pSource);
  ENSURE_SUCCESS(hr, hr);

  // Convert the image format to 32bppPBGRA
  // (DXGI_FORMAT_B8G8R8A8_UNORM + D2D1_ALPHA_MODE_PREMULTIPLIED).
  hr = sWICFactory->CreateFormatConverter(&pConverter);
  ENSURE_SUCCESS(hr, hr);

  // If a new width or height was specified, create an
  // IWICBitmapScaler and use it to resize the image.
  if (destinationWidth != 0 || destinationHeight != 0)
  {
    UINT originalWidth, originalHeight;
    hr = pSource->GetSize(&originalWidth, &originalHeight);
    ENSURE_SUCCESS(hr, hr);
    if (destinationWidth == 0)
    {
      FLOAT scalar = static_cast<FLOAT>(destinationHeight) / static_cast<FLOAT>(originalHeight);
      destinationWidth = static_cast<UINT>(scalar * static_cast<FLOAT>(originalWidth));
    }
    else if (destinationHeight == 0)
    {
      FLOAT scalar = static_cast<FLOAT>(destinationWidth) / static_cast<FLOAT>(originalWidth);
      destinationHeight = static_cast<UINT>(scalar * static_cast<FLOAT>(originalHeight));
    }

    hr = sWICFactory->CreateBitmapScaler(&pScaler);
    ENSURE_SUCCESS(hr, hr);
    hr = pScaler->Initialize(pSource,
                             destinationWidth,
                             destinationHeight,
                             WICBitmapInterpolationModeCubic);
    ENSURE_SUCCESS(hr, hr);

    hr = pConverter->Initialize(pScaler,
                                GUID_WICPixelFormat32bppPBGRA,
                                WICBitmapDitherTypeNone,
                                NULL,
                                0.f,
                                WICBitmapPaletteTypeMedianCut);
  } else {
    hr = pConverter->Initialize(pSource,
                                GUID_WICPixelFormat32bppPBGRA,
                                WICBitmapDitherTypeNone,
                                NULL,
                                0.f,
                                WICBitmapPaletteTypeMedianCut);
    ENSURE_SUCCESS(hr, hr);
  }

  // Create a Direct2D bitmap from the WIC bitmap.
  hr = sRenderTarget->CreateBitmapFromWicBitmap(pConverter,
                                                NULL,
                                                ppBitmap);
  ENSURE_SUCCESS(hr, hr);

  return S_OK;
}

ID2D1Bitmap*
D2DManager::GetBitmap(ResourceId aResourceId)
{
  if (sResourceBitmaps[aResourceId]) {
    return sResourceBitmaps[aResourceId];
  }
  ID2D1BitmapPtr bitmap;
  HRESULT hr = LoadResourceBitmap(
            aResourceId,
            L"png",
            48,
            48,
            &bitmap);
  ENSURE_TRUE(SUCCEEDED(hr) && bitmap, nullptr);
  ENSURE_TRUE(sResourceBitmaps[aResourceId] == nullptr, nullptr);
  sResourceBitmaps[aResourceId] = bitmap.Detach();
  return sResourceBitmaps[aResourceId];
}

