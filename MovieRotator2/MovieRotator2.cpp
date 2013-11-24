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
#include "MovieRotator2.h"
#include "Utils.h"
#include "D2DManager.h"
#include "RoundButton.h"
#include "TranscodeJobList.h"
#include "JobListPane.h"
#include "JobListScrollBar.h"
#include "VideoPlayer.h"
#include "EventListeners.h"

#define SZ_WINDOW_CLASS L"MOVIEROTATOR2"
#define SZ_WINDOW_TITLE L"Movie Rotator"

using std::vector;
using std::wstring;

#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "wmcodecdspuuid")


// Uncomment to draw gridlines at 10px intervals on the background.
// For debug purposes.
//#define DRAW_GRIDLINES 1

// Global Variables:
HINSTANCE hInst;
HWND sMainWindowHWND = 0;

// Forward declarations of functions included in this code module:
LRESULT CALLBACK MovieRotatorWndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK RegistrationProc(HWND, UINT, WPARAM, LPARAM);

static const int PADDING = 10;
static const int BUTTON_SIZE = 50;
static const int ICON_SIZE = 32;
static const int VIDEO_PREVIEW_SIZE = 301;

static const int PREVIEW_BUTTONS_X_OFFSET =
  PADDING + (VIDEO_PREVIEW_SIZE - (5*BUTTON_SIZE + 4*PADDING)) / 2;

static const int JOB_LIST_WIDTH = 300;

static const int JOB_LIST_BUTTON_X_OFFSET =
  2*PADDING + VIDEO_PREVIEW_SIZE +
  (JOB_LIST_WIDTH - (3*BUTTON_SIZE  + 2*PADDING)) / 2;

static const int BUTTONS_Y_OFFSET = VIDEO_PREVIEW_SIZE + 2*PADDING;

static const int SCROLLBAR_WIDTH = 20;

static const int WINDOW_HEIGHT = 3*PADDING + BUTTON_SIZE + VIDEO_PREVIEW_SIZE;
static const int WINDOW_WIDTH = 3*PADDING + VIDEO_PREVIEW_SIZE + JOB_LIST_WIDTH + SCROLLBAR_WIDTH;


MovieRotator::MovieRotator()
  : mOpenButton(nullptr),
    mPlayPauseButton(nullptr),
    mRotateAntiClockwiseButton(nullptr),
    mRotateClockwiseButton(nullptr),
    mSaveRotationButton(nullptr),
    mTranscodeManager(nullptr),
    mJobListPane(nullptr),
    mJobListScrollBar(nullptr),
    mVideoPlayer(nullptr)
{
}

MovieRotator::~MovieRotator()
{
  mEventHandlers.clear();
  mPaintables.clear();

  delete mOpenButton;
  delete mPlayPauseButton;
  delete mRotateAntiClockwiseButton;
  delete mRotateClockwiseButton;
  delete mSaveRotationButton;

  delete mTranscodeManager;
  delete mJobListPane;
  delete mJobListScrollBar;
  delete mVideoPlayer;
}

ATOM
MovieRotator::RegisterWindowClass(HINSTANCE hInstance)
{
  WNDCLASSEX wcex;

  wcex.cbSize = sizeof(WNDCLASSEX);

  wcex.style			= CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc	= MovieRotatorWndProc;
  wcex.cbClsExtra		= 0;
  wcex.cbWndExtra		= sizeof(MovieRotator*);
  wcex.hInstance		= hInstance;
  wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPLICATION_ICON));
  wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground = (HBRUSH)GetStockObject(GRAY_BRUSH);

  wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_MOVIEROTATOR2);
  wcex.lpszClassName	= SZ_WINDOW_CLASS;
  wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_APPLICATION_ICON));

  return RegisterClassEx(&wcex);
}

typedef BOOL (WINAPI*ChangeWindowMessageFilterPtr)(UINT,DWORD);

