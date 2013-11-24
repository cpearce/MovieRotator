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

// Classes that implement this can handle window messages.
class EventHandler {
public:
  // Returns true if the object handles the event, false otherwise.
  virtual bool Handle(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) = 0;
};


class Paintable {
public:
  // Instructs the object to paint.
  virtual void Paint(ID2D1RenderTarget* aRenderTarget) = 0;
};

class Runnable {
public:
  virtual void Run() = 0;
};
