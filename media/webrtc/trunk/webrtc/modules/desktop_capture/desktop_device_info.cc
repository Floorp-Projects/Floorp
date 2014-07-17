/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "webrtc/modules/desktop_capture/desktop_device_info.h"

#include <cstddef>
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
  screenId_ = other.getScreenId();
  setUniqueIdName(other.getUniqueIdName());
  setDeviceName(other.getDeviceName());

  return *this;
}


DesktopApplication::DesktopApplication() {
  processId_ =0;
  processPathNameUTF8_= NULL;
  applicationNameUTF8_= NULL;
  processUniqueIdUTF8_= NULL;
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
  SetStringMember(&processPathNameUTF8_, appUniqueIdUTF8);
}

void DesktopApplication::setProcessAppName(const char *appNameUTF8) {
  SetStringMember(&applicationNameUTF8_, appNameUTF8);
}

ProcessId DesktopApplication::getProcessId() {
  return processId_;
}

const char *DesktopApplication::getProcessPathName() {
  return processPathNameUTF8_;
}

const char *DesktopApplication::getUniqueIdName() {
  return applicationNameUTF8_;
}

const char *DesktopApplication::getProcessAppName() {
  return applicationNameUTF8_;
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
  std::map<intptr_t,DesktopDisplayDevice*>::iterator iterDevice;
  for (iterDevice=desktop_display_list_.begin(); iterDevice!=desktop_display_list_.end(); iterDevice++){
    DesktopDisplayDevice * pDesktopDisplayDevice = iterDevice->second;
    if (pDesktopDisplayDevice) {
      delete pDesktopDisplayDevice;
    }
    iterDevice->second = NULL;
  }
  desktop_display_list_.clear();

  std::map<intptr_t,DesktopApplication*>::iterator iterApp;
  for (iterApp=desktop_application_list_.begin(); iterApp!=desktop_application_list_.end(); iterApp++){
    DesktopApplication * pDesktopApplication = iterApp->second;
    if (pDesktopApplication) {
      delete pDesktopApplication;
    }
    iterApp->second = NULL;
  }
  desktop_application_list_.clear();
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

}
