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
#include "ClickableRegion.h"

// Windowless button class. This button handle events from the main WndProc
// that are destined for its coords, and is painted using Direct2D.
class RoundButton : public EventHandler,
                    public Paintable {
public:

  RoundButton(HWND aParent,
              OnClickFunctor aOnClick,
              FLOAT aX,
              FLOAT aY,
              FLOAT aButtonWidth,
              FLOAT aButtonHeight,
              FLOAT aIconSize,
              bool aEnabled,
              ResourceId aIconResourceId);

  virtual ~RoundButton();

  void SetEnabled(bool aEnabled);

  // Paintable
  void Paint(ID2D1RenderTarget* aRenderTarget);

  // EventHandler
  bool Handle(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

  void SetIcon(ResourceId aIconResourceId);

private:

  ClickableRegion mClickableRegion;

  HWND mParent;
  FLOAT mX;
  FLOAT mY;
  FLOAT mWidth;
  FLOAT mHeight;

  // Dimensions of the icon to draw, centred inside the button area.
  // We assume it's square.
  FLOAT mIconSize;

  bool mIsEnabled;

  ResourceId mIconResourceId;

  HICON GetIcon(bool aIsSelected);
};

