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
#include <vector>

class TranscodeJobList;
class TranscodeJob;
class RoundButton;

class JobListPane : public EventHandler,
                    public Paintable
{
public:

  JobListPane(HWND aParent,
              FLOAT aX,
              FLOAT aY,
              FLOAT aWidth,
              FLOAT aHeight,
              TranscodeJobList* aJobList);
  virtual ~JobListPane();

  bool Handle(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) override;

  void Paint(ID2D1RenderTarget* aRenderTarget) override;

  bool IsInsideBounds(long aX, long aY) const;

  // Returns number of slots in which we can render a job. i.e. the number of jobs
  // that JobListPane can paint inside its bounds.
  UINT32 GetMaxNumSlots() const;

  // Returns the number of slots being draw currently.
  UINT32 GetNumActiveSlots() const;

  // Sets the first slot that is visible.
  void SetScrollPos(UINT32 aScrollPos);

  // Returns the index which is the topmost in the scroll pane.
  UINT32 GetScrollPos() const;

  // Returns the job index of the last job that is still visible.
  UINT32 GetLastVisibleJobIndex() const;

private:

  void OnCancelButtonClick(UINT32 aSlotIndex);

  // Calculates the screen rect in client coordinates of the on screen "slot"
  // where we draw the jobs in the job list. This is not the logical coords of
  // jobs that have been scrolled offscreen; it's the screen coords where
  // we draw the nth visible job.
  D2D1_RECT_F GetJobSlotRect(UINT32 index);

  D2D1_RECT_F GetCancelButtonRect(UINT32 aSlotIndex);

  D2D1_RECT_F GetFilenameTextRect(UINT32 aIndex);

  D2D1_RECT_F GetProgressTextRect(UINT32 aIndex);

  HWND mParent;
  FLOAT mX;
  FLOAT mY;
  FLOAT mWidth;
  FLOAT mHeight;
  TranscodeJobList* mJobList;

  // The index which is at the top of the scroll pane.
  UINT32 mScrollPos;

  std::vector<RoundButton*> mCancelButtons;
};
