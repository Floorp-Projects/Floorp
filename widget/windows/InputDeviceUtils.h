/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sts=2 sw=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_InputDeviceUtils_h__
#define mozilla_widget_InputDeviceUtils_h__

#include <windows.h>

namespace mozilla {
namespace widget {

class InputDeviceUtils {
public:
  static HDEVNOTIFY RegisterNotification(HWND aHwnd);
  static void UnregisterNotification(HDEVNOTIFY aHandle);
};

} // namespace widget
} // namespace mozilla
#endif // mozilla_widget_InputDeviceUtils_h__
