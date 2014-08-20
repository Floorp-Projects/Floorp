/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "webrtc/modules/desktop_capture/mac/desktop_device_info_mac.h"
#include <AppKit/AppKit.h>
#include <Cocoa/Cocoa.h>
#include <unistd.h>
#include <stdio.h>

namespace webrtc {

#define MULTI_MONITOR_NO_SUPPORT 1

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

#if !defined(MULTI_MONITOR_SCREENSHARE)
void DesktopDeviceInfoMac::MultiMonitorScreenshare()
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

void DesktopDeviceInfoMac::InitializeScreenList() {
#if !defined(MULTI_MONITOR_SCREENSHARE)
  MultiMonitorScreenshare();
#endif
}
void DesktopDeviceInfoMac::InitializeApplicationList() {
  //List all running applications (excluding background processes).

  NSArray *running = [[NSWorkspace sharedWorkspace] runningApplications];
  for (NSRunningApplication *ra in running) {
    if (ra.activationPolicy != NSApplicationActivationPolicyRegular)
      continue;

    ProcessId pid = ra.processIdentifier;
    if (pid == 0) {
      continue;
    }
    if (pid == getpid()) {
      continue;
    }

    DesktopApplication *pDesktopApplication = new DesktopApplication;
    if (!pDesktopApplication) {
      continue;
    }

    pDesktopApplication->setProcessId(pid);

    NSString *str;
    str = [ra.executableURL absoluteString];
    pDesktopApplication->setProcessPathName([str UTF8String]);

    str = ra.localizedName;
    pDesktopApplication->setProcessAppName([str UTF8String]);

    char idStr[64];
    snprintf(idStr, sizeof(idStr), "%ld", pDesktopApplication->getProcessId());
    pDesktopApplication->setUniqueIdName(idStr);

    desktop_application_list_[pDesktopApplication->getProcessId()] = pDesktopApplication;
  }
}

} //namespace webrtc
