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
#include <thread>
#include <mutex>

class TranscodeJob;

// Manages the transcode, which is run in a worker thread.
// Thre thread posts a MSG_TRANSCODE_COMPLETE event just before it exits.
//
// Events:
//
// MSG_TRANSCODE_COMPLETE: posted when when the transcode job's
// worker thread finishes. This is sent if a job either runs to
// completion, or is canceled.
// wParam = 0, lParam = TranscodeJob* of job.
//
// MSG_TRANSCODE_PROGRESS posted every 0.1 percentage point of progress
// through transcode job.
// wParam = percent complete, lParam = TranscodeJob* of job.
//
class TranscodeJobRunner : public EventSource {
public:
  TranscodeJobRunner(TranscodeJob* aJob,
                     HWND aEventTarget);
  ~TranscodeJobRunner();

  // Starts the thread that runs the job.
  HRESULT Begin();

  // Cancels the job. Threadsafe.
  HRESULT Cancel();

  // Returns true if the job is canceled. Threadsafe.
  bool IsCanceled();

  // Performs the TranscodeJob to encode the rotation. This is called on
  // the worker thread. Don't call this directly, call Begin() instead.
  void DoTranscode();

  // The event target we dispatch messages to.
  const HWND mEventTarget;

  // This is guaranteed by our creator to be kept alive up until we post the
  // MSG_TRANSCODE_COMPLETE message.
  const TranscodeJob* mJob;

private:

  std::mutex mMutex;
  std::thread mThread;
  bool mIsCanceled;
};