static void
ChangeDragAndDropMessageFilters()
{
  ChangeWindowMessageFilterPtr fnptr =
    (ChangeWindowMessageFilterPtr)GetProcAddress(GetModuleHandle(L"user32.dll"),
                                                 "ChangeWindowMessageFilter");
  ENSURE_TRUE(fnptr, );

  fnptr(WM_DROPFILES, MSGFLT_ADD);
  fnptr(WM_COPYDATA, MSGFLT_ADD);
  fnptr(0x0049, MSGFLT_ADD);
 } 

HRESULT MovieRotator::Init(HINSTANCE hInstance, int nCmdShow)
{
  RegisterWindowClass(hInstance);

  hInst = hInstance; // Store instance handle in our global variable

  // Determine the required window size to ensure we have the client area
  // we require.
  RECT size = { 0, 0,
                Round(D2DManager::DpiScaleX(WINDOW_WIDTH)),
                Round(D2DManager::DpiScaleY(WINDOW_HEIGHT))};
  if (!AdjustWindowRect(&size,
                        WS_CAPTION | WS_SYSMENU,
                        TRUE)) {
    return E_FAIL;
  }

  // Create the main app window.
  UINT32 width = Round(D2DManager::DpiScaleX(size.right - size.left));
  UINT32 height = Round(D2DManager::DpiScaleY(size.bottom - size.top));
  HWND hWnd;
  hWnd = CreateWindow(SZ_WINDOW_CLASS,
                      SZ_WINDOW_TITLE,
                      WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN, // Window style
                      CW_USEDEFAULT, CW_USEDEFAULT, // x,y
                      width,
                      height,
                      NULL, NULL, hInstance, NULL);

  if (!hWnd) {
    return E_FAIL;
  }

  // Set a pointer to our instance, so we can get to it.
  SetWindowLongPtr(hWnd, 0, LONG_PTR(this));
  sMainWindowHWND = hWnd;

  // Now create the other controls that inhabit the window.
  mVideoPlayer = new VideoPlayer(hWnd,
                                 hInst,
                                 D2DManager::DpiScaleX(PADDING),
                                 D2DManager::DpiScaleY(PADDING),
                                 D2DManager::DpiScaleX(VIDEO_PREVIEW_SIZE),
                                 D2DManager::DpiScaleY(VIDEO_PREVIEW_SIZE));
  HRESULT hr = mVideoPlayer->Init();
  ENSURE_SUCCESS(hr, hr);
  mVideoPlayer->AddEventListener(VideoPlayer_Error,
    std::bind(&MovieRotator::OnVideoPlayerError, this));
  mVideoPlayer->AddEventListener(VideoPlayer_Loaded,
    std::bind(&MovieRotator::OnVideoPlayerLoaded, this));
  mVideoPlayer->AddEventListener(VideoPlayer_Playing,
    std::bind(&MovieRotator::OnVideoPlayerPlaying, this));
  mVideoPlayer->AddEventListener(VideoPlayer_Paused,
    std::bind(&MovieRotator::OnVideoPlayerPaused, this));

  mOpenButton =
    new RoundButton(hWnd,
                    std::bind(&MovieRotator::OnOpenFile, this),
                    D2DManager::DpiScaleX(PREVIEW_BUTTONS_X_OFFSET),
                    D2DManager::DpiScaleY(BUTTONS_Y_OFFSET),
                    D2DManager::DpiScaleX(BUTTON_SIZE),
                    D2DManager::DpiScaleY(BUTTON_SIZE),
                    D2DManager::DpiScaleY(ICON_SIZE),
                    true,
                    IDB_FILE_OPEN_ICON);
  mEventHandlers.push_back(mOpenButton);
  mPaintables.push_back(mOpenButton);

  mPlayPauseButton =
    new RoundButton(hWnd,
                    std::bind(&MovieRotator::OnPlayPause, this),
                    D2DManager::DpiScaleX(PREVIEW_BUTTONS_X_OFFSET + PADDING + BUTTON_SIZE),
                    D2DManager::DpiScaleY(BUTTONS_Y_OFFSET),
                    D2DManager::DpiScaleX(BUTTON_SIZE),
                    D2DManager::DpiScaleY(BUTTON_SIZE),
                    D2DManager::DpiScaleY(ICON_SIZE),
                    false,
                    IDB_PLAY_ICON);
  mEventHandlers.push_back(mPlayPauseButton);
  mPaintables.push_back(mPlayPauseButton);

  mRotateAntiClockwiseButton =
    new RoundButton(hWnd,
                    std::bind(&MovieRotator::OnRotateAntiClockwise, this),
                    D2DManager::DpiScaleX(PREVIEW_BUTTONS_X_OFFSET + 2*PADDING + 2*BUTTON_SIZE),
                    D2DManager::DpiScaleY(BUTTONS_Y_OFFSET),
                    D2DManager::DpiScaleX(BUTTON_SIZE),
                    D2DManager::DpiScaleY(BUTTON_SIZE),
                    D2DManager::DpiScaleY(ICON_SIZE),
                    false,
                    IDB_ROTATE_ACW_ICON);
  mEventHandlers.push_back(mRotateAntiClockwiseButton);
  mPaintables.push_back(mRotateAntiClockwiseButton);

  mRotateClockwiseButton =
    new RoundButton(hWnd,
                    std::bind(&MovieRotator::OnRotateClockwise, this),
                    D2DManager::DpiScaleX(PREVIEW_BUTTONS_X_OFFSET + 3*PADDING + 3*BUTTON_SIZE),
                    D2DManager::DpiScaleY(BUTTONS_Y_OFFSET),
                    D2DManager::DpiScaleX(BUTTON_SIZE),
                    D2DManager::DpiScaleY(BUTTON_SIZE),
                    D2DManager::DpiScaleY(ICON_SIZE),
                    false,
                    IDB_ROTATE_CW_ICON);
  mEventHandlers.push_back(mRotateClockwiseButton);
  mPaintables.push_back(mRotateClockwiseButton);

  mSaveRotationButton =
    new RoundButton(hWnd,
                    std::bind(&MovieRotator::OnSaveRotation, this),
                    D2DManager::DpiScaleX(PREVIEW_BUTTONS_X_OFFSET + 4*PADDING + 4*BUTTON_SIZE),
                    D2DManager::DpiScaleY(BUTTONS_Y_OFFSET),
                    D2DManager::DpiScaleX(BUTTON_SIZE),
                    D2DManager::DpiScaleY(BUTTON_SIZE),
                    D2DManager::DpiScaleY(ICON_SIZE),
                    false,
                    IDB_SAVE_ICON);
  mEventHandlers.push_back(mSaveRotationButton);
  mPaintables.push_back(mSaveRotationButton);

  mTranscodeManager = new TranscodeJobList(hWnd);
  mEventHandlers.push_back(mTranscodeManager);

  UINT32 jobListHeight = WINDOW_HEIGHT - 2 * PADDING;
  mJobListPane = new JobListPane(hWnd,
                                 D2DManager::DpiScaleX(2*PADDING + VIDEO_PREVIEW_SIZE),
                                 D2DManager::DpiScaleY(PADDING),
                                 D2DManager::DpiScaleX(VIDEO_PREVIEW_SIZE),
                                 D2DManager::DpiScaleY(jobListHeight),
                                 mTranscodeManager);
  mEventHandlers.push_back(mJobListPane);
  mPaintables.push_back(mJobListPane);

  UINT32 scrollBarX = 2*PADDING + VIDEO_PREVIEW_SIZE + VIDEO_PREVIEW_SIZE;
  mJobListScrollBar = new JobListScrollBar(hWnd,
                                           hInst,
                                           mJobListPane,
                                           D2DManager::DpiScaleX(scrollBarX),
                                           D2DManager::DpiScaleY(PADDING),
                                           D2DManager::DpiScaleX(SCROLLBAR_WIDTH),
                                           D2DManager::DpiScaleY(jobListHeight));
  mEventHandlers.push_back(mJobListScrollBar);
  // Note: JobListScrollBar is painted by GDI.

  ChangeDragAndDropMessageFilters();
  DragAcceptFiles(hWnd, TRUE);

  ShowWindow(hWnd, nCmdShow);
  UpdateWindow(hWnd);

  return S_OK;
}

