/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WidgetUtilsGtk_h__
#define WidgetUtilsGtk_h__

#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/MozPromise.h"

#include <stdint.h>

typedef struct _GdkDisplay GdkDisplay;
typedef struct _GdkDevice GdkDevice;
typedef struct _GError GError;
typedef union _GdkEvent GdkEvent;
class nsWindow;

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

bool IsXWaylandProtocol();

GdkDevice* GdkGetPointer();

// Sets / returns the last mouse press event we processed.
void SetLastMousePressEvent(GdkEvent*);
GdkEvent* GetLastMousePressEvent();

// Return the snap's instance name, or null when not running as a snap.
const char* GetSnapInstanceName();
bool IsRunningUnderSnap();
bool IsRunningUnderFlatpak();
bool IsPackagedAppFileExists();
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

// Tries to get a descriptive identifier for the desktop environment that the
// program is running under. Always normalized to lowercase.
// See the implementation for the different environment variables and desktop
// information we try to use.
//
// If we can't find a reasonable environment, the empty string is returned.
const nsCString& GetDesktopEnvironmentIdentifier();
bool IsGnomeDesktopEnvironment();
bool IsKdeDesktopEnvironment();

// Parse text/uri-list
nsTArray<nsCString> ParseTextURIList(const nsACString& data);

using FocusRequestPromise = MozPromise<nsCString, bool, false>;
RefPtr<FocusRequestPromise> RequestWaylandFocusPromise();

bool IsCancelledGError(GError* aGError);

}  // namespace mozilla::widget

#endif  // WidgetUtilsGtk_h__
