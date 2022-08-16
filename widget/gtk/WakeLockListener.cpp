/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_ENABLE_DBUS

#  include "WakeLockListener.h"

#  include <dbus/dbus.h>
#  include <dbus/dbus-glib-lowlevel.h>

#  include "WidgetUtilsGtk.h"

#  if defined(MOZ_X11)
#    include "prlink.h"
#    include <gdk/gdk.h>
#    include <gdk/gdkx.h>
#    include "X11UndefineNone.h"
#  endif

#  if defined(MOZ_WAYLAND)
#    include "mozilla/widget/nsWaylandDisplay.h"
#    include "nsWindow.h"
#    include "mozilla/dom/power/PowerManagerService.h"
#  endif

#  define FREEDESKTOP_SCREENSAVER_TARGET "org.freedesktop.ScreenSaver"
#  define FREEDESKTOP_SCREENSAVER_OBJECT "/ScreenSaver"
#  define FREEDESKTOP_SCREENSAVER_INTERFACE "org.freedesktop.ScreenSaver"

#  define FREEDESKTOP_POWER_TARGET "org.freedesktop.PowerManagement"
#  define FREEDESKTOP_POWER_OBJECT "/org/freedesktop/PowerManagement/Inhibit"
#  define FREEDESKTOP_POWER_INTERFACE "org.freedesktop.PowerManagement.Inhibit"

#  define SESSION_MANAGER_TARGET "org.gnome.SessionManager"
#  define SESSION_MANAGER_OBJECT "/org/gnome/SessionManager"
#  define SESSION_MANAGER_INTERFACE "org.gnome.SessionManager"

#  define DBUS_TIMEOUT (-1)

using namespace mozilla;
using namespace mozilla::widget;

NS_IMPL_ISUPPORTS(WakeLockListener, nsIDOMMozWakeLockListener)

StaticRefPtr<WakeLockListener> WakeLockListener::sSingleton;

