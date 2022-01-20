/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <gdk/gdk.h>
#include "nsAppShell.h"
#include "nsWindow.h"
#include "mozilla/Logging.h"
#include "prenv.h"
#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/Hal.h"
#include "mozilla/Unused.h"
#include "mozilla/WidgetUtils.h"
#include "GeckoProfiler.h"
#include "nsIPowerManagerService.h"
#ifdef MOZ_ENABLE_DBUS
#  include <dbus/dbus-glib-lowlevel.h>
#  include <gio/gio.h>
#  include "WakeLockListener.h"
#  include "nsIObserverService.h"
#endif
#include "gfxPlatform.h"
#include "ScreenHelperGTK.h"
#include "HeadlessScreenHelper.h"
#include "mozilla/widget/ScreenManager.h"
#ifdef MOZ_WAYLAND
#  include "nsWaylandDisplay.h"
#endif

using mozilla::LazyLogModule;
using mozilla::Unused;
using mozilla::widget::HeadlessScreenHelper;
using mozilla::widget::ScreenHelperGTK;
using mozilla::widget::ScreenManager;

#define NOTIFY_TOKEN 0xFA

LazyLogModule gWidgetLog("Widget");
LazyLogModule gWidgetDragLog("WidgetDrag");
LazyLogModule gWidgetWaylandLog("WidgetWayland");
LazyLogModule gWidgetPopupLog("WidgetPopup");
LazyLogModule gDmabufLog("Dmabuf");
LazyLogModule gClipboardLog("WidgetClipboard");

static GPollFunc sPollFunc;

// Wrapper function to disable hang monitoring while waiting in poll().
static gint PollWrapper(GPollFD* ufds, guint nfsd, gint timeout_) {
  mozilla::BackgroundHangMonitor().NotifyWait();
  gint result;
  {
    AUTO_PROFILER_LABEL("PollWrapper", IDLE);
    AUTO_PROFILER_THREAD_SLEEP;
    result = (*sPollFunc)(ufds, nfsd, timeout_);
  }
  mozilla::BackgroundHangMonitor().NotifyActivity();
  return result;
}

// Emit resume-events on GdkFrameClock if flush-events has not been
// balanced by resume-events at dispose.
// For https://bugzilla.gnome.org/show_bug.cgi?id=742636
static decltype(GObjectClass::constructed) sRealGdkFrameClockConstructed;
static decltype(GObjectClass::dispose) sRealGdkFrameClockDispose;
static GQuark sPendingResumeQuark;

static void OnFlushEvents(GObject* clock, gpointer) {
  g_object_set_qdata(clock, sPendingResumeQuark, GUINT_TO_POINTER(1));
}

static void OnResumeEvents(GObject* clock, gpointer) {
  g_object_set_qdata(clock, sPendingResumeQuark, nullptr);
}

static void WrapGdkFrameClockConstructed(GObject* object) {
  sRealGdkFrameClockConstructed(object);

  g_signal_connect(object, "flush-events", G_CALLBACK(OnFlushEvents), nullptr);
  g_signal_connect(object, "resume-events", G_CALLBACK(OnResumeEvents),
                   nullptr);
}

static void WrapGdkFrameClockDispose(GObject* object) {
  if (g_object_get_qdata(object, sPendingResumeQuark)) {
    g_signal_emit_by_name(object, "resume-events");
  }

  sRealGdkFrameClockDispose(object);
}

/*static*/
gboolean nsAppShell::EventProcessorCallback(GIOChannel* source,
                                            GIOCondition condition,
                                            gpointer data) {
  nsAppShell* self = static_cast<nsAppShell*>(data);

  unsigned char c;
  Unused << read(self->mPipeFDs[0], &c, 1);
  NS_ASSERTION(c == (unsigned char)NOTIFY_TOKEN, "wrong token");

  self->NativeEventCallback();
  return TRUE;
}

nsAppShell::~nsAppShell() {
#ifdef MOZ_ENABLE_DBUS
  StopDBusListening();
#endif
  mozilla::hal::Shutdown();

  if (mTag) g_source_remove(mTag);
  if (mPipeFDs[0]) close(mPipeFDs[0]);
  if (mPipeFDs[1]) close(mPipeFDs[1]);
}

