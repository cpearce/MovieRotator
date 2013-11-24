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
#include "PlaybackClocks.h"
#include "VideoDecoder.h"

using std::vector;
using std::unique_ptr;


static cubeb* sCubeb = nullptr;

AutoInitCubeb::AutoInitCubeb()
{
 cubeb_init(&sCubeb, "MovieRotator");
}

AutoInitCubeb::~AutoInitCubeb()
{
  cubeb_destroy(sCubeb);
}

class LeftoverDataBuffer {
public:
  LeftoverDataBuffer() :
    mOffset(0),
    mLength(0),
    mCapacity(32*1024)
  {
    mData = new short[mCapacity];
  }
  ~LeftoverDataBuffer() {
    delete mData;
  }

  unsigned Length() const {
    return mLength;
  }

  const short* Data() {
    return mData + mOffset;
  }

  void MarkConsumed(unsigned n) {
    mOffset += n;
    mLength -= n;
    if (mLength == 0) {
      mOffset = 0;
    }
  }

  HRESULT AddData(short* aData, unsigned n) {
    ENSURE_TRUE(mOffset == 0 && Length() == 0, E_FAIL);
    if (n > mCapacity) {
      delete mData;
      mCapacity = n;
      mData = new short[mCapacity];
    }
    memcpy(mData, aData, n*sizeof(short));
    mLength = n;
    return S_OK;
  }

private:
  short* mData;

  // Index in mData were actual data begins.
  unsigned mOffset;

  // Number of bytes stored after mOffset.
  unsigned mLength;

  // Total number of bytes in the storage.
  unsigned mCapacity;
};

class CubebAudioClock : public PlaybackClock
{
public:
  CubebAudioClock(VideoDecoder* aDecoder,
                  UINT32 aRate,
                  UINT32 aBytesPerSample,
                  UINT32 aChannels)
    : mDecoder(aDecoder),
      mRate(aRate),
      mBytesPerSample(aBytesPerSample),
      mChannels(aChannels),
      mStream(nullptr),
      mEOS(false),
      mIsPaused(true)
  {}

  ~CubebAudioClock() {
  }

  HRESULT Init();

  // PlaybackClock.
  HRESULT Start() override;
  HRESULT Pause() override;
  HRESULT Shutdown() override;
  HRESULT GetPosition(LONGLONG* aOutPosition) override;

  long OnDataCallback(void* buffer, long nframes);
  void OnStateCallback(cubeb_state state);

private:
  VideoDecoder* mDecoder;
  UINT32 mRate;
  UINT32 mChannels;
  UINT32 mBytesPerSample;
  cubeb_stream* mStream;
  LeftoverDataBuffer mLeftovers;
  bool mEOS;
  bool mIsPaused;
};

long
CubebAudioClock::OnDataCallback(void* aBuffer, long nframes)
{
  assert(sizeof(short) == mBytesPerSample);
  long samplesToWrite = nframes * mChannels;
  long framesWritten = 0;
  short* outSamples = (short*)(aBuffer);
  memset(outSamples, 0, sizeof(short)*samplesToWrite);

  // Copy and remaining samples from the previous decoded
  // IMFSample we pulled.
  if (mLeftovers.Length() > 0) {
    long toWrite = min(mLeftovers.Length(), samplesToWrite);
    memcpy(outSamples, mLeftovers.Data(), toWrite*sizeof(short));
    samplesToWrite -= toWrite;
    outSamples += toWrite;
    mLeftovers.MarkConsumed(toWrite);
    framesWritten += toWrite / mChannels;
  }
  if (samplesToWrite == 0) {
    return framesWritten;
  }

  if (mEOS) {
    return framesWritten;
  }

  // We should only get this far if we consumed all the previous leftover data.
  assert(mLeftovers.Length() == 0);

  while (samplesToWrite > 0) {

    // Otherwise, we've still not written a full buffer, get another MediaBuffer
    // from the decoder.
    unique_ptr<MediaSample> mediaSample(mDecoder->PopAudio(true));

    if (!mediaSample ||
        mediaSample->flags & MF_SOURCE_READERF_ENDOFSTREAM) {
      mEOS = true;
    }
    if (!mediaSample) {
      return framesWritten;
    }

    IMFMediaBufferPtr buffer;
    HRESULT hr = mediaSample->sample->ConvertToContiguousBuffer(&buffer);
    ENSURE_SUCCESS(hr, -1);

    BYTE* data = nullptr; // Note: *data will be owned by the IMFMediaBuffer, we don't need to free it.
    DWORD maxLength = 0, currentLength = 0;
    hr = buffer->Lock(&data, &maxLength, &currentLength);
    ENSURE_SUCCESS(hr, -1);

    uint32_t numSamples = currentLength / mBytesPerSample;

    unsigned toWrite = min(samplesToWrite, numSamples);
    memcpy(outSamples, data, sizeof(short) * toWrite);
    outSamples += toWrite;
    samplesToWrite -= toWrite;
    framesWritten += toWrite / mChannels;

    if (toWrite < numSamples) {
      // We have leftovers, copy them into the leftovers buffer.
      hr = mLeftovers.AddData((short*)(data) + toWrite, numSamples - toWrite);
      ENSURE_SUCCESS(hr, -1);
    } else {
      // Insufficient data to complete buffer.
      if (mEOS) {
        buffer->Unlock();
        break;
      }
      buffer->Unlock();
    }
  }

  return framesWritten;
}

