/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WidgetUtilsGtk_h__
#define WidgetUtilsGtk_h__

#include "nsString.h"
#include "nsTArray.h"

#include <stdint.h>
#include <gdk/gdk.h>

namespace mozilla::widget {

class WidgetUtilsGTK {
 public:
  /* See WidgetUtils::IsTouchDeviceSupportPresent(). */
  static int32_t IsTouchDeviceSupportPresent();
};

bool IsMainWindowTransparent();

bool GdkIsWaylandDisplay(GdkDisplay* display);
bool GdkIsX11Display(GdkDisplay* display);

bool GdkIsWaylandDisplay();
bool GdkIsX11Display();

// Parse text/uri-list
nsTArray<nsCString> ParseTextURIList(const nsACString& data);

}  // namespace mozilla::widget

#endif  // WidgetUtilsGtk_h__
