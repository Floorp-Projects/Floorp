/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "webrtc/modules/desktop_capture/win/desktop_device_info_win.h"
#include "webrtc/modules/desktop_capture/win/screen_capture_utils.h"
#include "webrtc/modules/desktop_capture/win/win_shared.h"
#include <inttypes.h>
#include <stdio.h>
#include <VersionHelpers.h>

// Duplicating declaration so that it always resolves in decltype use
// typedef BOOL (WINAPI *QueryFullProcessImageNameProc)(HANDLE hProcess, DWORD dwFlags, LPTSTR lpExeName, PDWORD lpdwSize);
BOOL WINAPI QueryFullProcessImageName(HANDLE hProcess, DWORD dwFlags, LPTSTR lpExeName, PDWORD lpdwSize);

// Duplicating declaration so that it always resolves in decltype use
// typedoef DWORD (WINAPI *GetProcessImageFileNameProc)(HANDLE hProcess, LPTSTR lpImageFileName, DWORD nSize);
DWORD WINAPI GetProcessImageFileName(HANDLE hProcess, LPTSTR lpImageFileName, DWORD nSize);

namespace webrtc {

DesktopDeviceInfo * DesktopDeviceInfoImpl::Create() {
  DesktopDeviceInfoWin * pDesktopDeviceInfo = new DesktopDeviceInfoWin();
  if(pDesktopDeviceInfo && pDesktopDeviceInfo->Init() != 0){
    delete pDesktopDeviceInfo;
    pDesktopDeviceInfo = NULL;
  }
  return pDesktopDeviceInfo;
}

DesktopDeviceInfoWin::DesktopDeviceInfoWin() {
}

DesktopDeviceInfoWin::~DesktopDeviceInfoWin() {
}

void DesktopDeviceInfoWin::MultiMonitorScreenshare()
{
#if !defined(MULTI_MONITOR_SCREENSHARE)
  DesktopDisplayDevice* desktop_device_info = new DesktopDisplayDevice;
  if (desktop_device_info) {
    desktop_device_info->setScreenId(webrtc::kFullDesktopScreenId);
    desktop_device_info->setDeviceName("Primary Monitor");

     char idStr[64];
    _snprintf_s(idStr, sizeof(idStr), sizeof(idStr) - 1, "%" PRIdPTR, desktop_device_info->getScreenId());
    desktop_device_info->setUniqueIdName(idStr);
    desktop_display_list_[desktop_device_info->getScreenId()] = desktop_device_info;
  }
#else
  ScreenCapturer::ScreenList screens;
  GetScreenList(&screens);
  auto num_of_screens = screens.size();

  for (decltype(num_of_screens) i = 0; i < num_of_screens; ++i) {
    DesktopDisplayDevice* desktop_device_info = new DesktopDisplayDevice;
    if (desktop_device_info) {
      desktop_device_info->setScreenId(screens[i].id);
      if (1 >= num_of_screens) {
        desktop_device_info->setDeviceName("Primary Monitor");
      } else {
        char nameStr[64];
        _snprintf_s(nameStr, sizeof(nameStr), sizeof(nameStr) - 1, "Screen %" PRIdPTR, i + 1);
        desktop_device_info->setDeviceName(nameStr);
      }

      char idStr[64];
      _snprintf_s(idStr, sizeof(idStr), sizeof(idStr) - 1, "%" PRIdPTR, desktop_device_info->getScreenId());
      desktop_device_info->setUniqueIdName(idStr);
      desktop_display_list_[desktop_device_info->getScreenId()] = desktop_device_info;
    }
  }
#endif
}

void DesktopDeviceInfoWin::InitializeScreenList() {
  MultiMonitorScreenshare();
}

void DesktopDeviceInfoWin::InitializeApplicationList() {
  // List all running applications exclude background process.
  HWND hWnd;
  for (hWnd = GetWindow(GetDesktopWindow(), GW_CHILD); hWnd; hWnd = GetWindow(hWnd, GW_HWNDNEXT)) {
    if (!IsWindowVisible(hWnd)) {
      continue;
    }

    DWORD dwProcessId = 0;
    GetWindowThreadProcessId(hWnd, &dwProcessId);

    // filter out non-process, current process
    if (dwProcessId == 0 || dwProcessId == GetCurrentProcessId()) {
      continue;
    }

    // Win8 introduced "Modern Apps" whose associated window is
    // non-shareable. We want to filter them out.
    const int classLength = 256;
    WCHAR class_name[classLength] = {0};
    GetClassName(hWnd, class_name, classLength);
    if (IsWindows8OrGreater() &&
        (wcscmp(class_name, L"ApplicationFrameWindow") == 0 ||
         wcscmp(class_name, L"Windows.UI.Core.CoreWindow") == 0)) {
      continue;
    }

    // filter out already-seen processes, after updating the window count
    DesktopApplicationList::iterator itr = desktop_application_list_.find(dwProcessId);
    if (itr != desktop_application_list_.end()) {
      itr->second->setWindowCount(itr->second->getWindowCount() + 1);
      continue;
    }

    // Add one application
    DesktopApplication *pDesktopApplication = new DesktopApplication;
    if (!pDesktopApplication) {
      continue;
    }

    // process id
    pDesktopApplication->setProcessId(dwProcessId);
    // initialize window count to 1 (updated on subsequent finds)
    pDesktopApplication->setWindowCount(1);

    // process path name
    WCHAR szFilePathName[MAX_PATH]={0};
    decltype(QueryFullProcessImageName) *lpfnQueryFullProcessImageNameProc =
      reinterpret_cast<decltype(QueryFullProcessImageName) *>(GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "QueryFullProcessImageNameW"));
    if (lpfnQueryFullProcessImageNameProc) {
      // After Vista
      DWORD dwMaxSize = _MAX_PATH;
      HANDLE hWndPro = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwProcessId);
      if(hWndPro) {
        lpfnQueryFullProcessImageNameProc(hWndPro, 0, szFilePathName, &dwMaxSize);
        CloseHandle(hWndPro);
      }
    } else {
      HMODULE hModPSAPI = LoadLibrary(TEXT("PSAPI.dll"));
      if (hModPSAPI) {
        decltype(GetProcessImageFileName) *pfnGetProcessImageFileName =
          reinterpret_cast<decltype(GetProcessImageFileName) *>(GetProcAddress(hModPSAPI, "GetProcessImageFileNameW"));

        if (pfnGetProcessImageFileName) {
          HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, 0, dwProcessId);
          if (hProcess) {
            DWORD dwMaxSize = _MAX_PATH;
            pfnGetProcessImageFileName(hProcess, szFilePathName, dwMaxSize);
            CloseHandle(hProcess);
          }
        }
        FreeLibrary(hModPSAPI);
      }
    }
    pDesktopApplication->setProcessPathName(Utf16ToUtf8(szFilePathName).c_str());

    // application name
    WCHAR szWndTitle[_MAX_PATH]={0};
    GetWindowText(hWnd, szWndTitle, MAX_PATH);
    if (lstrlen(szWndTitle) <= 0) {
      pDesktopApplication->setProcessAppName(Utf16ToUtf8(szFilePathName).c_str());
    } else {
      pDesktopApplication->setProcessAppName(Utf16ToUtf8(szWndTitle).c_str());
    }

    // unique id name
    char idStr[64];
    _snprintf_s(idStr, sizeof(idStr), sizeof(idStr) - 1, "%ld", pDesktopApplication->getProcessId());
    pDesktopApplication->setUniqueIdName(idStr);

    desktop_application_list_[pDesktopApplication->getProcessId()] = pDesktopApplication;
  }

  // re-walk the application list, prepending the window count to the application name
  DesktopApplicationList::iterator itr;
  for (itr = desktop_application_list_.begin(); itr != desktop_application_list_.end(); itr++) {
    DesktopApplication *pApp = itr->second;

    // localized application names can be *VERY* large
    char nameStr[BUFSIZ];
    _snprintf_s(nameStr, sizeof(nameStr), sizeof(nameStr) - 1, "%d\x1e%s",
                pApp->getWindowCount(), pApp->getProcessAppName());
    pApp->setProcessAppName(nameStr);
  }
}

} // namespace webrtc
