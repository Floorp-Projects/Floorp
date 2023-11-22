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
#ifdef MOZ_ENABLE_DBUS
#  include <gio/gio.h>
#  include "AsyncDBus.h"
#  include "WakeLockListener.h"
#  include "nsIObserverService.h"
#  include "mozilla/UniquePtrExtensions.h"
#endif

using mozilla::LogLevel;
static mozilla::LazyLogModule sIdleLog("nsIUserIdleService");

using namespace mozilla;
using namespace mozilla::widget;

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

  bool ProbeImplementation() override {
    MOZ_LOG(sIdleLog, LogLevel::Info,
            ("UserIdleServiceX11::UserIdleServiceX11()\n"));

    if (!mozilla::widget::GdkIsX11Display()) {
      return false;
    }

    // This will leak - See comments in ~UserIdleServiceX11().
    PRLibrary* xsslib = PR_LoadLibrary("libXss.so.1");
    if (!xsslib)  // ouch.
    {
      MOZ_LOG(sIdleLog, LogLevel::Warning, ("Failed to find libXss.so!\n"));
      return false;
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
    if (!mXSSQueryExtension || !mXSSAllocInfo || !mXSSQueryInfo) {
      // We're missing X11 symbols
      return false;
    }

    // UserIdleServiceX11 uses sync init, confirm it now.
    mUserIdleServiceGTK->AcceptServiceCallback();
    return true;
  }

  explicit UserIdleServiceX11(nsUserIdleServiceGTK* aUserIdleService)
      : UserIdleServiceImpl(aUserIdleService){};

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

#ifdef MOZ_ENABLE_DBUS
class UserIdleServiceMutter : public UserIdleServiceImpl {
 public:
  bool PollIdleTime(uint32_t* aIdleTime) override {
    MOZ_LOG(sIdleLog, LogLevel::Info, ("PollIdleTime() request\n"));

    // We're not ready yet
    if (!mProxy) {
      return false;
    }

    if (!mPollInProgress) {
      mPollInProgress = true;
      DBusProxyCall(mProxy, "GetIdletime", nullptr, G_DBUS_CALL_FLAGS_NONE, -1,
                    mCancellable)
          ->Then(
              GetCurrentSerialEventTarget(), __func__,
              // It's safe to capture this as we use mCancellable to stop
              // listening.
              [this](RefPtr<GVariant>&& aResult) {
                if (!g_variant_is_of_type(aResult, G_VARIANT_TYPE_TUPLE) ||
                    g_variant_n_children(aResult) != 1) {
                  MOZ_LOG(sIdleLog, LogLevel::Info,
                          ("PollIdleTime() Unexpected params type: %s\n",
                           g_variant_get_type_string(aResult)));
                  mLastIdleTime = 0;
                  return;
                }
                RefPtr<GVariant> iTime =
                    dont_AddRef(g_variant_get_child_value(aResult, 0));
                if (!g_variant_is_of_type(iTime, G_VARIANT_TYPE_UINT64)) {
                  MOZ_LOG(sIdleLog, LogLevel::Info,
                          ("PollIdleTime() Unexpected params type: %s\n",
                           g_variant_get_type_string(aResult)));
                  mLastIdleTime = 0;
                  return;
                }
                uint64_t idleTime = g_variant_get_uint64(iTime);
                if (idleTime > std::numeric_limits<uint32_t>::max()) {
                  idleTime = std::numeric_limits<uint32_t>::max();
                }
                mLastIdleTime = idleTime;
                mPollInProgress = false;
                MOZ_LOG(sIdleLog, LogLevel::Info,
                        ("Async handler got %d\n", mLastIdleTime));
              },
              [this](GUniquePtr<GError>&& aError) {
                mPollInProgress = false;
                if (!IsCancelledGError(aError.get())) {
                  MOZ_LOG(
                      sIdleLog, LogLevel::Warning,
                      ("Failed to call GetIdletime(): %s\n", aError->message));
                  mUserIdleServiceGTK->RejectAndTryNextServiceCallback();
                }
              });
    }

    *aIdleTime = mLastIdleTime;
    MOZ_LOG(sIdleLog, LogLevel::Info,
            ("PollIdleTime() returns %d\n", *aIdleTime));
    return true;
  }

  bool ProbeImplementation() override {
    MOZ_LOG(sIdleLog, LogLevel::Info,
            ("UserIdleServiceMutter::UserIdleServiceMutter()\n"));

    mCancellable = dont_AddRef(g_cancellable_new());
    CreateDBusProxyForBus(
        G_BUS_TYPE_SESSION,
        GDBusProxyFlags(G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS |
                        G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES),
        nullptr, "org.gnome.Mutter.IdleMonitor",
        "/org/gnome/Mutter/IdleMonitor/Core", "org.gnome.Mutter.IdleMonitor",
        mCancellable)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [this](RefPtr<GDBusProxy>&& aProxy) {
              mProxy = std::move(aProxy);
              mUserIdleServiceGTK->AcceptServiceCallback();
            },
            [this](GUniquePtr<GError>&& aError) {
              if (!IsCancelledGError(aError.get())) {
                mUserIdleServiceGTK->RejectAndTryNextServiceCallback();
              }
            });
    return true;
  }

  explicit UserIdleServiceMutter(nsUserIdleServiceGTK* aUserIdleService)
      : UserIdleServiceImpl(aUserIdleService){};

  ~UserIdleServiceMutter() {
    if (mCancellable) {
      g_cancellable_cancel(mCancellable);
      mCancellable = nullptr;
    }
    mProxy = nullptr;
  }

 private:
  RefPtr<GDBusProxy> mProxy;
  RefPtr<GCancellable> mCancellable;
  uint32_t mLastIdleTime = 0;
  bool mPollInProgress = false;
};
#endif

