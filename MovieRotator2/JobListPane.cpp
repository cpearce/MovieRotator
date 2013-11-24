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
#include "JobListPane.h"
#include "TranscodeJobList.h"
#include "D2DManager.h"
#include "RoundButton.h"

static const UINT32 NUM_SLOTS = 7;
static const UINT32 INNER_PADDING = 3;
static const UINT32 PROGRESS_PRECENT_TEXT_WIDTH = 50;
static const UINT32 JOB_HEIGHT = 48;
static const UINT32 CANCEL_BUTTON_SIZE = 18;
static const UINT32 CANCEL_ICON_SIZE = 14;

using std::vector;

JobListPane::JobListPane(HWND aParent,
                         FLOAT aX,
                         FLOAT aY,
                         FLOAT aWidth,
                         FLOAT aHeight,
                         TranscodeJobList* aJobList)
  : mParent(aParent),
    mX(aX),
    mY(aY),
    mWidth(aWidth),
    mHeight(aHeight),
    mJobList(aJobList),
    mScrollPos(0)
{
  for (int i=0; i<NUM_SLOTS; i++) {
    D2D1_RECT_F rect = GetCancelButtonRect(i);
    FLOAT width = D2DManager::DpiScaleX(CANCEL_BUTTON_SIZE);
    FLOAT height = D2DManager::DpiScaleY(CANCEL_BUTTON_SIZE);
    FLOAT padding = D2DManager::DpiScaleX(INNER_PADDING);
    RoundButton* cancelButton = new RoundButton(aParent,
                                                std::bind(&JobListPane::OnCancelButtonClick, this, i),
                                                rect.right - width - padding,
                                                rect.top + padding,
                                                width,
                                                D2DManager::DpiScaleY(CANCEL_BUTTON_SIZE),
                                                D2DManager::DpiScaleY(CANCEL_ICON_SIZE),
                                                true,
                                                IDB_CANCEL_ICON);
    mCancelButtons.push_back(cancelButton);
  }
}

JobListPane::~JobListPane()
{
  for (unsigned i=0; i<mCancelButtons.size(); i++) {
    delete mCancelButtons[i];
  }
  mCancelButtons.clear();
}

bool
JobListPane::Handle(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  // Deliver events to the cancel buttons that are currently visible,
  // i.e. to the jobs that are scrolled in view.
  UINT32 maxVisibleSlotIndex = GetLastVisibleJobIndex() - GetScrollPos();
  for (UINT32 slotIndex = 0; slotIndex < maxVisibleSlotIndex; slotIndex++) {
    bool handled = mCancelButtons[slotIndex]->Handle(hWnd, message, wParam, lParam);
    if (handled) {
      return true;
    }
  }
  return false;
}

D2D1_RECT_F
JobListPane::GetJobSlotRect(UINT32 aIndex)
{
  FLOAT paddingX = D2DManager::DpiScaleX(INNER_PADDING);
  FLOAT paddingY = D2DManager::DpiScaleY(INNER_PADDING);

  FLOAT width = mWidth - 2*paddingY;
  FLOAT height = D2DManager::DpiScaleX(JOB_HEIGHT);

  FLOAT x = mX + paddingX;
  FLOAT y = mY + (aIndex+1)*paddingY + aIndex * height;

  return D2D1::RectF(FLOAT(x),
                     FLOAT(y),
                     FLOAT(x + width),
                     FLOAT(y + height));
}

D2D1_RECT_F
JobListPane::GetCancelButtonRect(UINT32 aSlotIndex)
{
  FLOAT paddingX = D2DManager::DpiScaleX(INNER_PADDING);
  FLOAT paddingY = D2DManager::DpiScaleY(INNER_PADDING);

  FLOAT width = mWidth - 2*paddingY;
  FLOAT height = D2DManager::DpiScaleX(JOB_HEIGHT);

  FLOAT x = mX + paddingX;
  FLOAT y = mY + (aSlotIndex+1)*paddingY + aSlotIndex * height;

  return D2D1::RectF(FLOAT(x),
                     FLOAT(y),
                     FLOAT(x + width),
                     FLOAT(y + height));
}

void
JobListPane::OnCancelButtonClick(UINT32 aSlotIndex)
{
  UINT32 index = GetScrollPos() + aSlotIndex;
  mJobList->RemoveJobByIndex(index);
}

static std::wstring
GetShortFilename(const std::wstring& aFilename, UINT32 aIndex)
{
  if (aFilename.empty()) {
    return L"";
  }
  size_t seperator = aFilename.find_last_of(L"\\");
  if (seperator == std::wstring::npos) {
    return aFilename;
  }

  return std::wstring(L"File: ") + std::wstring(aFilename.c_str() + seperator + 1);
}

static std::wstring
GetProgressString(UINT32 aProgress)
{
  UINT32 whole = aProgress / 10;
  UINT32 frac = aProgress % 10;
  const unsigned textLen = 20;
  WCHAR text[textLen];
  StringCbPrintf(text,
                 textLen,
                 L"%u.%u%s",
                 whole, frac, L"%");
  return std::wstring(text);
}