#  define WAKE_LOCK_LOG(...) \
    MOZ_LOG(gLinuxWakeLockLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
static mozilla::LazyLogModule gLinuxWakeLockLog("LinuxWakeLock");

enum WakeLockDesktopEnvironment {
  FreeDesktopScreensaver,
  FreeDesktopPower,
  GNOME,
#  if defined(MOZ_X11)
  XScreenSaver,
#  endif
#  if defined(MOZ_WAYLAND)
  WaylandIdleInhibit,
#  endif
  Unsupported,
};

class WakeLockTopic {
 public:
  WakeLockTopic(const nsAString& aTopic, DBusConnection* aConnection)
      :
#  if defined(MOZ_WAYLAND)
        mWaylandInhibitor(nullptr),
#  endif
        mTopic(NS_ConvertUTF16toUTF8(aTopic)),
        mConnection(aConnection),
        mDesktopEnvironment(FreeDesktopScreensaver),
        mInhibitRequest(0),
        mShouldInhibit(false),
        mWaitingForReply(false) {
  }

  nsresult InhibitScreensaver(void);
  nsresult UninhibitScreensaver(void);

 private:
  bool SendInhibit();
  bool SendUninhibit();

  bool SendFreeDesktopPowerInhibitMessage();
  bool SendFreeDesktopScreensaverInhibitMessage();
  bool SendGNOMEInhibitMessage();
  bool SendMessage(DBusMessage* aMessage);

#  if defined(MOZ_X11)
  static bool CheckXScreenSaverSupport();
  static bool InhibitXScreenSaver(bool inhibit);
#  endif

#  if defined(MOZ_WAYLAND)
  zwp_idle_inhibitor_v1* mWaylandInhibitor;
  static bool CheckWaylandIdleInhibitSupport();
  bool InhibitWaylandIdle();
  bool UninhibitWaylandIdle();
#  endif

  static void ReceiveInhibitReply(DBusPendingCall* aPending, void* aUserData);
  void InhibitFailed();
  void InhibitSucceeded(uint32_t aInhibitRequest);

  nsCString mTopic;
  RefPtr<DBusConnection> mConnection;

  WakeLockDesktopEnvironment mDesktopEnvironment;

  uint32_t mInhibitRequest;

  bool mShouldInhibit;
  bool mWaitingForReply;
};

bool WakeLockTopic::SendMessage(DBusMessage* aMessage) {
  // send message and get a handle for a reply
  RefPtr<DBusPendingCall> reply;
  dbus_connection_send_with_reply(mConnection, aMessage,
                                  reply.StartAssignment(), DBUS_TIMEOUT);
  if (!reply) {
    return false;
  }

  dbus_pending_call_set_notify(reply, &ReceiveInhibitReply, this, NULL);

  return true;
}

bool WakeLockTopic::SendFreeDesktopPowerInhibitMessage() {
  RefPtr<DBusMessage> message =
      already_AddRefed<DBusMessage>(dbus_message_new_method_call(
          FREEDESKTOP_POWER_TARGET, FREEDESKTOP_POWER_OBJECT,
          FREEDESKTOP_POWER_INTERFACE, "Inhibit"));

  if (!message) {
    return false;
  }

  const char* app = g_get_prgname();
  const char* topic = mTopic.get();
  dbus_message_append_args(message, DBUS_TYPE_STRING, &app, DBUS_TYPE_STRING,
                           &topic, DBUS_TYPE_INVALID);

  return SendMessage(message);
}

bool WakeLockTopic::SendFreeDesktopScreensaverInhibitMessage() {
  RefPtr<DBusMessage> message =
      already_AddRefed<DBusMessage>(dbus_message_new_method_call(
          FREEDESKTOP_SCREENSAVER_TARGET, FREEDESKTOP_SCREENSAVER_OBJECT,
          FREEDESKTOP_SCREENSAVER_INTERFACE, "Inhibit"));

  if (!message) {
    return false;
  }

  const char* app = g_get_prgname();
  const char* topic = mTopic.get();
  dbus_message_append_args(message, DBUS_TYPE_STRING, &app, DBUS_TYPE_STRING,
                           &topic, DBUS_TYPE_INVALID);

  return SendMessage(message);
}

bool WakeLockTopic::SendGNOMEInhibitMessage() {
  RefPtr<DBusMessage> message =
      already_AddRefed<DBusMessage>(dbus_message_new_method_call(
          SESSION_MANAGER_TARGET, SESSION_MANAGER_OBJECT,
          SESSION_MANAGER_INTERFACE, "Inhibit"));

  if (!message) {
    return false;
  }

  static const uint32_t xid = 0;
  static const uint32_t flags = (1 << 3);  // Inhibit idle
  const char* app = g_get_prgname();
  const char* topic = mTopic.get();
  dbus_message_append_args(message, DBUS_TYPE_STRING, &app, DBUS_TYPE_UINT32,
                           &xid, DBUS_TYPE_STRING, &topic, DBUS_TYPE_UINT32,
                           &flags, DBUS_TYPE_INVALID);

  return SendMessage(message);
}

#  if defined(MOZ_X11)

typedef Bool (*_XScreenSaverQueryExtension_fn)(Display* dpy, int* event_base,
                                               int* error_base);
typedef Bool (*_XScreenSaverQueryVersion_fn)(Display* dpy, int* major,
                                             int* minor);
typedef void (*_XScreenSaverSuspend_fn)(Display* dpy, Bool suspend);

static PRLibrary* sXssLib = nullptr;
static _XScreenSaverQueryExtension_fn _XSSQueryExtension = nullptr;
static _XScreenSaverQueryVersion_fn _XSSQueryVersion = nullptr;
static _XScreenSaverSuspend_fn _XSSSuspend = nullptr;

/* static */
bool WakeLockTopic::CheckXScreenSaverSupport() {
  if (!sXssLib) {
    sXssLib = PR_LoadLibrary("libXss.so.1");
    if (!sXssLib) {
      return false;
    }
  }

  _XSSQueryExtension = (_XScreenSaverQueryExtension_fn)PR_FindFunctionSymbol(
      sXssLib, "XScreenSaverQueryExtension");
  _XSSQueryVersion = (_XScreenSaverQueryVersion_fn)PR_FindFunctionSymbol(
      sXssLib, "XScreenSaverQueryVersion");
  _XSSSuspend = (_XScreenSaverSuspend_fn)PR_FindFunctionSymbol(
      sXssLib, "XScreenSaverSuspend");
  if (!_XSSQueryExtension || !_XSSQueryVersion || !_XSSSuspend) {
    return false;
  }

  GdkDisplay* gDisplay = gdk_display_get_default();
  if (!GdkIsX11Display(gDisplay)) {
    return false;
  }
  Display* display = GDK_DISPLAY_XDISPLAY(gDisplay);

  int throwaway;
  if (!_XSSQueryExtension(display, &throwaway, &throwaway)) return false;

  int major, minor;
  if (!_XSSQueryVersion(display, &major, &minor)) return false;
  // Needs to be compatible with version 1.1
  if (major != 1) return false;
  if (minor < 1) return false;

  return true;
}

/* static */
bool WakeLockTopic::InhibitXScreenSaver(bool inhibit) {
  // Should only be called if CheckXScreenSaverSupport returns true.
  // There's a couple of safety checks here nonetheless.
  if (!_XSSSuspend) {
    return false;
  }
  GdkDisplay* gDisplay = gdk_display_get_default();
  if (!GdkIsX11Display(gDisplay)) {
    return false;
  }
  Display* display = GDK_DISPLAY_XDISPLAY(gDisplay);
  _XSSSuspend(display, inhibit);
  return true;
}

#  endif

#  if defined(MOZ_WAYLAND)

/* static */
bool WakeLockTopic::CheckWaylandIdleInhibitSupport() {
  RefPtr<nsWaylandDisplay> waylandDisplay = WaylandDisplayGet();
  return waylandDisplay && waylandDisplay->GetIdleInhibitManager() != nullptr;
}

bool WakeLockTopic::InhibitWaylandIdle() {
  RefPtr<nsWaylandDisplay> waylandDisplay = WaylandDisplayGet();
  if (!waylandDisplay) {
    return false;
  }

  nsWindow* focusedWindow = nsWindow::GetFocusedWindow();
  if (!focusedWindow) {
    return false;
  }

  UninhibitWaylandIdle();

  MozContainerSurfaceLock lock(focusedWindow->GetMozContainer());
  struct wl_surface* waylandSurface = lock.GetSurface();
  if (waylandSurface) {
    mWaylandInhibitor = zwp_idle_inhibit_manager_v1_create_inhibitor(
        waylandDisplay->GetIdleInhibitManager(), waylandSurface);
  }
  return true;
}

bool WakeLockTopic::UninhibitWaylandIdle() {
  if (mWaylandInhibitor == nullptr) return false;

  zwp_idle_inhibitor_v1_destroy(mWaylandInhibitor);
  mWaylandInhibitor = nullptr;

  return true;
}

#  endif

bool WakeLockTopic::SendInhibit() {
  bool sendOk = false;

  switch (mDesktopEnvironment) {
    case FreeDesktopScreensaver:
      sendOk = SendFreeDesktopScreensaverInhibitMessage();
      break;
    case FreeDesktopPower:
      sendOk = SendFreeDesktopPowerInhibitMessage();
      break;
    case GNOME:
      sendOk = SendGNOMEInhibitMessage();
      break;
#  if defined(MOZ_X11)
    case XScreenSaver:
      return InhibitXScreenSaver(true);
#  endif
#  if defined(MOZ_WAYLAND)
    case WaylandIdleInhibit:
      return InhibitWaylandIdle();
#  endif
    case Unsupported:
      return false;
  }

  if (sendOk) {
    mWaitingForReply = true;
  }

  return sendOk;
}

bool WakeLockTopic::SendUninhibit() {
  RefPtr<DBusMessage> message;

  if (mDesktopEnvironment == FreeDesktopScreensaver) {
    message = already_AddRefed<DBusMessage>(dbus_message_new_method_call(
        FREEDESKTOP_SCREENSAVER_TARGET, FREEDESKTOP_SCREENSAVER_OBJECT,
        FREEDESKTOP_SCREENSAVER_INTERFACE, "UnInhibit"));
  } else if (mDesktopEnvironment == FreeDesktopPower) {
    message = already_AddRefed<DBusMessage>(dbus_message_new_method_call(
        FREEDESKTOP_POWER_TARGET, FREEDESKTOP_POWER_OBJECT,
        FREEDESKTOP_POWER_INTERFACE, "UnInhibit"));
  } else if (mDesktopEnvironment == GNOME) {
    message = already_AddRefed<DBusMessage>(dbus_message_new_method_call(
        SESSION_MANAGER_TARGET, SESSION_MANAGER_OBJECT,
        SESSION_MANAGER_INTERFACE, "Uninhibit"));
  }
#  if defined(MOZ_X11)
  else if (mDesktopEnvironment == XScreenSaver) {
    return InhibitXScreenSaver(false);
  }
#  endif
#  if defined(MOZ_WAYLAND)
  else if (mDesktopEnvironment == WaylandIdleInhibit) {
    return UninhibitWaylandIdle();
  }
#  endif

  if (!message) {
    return false;
  }

  dbus_message_append_args(message, DBUS_TYPE_UINT32, &mInhibitRequest,
                           DBUS_TYPE_INVALID);

  dbus_connection_send(mConnection, message, nullptr);
  dbus_connection_flush(mConnection);

  mInhibitRequest = 0;

  return true;
}

nsresult WakeLockTopic::InhibitScreensaver() {
  if (mShouldInhibit) {
    // Screensaver is inhibited. Nothing to do here.
    return NS_OK;
  }

  mShouldInhibit = true;

  if (mWaitingForReply) {
    // We already have a screensaver inhibit request pending. This can happen
    // if InhibitScreensaver is called, then UninhibitScreensaver, then
    // InhibitScreensaver again quickly.
    return NS_OK;
  }

  return SendInhibit() ? NS_OK : NS_ERROR_FAILURE;
}

nsresult WakeLockTopic::UninhibitScreensaver() {
  if (!mShouldInhibit) {
    // Screensaver isn't inhibited. Nothing to do here.
    return NS_OK;
  }

  mShouldInhibit = false;

  if (mWaitingForReply) {
    // If we're still waiting for a response to our inhibit request, we can't
    // do anything until we get a dbus message back. The callbacks below will
    // check |mShouldInhibit| and act accordingly.
    return NS_OK;
  }

  return SendUninhibit() ? NS_OK : NS_ERROR_FAILURE;
}

void WakeLockTopic::InhibitFailed() {
  mWaitingForReply = false;

  if (mDesktopEnvironment == FreeDesktopScreensaver) {
    mDesktopEnvironment = GNOME;
  } else if (mDesktopEnvironment == GNOME) {
    mDesktopEnvironment = FreeDesktopPower;
#  if defined(MOZ_X11)
  } else if (mDesktopEnvironment == FreeDesktopPower &&
             CheckXScreenSaverSupport()) {
    mDesktopEnvironment = XScreenSaver;
#  endif
#  if defined(MOZ_WAYLAND)
  } else if (mDesktopEnvironment == FreeDesktopPower &&
             CheckWaylandIdleInhibitSupport()) {
    mDesktopEnvironment = WaylandIdleInhibit;
#  endif
  } else {
    mDesktopEnvironment = Unsupported;
    mShouldInhibit = false;
  }

  if (!mShouldInhibit) {
    // We were interrupted by UninhibitScreensaver() before we could find the
    // correct desktop environment.
    return;
  }

  SendInhibit();
}

void WakeLockTopic::InhibitSucceeded(uint32_t aInhibitRequest) {
  mWaitingForReply = false;
  mInhibitRequest = aInhibitRequest;

  if (!mShouldInhibit) {
    // We successfully inhibited the screensaver, but UninhibitScreensaver()
    // was called while we were waiting for a reply.
    SendUninhibit();
  }
}

/* static */
void WakeLockTopic::ReceiveInhibitReply(DBusPendingCall* pending,
                                        void* user_data) {
  if (!WakeLockListener::GetSingleton(false)) {
    // The WakeLockListener (and therefore our topic) was deleted while we were
    // waiting for a reply.
    return;
  }

  WakeLockTopic* self = static_cast<WakeLockTopic*>(user_data);

  RefPtr<DBusMessage> msg =
      already_AddRefed<DBusMessage>(dbus_pending_call_steal_reply(pending));
  if (!msg) {
    return;
  }

  if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_METHOD_RETURN) {
    uint32_t inhibitRequest;

    if (dbus_message_get_args(msg, nullptr, DBUS_TYPE_UINT32, &inhibitRequest,
                              DBUS_TYPE_INVALID)) {
      self->InhibitSucceeded(inhibitRequest);
    }
  } else {
    self->InhibitFailed();
  }
}

