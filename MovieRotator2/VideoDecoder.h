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

#include "EventListeners.h"

struct MediaSample {
  MediaSample(IMFSample* aSample,
              LONGLONG aTimestamp,
              DWORD aFlags,
              IMFMediaType* aType)
    : sample(aSample),
      timestamp(aTimestamp),
      flags(aFlags),
      type(aType)
  {}
  ~MediaSample() {}

  // The media sample itself.
  IMFSamplePtr sample;

  // Timestamp of the sample, in 100-nanosecond units.
  LONGLONG timestamp;

  // Flags, as reported by IMFSourceReader::ReadSample().
  // Bitwise or of MF_SOURCE_READER_FLAG enumeration.
  DWORD flags;

  // Media type of the sample. This may be share by subsequent media samples,
  // so don't change it!
  IMFMediaTypePtr type;
};

typedef std::queue< MediaSample* > MediaSampleQueue;

// The amount of decoded audio we try to keep in our audio queue.
#define AUDIO_SAMPLE_QUEUE_TARGET_MS 1000

// The number of decoded video frames we try to keep in our queue.
#define VIDEO_SAMPLE_QUEUE_TARGET_FRAMES 2

class VideoDecoder : public EventSource {
public:
  VideoDecoder(HWND aEventTarget,
               const std::wstring& aFilename,
               IDirect3DDeviceManager9* aDeviceManager,
               IDirect3D9* aD3D9);
  ~VideoDecoder();

  // Begins the decode. The decode automatically throttles itself once its
  // buffers are full.
  HRESULT Begin();

  // Shutsdown the decode. Synchronous!
  HRESULT Shutdown();

  // Public threadsafe methods to determine if we have audio and video,
  // and whether said streams have finished decoding. (Returns false
  // if we had a stream, and decode has reached the end).
  bool HasAudio();
  bool HasVideo();

  // Returns a copy of the current media types. Note: copies can become out
  // of date if decoding is in progress...
  HRESULT GetAudioMediaType(IMFMediaType** aOutType);
  HRESULT GetVideoMediaType(IMFMediaType** aOutType);

  // Pops and returns the audio MediaSample at the front of the queue, or nullptr
  // if the queue is empty. Caller must delete the sample. Threadsafe.
  // If aBlocking==true, this call blocks until a sample is available.
  MediaSample* PopAudio(bool aBlocking);

  // Pops and returns the video MediaSample at the front of the queue, or nullptr
  // if the queue is empty. Caller must delete the sample. Threadsafe.
  MediaSample* PopVideo();

  // Returns the video MediaSample at the front of the queue without popping,
  // or nullptr if the queue is empty. Caller must *not* delete the sample. Threadsafe.
  MediaSample* PeekVideo();

  // Returns duration in hundred nanosecond units.
  uint64_t GetDuration();

  // Note: Pop audio must maintian audio decoded counter!

  // Called on the decode thread. Don't call this.
  void Run();

private:

  // Initializes the SourceReader.
  HRESULT LoadMetadata();

  HRESULT CreateReader(bool aWithVideoProcessing);

  // High-level driver of the decode process.
  HRESULT Decode();

  // Returns true if we are/should shutdown.
  // Caller must acquire the mMutex first.
  bool IsShutdown();

  // Returns true if the audio queue is full, i.e. if we don't need to
  // decode audio. Caller must acquire the mMutex first.
  bool IsAudioQueueFull();

  // Returns true if the video queue is full, i.e. if we don't need to
  // decode video. Caller must acquire the mMutex first.
  bool IsVideoQueueFull();

  // Decodes one audio sample, pushing it onto the queue.
  HRESULT DecodeAudio();

  // Decodes one video sample, pushing it onto the queue.
  HRESULT DecodeVideo();

  // Filename of resource we're decoding.
  const std::wstring mFilename;

  // HWND of the main window.
  HWND mEventTarget;

  // The decode thread.
  std::thread mThread;

  // Mutex to protect data shared between decode and main thread.
  std::mutex mMutex;

  // Condition variable which the decode thread waits on while the sample
  // queues are full, or until we're shutdown.
  std::condition_variable mCondVar;

  // The output media types.
  // Synchronized by mMutex.
  IMFMediaTypePtr mAudioType;
  IMFMediaTypePtr mVideoType;

  // Queues of decoded media samples.
  // Synchronized by mMutex.
  MediaSampleQueue mVideoQueue;
  MediaSampleQueue mAudioQueue;

  // The following are accessed on the decode thread only:
  IMFSourceReaderPtr mReader;
  DWORD mAudioStreamIndex;
  DWORD mVideoStreamIndex;

  // The D3D9 device manager, so that we can have hardware accelerated video
  // decoding goodness! :D
  IDirect3DDeviceManager9Ptr mDeviceManager;

  // Store the D3D9 object so that we can query to see it can handle
  // YUV->RGB conversion.
  IDirect3D9Ptr mD3D9;

  // Duration of audio in mAudioQueue. This is maintained as we push/pop
  // audio samples. We keep this tally so that we can easily tell whether
  // we need to decode more data or not.
  // Synchronized by mMutex.
  LONGLONG mEnqueuedAudioDuration;

  // Synchronized by mMutex.
  uint64_t mDuration;

  // True if we either don't have an audio stream, or if we do and it's
  // finished decoding. Shared across threads. Only written with lock
  // held on the decode thread, so can be read on decode thread without
  // lock, but can only be read off-decode-thread if the lock is acquired.
  bool mHasAudio;
  bool mHasVideo;

  // Flag to denote that we should shutdown.
  // Synchronized by mMutex.
  bool mShutdown;
};
