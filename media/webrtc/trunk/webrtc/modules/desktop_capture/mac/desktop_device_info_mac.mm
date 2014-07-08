/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "webrtc/modules/desktop_capture/mac/desktop_device_info_mac.h"
#include <Cocoa/Cocoa.h>

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

int32_t DesktopDeviceInfoMac::Init() {
#if !defined(MULTI_MONITOR_SCREENSHARE)
  DesktopDisplayDevice *pDesktopDeviceInfo = new DesktopDisplayDevice;
  if(pDesktopDeviceInfo) {
    pDesktopDeviceInfo->setScreenId(0);
    pDesktopDeviceInfo->setDeviceName("Primary Monitor");
    pDesktopDeviceInfo->setUniqueIdName("\\screen\\monitor#1");
    desktop_display_list_[pDesktopDeviceInfo->getScreenId()] = pDesktopDeviceInfo;
  }
#endif
  return 0;
}

} //namespace webrtc
