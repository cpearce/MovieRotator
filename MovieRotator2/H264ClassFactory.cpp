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

#include "H264ClassFactory.h"
#include <assert.h>


// See: http://social.msdn.microsoft.com/Forums/en-US/mediafoundationdevelopment/thread/6da521e9-7bb3-4b79-a2b6-b31509224638
class H264ClassFactory : public IClassFactory
{
private:
  volatile long           mRefCount;     // Reference count.
  static volatile long    sServerLocks;  // Number of server locks

public:

  H264ClassFactory() : mRefCount(0) {
  }

  virtual ~H264ClassFactory() {
  }

  static bool IsLocked() {
    return (sServerLocks != 0);
  }

  // IUnknown methods
  STDMETHODIMP_(ULONG) AddRef()
  {
    return InterlockedIncrement(&mRefCount);
  }

  STDMETHODIMP_(ULONG) Release()
  {
    assert(mRefCount >= 0);
    ULONG uCount = InterlockedDecrement(&mRefCount);
    if (uCount == 0) {
      delete this;
    }
    // Return the temporary variable, not the member
    // variable, for thread safety.
    return uCount;
  }

  STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
  {
    if (NULL == ppv)  {
      return E_POINTER;
    } else if (riid == __uuidof(IUnknown)) {
      *ppv = static_cast<IUnknown*>(this);
    } else if (riid == __uuidof(IClassFactory)) {
      *ppv = static_cast<IClassFactory*>(this);
    } else {
      *ppv = NULL;
      return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
  }

  STDMETHODIMP CreateInstance( IUnknown *pUnkOuter, REFIID riid, void **ppv )
  {
    HRESULT hr = S_OK;
    // If the caller is aggregating the object, the caller may only request
    // IUknown. (See MSDN documenation for IClassFactory::CreateInstance.)
    if (pUnkOuter != NULL)  {
      if (riid != __uuidof(IUnknown)) {
        return E_NOINTERFACE;
      }
    }

    IMFTransformPtr pEncoder;
    hr = FindEncoderEx(MFVideoFormat_H264, &pEncoder);
    ENSURE_SUCCESS(hr, hr);

    ICodecAPIPtr pCodecApi = pEncoder; // QIs
    ENSURE_TRUE(pCodecApi, E_FAIL);

    VARIANT var;
    VariantInit(&var);
    var.vt = VT_UI4;
    var.ulVal = eAVEncCommonRateControlMode_UnconstrainedVBR;
    hr = pCodecApi->SetValue(&CODECAPI_AVEncCommonRateControlMode, &var);
    ENSURE_SUCCESS(hr, hr);
    VariantClear(&var);

    VariantInit(&var);
    var.vt = VT_UI4;
    var.ulVal = 100;
    hr = pCodecApi->SetValue(&CODECAPI_AVEncCommonQuality, &var);
    ENSURE_SUCCESS(hr, hr);
    VariantClear(&var);

    hr = pEncoder->QueryInterface(riid, ppv);
    ENSURE_SUCCESS(hr, hr);

    return hr;
  }

  HRESULT FindEncoderEx( const GUID& subtype, IMFTransform **ppEncoder )
  {
    HRESULT hr = S_OK;
    UINT32 count = 0;

    IMFActivate **ppActivate = NULL;

    MFT_REGISTER_TYPE_INFO info = { 0 };

    info.guidMajorType = MFMediaType_Video;
    info.guidSubtype = subtype;

    hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER,
                   MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER,
                   NULL,
                   &info,
                   &ppActivate,
                   &count);

    if (SUCCEEDED(hr) && count == 0) {
      hr = MF_E_TOPO_CODEC_NOT_FOUND;
    }

    // Create the first encoder in the list.
    if (SUCCEEDED(hr))  {
      hr = ppActivate[0]->ActivateObject(IID_PPV_ARGS(ppEncoder));
    }

    for (UINT32 i = 0; i < count; i++)  {
      ppActivate[i]->Release();
    }
    CoTaskMemFree(ppActivate);

    return hr;
  }

  STDMETHODIMP LockServer(BOOL lock)
  {
    if (lock) {
      LockServer();
    } else {
      UnlockServer();
    }
    return S_OK;
  }

  // Static methods to lock and unlock the the server.
  static void LockServer()
  {
    InterlockedIncrement(&sServerLocks);
  }

  static void UnlockServer()
  {
    InterlockedDecrement(&sServerLocks);
  }

};

volatile long H264ClassFactory::sServerLocks = 0;

AutoRegisterH264ClassFactory::AutoRegisterH264ClassFactory()
{
  mClassFactory = new H264ClassFactory();
  MFT_REGISTER_TYPE_INFO infoInput = { MFMediaType_Video, MFVideoFormat_RGB32 };
  MFT_REGISTER_TYPE_INFO infoOutput = { MFMediaType_Video, MFVideoFormat_H264 };
  MFTRegisterLocal(mClassFactory, MFT_CATEGORY_VIDEO_ENCODER, L"ClassFactory", 0, 1, &infoInput, 1, &infoOutput);
}

AutoRegisterH264ClassFactory::~AutoRegisterH264ClassFactory()
{
  MFTUnregisterLocal(mClassFactory);
  mClassFactory = nullptr;
}
