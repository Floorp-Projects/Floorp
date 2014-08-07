/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "webrtc/modules/desktop_capture/desktop_device_info.h"

namespace webrtc {

class DesktopDeviceInfoNull : public DesktopDeviceInfoImpl {
public:
  DesktopDeviceInfoNull();
  ~DesktopDeviceInfoNull();

  virtual int32_t Init();
  virtual int32_t Refresh();
};

DesktopDeviceInfo * DesktopDeviceInfoImpl::Create() {
  DesktopDeviceInfoNull * pDesktopDeviceInfo = new DesktopDeviceInfoNull();
  if (pDesktopDeviceInfo && pDesktopDeviceInfo->Init() != 0) {
    delete pDesktopDeviceInfo;
    pDesktopDeviceInfo = NULL;
  }
  return pDesktopDeviceInfo;
}

DesktopDeviceInfoNull::DesktopDeviceInfoNull() {
}

DesktopDeviceInfoNull::~DesktopDeviceInfoNull() {
}

int32_t
DesktopDeviceInfoNull::Init() {
  initializeWindowList();
  return 0;
}

int32_t
DesktopDeviceInfoNull::Refresh() {
  return 0;
}

} //namespace webrtc
