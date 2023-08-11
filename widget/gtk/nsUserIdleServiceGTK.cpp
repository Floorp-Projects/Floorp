/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <gtk/gtk.h>

#include "nsUserIdleServiceGTK.h"
#include "nsDebug.h"
#include "prlink.h"
#include "mozilla/Logging.h"
#include "WidgetUtilsGtk.h"

#ifdef MOZ_X11
#  include <X11/Xlib.h>
#  include <X11/Xutil.h>
#  include <gdk/gdkx.h>
#endif

using mozilla::LogLevel;
static mozilla::LazyLogModule sIdleLog("nsIUserIdleService");

#ifdef MOZ_X11
typedef struct {
  Window window;               // Screen saver window
  int state;                   // ScreenSaver(Off,On,Disabled)
  int kind;                    // ScreenSaver(Blanked,Internal,External)
  unsigned long til_or_since;  // milliseconds since/til screensaver kicks in
  unsigned long idle;          // milliseconds idle
  unsigned long event_mask;    // event stuff
} XScreenSaverInfo;

typedef bool (*_XScreenSaverQueryExtension_fn)(Display* dpy, int* event_base,
                                               int* error_base);
typedef XScreenSaverInfo* (*_XScreenSaverAllocInfo_fn)(void);
typedef void (*_XScreenSaverQueryInfo_fn)(Display* dpy, Drawable drw,
                                          XScreenSaverInfo* info);

class UserIdleServiceX11 : public UserIdleServiceImpl {
 public:
  bool PollIdleTime(uint32_t* aIdleTime) override {
    MOZ_ASSERT(!mInitialized);

    // Ask xscreensaver about idle time:
    *aIdleTime = 0;

    // We might not have a display (cf. in xpcshell)
    Display* dplay = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    if (!dplay) {
      MOZ_LOG(sIdleLog, LogLevel::Warning, ("No display found!\n"));
      return false;
    }

    int event_base, error_base;
    if (mXSSQueryExtension(dplay, &event_base, &error_base)) {
      if (!mXssInfo) mXssInfo = mXSSAllocInfo();
      if (!mXssInfo) return false;
      mXSSQueryInfo(dplay, GDK_ROOT_WINDOW(), mXssInfo);
      *aIdleTime = mXssInfo->idle;
      MOZ_LOG(sIdleLog, LogLevel::Info,
              ("UserIdleServiceX11::PollIdleTime() %d\n", *aIdleTime));
      return true;
    }
    // If we get here, we couldn't get to XScreenSaver:
    MOZ_LOG(sIdleLog, LogLevel::Warning,
            ("XSSQueryExtension returned false!\n"));
    return false;
  }

  UserIdleServiceX11() {
    MOZ_ASSERT(mozilla::widget::GdkIsX11Display());

    // This will leak - See comments in ~nsUserIdleServiceGTK().
    PRLibrary* xsslib = PR_LoadLibrary("libXss.so.1");
    if (!xsslib)  // ouch.
    {
      MOZ_LOG(sIdleLog, LogLevel::Warning, ("Failed to find libXss.so!\n"));
      return;
    }

    mXSSQueryExtension = (_XScreenSaverQueryExtension_fn)PR_FindFunctionSymbol(
        xsslib, "XScreenSaverQueryExtension");
    mXSSAllocInfo = (_XScreenSaverAllocInfo_fn)PR_FindFunctionSymbol(
        xsslib, "XScreenSaverAllocInfo");
    mXSSQueryInfo = (_XScreenSaverQueryInfo_fn)PR_FindFunctionSymbol(
        xsslib, "XScreenSaverQueryInfo");

    if (!mXSSQueryExtension) {
      MOZ_LOG(sIdleLog, LogLevel::Warning,
              ("Failed to get XSSQueryExtension!\n"));
    }
    if (!mXSSAllocInfo) {
      MOZ_LOG(sIdleLog, LogLevel::Warning, ("Failed to get XSSAllocInfo!\n"));
    }
    if (!mXSSQueryInfo) {
      MOZ_LOG(sIdleLog, LogLevel::Warning, ("Failed to get XSSQueryInfo!\n"));
    }

    mInitialized = mXSSQueryExtension && mXSSAllocInfo && mXSSQueryInfo;
  }

  ~UserIdleServiceX11() {
#  ifdef MOZ_X11
    if (mXssInfo) {
      XFree(mXssInfo);
    }
#  endif

// It is not safe to unload libXScrnSaver until each display is closed because
// the library registers callbacks through XESetCloseDisplay (Bug 397607).
// (Also the library and its functions are scoped for the file not the object.)
#  if 0
        if (xsslib) {
            PR_UnloadLibrary(xsslib);
            xsslib = nullptr;
        }
#  endif
  }

 private:
  XScreenSaverInfo* mXssInfo = nullptr;
  _XScreenSaverQueryExtension_fn mXSSQueryExtension = nullptr;
  _XScreenSaverAllocInfo_fn mXSSAllocInfo = nullptr;
  _XScreenSaverQueryInfo_fn mXSSQueryInfo = nullptr;
};
#endif

nsUserIdleServiceGTK::nsUserIdleServiceGTK() {
  mIdleService = mozilla::MakeUnique<UserIdleServiceX11>();
  if (mIdleService->IsAvailable()) {
    return;
  }
  // TODO: Gnome/KDE services.
  mIdleService = nullptr;
}

bool nsUserIdleServiceGTK::PollIdleTime(uint32_t* aIdleTime) {
  if (!mIdleService) {
    return false;
  }
  return mIdleService->PollIdleTime(aIdleTime);
}
