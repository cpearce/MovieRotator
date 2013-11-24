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

#include "cubeb\cubeb.h"
#include "EventListeners.h"

// Inteface to define behaviour of a clock that runs playback.
class PlaybackClock : public EventSource {
public:
  // TODO: a virtual destructor?

  // Starts the clock running. It will advance the position, and playback
  // content if it's an audio clock.
  virtual HRESULT Start() = 0;

  // Pauses the clock.
  virtual HRESULT Pause() = 0;

  // Shutsdown the clock. Synchronous!
  virtual HRESULT Shutdown() = 0;

  // Returns the position in 100-nanosecond units.
  virtual HRESULT GetPosition(LONGLONG* aOutPosition) = 0;
};

class AutoInitCubeb {
public:
  AutoInitCubeb();
  ~AutoInitCubeb();
};

class VideoDecoder;

HRESULT
CreateAudioPlaybackClock(VideoDecoder* aDecoder,
                         PlaybackClock** aOutPlaybackClock);

HRESULT
CreateSystemPlaybackClock(VideoDecoder* aDecoder,
                          PlaybackClock** aOutPlaybackClock);
