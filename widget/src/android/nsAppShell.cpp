/* -*- Mode: c++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4; -*- */
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
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
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
#include "nsWindow.h"
#include "nsThreadUtils.h"
#include "nsICommandLineRunner.h"
#include "nsIObserverService.h"
#include "nsIAppStartup.h"
#include "nsIGeolocationProvider.h"
#include "nsIPrefService.h"

#include "mozilla/Services.h"
#include "prenv.h"

#include "AndroidBridge.h"
#include "nsAccelerometerSystem.h"
#include <android/log.h>
#include <pthread.h>

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#include "prlog.h"
#endif

#ifdef ANDROID_DEBUG_EVENTS
#define EVLOG(args...)  ALOG(args)
#else
#define EVLOG(args...) do { } while (0)
#endif

using namespace mozilla;

#ifdef PR_LOGGING
PRLogModuleInfo *gWidgetLog = nsnull;
#endif

nsAccelerometerSystem *gAccel = nsnull;
nsIGeolocationUpdate *gLocationCallback = nsnull;

nsAppShell *nsAppShell::gAppShell = nsnull;
AndroidGeckoEvent *nsAppShell::gEarlyEvent = nsnull;

nsAppShell::nsAppShell()
    : mQueueLock(nsnull),
      mCondLock(nsnull),
      mQueueCond(nsnull),
      mNumDraws(0)
{
    gAppShell = this;
    if (gEarlyEvent) {
        mEventQueue.AppendElement(gEarlyEvent);
        gEarlyEvent = nsnull;
    }
}

nsAppShell::~nsAppShell()
{
    gAppShell = nsnull;
}

void
nsAppShell::NotifyNativeEvent()
{
    PR_Lock(mCondLock);
    PR_NotifyCondVar(mQueueCond);
    PR_Unlock(mCondLock);
}

nsresult
nsAppShell::Init()
{
#ifdef PR_LOGGING
    if (!gWidgetLog)
        gWidgetLog = PR_NewLogModule("Widget");
#endif

    mQueueLock = PR_NewLock();
    mCondLock = PR_NewLock();
    mQueueCond = PR_NewCondVar(mCondLock);

    mObserversHash.Init();

    return nsBaseAppShell::Init();
}


void
nsAppShell::ScheduleNativeEventCallback()
{
    EVLOG("nsAppShell::ScheduleNativeEventCallback pth: %p thread: %p main: %d", (void*) pthread_self(), (void*) NS_GetCurrentThread(), NS_IsMainThread());

    // this is valid to be called from any thread, so do so.
    PostEvent(new AndroidGeckoEvent(AndroidGeckoEvent::NATIVE_POKE));
}


