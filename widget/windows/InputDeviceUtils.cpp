/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=2 sts=2 sw=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputDeviceUtils.h"

#define INITGUID
#include <dbt.h>
#include <hidclass.h>
#include <ntddmou.h>

namespace mozilla {
namespace widget {

HDEVNOTIFY
InputDeviceUtils::RegisterNotification(HWND aHwnd)
{
  DEV_BROADCAST_DEVICEINTERFACE filter = {};

  filter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
  filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
  // We only need notifications for mouse type devices.
  filter.dbcc_classguid = GUID_DEVINTERFACE_MOUSE;
  return RegisterDeviceNotification(aHwnd,
                                    &filter,
                                    DEVICE_NOTIFY_WINDOW_HANDLE);
}

void
InputDeviceUtils::UnregisterNotification(HDEVNOTIFY aHandle)
{
  if (!aHandle) {
    return;
  }
  UnregisterDeviceNotification(aHandle);
}

} // namespace widget
} // namespace mozilla
