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

// TranscodeJobList, manages a list of rotation-transcode jobs

#pragma once

#include "Utils.h"
#include "Interfaces.h"

typedef UINT32 TranscodeJobId;
#define TRANSCODE_JOB_INVALID_ID ((TranscodeJobId)(-1))

class TranscodeJob {
public:
  TranscodeJob(const std::wstring& aInputFilename,
               const std::wstring& aOutputFilename,
               Rotation aRotation);

  const std::wstring& GetInputFilename() const;

  const std::wstring& GetOutputFilename() const;

  TranscodeJobId GetJobId() const;

  Rotation GetRotation() const;

  // Retrieves the measure of progress through the job, in thousandths.
  // Note that we round up to 1/1000 and down to 999/1000, so therefore:
  //    0         = job pending
  //    [1,999]   = job running
  //    1000      = job complete.
  UINT32 GetProgress();

  // Sets how far through the transcode we have progressed, in units of
  // 1/1000ths of the job. Note that if you set 1000/1000 complete, this
  // job will be considered complete, and if you set this to between
  // 1 and 999 this job will be considered running.
  void SetProgress(UINT32 aProgress);

  // Jobs are identified by ID, not index, so that movements in the list
  // don't affect our ability to retrieve a job.
  TranscodeJobId GetId() const;

  bool IsFailed() const { return mIsFailed; }
  void SetFailed() { mIsFailed = true; }

  bool IsCanceled() { return mIsCanceled; }
  void SetCanceled() { mIsCanceled = true; }

private:
  TranscodeJobId mId;
  const std::wstring mInputFilename;
  const std::wstring mOutputFilename;
  const Rotation mRotation;
  UINT32 mProgress;
  bool mIsFailed;
  bool mIsCanceled;
};

class TranscodeJobRunner;

// List of jobs. This list assumes ownership of the jobs that are added to it.
// It is not threadsafe; updates to a jobs' progress must be performed on the
// main thread, via a message.
class TranscodeJobList : public EventHandler {
public:

  TranscodeJobList(HWND aHWnd);
  virtual ~TranscodeJobList();

  HRESULT AddJob(TranscodeJob* aJob);
  HRESULT GetJobById(TranscodeJobId aId, TranscodeJob** aOutJob);
  HRESULT GetJobByIndex(UINT32 aIndex, TranscodeJob** aOutJob);
  UINT32 GetLength() const;

  // We can only remove jobs, not cancel them.
  HRESULT RemoveJobByIndex(UINT32 aIndex);
  HRESULT RemoveJobById(TranscodeJobId aIndex);

  UINT32 GetNumPendingJobs() const;

  // Moves a job towards the front of the queue. Note that you can only move or
  // displace a non-running job.
  HRESULT MoveJobTowardsFront(UINT32 aIndex);

  // Moves a job towards the back of the queue. Note that you can only move or
  // displace a non-running job.
  HRESULT MoveJobTowardsBack(UINT32 aIndex);

  // Jobs send events to notify this thread of their progress, and to notify us
  // that they've shutdown.
  bool Handle(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

  bool IsJobMovable(UINT32 aIndex);

  // Whether a job is currently running
  bool IsRunning();

private:

  UINT32 GetIndexFor(TranscodeJobId aJobId);

  HRESULT DeleteJob(UINT32 aIndex);

  HRESULT EnsureJobRunning();

  TranscodeJobId mRunningJobId;

  std::vector<TranscodeJob*> mJobs;
  HWND mHWnd;
  TranscodeJobRunner* mTranscoder;
};

