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
#include "TranscodeJobList.h"
#include "TranscodeJobRunner.h"
#include "D2DManager.h"

using std::wstring;

static TranscodeJobId sJobCounter = 0;

TranscodeJob::TranscodeJob(const wstring& aInputFilename,
                           const wstring& aOutputFilename,
                           Rotation aRotation)
  : mId(sJobCounter++),
    mInputFilename(aInputFilename),
    mOutputFilename(aOutputFilename),
    mRotation(aRotation),
    mProgress(0),
    mIsCanceled(false),
    mIsFailed(false)
{

}

const wstring&
TranscodeJob::GetInputFilename() const
{
  return mInputFilename;
}

const wstring&
TranscodeJob::GetOutputFilename() const
{
  return mOutputFilename;
}

TranscodeJobId
TranscodeJob::GetJobId() const
{
  return mId;
}

Rotation
TranscodeJob::GetRotation() const
{
  return mRotation;
}

UINT32
TranscodeJob::GetProgress()
{
  return mProgress;
}

void
TranscodeJob::SetProgress(UINT32 aProgress)
{
  assert(aProgress <= 1000);
  mProgress = aProgress;
}

TranscodeJobId
TranscodeJob::GetId() const
{
  return mId;
}


TranscodeJobList::TranscodeJobList(HWND aHWnd)
  : mRunningJobId(TRANSCODE_JOB_INVALID_ID),
    mTranscoder(nullptr),
    mHWnd(aHWnd)
{
}

TranscodeJobList::~TranscodeJobList()
{
}

HRESULT
TranscodeJobList::AddJob(TranscodeJob* aJob)
{
  mJobs.push_back(aJob);
  EnsureJobRunning();
  PostMessage(mHWnd, MSG_JOBLIST_UPDATE, 0, 0);
  D2DManager::Invalidate();
  return S_OK;
}

HRESULT
TranscodeJobList::GetJobById(TranscodeJobId aId, TranscodeJob** aOutJob)
{
  ENSURE_TRUE(aOutJob, E_POINTER);
  UINT32 index = GetIndexFor(aId);
  ENSURE_TRUE(index == -1, E_FAIL);
  *aOutJob = mJobs[index];
  return S_OK;
}

HRESULT
TranscodeJobList::GetJobByIndex(UINT32 aIndex, TranscodeJob** aOutJob)
{
  ENSURE_TRUE(aOutJob, E_POINTER);
  ENSURE_TRUE(aIndex < mJobs.size(), E_FAIL);
  *aOutJob = mJobs[aIndex];
  return S_OK;
}

UINT32
TranscodeJobList::GetLength() const
{
  return mJobs.size();
}

HRESULT
TranscodeJobList::RemoveJobById(TranscodeJobId aId)
{
  UINT32 index = GetIndexFor(aId);
  ENSURE_TRUE(index == -1, E_FAIL);
  return RemoveJobByIndex(index);
}

HRESULT
TranscodeJobList::RemoveJobByIndex(UINT32 aIndex)
{
  ENSURE_TRUE(aIndex < mJobs.size(), E_FAIL);

  TranscodeJob* job = mJobs[aIndex];
  if (job->GetId() == mRunningJobId) {
    job->SetCanceled();
    mTranscoder->Cancel();
    return S_OK;
  }

  DeleteJob(aIndex);
  return S_OK;
}

HRESULT
TranscodeJobList::DeleteJob(UINT32 aIndex)
{
  ENSURE_TRUE(aIndex < mJobs.size(), E_FAIL);

  delete mJobs[aIndex];
  mJobs.erase(mJobs.begin() + aIndex);

  // Ensure the next job is running.
  EnsureJobRunning();

  PostMessage(mHWnd, MSG_JOBLIST_UPDATE, 0, 0);

  return S_OK;
}

UINT32
TranscodeJobList::GetNumPendingJobs() const
{
  UINT32 count = 0;
  for (unsigned i=0; i<mJobs.size(); i++) {
    TranscodeJob* job = mJobs[i];
    if (job->GetProgress() == 0 && !job->IsFailed()) {
      count++;
    }
  }
  return count;
}

