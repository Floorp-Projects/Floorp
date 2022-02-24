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

bool IsRunningUnderFlatpak();

// When packaged as a snap, strict confinement needs to be accounted for.
// See https://snapcraft.io/docs for details.
// Name as defined on e.g.
// https://snapcraft.io/firefox or https://snapcraft.io/thunderbird
#ifdef MOZ_APP_NAME
#  define SNAP_INSTANCE_NAME MOZ_APP_NAME
#endif

bool IsRunningUnderSnap();
inline bool IsRunningUnderFlatpakOrSnap() {
  return IsRunningUnderFlatpak() || IsRunningUnderSnap();
}

// Return the snap's instance name, or null when not running as a snap.
const char* GetSnapInstanceName();

enum class PortalKind {
  FilePicker,
  MimeHandler,
  Print,
  Settings,
};
bool ShouldUsePortal(PortalKind);

// Parse text/uri-list
nsTArray<nsCString> ParseTextURIList(const nsACString& data);

}  // namespace mozilla::widget

#endif  // WidgetUtilsGtk_h__
