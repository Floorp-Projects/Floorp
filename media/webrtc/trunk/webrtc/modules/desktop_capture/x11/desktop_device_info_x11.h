/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_X11_DEVICE_INFO_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_X11_DEVICE_INFO_H_

#include "webrtc/typedefs.h"
#include "webrtc/modules/desktop_capture/desktop_device_info.h"

namespace webrtc {

class DesktopDeviceInfoX11 : public DesktopDeviceInfoImpl {
public:
  DesktopDeviceInfoX11();
  ~DesktopDeviceInfoX11();

  //DesktopDeviceInfo Interfaces
  virtual int32_t Init();
  virtual int32_t Refresh();

private:
#if !defined(MULTI_MONITOR_SCREENSHARE)
  int32_t MultiMonitorScreenshare();
#endif
};

}// namespace webrtc
#endif //WEBRTC_MODULES_DESKTOP_CAPTURE_X11_DEVICE_INFO_H_
