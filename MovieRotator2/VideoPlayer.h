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

#include "Interfaces.h"
#include "VideoDecoder.h"
#include "PlaybackClocks.h"
#include "VideoPainter.h"
#include "EventListeners.h"

// Manages the process of decoding and specifically painting the video frames.
class VideoPlayer : public EventSource {
public:
  VideoPlayer(HWND aEventTarget, HINSTANCE hInst,
              FLOAT aX, FLOAT aY, FLOAT aWidth, FLOAT aHeight);
  ~VideoPlayer();

  HRESULT Init();

  HRESULT Load(const std::wstring& aFilename);
  HRESULT Play();
  HRESULT Pause();

  // Shutsdown the decoder and player. Synchronous!
  HRESULT Shutdown();

  // Rotates 90 degrees in specified direction.
  void RotateClockwise();
  void RotateAntiClockwise();

  bool IsPaused();
  bool IsVideoLoaded();
  const std::wstring& GetOpenFilename() const;
  Rotation GetRotation() const;

  // Closes the currently open video file.
  void Reset();

  void OnDecoderLoaded();
  void OnDecoderError();
  void OnPlaybackEnded();

private:

  HRESULT CreateDecoder(const std::wstring& aFilename);

  std::wstring mOpenFilename;
  bool mHaveVideoOpen;

  HWND mEventTarget;

  Rotation mRotation;
  bool mIsPaused;
  bool mPlayOnLoad;

  VideoPainter mPainter;

  VideoDecoder* mDecoder;
  PlaybackClock* mClock;

};