PRBool
nsAppShell::ProcessNextNativeEvent(PRBool mayWait)
{
    EVLOG("nsAppShell::ProcessNextNativeEvent %d", mayWait);

    PR_Lock(mCondLock);

    nsAutoPtr<AndroidGeckoEvent> curEvent;
    AndroidGeckoEvent *nextEvent;

    curEvent = GetNextEvent();
    if (!curEvent && mayWait) {
        // hmm, should we really hardcode this 10s?
#if defined(ANDROID_DEBUG_EVENTS)
        PRTime t0, t1;
        EVLOG("nsAppShell: waiting on mQueueCond");
        t0 = PR_Now();

        PR_WaitCondVar(mQueueCond, PR_MillisecondsToInterval(10000));
        t1 = PR_Now();
        EVLOG("nsAppShell: wait done, waited %d ms", (int)(t1-t0)/1000);
#else
        PR_WaitCondVar(mQueueCond, PR_INTERVAL_NO_TIMEOUT);
#endif

        curEvent = GetNextEvent();
    }

    PR_Unlock(mCondLock);

    if (!curEvent)
        return false;

    // Combine subsequent events of the same type

    nextEvent = PeekNextEvent();

    while (nextEvent) {
        int curType = curEvent->Type();
        int nextType = nextEvent->Type();

        while (nextType == AndroidGeckoEvent::DRAW &&
               mNumDraws > 1)
        {
            // skip this draw, since there's a later one already in the queue.. this will let us
            // deal with sequences that look like:
            //   MOVE DRAW MOVE DRAW MOVE DRAW
            // and end up with just
            //   MOVE DRAW
            // when we process all the events.
            RemoveNextEvent();
            delete nextEvent;

#if defined(ANDROID_DEBUG_EVENTS)
            ALOG("# Removing DRAW event (%d outstanding)", mNumDraws);
#endif

            nextEvent = PeekNextEvent();
            nextType = nextEvent->Type();
        }

        // If the next type of event isn't the same as the current type,
        // we don't coalesce.
        if (nextType != curType)
            break;

        // Can only coalesce motion move events, for motion events
        if (curType != AndroidGeckoEvent::MOTION_EVENT)
            break;

        if (!(curEvent->Action() == AndroidMotionEvent::ACTION_MOVE &&
              nextEvent->Action() == AndroidMotionEvent::ACTION_MOVE))
            break;

#if defined(ANDROID_DEBUG_EVENTS)
        ALOG("# Removing % 2d event", curType);
#endif

        RemoveNextEvent();
        curEvent = nextEvent;
        nextEvent = PeekNextEvent();
    }

    EVLOG("nsAppShell: event %p %d [ndraws %d]", (void*)curEvent.get(), curEvent->Type(), mNumDraws);

    nsWindow *target = (nsWindow*) curEvent->NativeWindow();

    switch (curEvent->Type()) {
    case AndroidGeckoEvent::NATIVE_POKE:
        NativeEventCallback();
        break;

    case AndroidGeckoEvent::SENSOR_EVENT:
        gAccel->AccelerationChanged(-curEvent->X(), curEvent->Y(), curEvent->Z());
        break;

    case AndroidGeckoEvent::LOCATION_EVENT:
        if (!gLocationCallback)
            break;

        if (curEvent->GeoPosition())
            gLocationCallback->Update(curEvent->GeoPosition());
        else
            NS_WARNING("Received location event without geoposition!");
        break;

    case AndroidGeckoEvent::ACTIVITY_STOPPING: {
        nsCOMPtr<nsIObserverService> obsServ =
          mozilla::services::GetObserverService();
        NS_NAMED_LITERAL_STRING(context, "shutdown-persist");
        obsServ->NotifyObservers(nsnull, "quit-application-granted", nsnull);
        obsServ->NotifyObservers(nsnull, "quit-application-forced", nsnull);
        obsServ->NotifyObservers(nsnull, "profile-change-net-teardown", context.get());
        obsServ->NotifyObservers(nsnull, "profile-change-teardown", context.get());
        obsServ->NotifyObservers(nsnull, "profile-before-change", context.get());
        nsCOMPtr<nsIAppStartup> appSvc = do_GetService("@mozilla.org/toolkit/app-startup;1");
        if (appSvc)
            appSvc->Quit(nsIAppStartup::eForceQuit);
        break;
    }

    case AndroidGeckoEvent::ACTIVITY_PAUSING: {
        // We really want to send a notification like profile-before-change,
        // but profile-before-change ends up shutting some things down instead
        // of flushing data
        nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
        if (prefs)
            prefs->SavePrefFile(nsnull);

        break;
    }

    case AndroidGeckoEvent::LOAD_URI: {
        nsCOMPtr<nsICommandLineRunner> cmdline
            (do_CreateInstance("@mozilla.org/toolkit/command-line;1"));
        if (!cmdline)
            break;

        char *uri = ToNewUTF8String(curEvent->Characters());
        if (!uri)
            break;

        char* argv[3] = {
            "dummyappname",
            "-remote",
            uri
        };
        nsresult rv = cmdline->Init(3, argv, nsnull, nsICommandLine::STATE_REMOTE_AUTO);
        if (NS_SUCCEEDED(rv))
            cmdline->Run();
        nsMemory::Free(uri);
        break;
    }

    default:
        if (target)
            target->OnAndroidEvent(curEvent);
        else
            nsWindow::OnGlobalAndroidEvent(curEvent);
    }

    EVLOG("nsAppShell: -- done event %p %d", (void*)curEvent.get(), curEvent->Type());

    return true;
}

