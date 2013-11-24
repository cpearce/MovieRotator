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
#include "JobListScrollBar.h"
#include "JobListPane.h"

JobListScrollBar::JobListScrollBar(HWND aParent,
                                   HINSTANCE hInst,
                                   JobListPane* aJobListPane,
                                   FLOAT aX,
                                   FLOAT aY,
                                   FLOAT aWidth,
                                   FLOAT aHeight)
  : mParentHWND(aParent),
    mJobListPane(aJobListPane),
    mX(aX),
    mY(aY),
    mWidth(aWidth),
    mHeight(aHeight)
{
  mScrollHWND =
    CreateWindowEx(0,                      // no extended styles
                   L"SCROLLBAR",           // scroll bar control class
                   (PTSTR) NULL,           // no window text
                   WS_CHILD | WS_VISIBLE | SBS_VERT,  // window styles
                   aX,              // horizontal position
                   aY,
                   mWidth,             // width of the scroll bar
                   mHeight,               // height of the scroll bar
                   mParentHWND,             // handle to main window
                   (HMENU) NULL,           // no menu
                   hInst,                // instance owning this window
                   (PVOID) NULL);            // pointer not needed
}

JobListScrollBar::~JobListScrollBar()
{
}

void
JobListScrollBar::Update()
{
  SCROLLINFO si = {0};
  si.cbSize = sizeof(si);
  si.fMask = SIF_ALL | SIF_DISABLENOSCROLL;
  si.nMin = 0;
  si.nMax = mJobListPane->GetNumActiveSlots() - 1;
  si.nPage = mJobListPane->GetMaxNumSlots();
  si.nPos = mJobListPane->GetScrollPos();
  int x = SetScrollInfo(mScrollHWND, SB_CTL, &si, TRUE);
  GetScrollInfo(mScrollHWND, SB_CTL, &si);
  UpdateWindow(mScrollHWND);
}

void
JobListScrollBar::OnScroll(WPARAM wParam)
{
  const int scrollEvent = LOWORD(wParam);
  if (scrollEvent == SB_ENDSCROLL) {
    return;
  }

  SCROLLINFO si = {0};
  si.cbSize = sizeof(si);
  si.fMask = SIF_ALL;
  GetScrollInfo(mScrollHWND, SB_CTL, &si);
  const int pos = si.nPos;
  DBGMSG(L"OnScroll before action=%d pos=%d JobPanePos()=%d\n",
         scrollEvent, si.nPos, mJobListPane->GetScrollPos());

  switch (scrollEvent) {
      // User clicked the HOME keyboard key.
    case SB_TOP:
      si.nPos = si.nMin;
      break;

      // User clicked the END keyboard key.
    case SB_BOTTOM:
      si.nPos = si.nMax;
      break;

      // User clicked the top arrow.
    case SB_LINEUP:
      si.nPos -= 1;
      break;

      // User clicked the bottom arrow.
    case SB_LINEDOWN:
      si.nPos += 1;
      break;

      // User clicked the scroll bar shaft above the scroll box.
    case SB_PAGEUP:
      si.nPos -= si.nPage;
      break;

      // User clicked the scroll bar shaft below the scroll box.
    case SB_PAGEDOWN:
      si.nPos += si.nPage;
      break;

      // User dragged the scroll box.
    case SB_THUMBTRACK:
      si.nPos = si.nTrackPos;
      break;

    default:
      break;
  }

  if (si.nPos != pos) {
    si.fMask = SIF_POS;
    SetScrollInfo (mScrollHWND, SB_CTL, &si, TRUE);
    GetScrollInfo(mScrollHWND, SB_CTL, &si);
    mJobListPane->SetScrollPos(si.nPos);
    UpdateWindow(mScrollHWND);
  }

  DBGMSG(L"OnScroll after  action=%d delta=%d pos=%d si.nPos=%d JobPanePos()=%d\n",
         scrollEvent, si.nPos-pos, pos, si.nPos, mJobListPane->GetScrollPos());

}

void
JobListScrollBar::ScrollDown()
{
  SendMessage(mParentHWND, WM_VSCROLL, MAKELONG(SB_LINEDOWN, 0), 0L);
}

void
JobListScrollBar::ScrollUp()
{
  SendMessage(mParentHWND, WM_VSCROLL, MAKELONG(SB_LINEUP, 0), 0L);
}

bool
JobListScrollBar::OnMouseWheel(WPARAM wParam, LPARAM lParam)
{
  // Mouse wheel events are relative to the primary screen origin,
  // convert to client area.
  SHORT xPos = GET_X_LPARAM(lParam);
  SHORT yPos = GET_Y_LPARAM(lParam);
  POINT pt = {xPos, yPos};
  if (!ScreenToClient(mParentHWND, &pt)) {
    return false;
  }

  if (!mJobListPane->IsInsideBounds(pt.x, pt.y)) {
    return false;
  }
  SHORT zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
  if (zDelta > 0) {
    ScrollUp();
  } else {
    ScrollDown();
  }
  return true;
}

bool
JobListScrollBar::Handle(HWND hWnd,
                         UINT message,
                         WPARAM wParam,
                         LPARAM lParam)
{
  switch (message) {
    case WM_SIZE:
    case MSG_JOBLIST_UPDATE: {
      Update();
      return true;
    }
    case WM_VSCROLL: {
      OnScroll(wParam);
      return true;
    }
    case WM_MOUSEWHEEL: {
      return OnMouseWheel(wParam, lParam);
    }
  }
  return false;
}
