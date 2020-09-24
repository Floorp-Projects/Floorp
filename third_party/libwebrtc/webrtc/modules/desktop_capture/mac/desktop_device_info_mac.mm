/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "modules/desktop_capture/mac/desktop_device_info_mac.h"
#include <AppKit/AppKit.h>
#include <Cocoa/Cocoa.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdio.h>
#include <map>

namespace webrtc {

// Helper type to track the number of window instances for a given process
typedef std::map<ProcessId, uint32_t> AppWindowCountMap;


DesktopDeviceInfo * DesktopDeviceInfoImpl::Create() {
  DesktopDeviceInfoMac * pDesktopDeviceInfo = new DesktopDeviceInfoMac();
  if (pDesktopDeviceInfo && pDesktopDeviceInfo->Init() != 0){
    delete pDesktopDeviceInfo;
    pDesktopDeviceInfo = NULL;
  }
  return pDesktopDeviceInfo;
}

DesktopDeviceInfoMac::DesktopDeviceInfoMac() {
}

DesktopDeviceInfoMac::~DesktopDeviceInfoMac() {
}

void DesktopDeviceInfoMac::MultiMonitorScreenshare()
{
#if !defined(MULTI_MONITOR_SCREENSHARE)
  DesktopDisplayDevice* desktop_device_info = new DesktopDisplayDevice;
  if (desktop_device_info) {
    desktop_device_info->setScreenId(CGMainDisplayID());
    desktop_device_info->setDeviceName("Primary Monitor");

    char idStr[64];
    snprintf(idStr, sizeof(idStr), "%" PRIdPTR, desktop_device_info->getScreenId());
    desktop_device_info->setUniqueIdName(idStr);
    desktop_display_list_[desktop_device_info->getScreenId()] = desktop_device_info;
  }
#else
  const UInt32 kMaxScreens = 256;
  CGDirectDisplayID screens[kMaxScreens];
  CGDisplayCount num_of_screens;
  CGGetActiveDisplayList(kMaxScreens, screens, &num_of_screens);

  for (CFIndex i = 0; i < num_of_screens; ++i) {
    DesktopDisplayDevice* desktop_device_info = new DesktopDisplayDevice;
    if (desktop_device_info) {
      desktop_device_info->setScreenId(screens[i]);
      if (1 >= num_of_screens) {
        desktop_device_info->setDeviceName("Primary Monitor");
      } else {
        char nameStr[64];
        snprintf(nameStr, sizeof(nameStr), "Screen %" PRIdPTR, i + 1);
        desktop_device_info->setDeviceName(nameStr);
      }

      char idStr[64];
      snprintf(idStr, sizeof(idStr), "%" PRIdPTR, desktop_device_info->getScreenId());
      desktop_device_info->setUniqueIdName(idStr);
      desktop_display_list_[desktop_device_info->getScreenId()] = desktop_device_info;
    }
  }
#endif
}

void DesktopDeviceInfoMac::InitializeScreenList() {
  MultiMonitorScreenshare();
}

} //namespace webrtc