D2D1_RECT_F
JobListPane::GetFilenameTextRect(UINT32 aIndex)
{
  D2D1_RECT_F r = GetJobSlotRect(aIndex);
  FLOAT paddingX = D2DManager::DpiScaleX(INNER_PADDING);
  FLOAT paddingY = D2DManager::DpiScaleY(INNER_PADDING);
  FLOAT cancelButtonWidth = D2DManager::DpiScaleX(CANCEL_BUTTON_SIZE);
  r.left += 2*paddingX;
  r.right -= 2*paddingX + cancelButtonWidth;
  r.bottom -= 2*paddingY;
  r.top += 2*paddingY;
  return r;
}

D2D1_RECT_F
JobListPane::GetProgressTextRect(UINT32 aIndex)
{
  D2D1_RECT_F r = GetFilenameTextRect(aIndex);
  r.left = r.right;
  r.right = r.left + D2DManager::DpiScaleX(PROGRESS_PRECENT_TEXT_WIDTH);
  return r;
}

static const std::wstring sStatusFailed(L"Status: Failed");
static const std::wstring sStatusPending(L"Status: Pending");
static const std::wstring sStatusComplete(L"Status: Complete");
static const std::wstring sStatus(L"Status: ");

static const std::wstring
GetStatusString(TranscodeJob* aJob)
{
  if (aJob->IsFailed()) {
    return sStatusFailed;
  }
  std::wstring status;
  UINT32 progress = aJob->GetProgress();
  if (progress == 0) {
    return sStatusPending;
  }
  if (progress == 1000) {
    return sStatusComplete;
  }

  return sStatus + GetProgressString(progress);
}

static const std::wstring eolText(L"\r\n");

void
JobListPane::Paint(ID2D1RenderTarget* aRenderTarget)
{
  // Paint background.
  HRESULT hr;
  D2D1_RECT_F bounds = D2D1::RectF(FLOAT(mX),
                                   FLOAT(mY),
                                   FLOAT(mX + mWidth),
                                   FLOAT(mY + mHeight));
  ID2D1SolidColorBrushPtr backgroundBrush;
  hr =  aRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::DarkGray),
                                             &backgroundBrush);
  ENSURE_SUCCESS(hr,);
  aRenderTarget->FillRectangle(bounds, backgroundBrush);

  // Paint the job list.
  UINT32 numJobs = mJobList->GetLength();
  if (numJobs == 0) {
    return;
  }

  ID2D1SolidColorBrushPtr jobBackgroundBrush;
  hr =  aRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::WhiteSmoke),
                                             &jobBackgroundBrush);
  ENSURE_SUCCESS(hr,);

  ID2D1SolidColorBrushPtr jobForegroundBrush;
  hr =  aRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black),
                                             &jobForegroundBrush);
  ENSURE_SUCCESS(hr,);

  UINT32 scrollpos = GetScrollPos();
  UINT32 max = GetLastVisibleJobIndex();
  for (UINT32 index=scrollpos; index<max; index++) {

    TranscodeJob* job = nullptr;
    hr = mJobList->GetJobByIndex(index, &job);
    ENSURE_SUCCESS(hr,);

    UINT32 slot = index - scrollpos;
    D2D1_RECT_F background = GetJobSlotRect(slot);
    aRenderTarget->FillRectangle(background, jobBackgroundBrush);

    D2D1_RECT_F filenameTextRect = GetFilenameTextRect(slot);
    std::wstring statusText = GetShortFilename(job->GetInputFilename(), index) +
                              eolText + GetStatusString(job);
    IDWriteTextFormat* textFormat = D2DManager::GetGuiTextFormat();
    aRenderTarget->DrawText(statusText .c_str(),
                            statusText .size(),
                            textFormat,
                            filenameTextRect,
                            jobForegroundBrush);

    // Paint this slot's cancel button.
    mCancelButtons[slot]->Paint(aRenderTarget);
  }
}

// Returns number of slots in which we can render a job. i.e. the number of jobs
// that JobListPane can paint inside its bounds.
UINT32
JobListPane::GetMaxNumSlots() const
{
  return NUM_SLOTS;
}

// Returns the number of slots being draw currently.
UINT32
JobListPane::GetNumActiveSlots() const
{
  return mJobList->GetLength();
}

UINT32
JobListPane::GetScrollPos() const
{
  return mScrollPos;
}

void
JobListPane::SetScrollPos(UINT32 aScrollPos)
{
  mScrollPos = aScrollPos;
}

UINT32
JobListPane::GetLastVisibleJobIndex() const
{
  return min(mScrollPos + GetMaxNumSlots(), GetNumActiveSlots());
}

bool
JobListPane::IsInsideBounds(long aX, long aY) const
{
  return aX >= mX && aX <= mX + mWidth &&
         aY >= mY && aY <= mY + mHeight;
}
