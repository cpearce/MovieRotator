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

inline UINT32 Round(FLOAT aFloat) {
  return UINT32(aFloat + 0.5);
}

class D2DManager {
public:
  static HRESULT Init();
  static HRESULT Shutdown();

  // Deallocates device resources.
  static HRESULT DiscardDeviceResources();

  // Scales a device independent pixel value with respect to the DPI of the
  // display.
  static FLOAT DpiScaleX(UINT32 aPixelValue);
  static FLOAT DpiScaleY(UINT32 aPixelValue);

  // Returns the render targeting, ensuring it is allocated first.
  // Do not deref this (unless you AddRef it first!).
  static ID2D1DCRenderTarget* BeginPaint();
  static void EndPaint();

  static ID2D1SolidColorBrush* GetBlackBrush();
  static ID2D1SolidColorBrush* GetGrayBrush();

  static ID2D1Bitmap* GetBitmap(ResourceId aResourceId);

  static IDWriteTextFormat* GetGuiTextFormat();

  // Invalidates the entire main HWND.
  static void Invalidate();
};

class AutoD2DInit {
public:
  AutoD2DInit() {
    D2DManager::Init();
  }
  ~AutoD2DInit() {
    D2DManager::Shutdown();
  }
};
