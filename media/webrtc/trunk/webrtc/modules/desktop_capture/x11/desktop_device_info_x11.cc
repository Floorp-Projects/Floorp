/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "webrtc/modules/desktop_capture/x11/desktop_device_info_x11.h"
#include "webrtc/modules/desktop_capture/window_capturer.h"

namespace webrtc{

DesktopDeviceInfo * DesktopDeviceInfoImpl::Create() {
  DesktopDeviceInfoX11 * pDesktopDeviceInfo = new DesktopDeviceInfoX11();
  if (pDesktopDeviceInfo && pDesktopDeviceInfo->Init() != 0){
    delete pDesktopDeviceInfo;
    pDesktopDeviceInfo = NULL;
  }
  return pDesktopDeviceInfo;
}

DesktopDeviceInfoX11::DesktopDeviceInfoX11() {
}

DesktopDeviceInfoX11::~DesktopDeviceInfoX11() {
}

#if !defined(MULTI_MONITOR_SCREENSHARE)
int32_t DesktopDeviceInfoX11::MultiMonitorScreenshare()
{
  DesktopDisplayDevice *pDesktopDeviceInfo = new DesktopDisplayDevice;
  if (pDesktopDeviceInfo) {
    pDesktopDeviceInfo->setScreenId(0);
    pDesktopDeviceInfo->setDeviceName("Primary Monitor");
    pDesktopDeviceInfo->setUniqueIdName("\\screen\\monitor#1");

    desktop_display_list_[pDesktopDeviceInfo->getScreenId()] = pDesktopDeviceInfo;
  }
  return 0;
}
#endif

int32_t DesktopDeviceInfoX11::Init() {
#if !defined(MULTI_MONITOR_SCREENSHARE)
  MultiMonitorScreenshare();
#endif

  initializeWindowList();

  return 0;
}

int32_t DesktopDeviceInfoX11::Refresh() {
#if !defined(MULTI_MONITOR_SCREENSHARE)
  std::map<intptr_t,DesktopDisplayDevice*>::iterator iterDevice;
  for (iterDevice=desktop_display_list_.begin(); iterDevice!=desktop_display_list_.end(); iterDevice++){
    DesktopDisplayDevice * pDesktopDisplayDevice = iterDevice->second;
    delete pDesktopDisplayDevice;
    iterDevice->second = NULL;
  }
  desktop_display_list_.clear();
  MultiMonitorScreenshare();
#endif

  RefreshWindowList();

  return 0;
}

} //namespace webrtc
