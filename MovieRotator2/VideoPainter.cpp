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
#include "VideoPainter.h"
#include "PlaybackClocks.h"
#include "VideoDecoder.h"
#include "D2DManager.h"

#include <initguid.h>
#include <evr.h>
#include <math.h>

static const UINT32 VIDEO_FPS = 60;
#define VIDEO_PAINT_TIMER 1

RECT
MakeRect(long left, long top, long right, long bottom)
{
  RECT r;
  r.left = left;
  r.top = top;
  r.right = right;
  r.bottom = bottom;
  return r;
}

POINT
MakePoint(long x, long y)
{
  POINT p;
  p.x = x;
  p.y = y;
  return p;
}

bool
IsNull(const RECT& r)
{
  return r.left == 0 &&
         r.top == 0 &&
         r.right == 0 &&
         r.bottom == 0;
}

VideoPainter::VideoPainter(HWND aEventTarget, HINSTANCE hInst,
                           FLOAT aX, FLOAT aY, FLOAT aWidth, FLOAT aHeight)
  : mEventTarget(aEventTarget),
    mHInst(hInst),
    mX(aX),
    mY(aY),
    mWidth(aWidth),
    mHeight(aHeight),
    mHWND(0),
    mResetToken(0),
    mVideoFrameWidth(0),
    mVideoFrameHeight(0),
    mVideoStride(0),
    mDecoder(nullptr),
    mClock(nullptr),
    mCurrentFrame(nullptr),
    mRenderBounds(MakeRect(0,0,0,0)),
    mRotationDelta(0),
    mDegreesRotated(0),
    mTimer(0),
    mPaused(true)
{
}

VideoPainter::~VideoPainter()
{
  if (mTimer) {
    KillTimer(mHWND, mTimer);
    mTimer = 0;
  }
}

LRESULT CALLBACK D3DWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  // Note: the WindowLongPtr can be null if we're called before it's been set,
  // i.e. in the WM_CREATE message, and maybe others.
  VideoPainter* vp = (VideoPainter*)(GetWindowLongPtr(hWnd, 0));
  if (vp && vp->Handle(hWnd, message, wParam, lParam)) {
    return 0;
  }
  return DefWindowProc(hWnd, message, wParam, lParam);
}

static const WCHAR* sD3dWindowClassName = L"MovieRotator_D3D_Window";
static const WCHAR* sD3dWindowName = L"MovieRotator_D3D_Window1";
static const D3DFORMAT SCREEN_FORMAT = D3DFMT_X8R8G8B8;


// Registers
ATOM RegisterD3DWindowClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex = {0};

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= D3DWndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= sizeof(VideoPainter*);
	wcex.hInstance		= hInstance;
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
  wcex.hIcon      = LoadIcon (NULL, IDI_APPLICATION);
	wcex.lpszClassName	= sD3dWindowClassName;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_APPLICATION_ICON));
  wcex.lpszMenuName = NULL;

	return RegisterClassEx(&wcex);
}


VOID CALLBACK
InvaldateTimerProc(HWND hwnd,
                   UINT uMsg,
                   UINT_PTR idEvent,
                   DWORD dwTime)
{
  InvalidateRect(hwnd, NULL, FALSE);
}

