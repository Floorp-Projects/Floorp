/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WidgetUtilsGtk.h"

#include <gtk/gtk.h>

namespace mozilla {

namespace widget {

int32_t WidgetUtilsGTK::IsTouchDeviceSupportPresent() {
#if GTK_CHECK_VERSION(3, 4, 0)
  int32_t result = 0;
  GdkDisplay* display = gdk_display_get_default();
  if (!display) {
    return 0;
  }

  GdkDeviceManager* manager = gdk_display_get_device_manager(display);
  if (!manager) {
    return 0;
  }

  GList* devices =
      gdk_device_manager_list_devices(manager, GDK_DEVICE_TYPE_SLAVE);
  GList* list = devices;

  while (devices) {
    GdkDevice* device = static_cast<GdkDevice*>(devices->data);
    if (gdk_device_get_source(device) == GDK_SOURCE_TOUCHSCREEN) {
      result = 1;
      break;
    }
    devices = devices->next;
  }

  if (list) {
    g_list_free(list);
  }

  return result;
#else
  return 0;
#endif
}

}  // namespace widget

}  // namespace mozilla
