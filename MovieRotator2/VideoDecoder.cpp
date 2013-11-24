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
#include "VideoDecoder.h"
#include "Utils.h"
#include "Interfaces.h"

using std::wstring;
using std::thread;
using std::mutex;
using std::lock_guard;
using std::unique_lock;

VideoDecoder::VideoDecoder(HWND aEventTarget,
                           const std::wstring& aFilename,
                           IDirect3DDeviceManager9* aDeviceManager,
                           IDirect3D9* aD3D9)
  : mEventTarget(aEventTarget),
    mFilename(aFilename),
    mAudioStreamIndex(-1),
    mVideoStreamIndex(-1),
    mEnqueuedAudioDuration(0),
    mDuration(0),
    mDeviceManager(aDeviceManager),
    mD3D9(aD3D9),
    mShutdown(false),
    mHasAudio(false),
    mHasVideo(false)
{

}

VideoDecoder::~VideoDecoder()
{
}

static void
CallVideoDecoderRun(VideoDecoder* aDecoder)
{
  aDecoder->Run();
}

HRESULT
VideoDecoder::Begin()
{
  mThread = thread(CallVideoDecoderRun, this);
  return S_OK;
}

HRESULT
VideoDecoder::Shutdown()
{
  {
    lock_guard<mutex> guard(mMutex);
    mShutdown = true;
    mCondVar.notify_all();
  }
  if (mThread.joinable()) {
    mThread.join();
  }
  return S_OK;
}


// Returns the duration of the resource, in hundres of nanoseconds.
HRESULT
GetSourceReaderDuration(IMFSourceReader *aReader,
                        uint64_t& aOutDuration)
{
  AutoPropVar var;
  HRESULT hr = aReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE,
                                                 MF_PD_DURATION,
                                                 &var);
  ENSURE_TRUE(SUCCEEDED(hr), hr);

  int64_t duration_hns = 0;
  hr = PropVariantToInt64(var, &duration_hns);
  ENSURE_TRUE(SUCCEEDED(hr), hr);

  aOutDuration = duration_hns;

  return S_OK;
}

wstring
GetFileExtension(const wstring& aFilename)
{
  size_t seperator = aFilename.find_last_of(L".");
  if (seperator == wstring::npos) {
    return L"";
  }

  return wstring(aFilename, seperator, wstring::npos);
}

HRESULT
VideoDecoder::CreateReader(bool aWithVideoProcessing)
{
  HRESULT hr;

  IMFAttributesPtr attributes;
  hr = MFCreateAttributes(&attributes, 1);
  ENSURE_SUCCESS(hr, hr);

  if (aWithVideoProcessing) {
    hr = attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    ENSURE_SUCCESS(hr, hr);
  } else {
    hr = attributes->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, mDeviceManager);
    ENSURE_SUCCESS(hr, hr);
  }

  // Create the source mReader from the URL.
  hr = MFCreateSourceReaderFromURL(mFilename.c_str(), attributes, &mReader);
  ENSURE_SUCCESS(hr, hr);

  // Get stream indexes, disable other streams.
  hr = GetReaderStreamIndexes(mReader,
                              &mAudioStreamIndex,
                              &mVideoStreamIndex);
  ENSURE_SUCCESS(hr, hr);

  return S_OK;
}

