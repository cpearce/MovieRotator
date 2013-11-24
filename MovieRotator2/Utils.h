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

#pragma once

class AutoComInit {
public:
  AutoComInit() {
    if (FAILED(CoInitializeEx(0, COINIT_MULTITHREADED))) {
      exit(-1);
    }
  }
  ~AutoComInit() {
    CoUninitialize();
  }
};

class AutoWMFInit {
public:
  AutoWMFInit() {
    const int MF_WIN7_VERSION = (0x0002 << 16 | MF_API_VERSION);
    if (FAILED(MFStartup(MF_WIN7_VERSION, MFSTARTUP_FULL))) {
      exit(-1);
    }
  }
  ~AutoWMFInit() {
    MFShutdown();
  }
};

void DBGMSG(PCWSTR format, ...);
HRESULT LogMediaType(IMFMediaType *pType);
void ErrorMsgBox(PCWSTR format, ...);

#define ENSURE_SUCCESS(hr, ret) \
{ if (FAILED(hr)) { DBGMSG(L"FAILED: hr=0x%x %S:%d\n", hr, __FILE__, __LINE__); return ret; } }

#define ENSURE_TRUE(condition, ret) \
{ if (!(condition)) { DBGMSG(L"##condition## FAILED %S:%d\n", __FILE__, __LINE__); return ret; } }

float OffsetToFloat(const MFOffset& offset);

#define SAFE_RELEASE(ptr) { if (ptr) { ptr->Release(); ptr = nullptr; } }

class Lock {
  friend class AutoLock;
public:
  Lock() {
    InitializeCriticalSection(&critSec);
  }
  ~Lock() {
    DeleteCriticalSection(&critSec);
  }
private:
  CRITICAL_SECTION critSec;
};

class AutoLock {
public:
  AutoLock(Lock& lock) : mLock(lock) {
    EnterCriticalSection(&mLock.critSec);
  }
  ~AutoLock() {
    LeaveCriticalSection(&mLock.critSec);
  }
private:
  Lock& mLock;
};

// Auto manages the lifecycle of a PROPVARIANT.
class AutoPropVar {
public:
  AutoPropVar() {
    PropVariantInit(&mVar);
  }
  ~AutoPropVar() {
    PropVariantClear(&mVar);
  }
  operator PROPVARIANT&() {
    return mVar;
  }
  PROPVARIANT* operator->() {
    return &mVar;
  }
  PROPVARIANT* operator&() {
    return &mVar;
  }
private:
  PROPVARIANT mVar;
};


bool
IsMajorType(IMFMediaType* aMediaType, const GUID& aMajorType);

bool
IsVideoType(IMFMediaType* aType);

bool
IsAudioType(IMFMediaType* aType);

// Gets the first audio and video stream, deselects all other streams.
// Sets indexes to -1 if there's no such stream.
HRESULT
GetReaderStreamIndexes(IMFSourceReader* aReader,
                       DWORD* aOutFirstAudioIndex,
                       DWORD* aOutFirstVideoIndex);

inline LONGLONG
MStoHNS(uint32_t aMilliSeconds)
{
  return LONGLONG(aMilliSeconds) * 10000;
}

uint64_t GetTickCount64_DLL();

HRESULT EnumerateTypesForStream(IMFSourceReader *pReader, DWORD dwStreamIndex);

//-----------------------------------------------------------------------------
// CorrectAspectRatio
//
// Converts a rectangle from the source's pixel aspect ratio (PAR) to 1:1 PAR.
// Returns the corrected rectangle.
//
// For example, a 720 x 486 rect with a PAR of 9:10, when converted to 1x1 PAR,
// is stretched to 720 x 540.
//-----------------------------------------------------------------------------

RECT CorrectAspectRatio(const RECT& src, const MFRatio& srcPAR);
MFOffset MakeOffset(float v);
MFVideoArea MakeArea(float x, float y, DWORD width, DWORD height);
HRESULT GetVideoDisplayArea(IMFMediaType *pType, MFVideoArea *pArea);
RECT RectFromArea(const MFVideoArea& area);
void GetPixelAspectRatio(IMFMediaType *pType, MFRatio *pPar);

HRESULT
GetDefaultStride(IMFMediaType *aType, LONG* aOutStride);

HRESULT
GetSourceReaderDuration(IMFSourceReader *aReader,
                        LONGLONG* aOutDuration);

HRESULT
CloneSample(IMFSample* aSample, IMFSample** aClone);

// Opens a special file.
// Use fputws(L"...", file) to write, and close with fclose(file);
enum SpecialPath {
  RegistrationKeyPath,
  LogFilePath
};

enum FileMode {
  Read,
  Write,
  Append
};

HRESULT
OpenSpecialFile(SpecialPath aPath, FileMode aMode, FILE** aOutFile);
