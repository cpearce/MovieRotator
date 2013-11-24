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
#include "ClickableRegion.h"
#include "D2DManager.h"


ClickableRegion::ClickableRegion(FLOAT aX,
                                 FLOAT aY,
                                 FLOAT aWidth,
                                 FLOAT aHeight,
                                 OnClickFunctor aOnClick)
  : mX(aX),
    mY(aY),
    mWidth(aWidth),
    mHeight(aHeight),
    mOnClickFunction(aOnClick),
    mIsHovered(false),
    mLeftMouseDownOnThis(false),
    mIsDirty(false),
    mIsEnabled(true)
{

}

ClickableRegion::~ClickableRegion() {
}

void
ClickableRegion::SetEnabled(bool aEnabled)
{
  mIsEnabled = aEnabled;
  if (!mIsEnabled) {
    mLeftMouseDownOnThis = false;
    mIsHovered = false;
  }
}

bool
ClickableRegion::IsHovered() const
{
  return mIsHovered;
}

bool
ClickableRegion::IsDirty() const
{
  return mIsDirty;
}

bool
ClickableRegion::IsInside(const POINTS& aPoints) const
{
  return UINT32(aPoints.x) >= mX && UINT32(aPoints.x) <= mX+mWidth &&
         UINT32(aPoints.y) >= mY && UINT32(aPoints.y) <= mY+mHeight;
}

bool
ClickableRegion::Handle(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  mIsDirty = false;

  if (!mIsEnabled) {
    return false;
  }

  switch (message) {
    case WM_MOUSEMOVE: {
      if (IsInside(MAKEPOINTS(lParam))) {
        // Mouse move on this button. Ensure we're hovered.
        if (!mIsHovered) {
          mIsHovered = true;
          mIsDirty = true;
        }
        return true;
      }
      // Mouse move not on this button.
      bool wasHovered = mIsHovered;
      mLeftMouseDownOnThis = false;
      mIsHovered = false;
      if (wasHovered) {
        mIsDirty = true;
      }
      return false;
    }
    case WM_LBUTTONDOWN: {
      if (IsInside(MAKEPOINTS(lParam))) {
        // Left mouse button went down on us. If it goes up on us,
        // register that as a click.
        mLeftMouseDownOnThis = true;
        return true;
      }
      return false;
    }
    case WM_LBUTTONUP: {
      if (IsInside(MAKEPOINTS(lParam)) &&
          mLeftMouseDownOnThis) {
        // Left mouse went down and up on us, that's a click!
        mLeftMouseDownOnThis = false;
        // Reset the hover state, this is so that if the mouse leaves the
        // window area (like if it goes over the File Open dialog, and moves
        // outside of our window) we don't look like we're still hovered.
        mIsHovered = false;
        mIsDirty = true;
        mOnClickFunction();
        return true;
      } else {
        mLeftMouseDownOnThis = false;
      }
      return false;
    }
  }
  return false;
}
