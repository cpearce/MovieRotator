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
#include <functional>

typedef std::function<void(void)> OnClickFunctor;

class ClickableRegion : public EventHandler {
public:
  ClickableRegion(FLOAT aX, FLOAT aY, FLOAT aWidth, FLOAT aHeight, OnClickFunctor aOnClick);
  virtual ~ClickableRegion();

  // EventHandler
  bool Handle(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

  bool IsHovered() const;

  // Returns true if the region experienced a state change, that would require
  // the region's renderer to redraw it. Call this after calling Handle(),
  // irrespective of Handle()'s return value.
  bool IsDirty() const;

  void SetEnabled(bool aEnabled);

private:

  bool IsInside(const POINTS& aPoints) const;

  FLOAT mX;
  FLOAT mY;
  FLOAT mWidth;
  FLOAT mHeight;
  OnClickFunctor mOnClickFunction;
  bool mIsHovered;
  bool mLeftMouseDownOnThis;
  bool mIsDirty;
  bool mIsEnabled;
};