WakeLockListener::WakeLockListener() : mConnection(nullptr) {}

/* static */
WakeLockListener* WakeLockListener::GetSingleton(bool aCreate) {
  if (!sSingleton && aCreate) {
    sSingleton = new WakeLockListener();
  }

  return sSingleton;
}

/* static */
void WakeLockListener::Shutdown() { sSingleton = nullptr; }

bool WakeLockListener::EnsureDBusConnection() {
  if (!mConnection) {
    mConnection = already_AddRefed<DBusConnection>(
        dbus_bus_get(DBUS_BUS_SESSION, nullptr));

    if (mConnection) {
      dbus_connection_set_exit_on_disconnect(mConnection, false);
      dbus_connection_setup_with_g_main(mConnection, nullptr);
    }
  }

  return mConnection != nullptr;
}

nsresult WakeLockListener::Callback(const nsAString& topic,
                                    const nsAString& state) {
  if (!EnsureDBusConnection()) {
    return NS_ERROR_FAILURE;
  }

  if (!topic.Equals(u"screen"_ns) && !topic.Equals(u"audio-playing"_ns) &&
      !topic.Equals(u"video-playing"_ns))
    return NS_OK;

  WakeLockTopic* const topicLock =
      mTopics.GetOrInsertNew(topic, topic, mConnection);

  // Treat "locked-background" the same as "unlocked" on desktop linux.
  bool shouldLock = state.EqualsLiteral("locked-foreground");
  WAKE_LOCK_LOG("topic=%s, shouldLock=%d", NS_ConvertUTF16toUTF8(topic).get(),
                shouldLock);

  return shouldLock ? topicLock->InhibitScreensaver()
                    : topicLock->UninhibitScreensaver();
}

#endif
