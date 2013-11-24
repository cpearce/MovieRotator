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

#include "resource.h"


class RoundButton;
class TranscodeJobList;
class JobListPane;
class JobListScrollBar;
class VideoPlayer;

#include "Interfaces.h"

class MovieRotator {
public:
  MovieRotator();
  ~MovieRotator();

  // Initializes the instance, registers the class and creates
  // and shows the application window.
  HRESULT Init(HINSTANCE hInstance, int nCmdShow);

  // Application window procedure.
  LRESULT WndProc(HWND, UINT, WPARAM, LPARAM);

  bool OpenFile(const std::wstring& aFilename);

  void OnVideoPlayerError();
  void OnVideoPlayerLoaded();
  void OnVideoPlayerPlaying();
  void OnVideoPlayerPaused();

private:

  void UpdateButtonsState();

  // Registers the main window's custom class.
  ATOM RegisterWindowClass(HINSTANCE hInstance);

  // Paints the application window.
  HRESULT OnPaint(HWND hWnd);

  void Reset();

  void Rotate(unsigned delta);
  void OnKeyDown(WPARAM wParam);
  void OnWindowDestroyed();
  void OnOpenFile();
  void OnPlayPause();
  void OnRotateAntiClockwise();
  void OnRotateClockwise();
  void OnSaveRotation();
  void OnDropFiles(HDROP aDropHandle);

  HRESULT DisableRegistrationSubMenu(HWND aAppHwnd);

  // Video player buttons.
  RoundButton* mOpenButton;
  RoundButton* mPlayPauseButton;
  RoundButton* mRotateAntiClockwiseButton;
  RoundButton* mRotateClockwiseButton;
  RoundButton* mSaveRotationButton;

  // All the controls that can handle window events.
  std::vector<EventHandler*> mEventHandlers;

  // All the things that can be painted.
  std::vector<Paintable*> mPaintables;

  TranscodeJobList* mTranscodeManager;
  JobListPane* mJobListPane;
  JobListScrollBar* mJobListScrollBar;
  VideoPlayer* mVideoPlayer;
};
