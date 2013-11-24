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

#include <vector>
#include <utility>
#include <mutex>

enum EventType {
  VideoDecoder_Loaded,
  VideoDecoder_Error,

  VideoPlayer_Error,
  VideoPlayer_Loaded,
  VideoPlayer_Playing,
  VideoPlayer_Paused,

  PlaybackClock_Ended
};

typedef std::function<void(void)> EventListener;

class EventSource {
public:
  virtual void AddEventListener(EventType aEvent, EventListener aListener) final;
protected:
  virtual void NotifyListeners(EventType aEvent) final;
private:
  std::mutex mMutex;
  std::vector< std::pair<EventType, EventListener> > mListeners;
};
