/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#include "mozilla/HangMonitor.h"
#include "mozilla/Unused.h"
#include "mozilla/WidgetUtils.h"
#include "GeckoProfiler.h"
#include "nsIPowerManagerService.h"
#ifdef MOZ_ENABLE_DBUS
#include "WakeLockListener.h"
#endif
#include "gfxPlatform.h"
#include "ScreenHelperGTK.h"
#include "HeadlessScreenHelper.h"
#include "mozilla/widget/ScreenManager.h"

using mozilla::Unused;
using mozilla::widget::ScreenHelperGTK;
using mozilla::widget::HeadlessScreenHelper;
using mozilla::widget::ScreenManager;
using mozilla::LazyLogModule;

#define NOTIFY_TOKEN 0xFA

LazyLogModule gWidgetLog("Widget");
LazyLogModule gWidgetFocusLog("WidgetFocus");
LazyLogModule gWidgetDragLog("WidgetDrag");
LazyLogModule gWidgetDrawLog("WidgetDraw");

static GPollFunc sPollFunc;

// Wrapper function to disable hang monitoring while waiting in poll().
static gint
PollWrapper(GPollFD *ufds, guint nfsd, gint timeout_)
{
    mozilla::HangMonitor::Suspend();
    gint result;
    {
        AUTO_PROFILER_THREAD_SLEEP;
        result = (*sPollFunc)(ufds, nfsd, timeout_);
    }
    mozilla::HangMonitor::NotifyActivity();
    return result;
}

#ifdef MOZ_WIDGET_GTK
// For bug 726483.
static decltype(GtkContainerClass::check_resize) sReal_gtk_window_check_resize;

static void
wrap_gtk_window_check_resize(GtkContainer *container)
{
    GdkWindow* gdk_window = gtk_widget_get_window(&container->widget);
    if (gdk_window) {
        g_object_ref(gdk_window);
    }

    sReal_gtk_window_check_resize(container);

    if (gdk_window) {
        g_object_unref(gdk_window);
    }
}

// Emit resume-events on GdkFrameClock if flush-events has not been
// balanced by resume-events at dispose.
// For https://bugzilla.gnome.org/show_bug.cgi?id=742636
static decltype(GObjectClass::constructed) sRealGdkFrameClockConstructed;
static decltype(GObjectClass::dispose) sRealGdkFrameClockDispose;
static GQuark sPendingResumeQuark;

static void
OnFlushEvents(GObject* clock, gpointer)
{
    g_object_set_qdata(clock, sPendingResumeQuark, GUINT_TO_POINTER(1));
}

static void
OnResumeEvents(GObject* clock, gpointer)
{
    g_object_set_qdata(clock, sPendingResumeQuark, nullptr);
}

static void
WrapGdkFrameClockConstructed(GObject* object)
{
    sRealGdkFrameClockConstructed(object);

    g_signal_connect(object, "flush-events",
                     G_CALLBACK(OnFlushEvents), nullptr);
    g_signal_connect(object, "resume-events",
                     G_CALLBACK(OnResumeEvents), nullptr);
}

static void
WrapGdkFrameClockDispose(GObject* object)
{
    if (g_object_get_qdata(object, sPendingResumeQuark)) {
        g_signal_emit_by_name(object, "resume-events");
    }

    sRealGdkFrameClockDispose(object);
}
#endif

/*static*/ gboolean
nsAppShell::EventProcessorCallback(GIOChannel *source,
                                   GIOCondition condition,
                                   gpointer data)
{
    nsAppShell *self = static_cast<nsAppShell *>(data);

    unsigned char c;
    Unused << read(self->mPipeFDs[0], &c, 1);
    NS_ASSERTION(c == (unsigned char) NOTIFY_TOKEN, "wrong token");

    self->NativeEventCallback();
    return TRUE;
}

nsAppShell::~nsAppShell()
{
    if (mTag)
        g_source_remove(mTag);
    if (mPipeFDs[0])
        close(mPipeFDs[0]);
    if (mPipeFDs[1])
        close(mPipeFDs[1]);
}