HRESULT
VideoDecoder::LoadMetadata()
{
  HRESULT hr;

  hr = CreateReader(false);
  ENSURE_SUCCESS(hr, hr);

  // Set video stream output type.
  if (mVideoStreamIndex != -1) {
    IMFMediaTypePtr type;
    hr = MFCreateMediaType(&type);
    ENSURE_SUCCESS(hr, hr);

    hr = type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    ENSURE_SUCCESS(hr, hr);

    DWORD yuvTypes[] = {
      FOURCC_AYUV,
      FOURCC_YUY2,
      FOURCC_UYVY,
      FOURCC_NV12,
    };

    // Loop until we find a YUV type the reader can output.
    for (uint32_t i=0; i<ARRAYSIZE(yuvTypes); i++) {
      DWORD fourcc = yuvTypes[i];
      hr = mD3D9->CheckDeviceFormatConversion(D3DADAPTER_DEFAULT,
                                              D3DDEVTYPE_HAL,
                                              (D3DFORMAT)fourcc,
                                              D3DFMT_X8R8G8B8);
      if (FAILED(hr)) {
        // Can't convert this format to RGB using GPU.
        continue;
      }

      GUID videoFormat = MFVideoFormat_Base;
      videoFormat.Data1 = fourcc;

      hr = type->SetGUID(MF_MT_SUBTYPE, videoFormat);
      ENSURE_SUCCESS(hr, hr);

      hr = mReader->SetCurrentMediaType(mVideoStreamIndex, NULL, type);
      if (SUCCEEDED(hr)) {
        break;
      }
    }

    if (FAILED(hr)) {
      // Last chance, try to get the source reader to convert to RGB...
      hr = CreateReader(true);
      ENSURE_SUCCESS(hr, hr);

      hr = type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
      ENSURE_SUCCESS(hr, hr);

      hr = mReader->SetCurrentMediaType(mVideoStreamIndex, NULL, type);
      ENSURE_SUCCESS(hr, hr);
    }

    hr = mReader->GetCurrentMediaType(mVideoStreamIndex, &mVideoType);
    ENSURE_SUCCESS(hr, hr);

    // Notify the decoder that it should use hardware acceleration.
    IMFTransformPtr video_decoder;
    hr = mReader->GetServiceForStream(mVideoStreamIndex, GUID_NULL,
                                     IID_PPV_ARGS(&video_decoder));
    ENSURE_SUCCESS(hr, hr);

    hr = video_decoder->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER,
                                       reinterpret_cast<ULONG_PTR>(mDeviceManager.GetInterfacePtr()));
    if (FAILED(hr)) {
      DBGMSG(L"Decoder can't process MFT_MESSAGE_SET_D3D_MANAGER, no GPU accelerated decoding!");
    }

    lock_guard<mutex> lock(mMutex);
    mHasVideo = true;
  }

  // Set audio stream output type.
  if (mAudioStreamIndex != -1) {
    IMFMediaTypePtr type;
    hr = MFCreateMediaType(&type);
    ENSURE_SUCCESS(hr, hr);

    hr = type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    ENSURE_SUCCESS(hr, hr);

    hr = type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    ENSURE_SUCCESS(hr, hr);

    hr = mReader->SetCurrentMediaType(mAudioStreamIndex, NULL, type);
    ENSURE_SUCCESS(hr, hr);

    hr = mReader->GetCurrentMediaType(mAudioStreamIndex, &mAudioType);
    ENSURE_SUCCESS(hr, hr);

    {
      lock_guard<mutex> lock(mMutex);
      mHasAudio = true;
    }
    // Decode one sample. If the audio stream is HE-AAC the media type may not
    // take into account the effects of SBR/PS on the sample rate or number of
    // channels until after we've decoded the first sample.
    DecodeAudio();

  }

  hr = GetSourceReaderDuration(mReader, mDuration);

  return S_OK;
}

uint64_t
VideoDecoder::GetDuration()
{
  lock_guard<mutex> lock(mMutex);
  return mDuration;
}

bool
VideoDecoder::HasAudio()
{
  lock_guard<mutex> lock(mMutex);
  return mHasAudio || !mAudioQueue.empty();
}

bool
VideoDecoder::HasVideo()
{
  lock_guard<mutex> lock(mMutex);
  return mHasVideo || !mVideoQueue.empty();
}

bool
VideoDecoder::IsShutdown()
{
  return mShutdown;
}

bool
VideoDecoder::IsAudioQueueFull()
{
  return mEnqueuedAudioDuration > MStoHNS(AUDIO_SAMPLE_QUEUE_TARGET_MS);
}

bool
VideoDecoder::IsVideoQueueFull()
{
  return mVideoQueue.size() >= VIDEO_SAMPLE_QUEUE_TARGET_FRAMES;
}

HRESULT
VideoDecoder::DecodeAudio()
{
  ENSURE_TRUE(mHasAudio, E_FAIL);

  HRESULT hr;
  DWORD actualStreamIndex = 0;
  DWORD flags;
  LONGLONG timestamp;
  IMFSamplePtr sample;
  hr = mReader->ReadSample(mAudioStreamIndex, 0, &actualStreamIndex, &flags, &timestamp, &sample);
  ENSURE_SUCCESS(hr, hr);
  ENSURE_TRUE(!(flags & MF_SOURCE_READERF_ERROR), E_FAIL);

  if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
    lock_guard<mutex> lock(mMutex);
    mHasAudio = false;
  }

  // Null sample can sometimes just mean the decoder needed more data...
  if (!sample) {
    return S_FALSE;
  }

  if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
    hr = mReader->GetCurrentMediaType(mAudioStreamIndex, &mAudioType);
    ENSURE_SUCCESS(hr, hr);
  }

  LONGLONG duration = 0;
  hr = sample->GetSampleDuration(&duration);
  ENSURE_SUCCESS(hr, hr);

  MediaSample* m = new MediaSample(sample, timestamp, flags, mAudioType);
  {
    lock_guard<mutex> lock(mMutex);
    mEnqueuedAudioDuration += duration;
    mAudioQueue.push(m);
    mCondVar.notify_one();
  }

  return S_OK;
}

