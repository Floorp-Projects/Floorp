/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_DEVICE_INFO_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_DEVICE_INFO_H_

#include "webrtc/typedefs.h"
#include "webrtc/modules/desktop_capture/desktop_device_info.h"

namespace webrtc {

class DesktopDeviceInfoWin : public DesktopDeviceInfoImpl {
public:
  DesktopDeviceInfoWin();
  ~DesktopDeviceInfoWin();

protected:
  //DesktopDeviceInfo Interfaces
  virtual void InitializeApplicationList() OVERRIDE;
  virtual void InitializeScreenList() OVERRIDE;

private:
#if !defined(MULTI_MONITOR_SCREENSHARE)
  void MultiMonitorScreenshare();
#endif
};

}// namespace webrtc

#endif //WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_DEVICE_INFO_H_
