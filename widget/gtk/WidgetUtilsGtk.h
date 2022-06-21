/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WidgetUtilsGtk_h__
#define WidgetUtilsGtk_h__

#include "nsString.h"
#include "nsTArray.h"

#include <stdint.h>

typedef struct _GdkDisplay GdkDisplay;
typedef struct _GdkDevice GdkDevice;
typedef union _GdkEvent GdkEvent;

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

GdkDevice* GdkGetPointer();

// Sets / returns the last mouse press event we processed.
void SetLastMousePressEvent(GdkEvent*);
GdkEvent* GetLastMousePressEvent();

// Return the snap's instance name, or null when not running as a snap.
const char* GetSnapInstanceName();
inline bool IsRunningUnderSnap() { return !!GetSnapInstanceName(); }
bool IsRunningUnderFlatpak();
inline bool IsRunningUnderFlatpakOrSnap() {
  return IsRunningUnderFlatpak() || IsRunningUnderSnap();
}

enum class PortalKind {
  FilePicker,
  MimeHandler,
  Settings,
  Location,
  OpenUri,
};
bool ShouldUsePortal(PortalKind);

// Parse text/uri-list
nsTArray<nsCString> ParseTextURIList(const nsACString& data);

}  // namespace mozilla::widget

#endif  // WidgetUtilsGtk_h__
