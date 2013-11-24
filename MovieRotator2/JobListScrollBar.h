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

class JobListPane;

class JobListScrollBar : public EventHandler {
public:

  JobListScrollBar(HWND aParent,
                   HINSTANCE hInst,
                   JobListPane* aJobListPane,
                   FLOAT aX, FLOAT aY, FLOAT aWidth, FLOAT aHeight);
  ~JobListScrollBar();

  bool Handle(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) override;

  // Scrolls down/up by 1 job.
  void ScrollDown();
  void ScrollUp();

  void Update();

private:

  void OnScroll(WPARAM wParam);
  bool OnMouseWheel(WPARAM wParam, LPARAM lParam);

  HWND mParentHWND;
  HWND mScrollHWND;
  JobListPane* mJobListPane;
  FLOAT mX;
  FLOAT mY;
  FLOAT mWidth;
  FLOAT mHeight;
};
