/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "modules/desktop_capture/win/desktop_device_info_win.h"
#include "modules/desktop_capture/win/screen_capture_utils.h"
#include "modules/desktop_capture/win/win_shared.h"
#include <inttypes.h>
#include <stdio.h>
#include <VersionHelpers.h>

// Duplicating declaration so that it always resolves in decltype use
// typedef BOOL (WINAPI *QueryFullProcessImageNameProc)(HANDLE hProcess, DWORD dwFlags, LPTSTR lpExeName, PDWORD lpdwSize);
WINBASEAPI BOOL WINAPI QueryFullProcessImageName(HANDLE hProcess, DWORD dwFlags, LPWSTR lpExeName, PDWORD lpdwSize);

// Duplicating declaration so that it always resolves in decltype use
// typedoef DWORD (WINAPI *GetProcessImageFileNameProc)(HANDLE hProcess, LPTSTR lpImageFileName, DWORD nSize);
DWORD WINAPI GetProcessImageFileName(HANDLE hProcess, LPTSTR lpImageFileName, DWORD nSize);

namespace webrtc {

// static
DesktopDeviceInfo * DesktopDeviceInfoImpl::Create() {
  DesktopDeviceInfoWin * pDesktopDeviceInfo = new DesktopDeviceInfoWin();
  if(pDesktopDeviceInfo && pDesktopDeviceInfo->Init() != 0){
    delete pDesktopDeviceInfo;
    pDesktopDeviceInfo = nullptr;
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
  DesktopCapturer::SourceList screens;
  webrtc::GetScreenList(&screens);
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

} // namespace webrtc