HRESULT
VideoDecoder::DecodeVideo()
{
  ENSURE_TRUE(mHasVideo, E_FAIL);

  HRESULT hr;
  DWORD actualStreamIndex = 0;
  DWORD flags;
  LONGLONG timestamp;
  IMFSamplePtr sample;
  hr = mReader->ReadSample(mVideoStreamIndex, 0, &actualStreamIndex, &flags, &timestamp, &sample);
  ENSURE_SUCCESS(hr, hr);
  ENSURE_TRUE(!(flags & MF_SOURCE_READERF_ERROR), E_FAIL);

  if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
    lock_guard<mutex> lock(mMutex);
    mHasVideo = false;
  }

  // Null sample can sometimes just mean the decoder needed more data...
  if (!sample) {
    return S_FALSE;
  }

  if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
    hr = mReader->GetCurrentMediaType(mVideoStreamIndex, &mVideoType);
    ENSURE_SUCCESS(hr, hr);
  }

  MediaSample* m = new MediaSample(sample, timestamp, flags, mVideoType);
  {
    lock_guard<mutex> lock(mMutex);
    mVideoQueue.push(m);
  }

  return S_OK;
}

HRESULT
VideoDecoder::Decode()
{
  HRESULT hr;
  while (true) {
    {
      // Check if we've shutdown.
      lock_guard<mutex> guard(mMutex);
      if (IsShutdown()) {
        return S_OK;
      }
    }

    // Audio Decode.
    if (mHasAudio) {
      bool audioQueueFull;
      {
        lock_guard<mutex> guard(mMutex);
        audioQueueFull = IsAudioQueueFull();
      }
      if (!audioQueueFull) {
        hr = DecodeAudio();
        ENSURE_SUCCESS(hr, hr);
      }
    }

    // Video Decode.
    if (mHasVideo) {
      bool videoQueueFull;
      {
        lock_guard<mutex> guard(mMutex);
        videoQueueFull = IsVideoQueueFull();
      }
      if (!videoQueueFull) {
        hr = DecodeVideo();
        ENSURE_SUCCESS(hr, hr);
      }
    }

    {
      // Wait while both sample queues are full and we're not shutdown.
      // We'll break out of this loop when we're shutdown, or when a sample
      // is popped off one of the sample queues.
      unique_lock<mutex> lock(mMutex);
      while (!IsShutdown() &&
             (!mHasAudio || IsAudioQueueFull()) &&
             (!mHasVideo || IsVideoQueueFull())) {
        mCondVar.wait(lock);
      }
    }
  }
  return S_OK;
}

// Called on the decode thread. Don't call this.
void
VideoDecoder::Run()
{
  DBGMSG(L"VideoDecoder decode thread started\n");
  AutoComInit initCOM;
  AutoWMFInit initWMF;
  HRESULT hr;

  hr = LoadMetadata();
  if (SUCCEEDED(hr)) {
    NotifyListeners(VideoDecoder_Loaded);
  } else {
    mReader = nullptr;
    NotifyListeners(VideoDecoder_Error);
    return;
  }

  hr = Decode();
  if (FAILED(hr)) {
    NotifyListeners(VideoDecoder_Error);
  }
}

MediaSample*
VideoDecoder::PopAudio(bool aBlocking)
{
  unique_lock<mutex> lock(mMutex);
  if (!mHasAudio) {
    return nullptr;
  }
  while (mAudioQueue.empty() &&
         !IsShutdown()) {
    if (!aBlocking) {
      return nullptr;
    }
    mCondVar.wait(lock);
  }
  MediaSample* m = mAudioQueue.front();
  mAudioQueue.pop();
  LONGLONG duration = 0;
  m->sample->GetSampleDuration(&duration);
  mEnqueuedAudioDuration -= duration;
  mCondVar.notify_one();
  return m;
}

MediaSample*
VideoDecoder::PeekVideo()
{
  lock_guard<mutex> guard(mMutex);
  if (mVideoQueue.empty()) {
    return nullptr;
  }
  return mVideoQueue.front();
}

MediaSample*
VideoDecoder::PopVideo()
{
  lock_guard<mutex> guard(mMutex);
  if (mVideoQueue.empty()) {
    return nullptr;
  }
  MediaSample* m = mVideoQueue.front();
  mVideoQueue.pop();
  mCondVar.notify_one();
  return m;
}

HRESULT
VideoDecoder::GetAudioMediaType(IMFMediaType** aOutType)
{
  ENSURE_TRUE(aOutType, E_POINTER);
  ENSURE_TRUE(mHasAudio, E_UNEXPECTED);
  IMFMediaTypePtr type;
  HRESULT hr = MFCreateMediaType(&type);
  ENSURE_SUCCESS(hr, hr);
  lock_guard<mutex> guard(mMutex);
  ENSURE_TRUE(mAudioType, E_POINTER);
  mAudioType->CopyAllItems(type);
  *aOutType = type.Detach();
  return S_OK;
}

HRESULT
VideoDecoder::GetVideoMediaType(IMFMediaType** aOutType)
{
  ENSURE_TRUE(aOutType, E_POINTER);
  ENSURE_TRUE(mHasVideo, E_UNEXPECTED);
  IMFMediaTypePtr type;
  HRESULT hr = MFCreateMediaType(&type);
  ENSURE_SUCCESS(hr, hr);
  lock_guard<mutex> guard(mMutex);
  ENSURE_TRUE(mVideoType, E_POINTER);
  mVideoType->CopyAllItems(type);
  *aOutType = type.Detach();
  return S_OK;
}