HRESULT
VideoPainter::Init()
{
  HRESULT hr;

  if (!RegisterD3DWindowClass(mHInst)) {
    DWORD x = GetLastError();
    DBGMSG(L"Failed to register window class, error=0x%x\n", x);
    return E_FAIL;
  }

  // Maybe I need WS_CLIPCHILDREN in the parent?
  mHWND = CreateWindowEx(0, sD3dWindowClassName, sD3dWindowName, WS_CHILD | WS_VISIBLE,
                         mX, mY, mWidth, mHeight,
                         mEventTarget, NULL, mHInst, NULL);
  ENSURE_TRUE(mHWND, E_FAIL);

  // Set a pointer to our instance, so we can get to it.
  SetWindowLongPtr(mHWND, 0, LONG_PTR(this));

  mD3D9 = Direct3DCreate9(D3D_SDK_VERSION); //Create the presentation parameters
  ENSURE_TRUE(mD3D9, E_FAIL);

  // Create the D3d9 device ...
  D3DPRESENT_PARAMETERS params = {0};
  params.Windowed = true;
  params.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
  params.hDeviceWindow = mHWND;   //give the window handle of the window we created above
  params.BackBufferCount = 1;
  params.BackBufferWidth = mWidth;     //set the buffer to our window width
  params.BackBufferHeight = mHeight; //set the buffer to out window height
  params.BackBufferFormat = SCREEN_FORMAT; //the backbuffer format
  params.SwapEffect = D3DSWAPEFFECT_DISCARD; //SwapEffect

  hr = mD3D9->CreateDevice(D3DADAPTER_DEFAULT,
                           D3DDEVTYPE_HAL,
                           mHWND,
                           D3DCREATE_FPU_PRESERVE |
                           D3DCREATE_MULTITHREADED |
                           D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                           &params,
                           &mDevice);
  ENSURE_SUCCESS(hr, hr);


  // Disable lighting, since we didn't specify color for vertex
  mDevice->SetRenderState( D3DRS_LIGHTING , FALSE );

  // Create the device manager.
  hr = DXVA2CreateDirect3DDeviceManager9(&mResetToken, &mDeviceManager);
  ENSURE_SUCCESS(hr, hr);

  hr = mDeviceManager->ResetDevice(mDevice, mResetToken);
  ENSURE_SUCCESS(hr, hr);

  return S_OK;
}

bool
VideoPainter::Handle(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  if (hWnd != mHWND) {
    return false;
  }
  switch (message) {
    case WM_PAINT: {
      // Mark the dirty region as invalid. This is important, if we don't
      // do this, we'll keep getting spammed by WM_PAINT messages, and if
      // we're running another dialog's message loop on top of the main
      // message loop, we won't get a chance to properly service the dialog's
      // message loop.
      RECT rect;
      GetUpdateRect(hWnd, &rect, TRUE);
      ValidateRect(hWnd, &rect);
      OnPaint();
      return true;
    }
  }
  return false;
}

HRESULT
VideoPainter::GetD3DSurfaceFromCurrentFrame(IDirect3DSurface9** aOutSurface)
{
  ENSURE_SUCCESS(aOutSurface, E_POINTER);
  *aOutSurface = NULL;

  IMFMediaBufferPtr buffer;

  HRESULT hr = mCurrentFrame->sample->GetBufferByIndex(0, &buffer);

  return MFGetService(buffer, MR_BUFFER_SERVICE, IID_PPV_ARGS(aOutSurface));
}