void nsUserIdleServiceGTK::ProbeService() {
  MOZ_LOG(sIdleLog, LogLevel::Info,
          ("nsUserIdleServiceGTK::ProbeService() mIdleServiceType %d\n",
           mIdleServiceType));
  MOZ_ASSERT(!mIdleService);

  switch (mIdleServiceType) {
#ifdef MOZ_ENABLE_DBUS
    case IDLE_SERVICE_MUTTER:
      mIdleService = MakeUnique<UserIdleServiceMutter>(this);
      break;
#endif
#ifdef MOZ_X11
    case IDLE_SERVICE_XSCREENSAVER:
      mIdleService = MakeUnique<UserIdleServiceX11>(this);
      break;
#endif
    default:
      return;
  }

  if (!mIdleService->ProbeImplementation()) {
    RejectAndTryNextServiceCallback();
  }
}

void nsUserIdleServiceGTK::AcceptServiceCallback() {
  MOZ_LOG(sIdleLog, LogLevel::Info,
          ("nsUserIdleServiceGTK::AcceptServiceCallback() type %d\n",
           mIdleServiceType));
  mIdleServiceInitialized = true;
}

void nsUserIdleServiceGTK::RejectAndTryNextServiceCallback() {
  MOZ_LOG(sIdleLog, LogLevel::Info,
          ("nsUserIdleServiceGTK::RejectAndTryNextServiceCallback() type %d\n",
           mIdleServiceType));

  // Delete recent non-working service
  MOZ_ASSERT(mIdleService, "Nothing to reject?");
  mIdleService = nullptr;
  mIdleServiceInitialized = false;

  mIdleServiceType++;
  if (mIdleServiceType < IDLE_SERVICE_NONE) {
    MOZ_LOG(sIdleLog, LogLevel::Info,
            ("nsUserIdleServiceGTK try next idle service\n"));
    ProbeService();
  } else {
    MOZ_LOG(sIdleLog, LogLevel::Info, ("nsUserIdleServiceGTK failed\n"));
  }
}

bool nsUserIdleServiceGTK::PollIdleTime(uint32_t* aIdleTime) {
  if (!mIdleServiceInitialized) {
    return false;
  }
  return mIdleService->PollIdleTime(aIdleTime);
}