HRESULT
MovieRotator::DisableRegistrationSubMenu(HWND aAppHwnd)
{
  // Registration submenu must be the last item in the application menu.
  HMENU menu = GetMenu(aAppHwnd);
  ENSURE_TRUE(menu, E_FAIL);
  int count = GetMenuItemCount(menu);
  ENSURE_TRUE(count > 1, E_FAIL);
  EnableMenuItem(menu, count-1, MF_BYPOSITION|MF_GRAYED);
  return S_OK;
}

// Paints the application window.
HRESULT
MovieRotator::OnPaint(HWND hWnd)
{
  ID2D1DCRenderTarget* renderTarget = D2DManager::BeginPaint();
  ENSURE_TRUE(renderTarget, E_ABORT);

  renderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
  renderTarget->Clear(D2D1::ColorF(D2D1::ColorF::LightGray));

  // Draw a grid background. Useful for debugging layout.
  #ifdef DRAW_GRIDLINES
  D2D1_SIZE_F rtSize = renderTarget->GetSize();
  int width = static_cast<int>(rtSize.width);
  int height = static_cast<int>(rtSize.height);
  for (int x = 0; x < width; x += 10)
  {
    renderTarget->DrawLine(
          D2D1::Point2F(static_cast<FLOAT>(x), 0.0f),
          D2D1::Point2F(static_cast<FLOAT>(x), rtSize.height),
          ((x % 50) == 0) ? D2DManager::GetBlackBrush() : D2DManager::GetGrayBrush(),
          0.5f);
  }

  for (int y = 0; y < height; y += 10)
  {
    renderTarget->DrawLine(
        D2D1::Point2F(0.0f, static_cast<FLOAT>(y)),
        D2D1::Point2F(rtSize.width, static_cast<FLOAT>(y)),
        ((y % 50) == 0) ? D2DManager::GetBlackBrush() : D2DManager::GetGrayBrush(),
        0.5f);
  }
  #endif

  for (unsigned i=0; i<mPaintables.size(); i++) {
    mPaintables[i]->Paint(renderTarget);
  }

  D2DManager::EndPaint();

  return S_OK;
}

