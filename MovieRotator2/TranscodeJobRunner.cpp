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
#include "TranscodeJobRunner.h"
#include "TranscodeJobList.h"
#include "RotationTranscoder.h"

using std::wstring;


void
TranscodeJobRunner::DoTranscode()
{
  DBGMSG(L"New transcode starting:\n");
  DBGMSG(L"Input: %s\n", mJob->GetInputFilename().c_str());
  DBGMSG(L"Output: %s\n", mJob->GetOutputFilename().c_str());
  DBGMSG(L"Rotation: %d\n", mJob->GetRotation());
  AutoComInit x1;

  RotationTranscoder transcoder(mJob);

  HRESULT hr = transcoder.Initialize();
  if (FAILED(hr)) {
    DBGMSG(L"Failed to initialize transcode\n");
    PostMessage(mEventTarget,
                MSG_TRANSCODE_FAILED,
                0,
                (LPARAM)(mJob));
  }
  ENSURE_SUCCESS(hr,);

  // Note: Report a 1% progress event so that the progress UI
  // shows that the job has started.
  PostMessage(mEventTarget,
              MSG_TRANSCODE_PROGRESS,
              1,
              (LPARAM)(mJob));

  UINT32 progressAtLastReport = 1;
  UINT32 progress = 0;
  uint64_t start = GetTickCount64_DLL();
  while (progress < 1000 && !IsCanceled()) {
    hr = transcoder.Transcode();
    if (FAILED(hr)) {
      PostMessage(mEventTarget,
                  MSG_TRANSCODE_FAILED,
                  0,
                  (LPARAM)(mJob));
      break;
    }
    ENSURE_SUCCESS(hr,);


    progress = transcoder.GetProgress();
    if (progress > progressAtLastReport) {
      progressAtLastReport = progress;
      PostMessage(mEventTarget,
                  MSG_TRANSCODE_PROGRESS,
                  progress,
                  (LPARAM)(mJob));
    }
  }
  uint64_t elapsed = GetTickCount64_DLL() - start;

  DBGMSG(L"Trancode finished took %lld ms\n", elapsed);


  PostMessage(mEventTarget,
              MSG_TRANSCODE_COMPLETE,
              0,
              (LPARAM)(mJob));
}

TranscodeJobRunner::TranscodeJobRunner(TranscodeJob* aJob,
                                       HWND aEventTarget)
  : mEventTarget(aEventTarget),
    mJob(aJob),
    mIsCanceled(false)
{
}

TranscodeJobRunner::~TranscodeJobRunner()
{
  Cancel();
  if (mThread.joinable()) {
    mThread.join();
  }
}

HRESULT
TranscodeJobRunner::Begin()
{
  mThread = std::thread(&TranscodeJobRunner::DoTranscode, this);
  return S_OK;
}

HRESULT
TranscodeJobRunner::Cancel()
{
  std::lock_guard<std::mutex> lock(mMutex);
  mIsCanceled = true;
  return S_OK;
}

bool
TranscodeJobRunner::IsCanceled()
{
  std::lock_guard<std::mutex> lock(mMutex);
  return mIsCanceled;
}
