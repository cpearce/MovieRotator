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
#include "D2DManager.h"
#include "H264ClassFactory.h"
#include "PlaybackClocks.h"
#include "MovieRotator2.h"

static bool
Win7OrLater()
{
  OSVERSIONINFO version;
  ZeroMemory(&version, sizeof(version));
  version.dwOSVersionInfoSize = sizeof(version);
  GetVersionEx(&version);

  return (version.dwMajorVersion > 6) ||
         ((version.dwMajorVersion == 6) && (version.dwMinorVersion >= 1));
}

static const wchar_t* DLLs[] = {
  L"mfplat.dll",
  L"Mfreadwrite.dll",
  L"Propsys.dll",
  L"d3d9.dll",
  L"dxva2.dll",
  L"mf.dll",
  L"d2d1.dll",
  L"dwrite.dll"
};

static const wchar_t* Month[12] = {
  L"Jan", L"Feb", L"Mar",
  L"Apr", L"May", L"Jun",
  L"Jul", L"Aug", L"Sep",
  L"Oct", L"Nov", L"Dec"
};

void
LogLocalTime(wchar_t* aMessage)
{
  SYSTEMTIME lt;
  GetLocalTime(&lt);
  DBGMSG(L"%s - %02d:%02d:%02d %d %s %d\n",
         aMessage,
         lt.wHour, lt.wMinute, lt.wSecond,
         lt.wDay, Month[lt.wMonth - 1], lt.wYear);
}

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                       _In_opt_ HINSTANCE hPrevInstance,
                       _In_ LPTSTR    lpCmdLine,
                       _In_ int       nCmdShow)
{
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

  LogLocalTime(L"Application started");

  if (!Win7OrLater()) {
    ErrorMsgBox(L"MovieRotator only runs on Windows 7 or higher. Sorry!");
    return 0;
  }

  // Ensure all the DLLs we require are installed.
  for (UINT32 i = 0; i < ARRAYSIZE(DLLs); i++) {
    if (!LoadLibrary(DLLs[i])) {
      ErrorMsgBox(L"Required DLL '%s' not found! Can't start up!", DLLs[i]);
      return 0;
    }
  }

  AutoComInit initCOM;
  AutoWMFInit initWMF;
  AutoD2DInit initD2D;
  AutoRegisterH264ClassFactory initH264ClassFactory;
  AutoInitCubeb initCubeb;

  MSG msg;
  HACCEL hAccelTable;

  MovieRotator application;

  if (FAILED(application.Init(hInstance, nCmdShow))) {
    return FALSE;
  }

  hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MOVIEROTATOR2));

  // Main message loop:
  while (GetMessage(&msg, NULL, 0, 0)) {
    if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  LogLocalTime(L"Application exit");
  DBGMSG(L"\n\n");

  return (int) msg.wParam;
}
