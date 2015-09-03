/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "webrtc/modules/desktop_capture/x11/desktop_device_info_x11.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/scoped_refptr.h"
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>
#include "webrtc/modules/desktop_capture/x11/shared_x_util.h"
#include "webrtc/modules/desktop_capture/window_capturer.h"
#include "webrtc/modules/desktop_capture/x11/x_error_trap.h"
#include "webrtc/modules/desktop_capture/x11/x_server_pixel_buffer.h"

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

void DesktopDeviceInfoX11::InitializeApplicationList() {
  //List all running applications exclude background process.
  scoped_refptr<SharedXDisplay> SharedDisplay = SharedXDisplay::CreateDefault();
  XErrorTrap error_trap(SharedDisplay->display());

  WindowUtilX11 window_util_x11(SharedDisplay);
  int num_screens = XScreenCount(SharedDisplay->display());
  for (int screen = 0; screen < num_screens; ++screen) {
    ::Window root_window = XRootWindow(SharedDisplay->display(), screen);
    ::Window parent;
    ::Window *children;
    unsigned int num_children;
    int status = XQueryTree(SharedDisplay->display(), root_window, &root_window, &parent,
        &children, &num_children);
    if (status == 0) {
      LOG(LS_ERROR) << "Failed to query for child windows for screen " << screen;
      continue;
    }

    for (unsigned int i = 0; i < num_children; ++i) {
      ::Window app_window = window_util_x11.GetApplicationWindow(children[num_children - 1 - i]);

      if (!app_window
          || window_util_x11.IsDesktopElement(app_window)
          || window_util_x11.GetWindowStatus(app_window) == WithdrawnState) {
        continue;
      }

      unsigned int processId = window_util_x11.GetWindowProcessID(app_window);
      // filter out non-process
      if (processId == 0) {
        continue;
      }
      // filter out current process
      if (processId == getpid()) {
        continue;
      }

      // filter out existing applications, after incrementing its window count
      DesktopApplicationList::iterator itr = desktop_application_list_.find(processId);
      if (itr != desktop_application_list_.end()) {
        itr->second->setWindowCount(itr->second->getWindowCount() + 1);
        continue;
      }

      // Add one application
      DesktopApplication *pDesktopApplication = new DesktopApplication;
      if (!pDesktopApplication) {
        continue;
      }

      // process id
      pDesktopApplication->setProcessId(processId);
      // initialize count to 1
      pDesktopApplication->setWindowCount(1);

      // process path name
      pDesktopApplication->setProcessPathName("");

      // application name
      std::string strAppName;
      window_util_x11.GetWindowTitle(app_window, &strAppName);
      pDesktopApplication->setProcessAppName(strAppName.c_str());

      // unique id name
      char idStr[64];
      snprintf(idStr, sizeof(idStr), "%ld", pDesktopApplication->getProcessId());
      pDesktopApplication->setUniqueIdName(idStr);
      desktop_application_list_[processId] = pDesktopApplication;
    }

    // re-walk the application list, prepending the window count to the application name
    DesktopApplicationList::iterator itr;
    for (itr = desktop_application_list_.begin(); itr != desktop_application_list_.end(); itr++) {
      DesktopApplication *pApp = itr->second;
      // localized name can be *VERY* large
      char nameStr[BUFSIZ];
      snprintf(nameStr, sizeof(nameStr), "%d\x1e%s",
               pApp->getWindowCount(), pApp->getProcessAppName());
      pApp->setProcessAppName(nameStr);
    }

    if (children) {
      XFree(children);
    }
  }
}

} //namespace webrtc
