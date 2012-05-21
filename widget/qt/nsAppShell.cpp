/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAppShell.h"
#include <qapplication.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <qabstracteventdispatcher.h>
#include <qthread.h>

#include "prenv.h"
#include "nsQAppInstance.h"

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#include "prlog.h"
#endif

#ifdef PR_LOGGING
PRLogModuleInfo *gWidgetLog = nsnull;
PRLogModuleInfo *gWidgetFocusLog = nsnull;
PRLogModuleInfo *gWidgetIMLog = nsnull;
PRLogModuleInfo *gWidgetDrawLog = nsnull;
#endif

static int sPokeEvent;

nsAppShell::~nsAppShell()
{
    nsQAppInstance::Release();
}

nsresult
nsAppShell::Init()
{
#ifdef PR_LOGGING
    if (!gWidgetLog)
        gWidgetLog = PR_NewLogModule("Widget");
    if (!gWidgetFocusLog)
        gWidgetFocusLog = PR_NewLogModule("WidgetFocus");
    if (!gWidgetIMLog)
        gWidgetIMLog = PR_NewLogModule("WidgetIM");
    if (!gWidgetDrawLog)
        gWidgetDrawLog = PR_NewLogModule("WidgetDraw");
#endif
#if (QT_VERSION >= QT_VERSION_CHECK(4, 4, 0))
    sPokeEvent = QEvent::registerEventType();
#else
    sPokeEvent = QEvent::User+5000;
#endif

    nsQAppInstance::AddRef();

    return nsBaseAppShell::Init();
}

void
nsAppShell::ScheduleNativeEventCallback()
{
    QCoreApplication::postEvent(this,
                                new QEvent((QEvent::Type) sPokeEvent));
}


bool
nsAppShell::ProcessNextNativeEvent(bool mayWait)
{
    QEventLoop::ProcessEventsFlags flags = QEventLoop::AllEvents;

    if (mayWait)
        flags |= QEventLoop::WaitForMoreEvents;

    QAbstractEventDispatcher *dispatcher =  QAbstractEventDispatcher::instance(QThread::currentThread());
    if (!dispatcher)
        return false;

    return dispatcher->processEvents(flags) ? true : false;
}

bool
nsAppShell::event (QEvent *e)
{
    if (e->type() == sPokeEvent) {
        NativeEventCallback();
        return true;
    }

    return false;
}