static wstring GetOpenFilename()
{
  OPENFILENAME ofn;
  const int buflen = 255;
  wchar_t buf[buflen];

  memset(&ofn, 0, sizeof(OPENFILENAME));
  *buf = '\0';
  ofn.lStructSize = sizeof(OPENFILENAME);
  ofn.hwndOwner = GetActiveWindow();
  ofn.lpstrFile = (LPWSTR)buf;
  ofn.nMaxFile  = buflen;
  ofn.lpstrFilter = L"Movies (*.mp4, *.3gp, *.mov, *.avi, *.wmv) \0 *.mp4;*.3gp;*.mov;*.avi;*.wmv;\0All Files (*.*) \0 *.*\0";
  ofn.nFilterIndex = 1;
  ofn.lpstrInitialDir = NULL;
  ofn.lpstrTitle = L"Select movie to rotate";
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

  GetOpenFileName(&ofn);
  return wstring(buf);
}

void
MovieRotator::UpdateButtonsState()
{
  bool enabled = mVideoPlayer->IsVideoLoaded();

  mPlayPauseButton->SetEnabled(enabled);
  mRotateAntiClockwiseButton->SetEnabled(enabled);
  mRotateClockwiseButton->SetEnabled(enabled);

  ResourceId icon = mVideoPlayer->IsPaused() ? IDB_PLAY_ICON : IDB_PAUSE_ICON;
  mPlayPauseButton->SetIcon(icon);

  bool rotated = mVideoPlayer->GetRotation() != ROTATE_0;
  mSaveRotationButton->SetEnabled(rotated);
}