static HRESULT
CopyFourccImage(DWORD fourcc,
                BYTE* dstData,
                LONG dstStride,
                BYTE* srcData,
                LONG srcStride,
                UINT32 mVideoFrameWidth,
                UINT32 mVideoFrameHeight)
{
  HRESULT hr;

  if (fourcc == FOURCC_YUY2 || fourcc == FOURCC_UYVY) {
    // One plane, packed. 2:1 horzontal downsampling, no vertical downsampling.
    UINT32 numLines = mVideoFrameHeight;
    UINT32 lineWidthInBytes = 2 * mVideoFrameWidth;
    hr = MFCopyImage(dstData, dstStride, srcData, srcStride, lineWidthInBytes, numLines);
    ENSURE_SUCCESS(hr, hr);
  } else if (fourcc == FOURCC_AYUV) {
    // One plane, packed. No downsampling.
    UINT32 numLines = mVideoFrameHeight;
    UINT32 lineWidthInBytes = 4 * mVideoFrameWidth;
    hr = MFCopyImage(dstData, dstStride, srcData, srcStride, lineWidthInBytes, numLines);
    ENSURE_SUCCESS(hr, hr);
  } else if (fourcc == FOURCC_NV12) {
    // One Y plane, one packed UV plane, 2:1 horizontal and 2:1 vertical downsampling.
    // First copy Y plane.
    UINT32 yLines = mVideoFrameHeight;
    UINT32 yWidthInBytes = mVideoFrameWidth;
    hr = MFCopyImage(dstData, dstStride, srcData, srcStride, yWidthInBytes, yLines);
    ENSURE_SUCCESS(hr, hr);

    // Copy the packed UV plane.

    // The V and U planes are stored 16-row-aligned, so we need to add padding
    // to the row heights to ensure the Y'CbCr planes are referenced properly.
    UINT32 padding = 0;
    if (mVideoFrameHeight % 16 != 0) {
      padding = 16 - (mVideoFrameHeight % 16);
    }
    UINT32 yPlaneLines = mVideoFrameHeight + padding;
    UINT32 uvLines = mVideoFrameHeight / 2;
    UINT32 uvWidthInBytes = mVideoFrameWidth;

    UINT32 yDstPlaneSize = dstStride * yPlaneLines;
    UINT32 ySrcPlaneSize = srcStride * yPlaneLines;

    BYTE* uvDst = dstData + yDstPlaneSize;
    BYTE* uvSrc = srcData + ySrcPlaneSize;
    hr = MFCopyImage(uvDst, dstStride, uvSrc, srcStride, uvWidthInBytes, uvLines);
    ENSURE_SUCCESS(hr, hr);

  } else if (fourcc == MFVideoFormat_RGB32.Data1) {
    // We'll be doing an extra copy, but meh.
    UINT32 numLines = mVideoFrameHeight;
    UINT32 lineWidthInBytes = 4 * mVideoFrameWidth;
    hr = MFCopyImage(dstData, dstStride, srcData, srcStride, lineWidthInBytes, numLines);
    ENSURE_SUCCESS(hr, hr);
  } else {
    return E_FAIL;
  }
  return S_OK;
}

HRESULT
VideoPainter::UploadCurrentFrameToGPU(IDirect3DSurface9** aOutSurface)
{
  HRESULT hr;

  // Get the fourcc code; the YUV format of the frame.
  GUID subtype;
  mCurrentFrame->type->GetGUID(MF_MT_SUBTYPE, &subtype);
  DWORD fourcc = subtype.Data1;

  // Create a new surface to upload the sample to.
  IDirect3DSurface9Ptr surface;
  hr = mDevice->CreateOffscreenPlainSurface(mVideoFrameWidth,
                                            mVideoFrameHeight,
                                            D3DFORMAT(fourcc),
                                            D3DPOOL_DEFAULT,
                                            &surface,
                                            nullptr);
  ENSURE_SUCCESS(hr, hr);

  // Must convert to contiguous buffer to use IMD2DBuffer interface.
  IMFMediaBufferPtr buffer;
  hr =  mCurrentFrame->sample->ConvertToContiguousBuffer(&buffer);
  ENSURE_SUCCESS(hr, hr);

  // Try and use the IMF2DBuffer interface if available, otherwise fallback
  // to the IMFMediaBuffer interface. Apparently IMF2DBuffer is more efficient,
  // but only some systems (Windows 8?) support it.
  BYTE* srcData = nullptr;
  LONG srcStride = 0;
  IMF2DBufferPtr twoDBuffer;
  hr = buffer->QueryInterface(&twoDBuffer);
  if (SUCCEEDED(hr)) {
    hr = twoDBuffer->Lock2D(&srcData, &srcStride);
    ENSURE_SUCCESS(hr, hr);
  } else {
    hr = buffer->Lock(&srcData, NULL, NULL);
    ENSURE_SUCCESS(hr, hr);
    srcStride = mVideoStride;
  }

  D3DLOCKED_RECT lockedRect;
  hr = surface->LockRect(&lockedRect, NULL, 0);
  if (SUCCEEDED(hr)) {
    BYTE* dstData = (BYTE*)lockedRect.pBits;
    LONG dstStride = lockedRect.Pitch;

    hr = CopyFourccImage(fourcc, dstData, dstStride, srcData, srcStride, mVideoFrameWidth, mVideoFrameHeight);
    surface->UnlockRect();
  }

  if (twoDBuffer) {
    twoDBuffer->Unlock2D();
  } else {
    buffer->Unlock();
  }

  // Ensure the copy succeeded (we need to unlock above, so can't
  // just bail immediately).
  ENSURE_SUCCESS(hr, hr);

  *aOutSurface = surface.Detach();

  return S_OK;
}


