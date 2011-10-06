/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *  Oleg Romashin <romaxa@gmail.com>.
 *
 * Contributor(s):
 *  Oleg Romashin <romaxa@gmail.com>.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsAppShell.h"
#include <qapplication.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <qabstracteventdispatcher.h>

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

    QAbstractEventDispatcher *dispatcher =  QAbstractEventDispatcher::instance(qApp->thread());
    if (!dispatcher)
        return PR_FALSE;

    return dispatcher->processEvents(flags) ? PR_TRUE : PR_FALSE;
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