nsresult
nsAppShell::Init()
{
    // For any versions of Glib before 2.36, g_type_init must be explicitly called
    // to safely use the library. Failure to do so may cause various failures/crashes
    // in any code that uses Glib, Gdk, or Gtk. In later versions of Glib, this call
    // is a no-op.
    g_type_init();

#ifdef MOZ_ENABLE_DBUS
    if (XRE_IsParentProcess()) {
        nsCOMPtr<nsIPowerManagerService> powerManagerService =
            do_GetService(POWERMANAGERSERVICE_CONTRACTID);

        if (powerManagerService) {
            powerManagerService->AddWakeLockListener(WakeLockListener::GetSingleton());
        } else {
            NS_WARNING("Failed to retrieve PowerManagerService, wakelocks will be broken!");
        }
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
    }

    if (gtk_check_version(3, 16, 3) == nullptr) {
        // Before 3.16.3, GDK cannot override classname by --class command line
        // option when program uses gdk_set_program_class().
        //
        // See https://bugzilla.gnome.org/show_bug.cgi?id=747634
        nsAutoString brandName;
        mozilla::widget::WidgetUtils::GetBrandShortName(brandName);
        if (!brandName.IsEmpty()) {
            gdk_set_program_class(NS_ConvertUTF16toUTF8(brandName).get());
        }
    }

#ifdef MOZ_WIDGET_GTK
    if (!sReal_gtk_window_check_resize &&
        gtk_check_version(3,8,0) != nullptr) { // GTK 3.0 to GTK 3.6.
        // GtkWindow is a static class and so will leak anyway but this ref
        // makes sure it isn't recreated.
        gpointer gtk_plug_class = g_type_class_ref(GTK_TYPE_WINDOW);
        auto check_resize = &GTK_CONTAINER_CLASS(gtk_plug_class)->check_resize;
        sReal_gtk_window_check_resize = *check_resize;
        *check_resize = wrap_gtk_window_check_resize;
    }

    if (!sPendingResumeQuark &&
        gtk_check_version(3,14,7) != nullptr) { // GTK 3.0 to GTK 3.14.7.
        // GTK 3.8 - 3.14 registered this type when creating the frame clock
        // for the root window of the display when the display was opened.
        GType gdkFrameClockIdleType = g_type_from_name("GdkFrameClockIdle");
        if (gdkFrameClockIdleType) { // not in versions prior to 3.8
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
    if (gtk_check_version(3, 20, 0) != nullptr)
        unsetenv("GTK_CSD");
#endif

    if (PR_GetEnv("MOZ_DEBUG_PAINTS"))
        gdk_window_set_debug_updates(TRUE);

    // Whitelist of only common, stable formats - see bugs 1197059 and 1203078
    GSList* pixbufFormats = gdk_pixbuf_get_formats();
    for (GSList* iter = pixbufFormats; iter; iter = iter->next) {
        GdkPixbufFormat* format = static_cast<GdkPixbufFormat*>(iter->data);
        gchar* name = gdk_pixbuf_format_get_name(format);
        if (strcmp(name, "jpeg") &&
            strcmp(name, "png") &&
            strcmp(name, "gif") &&
            strcmp(name, "bmp") &&
            strcmp(name, "ico") &&
            strcmp(name, "xpm") &&
            strcmp(name, "svg")) {
          gdk_pixbuf_format_set_disabled(format, TRUE);
        }
        g_free(name);
    }
    g_slist_free(pixbufFormats);

    int err = pipe(mPipeFDs);
    if (err)
        return NS_ERROR_OUT_OF_MEMORY;

    GIOChannel *ioc;
    GSource *source;

    // make the pipe nonblocking

    int flags = fcntl(mPipeFDs[0], F_GETFL, 0);
    if (flags == -1)
        goto failed;
    err = fcntl(mPipeFDs[0], F_SETFL, flags | O_NONBLOCK);
    if (err == -1)
        goto failed;
    flags = fcntl(mPipeFDs[1], F_GETFL, 0);
    if (flags == -1)
        goto failed;
    err = fcntl(mPipeFDs[1], F_SETFL, flags | O_NONBLOCK);
    if (err == -1)
        goto failed;

    ioc = g_io_channel_unix_new(mPipeFDs[0]);
    source = g_io_create_watch(ioc, G_IO_IN);
    g_io_channel_unref(ioc);
    g_source_set_callback(source, (GSourceFunc)EventProcessorCallback, this, nullptr);
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

void
nsAppShell::ScheduleNativeEventCallback()
{
    unsigned char buf[] = { NOTIFY_TOKEN };
    Unused << write(mPipeFDs[1], buf, 1);
}

bool
nsAppShell::ProcessNextNativeEvent(bool mayWait)
{
    return g_main_context_iteration(nullptr, mayWait);
}