bool
TranscodeJobList::IsJobMovable(UINT32 aIndex)
{
  return aIndex > 0 &&
         aIndex < mJobs.size() &&
         mJobs[aIndex]->GetProgress() == 0 &&
         !mJobs[aIndex]->IsFailed();
}

HRESULT
TranscodeJobList::MoveJobTowardsFront(UINT32 aIndex)
{
  if (!IsJobMovable(aIndex)) return E_FAIL;
  if (!IsJobMovable(aIndex - 1)) return E_FAIL;

  TranscodeJob* temp = mJobs[aIndex];
  mJobs[aIndex] = mJobs[aIndex - 1];
  mJobs[aIndex - 1] = temp;

  D2DManager::Invalidate();

  return S_OK;
}

HRESULT
TranscodeJobList::MoveJobTowardsBack(UINT32 aIndex)
{
  if (!IsJobMovable(aIndex)) return E_FAIL;
  if (!IsJobMovable(aIndex + 1)) return E_FAIL;

  TranscodeJob* temp = mJobs[aIndex];
  mJobs[aIndex] = mJobs[aIndex + 1];
  mJobs[aIndex + 1] = temp;

  D2DManager::Invalidate();

  return S_OK;
}

UINT32
TranscodeJobList::GetIndexFor(TranscodeJobId aJobId)
{
  for (unsigned i=0; i<mJobs.size(); i++) {
    if (mJobs[i]->GetId() == aJobId) {
      return i;
    }
  }
  return -1;
}

bool
TranscodeJobList::IsRunning()
{
  for (unsigned i=0; i<mJobs.size(); i++) {
    TranscodeJob* job = mJobs[i];
    if (job->GetProgress() != 1000 &&
        !job->IsFailed()) {
      return true;
    }
  }
  return false;
}

bool
TranscodeJobList::Handle(HWND hWnd,
                         UINT message,
                         WPARAM wParam,
                         LPARAM lParam)
{
  switch (message) {
    case MSG_TRANSCODE_COMPLETE: {
      TranscodeJob* job = reinterpret_cast<TranscodeJob*>(lParam);
      DBGMSG(L"MSG_TRANSCODE_COMPLETE id=%u\n", job->GetId());
      assert(mRunningJobId == job->GetId());
      delete mTranscoder;
      mTranscoder = nullptr;
      mRunningJobId = TRANSCODE_JOB_INVALID_ID;
      if (job->IsCanceled()) {
        UINT32 index = GetIndexFor(job->GetId());
        if (index != -1) {
          DeleteJob(index);
        }
      }
      EnsureJobRunning();
      D2DManager::Invalidate();
      return true;
    }
    case MSG_TRANSCODE_PROGRESS: {
      TranscodeJob* job = reinterpret_cast<TranscodeJob*>(lParam);
      UINT32 progress = (UINT32)wParam;
      job->SetProgress(progress);
      D2DManager::Invalidate();
      return true;
    }
    case MSG_TRANSCODE_FAILED: {
      TranscodeJob* job = reinterpret_cast<TranscodeJob*>(lParam);
      job->SetFailed();
      return true;
    }
  }
  return false;
}


HRESULT
TranscodeJobList::EnsureJobRunning()
{
  if (mRunningJobId != TRANSCODE_JOB_INVALID_ID ||
      GetNumPendingJobs() == 0) {
    // No jobs, or there's already a running job.
    return S_OK;
  }

  // Find the first pending job.
  TranscodeJob* job = nullptr;
  for (unsigned index = 0; index < GetLength(); index++) {
    if (mJobs[index]->GetProgress() == 0 &&
        !mJobs[index]->IsFailed()) {
      job = mJobs[index];
      break;
    }
  }
  ENSURE_TRUE(job, E_FAIL);

  TranscodeJobRunner* transcoder = new TranscodeJobRunner(job, mHWnd);
  HRESULT hr = transcoder->Begin();
  if (FAILED(hr)) {
    delete transcoder;
    ENSURE_SUCCESS(hr, hr);
  }
  mTranscoder = transcoder;
  mRunningJobId = job->GetId();

  return S_OK;
}