static RECT
LetterBoxRect(const SIZE& aspectRatio, const RECT &rcDest)
{
  float width, height;

  float SrcWidth = aspectRatio.cx;
  float SrcHeight = aspectRatio.cy;
  float DestWidth = rcDest.right - rcDest.left;
  float DestHeight = rcDest.bottom - rcDest.top;

  RECT rcResult = MakeRect(0,0,0,0);

  // Avoid divide by zero.
  if (SrcWidth == 0 || SrcHeight == 0) {
    return rcResult;
  }

  // First try: Letterbox along the sides. ("pillarbox")
  width = DestHeight * SrcWidth / SrcHeight;
  height = DestHeight;
  if (width > DestWidth) {
    // Letterbox along the top and bottom.
    width = DestWidth;
    height = DestWidth * SrcHeight / SrcWidth;
  }

  // Fill in the rectangle
  rcResult.left = long(rcDest.left + ((DestWidth - width) / 2));
  rcResult.right = long(rcResult.left + width);
  rcResult.top = long(rcDest.top + ((DestHeight - height) / 2));
  rcResult.bottom = long(rcResult.top + height);

  return rcResult;
}

void
VideoPainter::RotateVertices(textured_vertex* vertices, unsigned length, float degrees)
{
  // Note: mathematical convention for measuring rotation is to have degrees measures
  // counter clockwise, whereas we're doing it here clockwise, hence the -degrees below.
  float angle = - degrees / 57.2957795;
  float x_origin = mWidth / 2;
  float y_origin = mHeight / 2;

  for (unsigned i=0; i<length; i++) {
    textured_vertex& v = vertices[i];
    float x = v.x;
    float y = v.y;
    v.x = ((x - x_origin) * cos(angle)) - ((y_origin - y) * sin(angle)) + x_origin;
    v.y = y_origin - ((y_origin - y) * cos(angle)) - ((x - x_origin) * sin(angle));
  }
}

HRESULT
VideoPainter::InitializeVertexBuffer()
{
  // Reset the old vertex buffer, if we have one.
  mVertexBuffer = nullptr;

  const RECT& r = mRenderBounds;

  // Set the vertex buffer to match the render bounds.
  textured_vertex data[] = {
    {r.left,r.bottom,1,1,0,1}, {r.left,r.top,1,1,0,0}, {r.right,r.top,1,1,1,0},
    {r.left,r.bottom,1,1,0,1}, {r.right,r.top,1,1,1,0}, {r.right,r.bottom,1,1,1,1}
  };

  int vert_count=sizeof(data)/sizeof(textured_vertex);
  int byte_count=vert_count*sizeof(textured_vertex);
  void *vb_vertices;
  HRESULT hr;

  if (mRotationDelta != 0) {
    if (mRotationDelta > 0) {
      // Go clockwise!
      mDegreesRotated += 5;
      mRotationDelta -= 5;
    } else {
      mDegreesRotated -= 5;
      mRotationDelta += 5;
    }
  }
  RotateVertices(data, vert_count, mDegreesRotated);

  mTriangleCount=vert_count/3;

  hr = mDevice->CreateVertexBuffer(byte_count,        //Length
                                   D3DUSAGE_WRITEONLY,//Usage
                                   tri_fvf,           //FVF
                                   D3DPOOL_MANAGED,   //Pool
                                   &mVertexBuffer,        //ppVertexBuffer
                                   NULL);             //Handle
  ENSURE_SUCCESS(hr, hr);

  hr = mVertexBuffer->Lock(0, //Offset
                           0, //SizeToLock
                           &vb_vertices, //Vertices
                           0);  //Flags
  ENSURE_SUCCESS(hr, hr);

  memcpy(vb_vertices, data, byte_count);

  mVertexBuffer->Unlock();

  return S_OK;
}