#ifdef MOZ_ENABLE_DBUS
static void SessionSleepCallback(DBusGProxy* aProxy, gboolean aSuspend,
                                 gpointer data) {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (!observerService) {
    return;
  }

  if (aSuspend) {
    // Post sleep_notification
    observerService->NotifyObservers(nullptr, NS_WIDGET_SLEEP_OBSERVER_TOPIC,
                                     nullptr);
  } else {
    // Post wake_notification
    observerService->NotifyObservers(nullptr, NS_WIDGET_WAKE_OBSERVER_TOPIC,
                                     nullptr);
  }
}

static DBusHandlerResult ConnectionSignalFilter(DBusConnection* aConnection,
                                                DBusMessage* aMessage,
                                                void* aData) {
  if (dbus_message_is_signal(aMessage, DBUS_INTERFACE_LOCAL, "Disconnected")) {
    auto* appShell = static_cast<nsAppShell*>(aData);
    appShell->StopDBusListening();
    // We do not return DBUS_HANDLER_RESULT_HANDLED here because the connection
    // might be shared and some other filters might want to do something.
  }

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

// Based on
// https://github.com/lcp/NetworkManager/blob/240f47c892b4e935a3e92fc09eb15163d1fa28d8/src/nm-sleep-monitor-systemd.c
// Use login1 to signal sleep and wake notifications.
void nsAppShell::StartDBusListening() {
  GError* error = nullptr;
  mDBusConnection = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
  if (!mDBusConnection) {
    NS_WARNING(nsPrintfCString("gds: Failed to open connection to bus %s\n",
                               error->message)
                   .get());
    g_error_free(error);
    return;
  }

  DBusConnection* dbusConnection =
      dbus_g_connection_get_connection(mDBusConnection);

  // Make sure we do not exit the entire program if DBus connection gets
  // lost.
  dbus_connection_set_exit_on_disconnect(dbusConnection, false);

  // Listening to signals the DBus connection is going to get so we will
  // know when it is lost and we will be able to disconnect cleanly.
  dbus_connection_add_filter(dbusConnection, ConnectionSignalFilter, this,
                             nullptr);

  mLogin1Proxy = dbus_g_proxy_new_for_name(
      mDBusConnection, "org.freedesktop.login1", "/org/freedesktop/login1",
      "org.freedesktop.login1.Manager");

  if (!mLogin1Proxy) {
    NS_WARNING("gds: error-no dbus proxy\n");
    return;
  }

  dbus_g_proxy_add_signal(mLogin1Proxy, "PrepareForSleep", G_TYPE_BOOLEAN,
                          G_TYPE_INVALID);
  dbus_g_proxy_connect_signal(mLogin1Proxy, "PrepareForSleep",
                              G_CALLBACK(SessionSleepCallback), this, nullptr);
}

void nsAppShell::StopDBusListening() {
  // If mDBusConnection isn't initialized, that means we are not really
  // listening.
  if (!mDBusConnection) {
    return;
  }
  dbus_connection_remove_filter(
      dbus_g_connection_get_connection(mDBusConnection), ConnectionSignalFilter,
      this);

  if (mLogin1Proxy) {
    dbus_g_proxy_disconnect_signal(mLogin1Proxy, "PrepareForSleep",
                                   G_CALLBACK(SessionSleepCallback), this);
    g_object_unref(mLogin1Proxy);
    mLogin1Proxy = nullptr;
  }
  dbus_g_connection_unref(mDBusConnection);
  mDBusConnection = nullptr;
}

#endif

nsresult nsAppShell::Init() {
  mozilla::hal::Init();

#ifdef MOZ_ENABLE_DBUS
  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsIPowerManagerService> powerManagerService =
        do_GetService(POWERMANAGERSERVICE_CONTRACTID);

    if (powerManagerService) {
      powerManagerService->AddWakeLockListener(
          WakeLockListener::GetSingleton());
    } else {
      NS_WARNING(
          "Failed to retrieve PowerManagerService, wakelocks will be broken!");
    }

    StartDBusListening();
  }
#endif

  if (!sPollFunc) {
    sPollFunc = g_main_context_get_poll_func(nullptr);
    g_main_context_set_poll_func(nullptr, &PollWrapper);
  }

  if (XRE_IsParentProcess()) {
    ScreenManager& screenManager = ScreenManager::GetSingleton();
    if (gfxPlatform::IsHeadless()) {
      screenManager.SetHelper(mozilla::MakeUnique<HeadlessScreenHelper>());
    } else {
      screenManager.SetHelper(mozilla::MakeUnique<ScreenHelperGTK>());
    }

    if (gtk_check_version(3, 16, 3) == nullptr) {
      // Before 3.16.3, GDK cannot override classname by --class command line
      // option when program uses gdk_set_program_class().
      //
      // See https://bugzilla.gnome.org/show_bug.cgi?id=747634
      //
      // Only bother doing this for the parent process, since it's the one
      // creating top-level windows. (At this point, a child process hasn't
      // received the list of registered chrome packages, so the
      // GetBrandShortName call would fail anyway.)
      nsAutoString brandName;
      mozilla::widget::WidgetUtils::GetBrandShortName(brandName);
      if (!brandName.IsEmpty()) {
        gdk_set_program_class(NS_ConvertUTF16toUTF8(brandName).get());
      }
    }
  }

  if (!sPendingResumeQuark &&
      gtk_check_version(3, 14, 7) != nullptr) {  // GTK 3.0 to GTK 3.14.7.
    // GTK 3.8 - 3.14 registered this type when creating the frame clock
    // for the root window of the display when the display was opened.
    GType gdkFrameClockIdleType = g_type_from_name("GdkFrameClockIdle");
    if (gdkFrameClockIdleType) {  // not in versions prior to 3.8
      sPendingResumeQuark = g_quark_from_string("moz-resume-is-pending");
      auto gdk_frame_clock_idle_class =
          G_OBJECT_CLASS(g_type_class_peek_static(gdkFrameClockIdleType));
      auto constructed = &gdk_frame_clock_idle_class->constructed;
      sRealGdkFrameClockConstructed = *constructed;
      *constructed = WrapGdkFrameClockConstructed;
      auto dispose = &gdk_frame_clock_idle_class->dispose;
      sRealGdkFrameClockDispose = *dispose;
      *dispose = WrapGdkFrameClockDispose;
    }
  }

  // Workaround for bug 1209659 which is fixed by Gtk3.20
  if (gtk_check_version(3, 20, 0) != nullptr) {
    unsetenv("GTK_CSD");
  }

  if (PR_GetEnv("MOZ_DEBUG_PAINTS")) {
    gdk_window_set_debug_updates(TRUE);
  }

  // Whitelist of only common, stable formats - see bugs 1197059 and 1203078
  GSList* pixbufFormats = gdk_pixbuf_get_formats();
  for (GSList* iter = pixbufFormats; iter; iter = iter->next) {
    GdkPixbufFormat* format = static_cast<GdkPixbufFormat*>(iter->data);
    gchar* name = gdk_pixbuf_format_get_name(format);
    if (strcmp(name, "jpeg") && strcmp(name, "png") && strcmp(name, "gif") &&
        strcmp(name, "bmp") && strcmp(name, "ico") && strcmp(name, "xpm") &&
        strcmp(name, "svg")) {
      gdk_pixbuf_format_set_disabled(format, TRUE);
    }
    g_free(name);
  }
  g_slist_free(pixbufFormats);

  int err = pipe(mPipeFDs);
  if (err) return NS_ERROR_OUT_OF_MEMORY;

  GIOChannel* ioc;
  GSource* source;

  // make the pipe nonblocking

  int flags = fcntl(mPipeFDs[0], F_GETFL, 0);
  if (flags == -1) goto failed;
  err = fcntl(mPipeFDs[0], F_SETFL, flags | O_NONBLOCK);
  if (err == -1) goto failed;
  flags = fcntl(mPipeFDs[1], F_GETFL, 0);
  if (flags == -1) goto failed;
  err = fcntl(mPipeFDs[1], F_SETFL, flags | O_NONBLOCK);
  if (err == -1) goto failed;

  ioc = g_io_channel_unix_new(mPipeFDs[0]);
  source = g_io_create_watch(ioc, G_IO_IN);
  g_io_channel_unref(ioc);
  g_source_set_callback(source, (GSourceFunc)EventProcessorCallback, this,
                        nullptr);
  g_source_set_can_recurse(source, TRUE);
  mTag = g_source_attach(source, nullptr);
  g_source_unref(source);

  return nsBaseAppShell::Init();
failed:
  close(mPipeFDs[0]);
  close(mPipeFDs[1]);
  mPipeFDs[0] = mPipeFDs[1] = 0;
  return NS_ERROR_FAILURE;
}

void nsAppShell::ScheduleNativeEventCallback() {
  unsigned char buf[] = {NOTIFY_TOKEN};
  Unused << write(mPipeFDs[1], buf, 1);
}

bool nsAppShell::ProcessNextNativeEvent(bool mayWait) {
  bool ret = g_main_context_iteration(nullptr, mayWait);
#ifdef MOZ_WAYLAND
  mozilla::widget::WaylandDispatchDisplays();
#endif
  return ret;
}