void
MovieRotator::Reset()
{
  mPlayPauseButton->SetEnabled(false);
  mPlayPauseButton->SetIcon(IDB_PLAY_ICON);
  mRotateAntiClockwiseButton->SetEnabled(false);
  mRotateClockwiseButton->SetEnabled(false);
  mSaveRotationButton->SetEnabled(false);
}

bool
MovieRotator::OpenFile(const wstring& aFilename)
{
  if (aFilename.empty()) {
    return false;
  }
  mVideoPlayer->Load(aFilename);
  return true;
}

// TODO: These need to be runnables, or mem_functions?
void
MovieRotator::OnOpenFile()
{
  OpenFile(GetOpenFilename());
}

void
MovieRotator::OnPlayPause()
{
  if (!mVideoPlayer || !mVideoPlayer->IsVideoLoaded()) {
    return;
  }
  if (mVideoPlayer->IsPaused()) {
    mVideoPlayer->Play();
  } else {
    mVideoPlayer->Pause();
  }
}

void
MovieRotator::OnVideoPlayerError()
{
  const int len = 1024;
  wchar_t str[len];
  StringCbPrintf(str,
                  len,
                  L"Error playing file:\n%s\n",
                  mVideoPlayer->GetOpenFilename().c_str());
  MessageBox(NULL, str, NULL, MB_OK | MB_ICONERROR);
  UpdateButtonsState();
}

void
MovieRotator::OnVideoPlayerLoaded()
{
  UpdateButtonsState();
}

void
MovieRotator::OnVideoPlayerPlaying()
{
  UpdateButtonsState();
}

void
MovieRotator::OnVideoPlayerPaused()
{
  UpdateButtonsState();
}

void
MovieRotator::OnRotateAntiClockwise()
{
  DBGMSG(L"Rotate Anti clockwise\n");
  mVideoPlayer->RotateAntiClockwise();
  UpdateButtonsState();
}

void
MovieRotator::OnRotateClockwise()
{
  DBGMSG(L"Rotate clockwise\n");
  mVideoPlayer->RotateClockwise();
  UpdateButtonsState();
}

static HRESULT
SetSuggestedOutputFilename(const wstring& aInFilename, wchar_t* aBuffer, const int aBufLen)
{
  if (aInFilename.empty()) {
    return E_FAIL;
  }
  size_t seperator = aInFilename.find_last_of(L".");
  if (seperator == wstring::npos) {
    return E_FAIL;
  }

  wstring filenameBeforeExtension = wstring(aInFilename, 0, seperator);
  wstring outputFilename = filenameBeforeExtension + L".rotated.mp4";

  DBGMSG(L"outputfilename= %s\n", outputFilename.c_str());

  UINT32 size = sizeof(outputFilename.c_str()[0]) * (outputFilename.length() + 1);
  memcpy(aBuffer, outputFilename.c_str(), size);

  return S_OK;
}