HRESULT
VideoPainter::CreateTexture(IMFMediaType* aType)
{
  HRESULT hr;

  UINT32 width, height;
  hr = MFGetAttributeSize(aType, MF_MT_FRAME_SIZE, &width, &height);
  ENSURE_SUCCESS(hr, hr);

  // Create the texture, so that it matches the image size.
  HANDLE shareHandle = NULL;
  hr = mDevice->CreateTexture(width,
                              height,
                              1,
                              D3DUSAGE_RENDERTARGET,
                              D3DFMT_X8R8G8B8,
                              D3DPOOL_DEFAULT,
                              &mTexture,
                              NULL);
  ENSURE_SUCCESS(hr, hr);

  return S_OK;
}

HRESULT
VideoPainter::UpdateBounds()
{
  HRESULT hr;

  IMFMediaTypePtr type(mCurrentFrame->type);

  // Stash the frame width/height, in case we need it for texture upload.
  hr = MFGetAttributeSize(type,
                          MF_MT_FRAME_SIZE,
                          &mVideoFrameWidth,
                          &mVideoFrameHeight);
  ENSURE_SUCCESS(hr, hr);

  hr = GetDefaultStride(mCurrentFrame->type, &mVideoStride);
  ENSURE_SUCCESS(hr, hr);

  // Calculate the rect which the frame is scaled to, WRT the pixel aspect
  // ratio.
  MFVideoArea area;
  hr = GetVideoDisplayArea(type, &area);
  ENSURE_SUCCESS(hr, hr);
  RECT rcSrc = RectFromArea(area);
  MFRatio par;
  GetPixelAspectRatio(type, &par);
  RECT renderRect = CorrectAspectRatio(rcSrc, par);

  // Extract the width/height.
  SIZE renderSize;
  renderSize.cx = renderRect.right - renderRect.left;
  renderSize.cy = renderRect.bottom - renderRect.top;

  int paddingX = D2DManager::DpiScaleX(10);
  int paddingY = D2DManager::DpiScaleY(10);

  RECT canvasBounds = MakeRect(paddingX, paddingY,
                               mWidth-paddingX, mHeight-paddingY);

  mRenderBounds = LetterBoxRect(renderSize, canvasBounds);

  hr = CreateTexture(type);
  ENSURE_SUCCESS(hr, hr);

  return S_OK;
}

HRESULT
VideoPainter::UpdateTexture()
{
  HRESULT hr;

  IDirect3DSurface9Ptr frameSurface;
  hr = GetD3DSurfaceFromCurrentFrame(&frameSurface);
  if (FAILED(hr)) {
    hr = UploadCurrentFrameToGPU(&frameSurface);
    ENSURE_SUCCESS(hr, hr);
  }

  // Copy the image onto the texture, preforming YUV -> RGB conversion.
  IDirect3DSurface9Ptr textureSurface;
  hr = mTexture->GetSurfaceLevel(0, &textureSurface);
  ENSURE_SUCCESS(hr,hr);

  hr = mDevice->StretchRect(frameSurface, NULL, textureSurface, NULL, D3DTEXF_NONE);
  ENSURE_SUCCESS(hr,hr);

  mDevice->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
  mDevice->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
  mDevice->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);

  mDevice->SetTexture(0,mTexture);

  return S_OK;
}

