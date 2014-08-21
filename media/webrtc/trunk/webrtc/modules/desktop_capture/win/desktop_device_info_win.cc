/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "webrtc/modules/desktop_capture/win/desktop_device_info_win.h"
#include "webrtc/modules/desktop_capture/win/win_shared.h"
#include <stdio.h>

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

#if !defined(MULTI_MONITOR_SCREENSHARE)
void DesktopDeviceInfoWin::MultiMonitorScreenshare()
{
  DesktopDisplayDevice *pDesktopDeviceInfo = new DesktopDisplayDevice;
  if (pDesktopDeviceInfo) {
    pDesktopDeviceInfo->setScreenId(0);
    pDesktopDeviceInfo->setDeviceName("Primary Monitor");
    pDesktopDeviceInfo->setUniqueIdName("\\screen\\monitor#1");

    desktop_display_list_[pDesktopDeviceInfo->getScreenId()] = pDesktopDeviceInfo;
  }
}
#endif

void DesktopDeviceInfoWin::InitializeScreenList() {
#if !defined(MULTI_MONITOR_SCREENSHARE)
  MultiMonitorScreenshare();
#endif
}
void DesktopDeviceInfoWin::InitializeApplicationList() {
  // List all running applications exclude background process.
  HWND hWnd;
  for (hWnd = GetWindow(GetDesktopWindow(), GW_CHILD); hWnd; hWnd = GetWindow(hWnd, GW_HWNDNEXT)) {
    if (!IsWindowVisible(hWnd))
      continue;

    DWORD dwProcessId = 0;
    GetWindowThreadProcessId(hWnd, &dwProcessId);

    // filter out non-process, current process, or any already seen processes
    if (dwProcessId == 0 || dwProcessId == GetCurrentProcessId() ||
        desktop_application_list_.find(dwProcessId) != desktop_application_list_.end()) {
      continue;
    }

    // Add one application
    DesktopApplication *pDesktopApplication = new DesktopApplication;
    if (!pDesktopApplication) {
      continue;
    }

    // process id
    pDesktopApplication->setProcessId(dwProcessId);

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
}

} // namespace webrtc