void
CubebAudioClock::OnStateCallback(cubeb_state state)
{
  const wchar_t* s = L"?";
  switch (state) {
    case CUBEB_STATE_STARTED: s = L"CUBEB_STATE_STARTED"; break;
    case CUBEB_STATE_STOPPED: s = L"CUBEB_STATE_STOPPED"; break;
    case CUBEB_STATE_DRAINED: s = L"CUBEB_STATE_DRAINED"; {
      NotifyListeners(PlaybackClock_Ended);
      break;
    }
    case CUBEB_STATE_ERROR: s = L"CUBEB_STATE_ERROR"; break;
  }
  DBGMSG(L"CubebAudioClock::OnStateCallback() state=%s\n", s);
}

long
CubebDataCallback(cubeb_stream * stm, void * user, void * buffer, long nframes)
{
  CubebAudioClock* x = (CubebAudioClock*)user;
  return x->OnDataCallback(buffer, nframes);
}

void
CubebStateCallback(cubeb_stream * stm, void * user, cubeb_state state)
{
  CubebAudioClock* x = (CubebAudioClock*)user;
  x->OnStateCallback(state);
}

HRESULT
CubebAudioClock::Init()
{
  cubeb_stream_params params;
  params.format = CUBEB_SAMPLE_S16NE;
  params.rate = mRate;
  params.channels = mChannels;
  int res = cubeb_stream_init(sCubeb,
                              &mStream,
                              "MovieRotator Stream",
                              params,
                              250,
                              CubebDataCallback,
                              CubebStateCallback,
                              (void*)this);
  if (res != CUBEB_OK) {
    return E_FAIL;
  }
  return S_OK;
}

HRESULT
CubebAudioClock::Start()
{
  if (mIsPaused) {
    cubeb_stream_start(mStream);
    mIsPaused = false;
  }
  return S_OK;
}

HRESULT
CubebAudioClock::Pause()
{
  if (!mIsPaused) {
    cubeb_stream_stop(mStream);
    mIsPaused = true;
  }
  return S_OK;
}

HRESULT
CubebAudioClock::Shutdown()
{
  Pause();
  cubeb_stream_destroy(mStream);
  return S_OK;
}


HRESULT
CubebAudioClock::GetPosition(LONGLONG* aOutPosition)
{
  uint64_t nframes;
  if (CUBEB_OK != cubeb_stream_get_position(mStream, &nframes)) {
    return E_FAIL;
  }
  // Cubeb reports the time in frames, but we require it in 100-nanosecond
  // units. Convert it.
  static const double HundredNsPerS = 10000000;
  double pos = double(nframes) * HundredNsPerS / double(mRate);
  *aOutPosition = (LONGLONG)pos;
  return S_OK;
}

HRESULT
CreateAudioPlaybackClock(VideoDecoder* aDecoder,
                         PlaybackClock** aOutPlaybackClock)
{
  HRESULT hr;
  IMFMediaTypePtr type;

  hr = aDecoder->GetAudioMediaType(&type);
  ENSURE_SUCCESS(hr, hr);

  UINT32 rate = MFGetAttributeUINT32(type, MF_MT_AUDIO_SAMPLES_PER_SECOND, 0);
  UINT32 channels = MFGetAttributeUINT32(type, MF_MT_AUDIO_NUM_CHANNELS, 0);
  UINT32 bps = MFGetAttributeUINT32(type, MF_MT_AUDIO_BITS_PER_SAMPLE, 16) / 8;

  CubebAudioClock* clock = new CubebAudioClock(aDecoder, rate, bps, channels);
  hr = clock->Init();
  if (FAILED(hr)) {
    delete clock;
    return hr;
  }

  *aOutPlaybackClock = clock;

  return S_OK;
}



class SystemClock : public PlaybackClock
{
public:
  SystemClock(VideoDecoder* aDecoder)
    : mDecoder(aDecoder),
      mEOS(false),
      mIsPaused(true),
      mStart(0),
      mElapsed(0)
  {
    mDuration = aDecoder->GetDuration();
  }

  ~SystemClock() {
  }

  // PlaybackClock.
  HRESULT Start() override;
  HRESULT Pause() override;
  HRESULT Shutdown() override;
  HRESULT GetPosition(LONGLONG* aOutPosition) override;

private:
  VideoDecoder* mDecoder;

  uint64_t mStart;
  uint64_t mElapsed;
  uint64_t mDuration;

  bool mEOS;
  bool mIsPaused;

};

HRESULT
SystemClock::Start()
{
  if (mIsPaused) {
    mStart = GetTickCount64_DLL();
    mIsPaused = false;
  }
  return S_OK;
}

HRESULT
SystemClock::Pause()
{
  if (!mIsPaused) {
    uint64_t end = GetTickCount64_DLL();
    uint64_t elapsed = end - mStart;
    mElapsed += elapsed;
    mIsPaused = true;
  }
  return S_OK;
}

HRESULT
SystemClock::Shutdown()
{
  return S_OK;
}

HRESULT
SystemClock::GetPosition(LONGLONG* aOutPosition)
{
  ENSURE_TRUE(aOutPosition, E_POINTER);
  if (mIsPaused) {
    *aOutPosition = mElapsed;
    return S_OK;
  }
  uint64_t elapsed = mElapsed + GetTickCount64_DLL() - mStart;
  uint64_t hns = elapsed * 10000;
  if (hns > mDuration) {
    hns = mDuration;
    if (!mEOS) {
      mEOS = true;
      NotifyListeners(PlaybackClock_Ended);
    }
  }
  *aOutPosition = hns <= mDuration ? hns : mDuration;
  return S_OK;
}

HRESULT
CreateSystemPlaybackClock(VideoDecoder* aDecoder,
                          PlaybackClock** aOutPlaybackClock)
{
  *aOutPlaybackClock = new SystemClock(aDecoder);
  return S_OK;
}