static HRESULT
GetOutputFileName(const wstring& aInFilename, wstring& aOutFilename, wstring& aOutFileExtension)
{
  OPENFILENAME ofn;
  const int buflen = 255;
  wchar_t buf[buflen];
  if (FAILED(SetSuggestedOutputFilename(aInFilename, buf, buflen))) {
    // For some reason, can't set output filename, just
    // give up and let the user choose.
    *buf = '\0';
  }

  memset(&ofn, 0, sizeof(OPENFILENAME));
  ofn.lStructSize = sizeof(OPENFILENAME);
  ofn.hwndOwner = GetActiveWindow();
  ofn.lpstrFile = (LPWSTR)buf;
  ofn.nMaxFile  = buflen;
  ofn.lpstrFilter = L"MP4 (*.mp4) \0 *.mp4;\0";
  ofn.nFilterIndex = 1;
  ofn.lpstrInitialDir = NULL;
  ofn.lpstrDefExt = L"mp4";
  ofn.lpstrTitle = L"Save rotated movie as";
  #ifndef _DEBUG
  ofn.Flags = OFN_OVERWRITEPROMPT;
  #endif

  if (GetSaveFileName(&ofn)) {
    aOutFilename = wstring(buf);
    aOutFileExtension = wstring(buf + ofn.nFileExtension);
    return S_OK;
  }

  return E_FAIL;
}

void
MovieRotator::OnSaveRotation()
{
  wstring outFilename, extension;
  const wstring& inFilename = mVideoPlayer->GetOpenFilename();
  if (FAILED(GetOutputFileName(inFilename, outFilename, extension)) ||
      outFilename.empty()) {
    return;
  }

  if (extension.empty() ||
      extension != L"mp4") {
    MessageBox(NULL,
               L"Save filename must have .mp4 extension.\n"
               L"Can only encode rotated video into MP4!",
               NULL,
               MB_OK | MB_ICONERROR);
    return;
  }

  if (outFilename == inFilename) {
    MessageBox(NULL,
               L"Input and output files must be different!",
               NULL,
               MB_OK | MB_ICONERROR);
    return;
  }

  // Start new transcode of rotation... Job starts automatically.
  mTranscodeManager->AddJob(new TranscodeJob(inFilename,
                                             outFilename,
                                             mVideoPlayer->GetRotation()));

  mVideoPlayer->Reset();
}

void
MovieRotator::OnKeyDown(WPARAM wParam)
{
  #ifdef _DEBUG
  const int KEY_0 = 0x30;
  switch (wParam) {
    case KEY_0 + 1: {
      mTranscodeManager->AddJob(new TranscodeJob(L"C:\\Users\\Bob\\Videos\\Audrey.mp4",
        L"C:\\Users\\Bob\\Videos\\Audrey.rotated.mp4",
        ROTATE_90));
      Reset();
      break;
    }

    case KEY_0 + 2: {
      mTranscodeManager->AddJob(new TranscodeJob(L"C:\\Users\\Bob\\Videos\\bruce_vs_ironman.mp4",
        L"C:\\Users\\Bob\\Videos\\bruce_vs_ironman.rotated.mp4",
        ROTATE_90));
      Reset();
      break;
    }

    case KEY_0 + 3: {
      mTranscodeManager->AddJob(new TranscodeJob(L"C:\\Users\\Bob\\Videos\\clone-wars.mp4",
        L"C:\\Users\\Bob\\Videos\\clone-wars.rotated.mp4",
        ROTATE_90));
      Reset();
      break;
    }

    case KEY_0 + 4: {
      mTranscodeManager->AddJob(new TranscodeJob(L"C:\\Users\\Bob\\Videos\\video-2012-08-05-10-47-32__video-2012-08-05-10-47-32video-2012-08-05-10-47-32__video-2012-08-05-10-47-32video-2012-08-05-10-47-32__video-2012-08-05-10-47-32.mp4",
        L"C:\\Users\\Bob\\Videos\\video-2012-08-05-10-47-32__video-2012-08-05-10-47-32video-2012-08-05-10-47-32__video-2012-08-05-10-47-32video-2012-08-05-10-47-32__video-2012-08-05-10-47-32.rotated.mp4",
        ROTATE_90));
      Reset();
      break;
    }

  }
  #endif // _DEBUG
}