void
VideoPainter::PaintFrame()
{
  HRESULT hr;
  if (!mDecoder || !mClock) {
    return;
  }
  MediaSample* nextSample = mDecoder->PeekVideo();
  if (!mCurrentFrame && !nextSample) {
    // No frames to paint!
    return;
  }

  // Otherwise, if we've got video! Skip up to the frame that matches
  // the clock's time.
  LONGLONG pos = 0;
  hr = mClock->GetPosition(&pos);
  ENSURE_SUCCESS(hr,);

  bool mustUpdateTexture = false;
  while (nextSample && nextSample->timestamp <= pos) {
    if (mCurrentFrame) {
      delete mCurrentFrame;
      mCurrentFrame = nullptr;
    }
    mustUpdateTexture = true;
    mCurrentFrame = nextSample;
    mDecoder->PopVideo();
    nextSample = mDecoder->PeekVideo();
  }

  if (!mCurrentFrame) {
    // Somehow we didn't get a frame. Maybe this is the first frame,
    // but it's missing
    return;
  }

  // Calculate the render bounds if we've not done so already, or if
  // the stream properties changed.
  if (IsNull(mRenderBounds) ||
    (mCurrentFrame->flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)) {
      UpdateBounds();
  }

  if (mustUpdateTexture) {
    UpdateTexture();
  }

  hr = InitializeVertexBuffer();
  ENSURE_SUCCESS(hr, );


  // Paint the frame! :D
  mDevice->SetFVF(tri_fvf);

  //Bind our Vertex Buffer
  mDevice->SetStreamSource(0,                   //StreamNumber
                           mVertexBuffer,           //StreamData
                           0,                   //OffsetInBytes
                           sizeof(textured_vertex)); //Stride

  //Render from our Vertex Buffer
  mDevice->DrawPrimitive(D3DPT_TRIANGLELIST, //PrimitiveType
                         0,                  //StartVertex
                         mTriangleCount);      //PrimitiveCount

}

// Paint function. We run a timer which invalidates our
// window 30 times per second, causing the paint to occur.
void
VideoPainter::OnPaint()
{
  if (!mDevice) {
    return;
  }

  mDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

  mDevice->BeginScene();

  PaintFrame();

  mDevice->EndScene();

  mDevice->Present(NULL, NULL, NULL, NULL);
}

HRESULT
VideoPainter::SetInputs(VideoDecoder* aDecoder,
                        PlaybackClock* aClock,
                        Rotation aRotation)
{
  DBGMSG(L"VideoPainter::Reset(%p, %p)\n", aDecoder, aClock);
  if (mCurrentFrame) {
    delete mCurrentFrame;
    mCurrentFrame = nullptr;
  }

  mRenderBounds = MakeRect(0, 0, 0, 0);

  mDegreesRotated = 90 * aRotation;

  mDecoder = aDecoder;
  mClock = aClock;

  if (aDecoder) {
    // Create a timer to invalidate our window at the frame rate. This isn't the
    // frame rate of our video, but our animations are tied to this rate...
    mTimer = SetTimer(mHWND, VIDEO_PAINT_TIMER, 1000 / VIDEO_FPS, InvaldateTimerProc);
  } else if (mTimer) {
    KillTimer(mHWND, mTimer);
    mTimer = 0;
  }
  // We must repaint if we closed, so that the frame is cleared.
  InvalidateRect(mHWND, NULL, TRUE);
  return S_OK;
}

HRESULT
VideoPainter::GetDeviceManager(IDirect3DDeviceManager9** aOutDeviceManager)
{
  ENSURE_TRUE(aOutDeviceManager, E_FAIL);
  ENSURE_TRUE(mDeviceManager, E_FAIL);
  *aOutDeviceManager = mDeviceManager;
  (*aOutDeviceManager)->AddRef();
  return S_OK;
}

HRESULT
VideoPainter::GetD3D9(IDirect3D9** aOutD3D9)
{
  ENSURE_TRUE(aOutD3D9, E_FAIL);
  ENSURE_TRUE(mD3D9, E_FAIL);
  *aOutD3D9 = mD3D9;
  (*aOutD3D9)->AddRef();
  return S_OK;
}

void
VideoPainter::Reset()
{
  SetInputs(nullptr, nullptr, ROTATE_0);
  mRotationDelta = 0;
  mDegreesRotated = 0;
}

void
VideoPainter::RotateClockwise()
{
  mRotationDelta += 90;
}

void
VideoPainter::RotateAntiClockwise()
{
  mRotationDelta -= 90;
}
