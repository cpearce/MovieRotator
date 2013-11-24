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

#include "RoundButton.h"
#include "D2DManager.h"

RoundButton::RoundButton(HWND aParent,
                         OnClickFunctor aOnClickFunction,
                         FLOAT aX,
                         FLOAT aY,
                         FLOAT aWidth,
                         FLOAT aHeight,
                         FLOAT aIconSize,
                         bool aEnabled,
                         WORD aIconResourceId)
  : mClickableRegion(aX, aY, aWidth, aHeight, aOnClickFunction),
    mParent(aParent),
    mX(aX),
    mY(aY),
    mWidth(aWidth),
    mHeight(aHeight),
    mIconSize(aIconSize),
    mIconResourceId(aIconResourceId),
    mIsEnabled(aEnabled)
{
  SetEnabled(aEnabled);
}

RoundButton::~RoundButton() {
}

void RoundButton::SetEnabled(bool aEnabled) {
  mIsEnabled = aEnabled;
  mClickableRegion.SetEnabled(aEnabled);
  D2DManager::Invalidate();
}

static D2D1::ColorF
GetButtonColor(bool aIsHovered, bool aIsEnabled)
{
  if (aIsHovered && aIsEnabled) {
    return D2D1::ColorF(D2D1::ColorF::LightBlue);
  }
  return D2D1::ColorF(D2D1::ColorF::Lavender);
}

void RoundButton::Paint(ID2D1RenderTarget* aRenderTarget)
{
  HRESULT hr;

  // Reset the transform to identity.
  aRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

  // Draw button background.
  ID2D1SolidColorBrushPtr brush;
  hr = aRenderTarget->CreateSolidColorBrush(
    GetButtonColor(mClickableRegion.IsHovered(), mIsEnabled), &brush);
  ENSURE_SUCCESS(hr,);

  D2D1_SIZE_F sizeRT = aRenderTarget->GetSize();
  D2D1_RECT_F bounds = D2D1::RectF(FLOAT(mX),
                                   FLOAT(mY),
                                   FLOAT(mX + mWidth),
                                   FLOAT(mY + mHeight));
  // Define a rounded rectangle.
  ID2D1SolidColorBrushPtr borderBrush;
  if (mIsEnabled) {
    hr = aRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black),
                                              &borderBrush);
  } else {
    hr = aRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::DarkGray),
                                              &borderBrush);
  }
  ENSURE_SUCCESS(hr,);

  FLOAT radiusX = D2DManager::DpiScaleX(mWidth / 10);
  FLOAT radiusY = D2DManager::DpiScaleX(mHeight / 10);
  D2D1_ROUNDED_RECT roundedBounds =
    D2D1::RoundedRect(bounds, radiusX, radiusY);
  aRenderTarget->DrawRoundedRectangle(&roundedBounds,
                                      borderBrush,
                                      D2DManager::DpiScaleX(1));
  aRenderTarget->FillRoundedRectangle(roundedBounds, brush);

  // Draw the icon.
  ID2D1Bitmap* bitmap = D2DManager::GetBitmap(mIconResourceId);
  ENSURE_TRUE(bitmap,);
  D2D1_SIZE_F size = bitmap->GetSize();
  // Icon must be square for our logic to work...
  ENSURE_TRUE(size.width == size.height, );
  D2D1_SIZE_F rts = aRenderTarget->GetSize();

  FLOAT padding = (mWidth - mIconSize) / 2.0f;
  aRenderTarget->DrawBitmap(bitmap,
                            D2D1::RectF(FLOAT(mX) + padding,
                                        FLOAT(mY) + padding,
                                        FLOAT(mX + padding + mIconSize),
                                        FLOAT(mY + padding + mIconSize)),
                            mIsEnabled ? 1.0f : 0.5f);

}

bool
RoundButton::Handle(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  bool handled = mClickableRegion.Handle(hWnd, message, wParam, lParam);
  if (mClickableRegion.IsDirty()) {
    D2DManager::Invalidate();
  }
  return handled;
}

void
RoundButton::SetIcon(ResourceId aIconResourceId)
{
  mIconResourceId = aIconResourceId;
  D2DManager::Invalidate();
}
