/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_DEVICE_INFO_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_DEVICE_INFO_H_

#include <map>
#include "webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace webrtc {

class DesktopDisplayDevice {
public:
  DesktopDisplayDevice();
  ~DesktopDisplayDevice();

  void setScreenId(const ScreenId screenId);
  void setDeviceName(const char *deviceNameUTF8);
  void setUniqueIdName(const char *deviceUniqueIdUTF8);

  ScreenId getScreenId();
  const char *getDeviceName();
  const char *getUniqueIdName();

  DesktopDisplayDevice& operator= (DesktopDisplayDevice& other);

protected:
  ScreenId screenId_;
  char* deviceNameUTF8_;
  char* deviceUniqueIdUTF8_;
};

typedef std::map<intptr_t,DesktopDisplayDevice*> DesktopDisplayDeviceList;

class DesktopApplication {
public:
  DesktopApplication();
  ~DesktopApplication();

  void setProcessId(const ProcessId processId);
  void setProcessPathName(const char *appPathNameUTF8);
  void setUniqueIdName(const char *appUniqueIdUTF8);
  void setProcessAppName(const char *appNameUTF8);

  ProcessId getProcessId();
  const char *getProcessPathName();
  const char *getUniqueIdName();
  const char *getProcessAppName();

  DesktopApplication& operator= (DesktopApplication& other);

protected:
  ProcessId processId_;
  char* processPathNameUTF8_;
  char* applicationNameUTF8_;
  char* processUniqueIdUTF8_;
};

typedef std::map<intptr_t,DesktopApplication*> DesktopApplicationList;

class DesktopDeviceInfo {
public:
  virtual ~DesktopDeviceInfo() {};

  virtual int32_t Init() = 0;
  virtual int32_t getDisplayDeviceCount() = 0;
  virtual int32_t getDesktopDisplayDeviceInfo(int32_t nIndex,
                                              DesktopDisplayDevice & desktopDisplayDevice) = 0;
  virtual int32_t getApplicationCount() = 0;
  virtual int32_t getApplicationInfo(int32_t nIndex,
                                     DesktopApplication & desktopApplication) = 0;
};

class DesktopDeviceInfoImpl : public DesktopDeviceInfo {
public:
  DesktopDeviceInfoImpl();
  ~DesktopDeviceInfoImpl();

  virtual int32_t getDisplayDeviceCount();
  virtual int32_t getDesktopDisplayDeviceInfo(int32_t nIndex,
                                              DesktopDisplayDevice & desktopDisplayDevice);
  virtual int32_t getApplicationCount();
  virtual int32_t getApplicationInfo(int32_t nIndex,
                                     DesktopApplication & desktopApplication);

  static DesktopDeviceInfo * Create();

protected:
  DesktopDisplayDeviceList desktop_display_list_;
  DesktopApplicationList desktop_application_list_;
};
};

#endif
