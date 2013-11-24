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

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <Windowsx.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <stdio.h>
#include <assert.h>

#include <D2d1.h>
#include <D2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#include <commdlg.h>
#include <comdef.h>
#include <Shellapi.h>

#include <initguid.h>
#include <d3d9.h>

#include <Ks.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <propvarutil.h>
#include <wmcodecdsp.h>
#include <codecapi.h>
#include <atlbase.h>
#include <evr.h>
#include <mftransform.h>
#include <mferror.h>
#include <mfidl.h>
#include <dxva2api.h>
#include <wmcodecdsp.h>
#include <propvarutil.h>
#include <codecapi.h>
#include <evr.h>
 	

#include <Shlobj.h>

#include <comdef.h>


#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

// TODO: Must be last to avoid compile errors...
#include <strsafe.h>

#ifndef HINST_THISCOMPONENT
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)
#endif

#define COM_SMARTPTR(type) _COM_SMARTPTR_TYPEDEF(type, __uuidof(type))

COM_SMARTPTR(IMFSourceReader);
COM_SMARTPTR(IMFMediaType);
COM_SMARTPTR(IMFMediaSession);
COM_SMARTPTR(IMFSample);
COM_SMARTPTR(IMFMediaBuffer);
COM_SMARTPTR(IMFTransform);
COM_SMARTPTR(IMFAttributes);
COM_SMARTPTR(IMFSinkWriter);
COM_SMARTPTR(IClassFactory);
COM_SMARTPTR(IMFSourceResolver);
COM_SMARTPTR(IMFMediaSource);
COM_SMARTPTR(IMFPresentationDescriptor);
COM_SMARTPTR(IMFTopology);
COM_SMARTPTR(IMFStreamDescriptor);
COM_SMARTPTR(IMFActivate);
COM_SMARTPTR(IMFTopologyNode);
COM_SMARTPTR(IMFMediaTypeHandler);
COM_SMARTPTR(IMFAsyncResult);
COM_SMARTPTR(IMFMediaEvent);
COM_SMARTPTR(IMFVideoDisplayControl);
COM_SMARTPTR(IMFCollection);
COM_SMARTPTR(IMFTopoLoader);
COM_SMARTPTR(ID2D1Bitmap);
COM_SMARTPTR(ID2D1RenderTarget);
COM_SMARTPTR(ID2D1HwndRenderTarget);
COM_SMARTPTR(ID2D1Factory);
COM_SMARTPTR(ID2D1SolidColorBrush);
COM_SMARTPTR(IWICBitmapDecoder);
COM_SMARTPTR(IWICBitmapFrameDecode);
COM_SMARTPTR(IWICStream);
COM_SMARTPTR(IWICFormatConverter);
COM_SMARTPTR(IWICBitmapScaler);
COM_SMARTPTR(IDWriteFactory);
COM_SMARTPTR(IDirect3DDeviceManager9);
COM_SMARTPTR(IDirect3DDevice9);
COM_SMARTPTR(IDirect3DSurface9);
COM_SMARTPTR(IDirect3DSwapChain9);
COM_SMARTPTR(IDirect3DTexture9);
COM_SMARTPTR(IDirect3D9);
COM_SMARTPTR(IDirect3DVertexBuffer9);
COM_SMARTPTR(IWICBitmap);
COM_SMARTPTR(IWICBitmapLock);
COM_SMARTPTR(IWICImagingFactory);
COM_SMARTPTR(IDWriteTextFormat);
COM_SMARTPTR(IWMResamplerProps);
COM_SMARTPTR(IMF2DBuffer);
COM_SMARTPTR(IDWriteInlineObject);
COM_SMARTPTR(ICodecAPI);

enum Rotation {
  ROTATE_0 = 0,
  ROTATE_90 = 1,
  ROTATE_180  = 2,
  ROTATE_270 = 3
};

// TranscodeJobRunner Events:
//
// Sent when the transcode job's worker thread finishes.
// This is sent if a job either runs to completion, or is canceled.
// wParam = 0, lParam = TranscodeJob* of job.
#define MSG_TRANSCODE_COMPLETE (WM_USER + 1)

// Sent every percentage point to notify parent of job progress.
// wParam = percent complete, lParam = TranscodeJob* of job.
#define MSG_TRANSCODE_PROGRESS (WM_USER + 2)

// Sent when a transcode fails.
// wParam = 0, lParam = TranscodeJob* of job.
#define MSG_TRANSCODE_FAILED (WM_USER + 3)

// TranscodeJobList events:
//
// Sent when the TranscodeJobList has been changed, i.e. a job added or removed.
#define MSG_JOBLIST_UPDATE (WM_USER + 10)

// Send when an object wants to a Runnable called asynchronously on the main
// thread's event loop.
// lParam = std::function<void(void)>*, event handler *must* delete the function.
#define MSG_CALL_FUNCTION (WM_USER + 31)

typedef WORD ResourceId;

#include "resource.h"
#include "Utils.h"

HWND GetMainWindowHWND();

#define FOURCC_AYUV MAKEFOURCC('A','Y','U','V')
#define FOURCC_YUY2 MAKEFOURCC('Y','U','Y','2')
#define FOURCC_UYVY MAKEFOURCC('U','Y','V','Y')
#define FOURCC_IMC1 MAKEFOURCC('I','M','C','1')
#define FOURCC_IMC3 MAKEFOURCC('I','M','C','3')
#define FOURCC_IMC2 MAKEFOURCC('I','M','C','2')
#define FOURCC_IMC4 MAKEFOURCC('I','M','C','4')
#define FOURCC_NV12 MAKEFOURCC('N','V','1','2')
#define FOURCC_YV12 MAKEFOURCC('Y','V','1','2')
