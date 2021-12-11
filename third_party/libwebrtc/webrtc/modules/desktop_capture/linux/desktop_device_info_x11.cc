/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "modules/desktop_capture/linux/desktop_device_info_x11.h"
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>
#include "modules/desktop_capture/linux/shared_x_util.h"
#include "modules/desktop_capture/linux/x_error_trap.h"
#include "modules/desktop_capture/linux/x_server_pixel_buffer.h"
#include "rtc_base/logging.h"

namespace webrtc {

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

void DesktopDeviceInfoX11::MultiMonitorScreenshare()
{
  DesktopDisplayDevice* desktop_device_info = new DesktopDisplayDevice;
  if (desktop_device_info) {
    desktop_device_info->setScreenId(webrtc::kFullDesktopScreenId);
    desktop_device_info->setDeviceName("Primary Monitor");

    char idStr[64];
    snprintf(idStr, sizeof(idStr), "%" PRIdPTR, desktop_device_info->getScreenId());
    desktop_device_info->setUniqueIdName(idStr);
    desktop_display_list_[desktop_device_info->getScreenId()] = desktop_device_info;
  }
}

void DesktopDeviceInfoX11::InitializeScreenList() {
  MultiMonitorScreenshare();
}

} //namespace webrtc
