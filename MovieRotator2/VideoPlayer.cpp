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
#include "VideoPlayer.h"
#include "VideoDecoder.h"
#include "PlaybackClocks.h"

using std::wstring;

VideoPlayer::VideoPlayer(HWND aEventTarget, HINSTANCE hInst, FLOAT aX, FLOAT aY, FLOAT aWidth, FLOAT aHeight)
  : mEventTarget(aEventTarget),
    mPainter(aEventTarget, hInst, aX, aY, aWidth, aHeight),
    mDecoder(nullptr),
    mIsPaused(true),
    mHaveVideoOpen(false),
    mPlayOnLoad(false)
{
}

VideoPlayer::~VideoPlayer()
{
}

HRESULT
VideoPlayer::Init()
{
  return mPainter.Init();
}

HRESULT
VideoPlayer::CreateDecoder(const wstring& aFilename)
{
  HRESULT hr;

  IDirect3DDeviceManager9Ptr deviceManager;
  hr = mPainter.GetDeviceManager(&deviceManager);
  ENSURE_SUCCESS(hr, hr);

  IDirect3D9Ptr d3d9;
  hr = mPainter.GetD3D9(&d3d9);
  ENSURE_SUCCESS(hr, hr);

  mDecoder = new VideoDecoder(mEventTarget, aFilename, deviceManager, d3d9);
  mDecoder->AddEventListener(VideoDecoder_Loaded,
                             std::bind(&VideoPlayer::OnDecoderLoaded, this));
  mDecoder->AddEventListener(VideoDecoder_Error,
                             std::bind(&VideoPlayer::OnDecoderError, this));

  hr = mDecoder->Begin();
  ENSURE_SUCCESS(hr, hr);

  return S_OK;
}

HRESULT
VideoPlayer::Load(const wstring& aFilename)
{
  HRESULT hr;

  // Shutdown any exiting playback.
  hr = Shutdown();
  ENSURE_SUCCESS(hr, hr);

  // Start a new decoder.
  hr = CreateDecoder(aFilename);
  ENSURE_SUCCESS(hr, hr);

  mPlayOnLoad = true;
  mOpenFilename = aFilename;

  return S_OK;
}

HRESULT
VideoPlayer::Play()
{
  if (!mClock) {
    return E_FAIL;
  }
  if (!mIsPaused) {
    return S_OK;
  }
  mClock->Start();
  mIsPaused = false;
  NotifyListeners(VideoPlayer_Playing);
  return S_OK;
}

HRESULT
VideoPlayer::Pause()
{
  DBGMSG(L"VideoPlayer::Pause()\n");

  if (!mClock) {
    return E_FAIL;
  }
  if (mIsPaused) {
    return S_OK;
  }
  mClock->Pause();
  mIsPaused = true;

  NotifyListeners(VideoPlayer_Paused);

  return S_OK;
}

bool
VideoPlayer::IsPaused()
{
  return mIsPaused;
}

HRESULT
VideoPlayer::Shutdown()
{
  DBGMSG(L"VideoPlayer::Shutdown()\n");
  Pause();
  mHaveVideoOpen = false;
  mPlayOnLoad = false;
  mRotation = ROTATE_0;
  mOpenFilename.clear();
  mPainter.Reset();
  if (mClock) {
    mClock->Shutdown();
    delete mClock;
    mClock = nullptr;
  }
  if (mDecoder) {
    mDecoder->Shutdown();
    delete mDecoder;
    mDecoder = nullptr;
  }

  return S_OK;
}

void
VideoPlayer::Reset()
{
  Pause();
  mPainter.Reset();
  if (mClock) {
    mClock->Shutdown();
    delete mClock;
    mClock = nullptr;
  }
  if (mDecoder) {
    mDecoder->Shutdown();
    delete mDecoder;
    mDecoder = nullptr;
  }
  mPlayOnLoad = false;
}

void
VideoPlayer::RotateClockwise()
{
  mPainter.RotateClockwise();
  mRotation = (Rotation)((mRotation + 1) % 4);
}

void
VideoPlayer::RotateAntiClockwise()
{
  mPainter.RotateAntiClockwise();
  mRotation = (Rotation)((mRotation + 3) % 4);
}

void
VideoPlayer::OnDecoderError()
{
  Shutdown();
  NotifyListeners(VideoPlayer_Error);
}

void
VideoPlayer::OnDecoderLoaded()
{
  HRESULT hr;
  DBGMSG(L"VideoPlayer::OnLoaded\n");

  if (!mDecoder->HasAudio()) {
    hr = CreateSystemPlaybackClock(mDecoder, &mClock);
    ENSURE_SUCCESS(hr, );
  } else {
    hr = CreateAudioPlaybackClock(mDecoder, &mClock);
    ENSURE_SUCCESS(hr, );
  }
  ENSURE_TRUE(mClock, );
  mClock->AddEventListener(PlaybackClock_Ended,
    std::bind(&VideoPlayer::OnPlaybackEnded, this));
  mHaveVideoOpen = true;
  mPainter.SetInputs(mDecoder, mClock, mRotation);

  // Inform the application.
  NotifyListeners(VideoPlayer_Loaded);

  if (mPlayOnLoad) {
    Play();
    mPlayOnLoad = false;
  }
}

void
VideoPlayer::OnPlaybackEnded()
{
  DBGMSG(L"VideoPlayer::OnPlaybackEnded\n");
  Reset();
  CreateDecoder(mOpenFilename);
}

bool
VideoPlayer::IsVideoLoaded()
{
  return mHaveVideoOpen;
}


const wstring&
VideoPlayer::GetOpenFilename() const
{
  return mOpenFilename;
}

Rotation
VideoPlayer::GetRotation() const
{
  return mRotation;
}

