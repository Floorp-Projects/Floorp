/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
// vie_autotest_windows.cc
//

#include "vie_autotest_windows.h"

#include "vie_autotest_defines.h"
#include "vie_autotest_main.h"

#include "engine_configurations.h"
#include "critical_section_wrapper.h"
#include "thread_wrapper.h"

#include <windows.h>

#ifdef _DEBUG
//#include "vld.h"
#endif

// Disable Visual studio warnings
// 'this' : used in base member initializer list
#pragma warning(disable: 4355)
// new behavior: elements of array 'XXX' will be default initialized
#pragma warning(disable: 4351)

LRESULT CALLBACK ViEAutoTestWinProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                    LPARAM lParam) {
  switch (uMsg) {
    case WM_DESTROY:
      PostQuitMessage( WM_QUIT);
      break;
    case WM_COMMAND:
      break;
  }
  return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

ViEAutoTestWindowManager::ViEAutoTestWindowManager()
    : _window1(NULL),
      _window2(NULL),
      _terminate(false),
      _eventThread(*webrtc::ThreadWrapper::CreateThread(
          EventProcess, this, webrtc::kNormalPriority,
          "ViEAutotestEventThread")),
      _crit(*webrtc::CriticalSectionWrapper::CreateCriticalSection()),
      _hwnd1(NULL),
      _hwnd2(NULL),
      _hwnd1Size(),
      _hwnd2Size(),
      _hwnd1Title(),
      _hwnd2Title() {
}

ViEAutoTestWindowManager::~ViEAutoTestWindowManager() {
  if (_hwnd1) {
    ViEDestroyWindow(_hwnd1);
  }
  if (_hwnd2) {
    ViEDestroyWindow(_hwnd1);
  }
  delete &_crit;
}

void* ViEAutoTestWindowManager::GetWindow1() {
  return _window1;
}

void* ViEAutoTestWindowManager::GetWindow2() {
  return _window2;
}

int ViEAutoTestWindowManager::CreateWindows(AutoTestRect window1Size,
                                            AutoTestRect window2Size,
                                            void* window1Title,
                                            void* window2Title) {
  _hwnd1Size.Copy(window1Size);
  _hwnd2Size.Copy(window2Size);
  memcpy(_hwnd1Title, window1Title, TITLE_LENGTH);
  memcpy(_hwnd2Title, window2Title, TITLE_LENGTH);

  unsigned int tId = 0;
  _eventThread.Start(tId);

  do {
    _crit.Enter();
    if (_window1 != NULL) {
      break;
    }
    _crit.Leave();
    AutoTestSleep(10);
  } while (true);
  _crit.Leave();
  return 0;
}

int ViEAutoTestWindowManager::TerminateWindows() {
  _eventThread.SetNotAlive();

  _terminate = true;
  if (_eventThread.Stop()) {
    _crit.Enter();
    delete &_eventThread;
    _crit.Leave();
  }

  return 0;
}

bool ViEAutoTestWindowManager::EventProcess(void* obj) {
  return static_cast<ViEAutoTestWindowManager*> (obj)->EventLoop();
}

bool ViEAutoTestWindowManager::EventLoop() {
  _crit.Enter();

  ViECreateWindow(_hwnd1, _hwnd1Size.origin.x, _hwnd1Size.origin.y,
                  _hwnd1Size.size.width, _hwnd1Size.size.height, _hwnd1Title);
  ViECreateWindow(_hwnd2, _hwnd2Size.origin.x, _hwnd2Size.origin.y,
                  _hwnd2Size.size.width, _hwnd2Size.size.height, _hwnd2Title);

  _window1 = (void*) _hwnd1;
  _window2 = (void*) _hwnd2;
  MSG msg;
  while (!_terminate) {
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    _crit.Leave();
    AutoTestSleep(10);
    _crit.Enter();
  }
  ViEDestroyWindow(_hwnd1);
  ViEDestroyWindow(_hwnd2);
  _crit.Leave();

  return false;
}

int ViEAutoTestWindowManager::ViECreateWindow(HWND &hwndMain, int xPos,
                                              int yPos, int width, int height,
                                              TCHAR* className) {
  HINSTANCE hinst = GetModuleHandle(0);
  WNDCLASSEX wcx;
  wcx.hInstance = hinst;
  wcx.lpszClassName = className;
  wcx.lpfnWndProc = (WNDPROC) ViEAutoTestWinProc;
  wcx.style = CS_DBLCLKS;
  wcx.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  wcx.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
  wcx.hCursor = LoadCursor(NULL, IDC_ARROW);
  wcx.lpszMenuName = NULL;
  wcx.cbSize = sizeof(WNDCLASSEX);
  wcx.cbClsExtra = 0;
  wcx.cbWndExtra = 0;
  wcx.hbrBackground = GetSysColorBrush(COLOR_3DFACE);

  RegisterClassEx(&wcx);

  // Create the main window.
  hwndMain = CreateWindowEx(0,          // no extended styles
                            className,  // class name
                            className,  // window name
                            WS_OVERLAPPED | WS_THICKFRAME,  // overlapped window
                            xPos,    // horizontal position
                            yPos,    // vertical position
                            width,   // width
                            height,  // height
                            (HWND) NULL,   // no parent or owner window
                            (HMENU) NULL,  // class menu used
                            hinst,  // instance handle
                            NULL);  // no window creation data

  if (!hwndMain)
    return -1;

  // Show the window using the flag specified by the program
  // that started the application, and send the application
  // a WM_PAINT message.
  ShowWindow(hwndMain, SW_SHOWDEFAULT);
  UpdateWindow(hwndMain);

  ::SetWindowPos(hwndMain, HWND_TOP, xPos, yPos, width, height,
                 SWP_FRAMECHANGED);

  return 0;
}

int ViEAutoTestWindowManager::ViEDestroyWindow(HWND& hwnd) {
  ::DestroyWindow(hwnd);
  return 0;
}

bool ViEAutoTestWindowManager::SetTopmostWindow() {
  // Meant to put terminal window on top
  return true;
}

int main(int argc, char* argv[]) {
  ViEAutoTestMain auto_test;
  return auto_test.RunTests(argc, argv);
}