void
MovieRotator::OnDropFiles(HDROP aDropHandle)
{
  if (DragQueryFile(aDropHandle, -1, nullptr, 0) == 0) {
    // No files in payload?
    return;
  }

  // Get the first filename/path. Ignore any others.
  wchar_t filename[MAX_PATH];
  if (DragQueryFile(aDropHandle, 0, filename, MAX_PATH) == 0) {
    return;
  }
  OpenFile(wstring(filename));
}

void
MovieRotator::OnWindowDestroyed()
{
  if (mVideoPlayer) {
    mVideoPlayer->Shutdown();
  }
  // TODO: Shutdown transcoder cleanly.
  PostQuitMessage(0);
}

LRESULT
MovieRotator::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  int wmId, wmEvent;

  // See if any of our registered event handlers can handle the event.
  for (unsigned i=0; i<mEventHandlers.size(); i++) {
    if (mEventHandlers[i]->Handle(hWnd, message, wParam, lParam)) {
      return 0;
    }
  }

  switch (message)
  {
  case MSG_CALL_FUNCTION: {
    EventListener* fn = (EventListener*)(lParam);
    (*fn)();
    delete fn;
    break;
  }
  case WM_COMMAND:
    wmId    = LOWORD(wParam);
    wmEvent = HIWORD(wParam);
    // Parse the menu selections:
    switch (wmId)
    {
    case IDM_ABOUT:
      DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
      break;
    case IDM_EXIT:
      DestroyWindow(hWnd);
      break;
    case IDM_OPEN:
      OnOpenFile();
      break;
    case IDM_ROTATE_CLOCKWISE:
      OnRotateClockwise();
      break;
    case IDM_ROTATE_ANTI_CLOCKWISE:
      OnRotateAntiClockwise();
      break;
    case IDM_SAVE:
      OnSaveRotation();
      break;
    case IDM_PLAY_PAUSE:
      OnPlayPause();
      break;
    default:
      return DefWindowProc(hWnd, message, wParam, lParam);
    }
    break;
  case WM_KEYDOWN:
    OnKeyDown(wParam);
    break;
  case WM_PAINT:
    OnPaint(hWnd);
    break;
  case WM_DESTROY:
    OnWindowDestroyed();
    break;
  case WM_MOVE:
    // Repaint if we move, to prevent any tearing.
    D2DManager::Invalidate();
    break;
  case WM_DROPFILES: {
    HDROP dropHandle = (HDROP)wParam;
    OnDropFiles(dropHandle);
    DragFinish(dropHandle);
    break;
  }
#ifndef DEBUG
  case WM_CLOSE:
    // Prompt for confirmation if any unsaved rotation.
    if (mTranscodeManager->IsRunning() &&
        IDNO == MessageBox(hWnd,
                            L"You still have rotations being saved.\n"
                            L"Are you sure you want to exit?",
                            L"Confirm exit",
                            MB_YESNO + MB_DEFBUTTON2 + MB_ICONEXCLAMATION))
    {
      return 0;
    }
#endif
  default:
    return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  UNREFERENCED_PARAMETER(lParam);
  switch (message)
  {
    case WM_INITDIALOG: {
      SetWindowLongPtr(hDlg, DWLP_USER, lParam);
      return (INT_PTR)TRUE;
    }
    case WM_COMMAND: {
      if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
      {
        EndDialog(hDlg, LOWORD(wParam));
        return (INT_PTR)TRUE;
      }
      break;
    }
  }
  return (INT_PTR)FALSE;
}

LRESULT CALLBACK
MovieRotatorWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  // Note: the WindowLongPtr can be null if we're called before it's been set,
  // i.e. in the WM_CREATE message, and maybe others.
  MovieRotator* mr = (MovieRotator*)(GetWindowLongPtr(hWnd, 0));
  if (mr) {
    return mr->WndProc(hWnd, message, wParam, lParam);
  }
  return DefWindowProc(hWnd, message, wParam, lParam);
}

HWND
GetMainWindowHWND()
{
  return sMainWindowHWND;
}
