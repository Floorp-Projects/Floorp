/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "webrtc/modules/desktop_capture/desktop_device_info.h"

namespace webrtc {

class DesktopDeviceInfoNull : public DesktopDeviceInfoImpl {
public:
  DesktopDeviceInfoNull();
  ~DesktopDeviceInfoNull();

protected:
  virtual void InitializeScreenList();
  virtual void InitializeApplicationList();
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

void
DesktopDeviceInfoNull::InitializeScreenList() {
}

void
DesktopDeviceInfoNull::InitializeApplicationList() {
}

} //namespace webrtc
