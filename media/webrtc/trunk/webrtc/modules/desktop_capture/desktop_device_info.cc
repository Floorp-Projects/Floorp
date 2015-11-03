/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "webrtc/modules/desktop_capture/desktop_device_info.h"
#include "webrtc/modules/desktop_capture/window_capturer.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <cstring>

namespace webrtc {

static inline void SetStringMember(char **member, const char *value) {
  if (!value) {
    return;
  }

  if (*member) {
    delete [] *member;
    *member = NULL;
  }

  size_t  nBufLen = strlen(value) + 1;
  char *buffer = new char[nBufLen];
  memcpy(buffer, value, nBufLen - 1);
  buffer[nBufLen - 1] = '\0';
  *member = buffer;
}

DesktopDisplayDevice::DesktopDisplayDevice() {
  screenId_ = kInvalidScreenId;
  deviceUniqueIdUTF8_ = NULL;
  deviceNameUTF8_ = NULL;
}

DesktopDisplayDevice::~DesktopDisplayDevice() {
  screenId_ = kInvalidScreenId;

  if (deviceUniqueIdUTF8_){
    delete [] deviceUniqueIdUTF8_;
  }

  if (deviceNameUTF8_){
    delete [] deviceNameUTF8_;
  }

  deviceUniqueIdUTF8_ = NULL;
  deviceNameUTF8_ = NULL;
}

void DesktopDisplayDevice::setScreenId(const ScreenId screenId) {
  screenId_ = screenId;
}

void DesktopDisplayDevice::setDeviceName(const char *deviceNameUTF8) {
  SetStringMember(&deviceNameUTF8_, deviceNameUTF8);
}

void DesktopDisplayDevice::setUniqueIdName(const char *deviceUniqueIdUTF8) {
  SetStringMember(&deviceUniqueIdUTF8_, deviceUniqueIdUTF8);
}

ScreenId DesktopDisplayDevice::getScreenId() {
  return screenId_;
}

const char *DesktopDisplayDevice::getDeviceName() {
  return deviceNameUTF8_;
}

const char *DesktopDisplayDevice::getUniqueIdName() {
  return deviceUniqueIdUTF8_;
}

DesktopDisplayDevice& DesktopDisplayDevice::operator= (DesktopDisplayDevice& other) {
  if (&other == this) {
    return *this;
  }
  screenId_ = other.getScreenId();
  setUniqueIdName(other.getUniqueIdName());
  setDeviceName(other.getDeviceName());

  return *this;
}


DesktopApplication::DesktopApplication() {
  processId_ = 0;
  processPathNameUTF8_= NULL;
  applicationNameUTF8_= NULL;
  processUniqueIdUTF8_= NULL;
  windowCount_ = 0;
}

DesktopApplication::~DesktopApplication() {
}

void DesktopApplication::setProcessId(const ProcessId processId) {
  processId_ = processId;
}

void DesktopApplication::setProcessPathName(const char *appPathNameUTF8) {
  SetStringMember(&processPathNameUTF8_, appPathNameUTF8);
}

void DesktopApplication::setUniqueIdName(const char *appUniqueIdUTF8) {
  SetStringMember(&processUniqueIdUTF8_, appUniqueIdUTF8);
}

void DesktopApplication::setProcessAppName(const char *appNameUTF8) {
  SetStringMember(&applicationNameUTF8_, appNameUTF8);
}

void DesktopApplication::setWindowCount(const uint32_t count) {
  windowCount_ = count;
}

ProcessId DesktopApplication::getProcessId() {
  return processId_;
}

const char *DesktopApplication::getProcessPathName() {
  return processPathNameUTF8_;
}

const char *DesktopApplication::getUniqueIdName() {
  return processUniqueIdUTF8_;
}

const char *DesktopApplication::getProcessAppName() {
  return applicationNameUTF8_;
}

uint32_t DesktopApplication::getWindowCount() {
  return windowCount_;
}

DesktopApplication& DesktopApplication::operator= (DesktopApplication& other) {
  processId_ = other.getProcessId();
  setProcessPathName(other.getProcessPathName());
  setUniqueIdName(other.getUniqueIdName());
  setProcessAppName(other.getProcessAppName());

  return *this;
}

DesktopDeviceInfoImpl::DesktopDeviceInfoImpl() {
}

DesktopDeviceInfoImpl::~DesktopDeviceInfoImpl() {
  CleanUp();
}

int32_t DesktopDeviceInfoImpl::getDisplayDeviceCount() {
  return desktop_display_list_.size();
}

int32_t DesktopDeviceInfoImpl::getDesktopDisplayDeviceInfo(int32_t nIndex,
                                                           DesktopDisplayDevice & desktopDisplayDevice) {
  if(nIndex < 0 || nIndex >= desktop_display_list_.size()) {
    return -1;
  }

  std::map<intptr_t,DesktopDisplayDevice*>::iterator iter = desktop_display_list_.begin();
  std::advance (iter, nIndex);
  DesktopDisplayDevice * pDesktopDisplayDevice = iter->second;
  if(pDesktopDisplayDevice) {
    desktopDisplayDevice = (*pDesktopDisplayDevice);
  }

  return 0;
}

int32_t DesktopDeviceInfoImpl::getWindowCount() {
  return desktop_window_list_.size();
}
int32_t DesktopDeviceInfoImpl::getWindowInfo(int32_t nIndex,
                                             DesktopDisplayDevice &windowDevice) {
  if (nIndex < 0 || nIndex >= desktop_window_list_.size()) {
    return -1;
  }

  std::map<intptr_t, DesktopDisplayDevice *>::iterator itr = desktop_window_list_.begin();
  std::advance(itr, nIndex);
  DesktopDisplayDevice * pWindow = itr->second;
  if (!pWindow) {
    return -1;
  }

  windowDevice = (*pWindow);
  return 0;
}

int32_t DesktopDeviceInfoImpl::getApplicationCount() {
  return desktop_application_list_.size();
}

int32_t DesktopDeviceInfoImpl::getApplicationInfo(int32_t nIndex,
                                                  DesktopApplication & desktopApplication) {
  if(nIndex < 0 || nIndex >= desktop_application_list_.size()) {
    return -1;
  }

  std::map<intptr_t,DesktopApplication*>::iterator iter = desktop_application_list_.begin();
  std::advance (iter, nIndex);
  DesktopApplication * pDesktopApplication = iter->second;
  if (pDesktopApplication) {
    desktopApplication = (*pDesktopApplication);
  }

  return 0;
}

void DesktopDeviceInfoImpl::CleanUp() {
  CleanUpScreenList();
  CleanUpWindowList();
  CleanUpApplicationList();
}
int32_t DesktopDeviceInfoImpl::Init() {
  InitializeScreenList();
  InitializeWindowList();
  InitializeApplicationList();

  return 0;
}
int32_t DesktopDeviceInfoImpl::Refresh() {
  RefreshScreenList();
  RefreshWindowList();
  RefreshApplicationList();

  return 0;
}

void DesktopDeviceInfoImpl::CleanUpWindowList() {
  std::map<intptr_t, DesktopDisplayDevice *>::iterator iterWindow;
  for (iterWindow = desktop_window_list_.begin(); iterWindow != desktop_window_list_.end(); iterWindow++) {
    DesktopDisplayDevice *pWindow = iterWindow->second;
    delete pWindow;
    iterWindow->second = NULL;
  }
  desktop_window_list_.clear();
}
void DesktopDeviceInfoImpl::InitializeWindowList() {
  scoped_ptr<WindowCapturer> pWinCap(WindowCapturer::Create());
  WindowCapturer::WindowList list;
  if (pWinCap && pWinCap->GetWindowList(&list)) {
    WindowCapturer::WindowList::iterator itr;
    for (itr = list.begin(); itr != list.end(); itr++) {
      DesktopDisplayDevice *pWinDevice = new DesktopDisplayDevice;
      if (!pWinDevice) {
        continue;
      }

      pWinDevice->setScreenId(itr->id);
      pWinDevice->setDeviceName(itr->title.c_str());

      char idStr[BUFSIZ];
#if XP_WIN
      _snprintf_s(idStr, sizeof(idStr), sizeof(idStr) - 1, "%ld", pWinDevice->getScreenId());
#else
      snprintf(idStr, sizeof(idStr), "%ld", pWinDevice->getScreenId());
#endif
      pWinDevice->setUniqueIdName(idStr);
      desktop_window_list_[pWinDevice->getScreenId()] = pWinDevice;
    }
  }
}
void DesktopDeviceInfoImpl::RefreshWindowList() {
  CleanUpWindowList();
  InitializeWindowList();
}

void DesktopDeviceInfoImpl::CleanUpApplicationList() {
  std::map<intptr_t,DesktopApplication*>::iterator iterApp;
  for (iterApp = desktop_application_list_.begin(); iterApp != desktop_application_list_.end(); iterApp++){
    DesktopApplication *pDesktopApplication = iterApp->second;
    delete pDesktopApplication;
    iterApp->second = NULL;
  }
  desktop_application_list_.clear();
}
void DesktopDeviceInfoImpl::RefreshApplicationList() {
  CleanUpApplicationList();
  InitializeApplicationList();
}

void DesktopDeviceInfoImpl::CleanUpScreenList() {
  std::map<intptr_t,DesktopDisplayDevice*>::iterator iterDevice;
  for (iterDevice=desktop_display_list_.begin(); iterDevice != desktop_display_list_.end(); iterDevice++){
    DesktopDisplayDevice *pDesktopDisplayDevice = iterDevice->second;
    delete pDesktopDisplayDevice;
    iterDevice->second = NULL;
  }
  desktop_display_list_.clear();
 }
void DesktopDeviceInfoImpl::RefreshScreenList() {
  CleanUpScreenList();
  InitializeScreenList();
}
}
