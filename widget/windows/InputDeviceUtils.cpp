/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sts=2 sw=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputDeviceUtils.h"

#define INITGUID
#include <dbt.h>
#include <hidclass.h>
#include <ntddmou.h>
#include <setupapi.h>

namespace mozilla {
namespace widget {

HDEVNOTIFY
InputDeviceUtils::RegisterNotification(HWND aHwnd) {
  DEV_BROADCAST_DEVICEINTERFACE filter = {};

  filter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
  filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
  // Some touchsreen devices are not GUID_DEVINTERFACE_MOUSE, so here we use
  // GUID_DEVINTERFACE_HID instead.
  filter.dbcc_classguid = GUID_DEVINTERFACE_HID;
  return RegisterDeviceNotification(aHwnd, &filter,
                                    DEVICE_NOTIFY_WINDOW_HANDLE);
}

void InputDeviceUtils::UnregisterNotification(HDEVNOTIFY aHandle) {
  if (!aHandle) {
    return;
  }
  UnregisterDeviceNotification(aHandle);
}

DWORD
InputDeviceUtils::CountMouseDevices() {
  HDEVINFO hdev =
      SetupDiGetClassDevs(&GUID_DEVINTERFACE_MOUSE, nullptr, nullptr,
                          DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
  if (hdev == INVALID_HANDLE_VALUE) {
    return 0;
  }

  DWORD count = 0;
  SP_INTERFACE_DEVICE_DATA info = {};
  info.cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);
  while (SetupDiEnumDeviceInterfaces(hdev, nullptr, &GUID_DEVINTERFACE_MOUSE,
                                     count, &info)) {
    if (info.Flags & SPINT_ACTIVE) {
      count++;
    }
  }
  SetupDiDestroyDeviceInfoList(hdev);
  return count;
}

}  // namespace widget
}  // namespace mozilla
