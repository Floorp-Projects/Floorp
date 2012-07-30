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
#include "prlog.h"
#include "prenv.h"
#include "mozilla/HangMonitor.h"

#define NOTIFY_TOKEN 0xFA

#ifdef PR_LOGGING
PRLogModuleInfo *gWidgetLog = nullptr;
PRLogModuleInfo *gWidgetFocusLog = nullptr;
PRLogModuleInfo *gWidgetDragLog = nullptr;
PRLogModuleInfo *gWidgetDrawLog = nullptr;
#endif

static GPollFunc sPollFunc;

// Wrapper function to disable hang monitoring while waiting in poll().
static gint
PollWrapper(GPollFD *ufds, guint nfsd, gint timeout_)
{
    mozilla::HangMonitor::Suspend();
    gint result = (*sPollFunc)(ufds, nfsd, timeout_);
    mozilla::HangMonitor::NotifyActivity();
    return result;
}

/*static*/ gboolean
nsAppShell::EventProcessorCallback(GIOChannel *source, 
                                   GIOCondition condition,
                                   gpointer data)
{
    nsAppShell *self = static_cast<nsAppShell *>(data);

    unsigned char c;
    read(self->mPipeFDs[0], &c, 1);
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
#ifdef PR_LOGGING
    if (!gWidgetLog)
        gWidgetLog = PR_NewLogModule("Widget");
    if (!gWidgetFocusLog)
        gWidgetFocusLog = PR_NewLogModule("WidgetFocus");
    if (!gWidgetDragLog)
        gWidgetDragLog = PR_NewLogModule("WidgetDrag");
    if (!gWidgetDrawLog)
        gWidgetDrawLog = PR_NewLogModule("WidgetDraw");
#endif

    if (!sPollFunc) {
        sPollFunc = g_main_context_get_poll_func(NULL);
        g_main_context_set_poll_func(NULL, &PollWrapper);
    }

    GIOChannel *ioc;

    if (PR_GetEnv("MOZ_DEBUG_PAINTS"))
        gdk_window_set_debug_updates(TRUE);

    int err = pipe(mPipeFDs);
    if (err)
        return NS_ERROR_OUT_OF_MEMORY;

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
    mTag = g_io_add_watch_full(ioc, G_PRIORITY_DEFAULT, G_IO_IN,
                               EventProcessorCallback, this, nullptr);
    g_io_channel_unref(ioc);

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
    write(mPipeFDs[1], buf, 1);
}

bool
nsAppShell::ProcessNextNativeEvent(bool mayWait)
{
    return g_main_context_iteration(NULL, mayWait);
}