AndroidGeckoEvent*
nsAppShell::GetNextEvent()
{
    AndroidGeckoEvent *ae = nsnull;
    PR_Lock(mQueueLock);
    if (mEventQueue.Length()) {
        ae = mEventQueue[0];
        mEventQueue.RemoveElementAt(0);
        if (ae->Type() == AndroidGeckoEvent::DRAW) {
            mNumDraws--;
        }
    }
    PR_Unlock(mQueueLock);

    return ae;
}

AndroidGeckoEvent*
nsAppShell::PeekNextEvent()
{
    AndroidGeckoEvent *ae = nsnull;
    PR_Lock(mQueueLock);
    if (mEventQueue.Length()) {
        ae = mEventQueue[0];
    }
    PR_Unlock(mQueueLock);

    return ae;
}

void
nsAppShell::PostEvent(AndroidGeckoEvent *ae)
{
    PR_Lock(mQueueLock);
    mEventQueue.AppendElement(ae);
    if (ae->Type() == AndroidGeckoEvent::DRAW) {
        mNumDraws++;
    }
    PR_Unlock(mQueueLock);
    NotifyNativeEvent();
}

void
nsAppShell::RemoveNextEvent()
{
    AndroidGeckoEvent *ae = nsnull;
    PR_Lock(mQueueLock);
    if (mEventQueue.Length()) {
        ae = mEventQueue[0];
        mEventQueue.RemoveElementAt(0);
        if (ae->Type() == AndroidGeckoEvent::DRAW) {
            mNumDraws--;
        }
    }
    PR_Unlock(mQueueLock);
}

void
nsAppShell::OnResume()
{
}

nsresult
nsAppShell::AddObserver(const nsAString &aObserverKey, nsIObserver *aObserver)
{
    NS_ASSERTION(aObserver != nsnull, "nsAppShell::AddObserver: aObserver is null!");
    return mObserversHash.Put(aObserverKey, aObserver) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/**
 * The XPCOM event that will call the observer on the main thread.
 */
class ObserverCaller : public nsRunnable {
public:
    ObserverCaller(nsIObserver *aObserver, const char *aTopic, const PRUnichar *aData) :
        mObserver(aObserver), mTopic(aTopic), mData(aData) {
        NS_ASSERTION(aObserver != nsnull, "ObserverCaller: aObserver is null!");
    }

    NS_IMETHOD Run() {
        ALOG("ObserverCaller::Run: observer = %p, topic = '%s')",
             (nsIObserver*)mObserver, mTopic.get());
        mObserver->Observe(nsnull, mTopic.get(), mData.get());
        return NS_OK;
    }

private:
    nsCOMPtr<nsIObserver> mObserver;
    nsCString mTopic;
    nsString mData;
};

void
nsAppShell::CallObserver(const nsAString &aObserverKey, const nsAString &aTopic, const nsAString &aData)
{
    nsCOMPtr<nsIObserver> observer;
    mObserversHash.Get(aObserverKey, getter_AddRefs(observer));

    if (!observer) {
        ALOG("nsAppShell::CallObserver: Observer was not found!");
        return;
    }

    const NS_ConvertUTF16toUTF8 sTopic(aTopic);
    const nsPromiseFlatString& sData = PromiseFlatString(aData);
    
    if (NS_IsMainThread()) {
        // This branch will unlikely be hit, have it just in case
        observer->Observe(nsnull, sTopic.get(), sData.get());
    } else {
        // Java is not running on main thread, so we have to use NS_DispatchToMainThread
        nsCOMPtr<nsIRunnable> observerCaller = new ObserverCaller(observer, sTopic.get(), sData.get());
        nsresult rv = NS_DispatchToMainThread(observerCaller);
        ALOG("NS_DispatchToMainThread result: %d", rv);
    }
}

void
nsAppShell::RemoveObserver(const nsAString &aObserverKey)
{
    mObserversHash.Remove(aObserverKey);
}

// Used by IPC code
namespace mozilla {

bool ProcessNextEvent()
{
    return nsAppShell::gAppShell->ProcessNextNativeEvent(PR_TRUE) ? true : false;
}

void NotifyEvent()
{
    nsAppShell::gAppShell->NotifyNativeEvent();
}

}
