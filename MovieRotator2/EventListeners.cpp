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
#include "EventListeners.h"
#include "MovieRotator2.h"

void
EventSource::AddEventListener(EventType aEvent, EventListener aListener)
{
  std::lock_guard<std::mutex> lock(mMutex);
  mListeners.push_back(make_pair(aEvent, aListener));
}

void
EventSource::NotifyListeners(EventType aEvent)
{
  std::lock_guard<std::mutex> lock(mMutex);
  for (auto itr = mListeners.begin(); itr != mListeners.end(); itr++) {
    EventType type = itr->first;
    if (type == aEvent) {
      EventListener* n = new EventListener(itr->second);
      PostMessage(GetMainWindowHWND(), MSG_CALL_FUNCTION, 0, (LPARAM)n);
    }
  }
}
