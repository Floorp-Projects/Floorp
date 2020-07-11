/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_DEVICE_INFO_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_DEVICE_INFO_H_

#include <map>
#include "modules/desktop_capture/desktop_capture_types.h"

namespace webrtc {

class DesktopDisplayDevice {
public:
  DesktopDisplayDevice();
  ~DesktopDisplayDevice();

  void setScreenId(const ScreenId screenId);
  void setDeviceName(const char *deviceNameUTF8);
  void setUniqueIdName(const char *deviceUniqueIdUTF8);
  void setPid(pid_t pid);

  ScreenId getScreenId();
  const char *getDeviceName();
  const char *getUniqueIdName();
  pid_t getPid();

  DesktopDisplayDevice& operator= (DesktopDisplayDevice& other);

protected:
  ScreenId screenId_;
  char* deviceNameUTF8_;
  char* deviceUniqueIdUTF8_;
  pid_t pid_;
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
  void setWindowCount(const uint32_t count);

  ProcessId getProcessId();
  const char *getProcessPathName();
  const char *getUniqueIdName();
  const char *getProcessAppName();
  uint32_t getWindowCount();

  DesktopApplication& operator= (DesktopApplication& other);

protected:
  ProcessId processId_;
  char* processPathNameUTF8_;
  char* applicationNameUTF8_;
  char* processUniqueIdUTF8_;
  uint32_t windowCount_;
};

typedef std::map<intptr_t, DesktopApplication*> DesktopApplicationList;

class DesktopTab {
public:
  DesktopTab();
  ~DesktopTab();

  void setTabBrowserId(uint64_t tabBrowserId);
  void setUniqueIdName(const char *tabUniqueIdUTF8);
  void setTabName(const char *tabNameUTF8);
  void setTabCount(const uint32_t count);

  uint64_t getTabBrowserId();
  const char *getUniqueIdName();
  const char *getTabName();
  uint32_t getTabCount();

  DesktopTab& operator= (DesktopTab& other);

protected:
  uint64_t tabBrowserId_;
  char* tabNameUTF8_;
  char* tabUniqueIdUTF8_;
  uint32_t tabCount_;
};

typedef std::map<intptr_t, DesktopTab*> DesktopTabList;

class DesktopDeviceInfo {
public:
  virtual ~DesktopDeviceInfo() {};

  virtual int32_t Init() = 0;
  virtual int32_t Refresh() = 0;
  virtual int32_t getDisplayDeviceCount() = 0;
  virtual int32_t getDesktopDisplayDeviceInfo(int32_t nIndex,
                                              DesktopDisplayDevice & desktopDisplayDevice) = 0;
  virtual int32_t getWindowCount() = 0;
  virtual int32_t getWindowInfo(int32_t nindex,
                                DesktopDisplayDevice &windowDevice) = 0;
  virtual int32_t getApplicationCount() = 0;
  virtual int32_t getApplicationInfo(int32_t nIndex,
                                     DesktopApplication & desktopApplication) = 0;
  virtual int32_t getTabCount() = 0;
  virtual int32_t getTabInfo(int32_t nIndex,
                             DesktopTab & desktopTab) = 0;
};

class DesktopDeviceInfoImpl : public DesktopDeviceInfo {
public:
  DesktopDeviceInfoImpl();
  ~DesktopDeviceInfoImpl();

  int32_t Init() override;
  int32_t Refresh() override;
  int32_t getDisplayDeviceCount() override;
  int32_t getDesktopDisplayDeviceInfo(int32_t nIndex,
                                              DesktopDisplayDevice & desktopDisplayDevice) override;
  int32_t getWindowCount() override;
  int32_t getWindowInfo(int32_t nindex,
                                DesktopDisplayDevice &windowDevice) override;
  int32_t getApplicationCount() override;
  int32_t getApplicationInfo(int32_t nIndex,
                                     DesktopApplication & desktopApplication) override;
  int32_t getTabCount() override;
  int32_t getTabInfo(int32_t nIndex,
                             DesktopTab & desktopTab) override;
  static DesktopDeviceInfo * Create();
protected:
  DesktopDisplayDeviceList desktop_display_list_;
  DesktopDisplayDeviceList desktop_window_list_;
  DesktopApplicationList desktop_application_list_;
  DesktopTabList desktop_tab_list_;

  void CleanUp();
  void CleanUpWindowList();
  void CleanUpApplicationList();
  void CleanUpTabList();
  void CleanUpScreenList();

  void InitializeWindowList();
  virtual void InitializeApplicationList() = 0;
  virtual void InitializeTabList();
  virtual void InitializeScreenList() = 0;

  void RefreshWindowList();
  void RefreshApplicationList();
  void RefreshTabList();
  void RefreshScreenList();

  void DummyTabList(DesktopTabList &list);
};
};

#endif
