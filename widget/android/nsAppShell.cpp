/* -*- Mode: c++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Make sure the order of included headers
#include "base/basictypes.h"
#include "nspr/prtypes.h"
#include "base/message_loop.h"
#include "base/task.h"

#include "mozilla/Hal.h"
#include "nsAppShell.h"
#include "nsWindow.h"
#include "nsThreadUtils.h"
#include "nsICommandLineRunner.h"
#include "nsIObserverService.h"
#include "nsIAppStartup.h"
#include "nsIGeolocationProvider.h"
#include "nsCacheService.h"
#include "nsIDOMEventListener.h"
#include "nsDOMNotifyPaintEvent.h"
#include "nsIDOMClientRectList.h"
#include "nsIDOMClientRect.h"
#include "nsIDOMWakeLockListener.h"
#include "nsIPowerManagerService.h"
#include "nsFrameManager.h"

#include "mozilla/Services.h"
#include "mozilla/unused.h"
#include "mozilla/Preferences.h"
#include "mozilla/Hal.h"
#include "prenv.h"

#include "AndroidBridge.h"
#include <android/log.h>
#include <pthread.h>
#include <wchar.h>

#include "mozilla/dom/ScreenOrientation.h"

#include "sampler.h"
#ifdef MOZ_ANDROID_HISTORY
#include "nsAndroidHistory.h"
#endif

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#include "prlog.h"
#endif

#ifdef DEBUG_ANDROID_EVENTS
#define EVLOG(args...)  ALOG(args)
#else
#define EVLOG(args...) do { } while (0)
#endif

using namespace mozilla;

#ifdef PR_LOGGING
PRLogModuleInfo *gWidgetLog = nsnull;
#endif

nsIGeolocationUpdate *gLocationCallback = nsnull;
nsAutoPtr<mozilla::AndroidGeckoEvent> gLastSizeChange;

nsAppShell *nsAppShell::gAppShell = nsnull;

NS_IMPL_ISUPPORTS_INHERITED1(nsAppShell, nsBaseAppShell, nsIObserver)

class ScreenshotRunnable : public nsRunnable {
public:
    ScreenshotRunnable(nsIAndroidBrowserApp* aBrowserApp, int aTabId, nsTArray<nsIntPoint>& aPoints, int aToken, RefCountedJavaObject* aBuffer):
        mBrowserApp(aBrowserApp), mPoints(aPoints), mTabId(aTabId), mToken(aToken), mBuffer(aBuffer) {}

    virtual nsresult Run() {
        nsCOMPtr<nsIDOMWindow> domWindow;
        nsCOMPtr<nsIBrowserTab> tab;
        mBrowserApp->GetBrowserTab(mTabId, getter_AddRefs(tab));
        if (!tab)
            return NS_OK;

        tab->GetWindow(getter_AddRefs(domWindow));
        if (!domWindow)
            return NS_OK;

        NS_ASSERTION(mPoints.Length() == 5, "Screenshot event does not have enough coordinates");

        AndroidBridge::Bridge()->TakeScreenshot(domWindow, mPoints[0].x, mPoints[0].y, mPoints[1].x, mPoints[1].y, mPoints[2].x, mPoints[2].y, mPoints[3].x, mPoints[3].y, mPoints[4].x, mPoints[4].y, mTabId, mToken, mBuffer->GetObject());
        return NS_OK;
    }
private:
    nsCOMPtr<nsIAndroidBrowserApp> mBrowserApp;
    nsTArray<nsIntPoint> mPoints;
    int mTabId, mToken;
    nsRefPtr<RefCountedJavaObject> mBuffer;
};

class AfterPaintListener : public nsIDOMEventListener {
  public:
    NS_DECL_ISUPPORTS

    void Register(nsIDOMWindow* window) {
        if (mEventTarget)
            Unregister();
        nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(window);
        if (!win)
            return;
        mEventTarget = win->GetChromeEventHandler();
        if (mEventTarget)
            mEventTarget->AddEventListener(NS_LITERAL_STRING("MozAfterPaint"), this, false);
    }

    void Unregister() {
        if (mEventTarget)
            mEventTarget->RemoveEventListener(NS_LITERAL_STRING("MozAfterPaint"), this, false);
        mEventTarget = nsnull;
    }

    virtual nsresult HandleEvent(nsIDOMEvent* aEvent) {
        PRUint32 generation = nsFrameManager::GetGlobalGenerationNumber();
        if (mLastGeneration == generation) {
            // the frame tree has not changed since our last AfterPaint
            // so we can drop this event.
            return NS_OK;
        }

        mLastGeneration = generation;

        nsCOMPtr<nsIDOMNotifyPaintEvent> paintEvent = do_QueryInterface(aEvent);
        if (!paintEvent)
            return NS_OK;

        nsCOMPtr<nsIDOMClientRect> rect;
        paintEvent->GetBoundingClientRect(getter_AddRefs(rect));
        float top, left, bottom, right;
        rect->GetTop(&top);
        rect->GetLeft(&left);
        rect->GetRight(&right);
        rect->GetBottom(&bottom);
        __android_log_print(ANDROID_LOG_INFO, "GeckoScreenshot", "rect: %f, %f, %f, %f", top, left, right, bottom);
        AndroidBridge::NotifyPaintedRect(top, left, bottom, right);
        return NS_OK;
    }

    ~AfterPaintListener() {
        if (mEventTarget)
            Unregister();
    }

  private:
    PRUint32 mLastGeneration;
    nsCOMPtr<nsIDOMEventTarget> mEventTarget;
};

class WakeLockListener : public nsIDOMMozWakeLockListener {
 public:
  NS_DECL_ISUPPORTS;

  nsresult Callback(const nsAString& topic, const nsAString& state) {
    AndroidBridge::Bridge()->NotifyWakeLockChanged(topic, state);
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS1(AfterPaintListener, nsIDOMEventListener)
nsCOMPtr<AfterPaintListener> sAfterPaintListener = nsnull;

NS_IMPL_ISUPPORTS1(WakeLockListener, nsIDOMMozWakeLockListener)
nsCOMPtr<nsIPowerManagerService> sPowerManagerService = nsnull;
nsCOMPtr<nsIDOMMozWakeLockListener> sWakeLockListener = nsnull;

nsAppShell::nsAppShell()
    : mQueueLock("nsAppShell.mQueueLock"),
      mCondLock("nsAppShell.mCondLock"),
      mQueueCond(mCondLock, "nsAppShell.mQueueCond"),
      mQueuedDrawEvent(nsnull),
      mQueuedViewportEvent(nsnull),
      mAllowCoalescingNextDraw(false)
{
    gAppShell = this;
    sAfterPaintListener = new AfterPaintListener();

    sPowerManagerService = do_GetService(POWERMANAGERSERVICE_CONTRACTID);

    if (sPowerManagerService) {
        sWakeLockListener = new WakeLockListener();
    } else {
        NS_WARNING("Failed to retrieve PowerManagerService, wakelocks will be broken!");
    }

}

nsAppShell::~nsAppShell()
{
    gAppShell = nsnull;
    delete sAfterPaintListener;

    if (sPowerManagerService) {
        sPowerManagerService->RemoveWakeLockListener(sWakeLockListener);

        sPowerManagerService = nsnull;
        sWakeLockListener = nsnull;
    }
}

void
nsAppShell::NotifyNativeEvent()
{
    MutexAutoLock lock(mCondLock);
    mQueueCond.Notify();
}

#define PREFNAME_MATCH_OS  "intl.locale.matchOS"
#define PREFNAME_UA_LOCALE "general.useragent.locale"
static const char* kObservedPrefs[] = {
  PREFNAME_MATCH_OS,
  PREFNAME_UA_LOCALE,
  nsnull
};

nsresult
nsAppShell::Init()
{
#ifdef PR_LOGGING
    if (!gWidgetLog)
        gWidgetLog = PR_NewLogModule("Widget");
#endif

    mObserversHash.Init();

    nsresult rv = nsBaseAppShell::Init();
    AndroidBridge* bridge = AndroidBridge::Bridge();

    nsCOMPtr<nsIObserverService> obsServ =
        mozilla::services::GetObserverService();
    if (obsServ) {
        obsServ->AddObserver(this, "xpcom-shutdown", false);
    }

    if (sPowerManagerService)
        sPowerManagerService->AddWakeLockListener(sWakeLockListener);

    if (!bridge)
        return rv;

    Preferences::AddStrongObservers(this, kObservedPrefs);

    bool match;
    rv = Preferences::GetBool(PREFNAME_MATCH_OS, &match);
    NS_ENSURE_SUCCESS(rv, rv);

    if (match) {
        bridge->SetSelectedLocale(EmptyString());
        return NS_OK;
    }

    nsAutoString locale;
    rv = Preferences::GetLocalizedString(PREFNAME_UA_LOCALE, &locale);
    if (NS_FAILED(rv)) {
        rv = Preferences::GetString(PREFNAME_UA_LOCALE, &locale);
    }

    bridge->SetSelectedLocale(locale);
    return rv;
}

NS_IMETHODIMP
nsAppShell::Observe(nsISupports* aSubject,
                    const char* aTopic,
                    const PRUnichar* aData)
{
    if (!strcmp(aTopic, "xpcom-shutdown")) {
        // We need to ensure no observers stick around after XPCOM shuts down
        // or we'll see crashes, as the app shell outlives XPConnect.
        mObserversHash.Clear();
        return nsBaseAppShell::Observe(aSubject, aTopic, aData);
    } else if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) && aData && (
                   nsDependentString(aData).Equals(
                       NS_LITERAL_STRING(PREFNAME_UA_LOCALE)) ||
                   nsDependentString(aData).Equals(
                       NS_LITERAL_STRING(PREFNAME_MATCH_OS)))) {
        AndroidBridge* bridge = AndroidBridge::Bridge();
        if (!bridge) {
            return NS_OK;
        }

        bool match;
        nsresult rv = Preferences::GetBool(PREFNAME_MATCH_OS, &match);
        NS_ENSURE_SUCCESS(rv, rv);

        if (match) {
            bridge->SetSelectedLocale(EmptyString());
            return NS_OK;
        }

        nsAutoString locale;
        if (NS_FAILED(Preferences::GetLocalizedString(PREFNAME_UA_LOCALE,
                                                      &locale))) {
            locale = Preferences::GetString(PREFNAME_UA_LOCALE);
        }

        bridge->SetSelectedLocale(locale);
        return NS_OK;
    }
    return NS_OK;
}

void
nsAppShell::ScheduleNativeEventCallback()
{
    EVLOG("nsAppShell::ScheduleNativeEventCallback pth: %p thread: %p main: %d", (void*) pthread_self(), (void*) NS_GetCurrentThread(), NS_IsMainThread());

    // this is valid to be called from any thread, so do so.
    PostEvent(new AndroidGeckoEvent(AndroidGeckoEvent::NATIVE_POKE));
}

bool
nsAppShell::ProcessNextNativeEvent(bool mayWait)
{
    EVLOG("nsAppShell::ProcessNextNativeEvent %d", mayWait);

    SAMPLE_LABEL("nsAppShell", "ProcessNextNativeEvent");
    nsAutoPtr<AndroidGeckoEvent> curEvent;
    {
        MutexAutoLock lock(mCondLock);

        curEvent = PopNextEvent();
        if (!curEvent && mayWait) {
            SAMPLE_LABEL("nsAppShell::ProcessNextNativeEvent", "Wait");
            // hmm, should we really hardcode this 10s?
#if defined(DEBUG_ANDROID_EVENTS)
            PRTime t0, t1;
            EVLOG("nsAppShell: waiting on mQueueCond");
            t0 = PR_Now();
            mQueueCond.Wait(PR_MillisecondsToInterval(10000));
            t1 = PR_Now();
            EVLOG("nsAppShell: wait done, waited %d ms", (int)(t1-t0)/1000);
#else
            mQueueCond.Wait();
#endif

            curEvent = PopNextEvent();
        }
    }

    if (!curEvent)
        return false;

    EVLOG("nsAppShell: event %p %d", (void*)curEvent.get(), curEvent->Type());

    switch (curEvent->Type()) {
    case AndroidGeckoEvent::NATIVE_POKE:
        NativeEventCallback();
        break;

    case AndroidGeckoEvent::SENSOR_EVENT:
      {
        InfallibleTArray<float> values;
        mozilla::hal::SensorType type = (mozilla::hal::SensorType) curEvent->Flags();

        switch (type) {
          case hal::SENSOR_ORIENTATION:
          case hal::SENSOR_LINEAR_ACCELERATION:
          case hal::SENSOR_ACCELERATION:
          case hal::SENSOR_GYROSCOPE:
          case hal::SENSOR_PROXIMITY:
            values.AppendElement(curEvent->X());
            values.AppendElement(curEvent->Y()); 
            values.AppendElement(curEvent->Z());
            break;

        case hal::SENSOR_LIGHT:
            values.AppendElement(curEvent->X());
            break;

        default:
            __android_log_print(ANDROID_LOG_ERROR,
                                "Gecko", "### SENSOR_EVENT fired, but type wasn't known %d",
                                type);
        }

        const hal::SensorAccuracyType &accuracy = (hal::SensorAccuracyType) curEvent->MetaState();
        hal::SensorData sdata(type, PR_Now(), values, accuracy);
        hal::NotifySensorChange(sdata);
      }
      break;

    case AndroidGeckoEvent::LOCATION_EVENT: {
        if (!gLocationCallback)
            break;

        nsGeoPosition* p = curEvent->GeoPosition();
        if (p)
            gLocationCallback->Update(curEvent->GeoPosition());
        else
            NS_WARNING("Received location event without geoposition!");
        break;
    }

    case AndroidGeckoEvent::ACTIVITY_STOPPING: {
        if (curEvent->Flags() > 0)
            break;

        nsCOMPtr<nsIObserverService> obsServ =
            mozilla::services::GetObserverService();
        NS_NAMED_LITERAL_STRING(minimize, "heap-minimize");
        obsServ->NotifyObservers(nsnull, "memory-pressure", minimize.get());
        obsServ->NotifyObservers(nsnull, "application-background", nsnull);

        break;
    }

    case AndroidGeckoEvent::ACTIVITY_SHUTDOWN: {
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
        if (curEvent->Flags() == 0) {
            // We aren't transferring to one of our own activities, so set
            // background status
            nsCOMPtr<nsIObserverService> obsServ =
                mozilla::services::GetObserverService();
            obsServ->NotifyObservers(nsnull, "application-background", nsnull);

            // If we are OOM killed with the disk cache enabled, the entire
            // cache will be cleared (bug 105843), so shut down the cache here
            // and re-init on resume
            if (nsCacheService::GlobalInstance())
                nsCacheService::GlobalInstance()->Shutdown();
        }

        // We really want to send a notification like profile-before-change,
        // but profile-before-change ends up shutting some things down instead
        // of flushing data
        nsIPrefService* prefs = Preferences::GetService();
        if (prefs) {
            // reset the crash loop state
            nsCOMPtr<nsIPrefBranch> prefBranch;
            prefs->GetBranch("browser.sessionstore.", getter_AddRefs(prefBranch));
            if (prefBranch)
                prefBranch->SetIntPref("recent_crashes", 0);

            prefs->SavePrefFile(nsnull);
        }

        break;
    }

    case AndroidGeckoEvent::ACTIVITY_START: {
        if (curEvent->Flags() > 0)
            break;

        nsCOMPtr<nsIObserverService> obsServ =
            mozilla::services::GetObserverService();
        obsServ->NotifyObservers(nsnull, "application-foreground", nsnull);

        break;
    }

    case AndroidGeckoEvent::PAINT_LISTEN_START_EVENT: {
        nsCOMPtr<nsIDOMWindow> domWindow;
        nsCOMPtr<nsIBrowserTab> tab;
        mBrowserApp->GetBrowserTab(curEvent->MetaState(), getter_AddRefs(tab));
        if (!tab)
            break;

        tab->GetWindow(getter_AddRefs(domWindow));
        if (!domWindow)
            break;

        sAfterPaintListener->Register(domWindow);
        break;
    }

    case AndroidGeckoEvent::SCREENSHOT: {
        if (!mBrowserApp)
            break;

        AndroidBridge* bridge = AndroidBridge::Bridge();
        if (!bridge)
            break;

        PRInt32 token = curEvent->Flags();
        PRInt32 tabId = curEvent->MetaState();
        nsTArray<nsIntPoint> points = curEvent->Points();
        RefCountedJavaObject* buffer = curEvent->ByteBuffer();
        nsCOMPtr<ScreenshotRunnable> sr = 
            new ScreenshotRunnable(mBrowserApp, tabId, points, token, buffer);
        MessageLoop::current()->PostIdleTask(
            FROM_HERE, NewRunnableMethod(sr.get(), &ScreenshotRunnable::Run));
        break;
    }

    case AndroidGeckoEvent::VIEWPORT:
    case AndroidGeckoEvent::BROADCAST: {

        if (curEvent->Characters().Length() == 0)
            break;

        nsCOMPtr<nsIObserverService> obsServ =
            mozilla::services::GetObserverService();

        const NS_ConvertUTF16toUTF8 topic(curEvent->Characters());
        const nsPromiseFlatString& data = PromiseFlatString(curEvent->CharactersExtra());

        obsServ->NotifyObservers(nsnull, topic.get(), data.get());
        break;
    }

    case AndroidGeckoEvent::LOAD_URI: {
        nsCOMPtr<nsICommandLineRunner> cmdline
            (do_CreateInstance("@mozilla.org/toolkit/command-line;1"));
        if (!cmdline)
            break;

        if (curEvent->Characters().Length() == 0)
            break;

        char *uri = ToNewUTF8String(curEvent->Characters());
        if (!uri)
            break;

        char *flag = ToNewUTF8String(curEvent->CharactersExtra());

        const char *argv[4] = {
            "dummyappname",
            "-url",
            uri,
            flag ? flag : ""
        };
        nsresult rv = cmdline->Init(4, const_cast<char **>(argv), nsnull, nsICommandLine::STATE_REMOTE_AUTO);
        if (NS_SUCCEEDED(rv))
            cmdline->Run();
        nsMemory::Free(uri);
        if (flag)
            nsMemory::Free(flag);
        break;
    }

    case AndroidGeckoEvent::SIZE_CHANGED: {
        // store the last resize event to dispatch it to new windows with a FORCED_RESIZE event
        if (curEvent != gLastSizeChange) {
            gLastSizeChange = new AndroidGeckoEvent(curEvent);
        }
        nsWindow::OnGlobalAndroidEvent(curEvent);
        break;
    }

    case AndroidGeckoEvent::VISITED: {
#ifdef MOZ_ANDROID_HISTORY
        nsAndroidHistory::NotifyURIVisited(nsString(curEvent->Characters()));
#endif
        break;
    }

    case AndroidGeckoEvent::NETWORK_CHANGED: {
        hal::NotifyNetworkChange(hal::NetworkInformation(curEvent->Bandwidth(),
                                                         curEvent->CanBeMetered()));
        break;
    }

    case AndroidGeckoEvent::ACTIVITY_RESUMING: {
        if (curEvent->Flags() == 0) {
            // If we are OOM killed with the disk cache enabled, the entire
            // cache will be cleared (bug 105843), so shut down cache on pause
            // and re-init here
            if (nsCacheService::GlobalInstance())
                nsCacheService::GlobalInstance()->Init();

            // We didn't return from one of our own activities, so restore
            // to foreground status
            nsCOMPtr<nsIObserverService> obsServ =
                mozilla::services::GetObserverService();
            obsServ->NotifyObservers(nsnull, "application-foreground", nsnull);
        }
        break;
    }

    case AndroidGeckoEvent::SCREENORIENTATION_CHANGED: {
        nsresult rv;
        nsCOMPtr<nsIScreenManager> screenMgr =
            do_GetService("@mozilla.org/gfx/screenmanager;1", &rv);
        if (NS_FAILED(rv)) {
            NS_ERROR("Can't find nsIScreenManager!");
            break;
        }

        nsIntRect rect;
        PRInt32 colorDepth, pixelDepth;
        dom::ScreenOrientation orientation;
        nsCOMPtr<nsIScreen> screen;

        screenMgr->GetPrimaryScreen(getter_AddRefs(screen));
        screen->GetRect(&rect.x, &rect.y, &rect.width, &rect.height);
        screen->GetColorDepth(&colorDepth);
        screen->GetPixelDepth(&pixelDepth);
        orientation =
            static_cast<dom::ScreenOrientation>(curEvent->ScreenOrientation());

        hal::NotifyScreenConfigurationChange(
            hal::ScreenConfiguration(rect, orientation, colorDepth, pixelDepth));
        break;
    }

    default:
        nsWindow::OnGlobalAndroidEvent(curEvent);
    }

    EVLOG("nsAppShell: -- done event %p %d", (void*)curEvent.get(), curEvent->Type());

    return true;
}

void
nsAppShell::ResendLastResizeEvent(nsWindow* aDest) {
    if (gLastSizeChange) {
        nsWindow::OnGlobalAndroidEvent(gLastSizeChange);
    }
}

AndroidGeckoEvent*
nsAppShell::PopNextEvent()
{
    AndroidGeckoEvent *ae = nsnull;
    MutexAutoLock lock(mQueueLock);
    if (mEventQueue.Length()) {
        ae = mEventQueue[0];
        mEventQueue.RemoveElementAt(0);
        if (mQueuedDrawEvent == ae) {
            mQueuedDrawEvent = nsnull;
        } else if (mQueuedViewportEvent == ae) {
            mQueuedViewportEvent = nsnull;
        }
    }

    return ae;
}

AndroidGeckoEvent*
nsAppShell::PeekNextEvent()
{
    AndroidGeckoEvent *ae = nsnull;
    MutexAutoLock lock(mQueueLock);
    if (mEventQueue.Length()) {
        ae = mEventQueue[0];
    }

    return ae;
}

void
nsAppShell::PostEvent(AndroidGeckoEvent *ae)
{
    {
        // set this to true when inserting events that we can coalesce
        // viewport events across. this is effectively maintaining a whitelist
        // of events that are unaffected by viewport changes.
        bool allowCoalescingNextViewport = false;

        MutexAutoLock lock(mQueueLock);
        EVLOG("nsAppShell::PostEvent %p %d", ae, ae->Type());
        switch (ae->Type()) {
        case AndroidGeckoEvent::SURFACE_DESTROYED:
            // Give priority to this event, and discard any pending
            // SURFACE_CREATED events.
            mEventQueue.InsertElementAt(0, ae);
            AndroidGeckoEvent *event;
            for (int i = mEventQueue.Length() - 1; i >= 1; i--) {
                event = mEventQueue[i];
                if (event->Type() == AndroidGeckoEvent::SURFACE_CREATED) {
                    EVLOG("nsAppShell: Dropping old SURFACE_CREATED event at %p %d", event, i);
                    mEventQueue.RemoveElementAt(i);
                    delete event;
                }
            }
            break;

        case AndroidGeckoEvent::COMPOSITOR_PAUSE:
        case AndroidGeckoEvent::COMPOSITOR_RESUME:
            // Give priority to these events, but maintain their order wrt each other.
            {
                PRUint32 i = 0;
                while (i < mEventQueue.Length() &&
                       (mEventQueue[i]->Type() == AndroidGeckoEvent::COMPOSITOR_PAUSE ||
                        mEventQueue[i]->Type() == AndroidGeckoEvent::COMPOSITOR_RESUME)) {
                    i++;
                }
                EVLOG("nsAppShell: Inserting compositor event %d at position %d to maintain priority order", ae->Type(), i);
                mEventQueue.InsertElementAt(i, ae);
            }
            break;

        case AndroidGeckoEvent::DRAW:
            if (mQueuedDrawEvent) {
                // coalesce this new draw event with the one already in the queue
                const nsIntRect& oldRect = mQueuedDrawEvent->Rect();
                const nsIntRect& newRect = ae->Rect();
                int combinedArea = (oldRect.width * oldRect.height) +
                                   (newRect.width * newRect.height);

                nsIntRect combinedRect = oldRect.Union(newRect);
                // XXX We may want to consider using regions instead of rectangles.
                //     Print an error if we're upload a lot more than we would
                //     if we handled this as two separate events.
                int boundsArea = combinedRect.width * combinedRect.height;
                if (boundsArea > combinedArea * 8)
                    ALOG("nsAppShell: Area of bounds greatly exceeds combined area: %d > %d",
                         boundsArea, combinedArea);

                // coalesce into the new draw event rather than the queued one because
                // it is not always safe to move draws earlier in the queue; there may
                // be events between the two draws that affect scroll position or something.
                ae->Init(AndroidGeckoEvent::DRAW, combinedRect);

                EVLOG("nsAppShell: Coalescing previous DRAW event at %p into new DRAW event %p", mQueuedDrawEvent, ae);
                mEventQueue.RemoveElement(mQueuedDrawEvent);
                delete mQueuedDrawEvent;
            }

            if (!mAllowCoalescingNextDraw) {
                // if we're not allowing coalescing of this draw event, then
                // don't set mQueuedDrawEvent to point to this; that way the
                // next draw event that comes in won't kill this one.
                mAllowCoalescingNextDraw = true;
                mQueuedDrawEvent = nsnull;
            } else {
                mQueuedDrawEvent = ae;
            }

            allowCoalescingNextViewport = true;

            mEventQueue.AppendElement(ae);
            break;

        case AndroidGeckoEvent::VIEWPORT:
            if (mQueuedViewportEvent) {
                // drop the previous viewport event now that we have a new one
                EVLOG("nsAppShell: Dropping old viewport event at %p in favour of new VIEWPORT event %p", mQueuedViewportEvent, ae);
                mEventQueue.RemoveElement(mQueuedViewportEvent);
                delete mQueuedViewportEvent;
            }
            mQueuedViewportEvent = ae;
            // temporarily turn off draw-coalescing, so that we process a draw
            // event as soon as possible after a viewport change
            mAllowCoalescingNextDraw = false;
            allowCoalescingNextViewport = true;

            mEventQueue.AppendElement(ae);
            break;

        case AndroidGeckoEvent::MOTION_EVENT:
            if (ae->Action() == AndroidMotionEvent::ACTION_MOVE) {
                int len = mEventQueue.Length();
                if (len > 0) {
                    AndroidGeckoEvent* event = mEventQueue[len - 1];
                    if (event->Type() == AndroidGeckoEvent::MOTION_EVENT && event->Action() == AndroidMotionEvent::ACTION_MOVE) {
                        // consecutive motion-move events; drop the last one before adding the new one
                        EVLOG("nsAppShell: Dropping old move event at %p in favour of new move event %p", event, ae);
                        mEventQueue.RemoveElementAt(len - 1);
                        delete event;
                    }
                }
            }
            mEventQueue.AppendElement(ae);
            break;

        case AndroidGeckoEvent::NATIVE_POKE:
            allowCoalescingNextViewport = true;
            // fall through

        default:
            mEventQueue.AppendElement(ae);
            break;
        }

        // if the event wasn't on our whitelist then reset mQueuedViewportEvent
        // so that we don't coalesce future viewport events into the last viewport
        // event we added
        if (!allowCoalescingNextViewport)
            mQueuedViewportEvent = nsnull;
    }
    NotifyNativeEvent();
}

void
nsAppShell::OnResume()
{
}

nsresult
nsAppShell::AddObserver(const nsAString &aObserverKey, nsIObserver *aObserver)
{
    NS_ASSERTION(aObserver != nsnull, "nsAppShell::AddObserver: aObserver is null!");
    mObserversHash.Put(aObserverKey, aObserver);
    return NS_OK;
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
        unused << rv;
    }
}

void
nsAppShell::RemoveObserver(const nsAString &aObserverKey)
{
    mObserversHash.Remove(aObserverKey);
}

// NotifyObservers support.  NotifyObservers only works on main thread.

class NotifyObserversCaller : public nsRunnable {
public:
    NotifyObserversCaller(nsISupports *aSupports,
                          const char *aTopic, const PRUnichar *aData) :
        mSupports(aSupports), mTopic(aTopic), mData(aData) {
    }

    NS_IMETHOD Run() {
        nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
        if (os)
            os->NotifyObservers(mSupports, mTopic.get(), mData.get());

        return NS_OK;
    }

private:
    nsCOMPtr<nsISupports> mSupports;
    nsCString mTopic;
    nsString mData;
};

void
nsAppShell::NotifyObservers(nsISupports *aSupports,
                            const char *aTopic,
                            const PRUnichar *aData)
{
    // This isn't main thread, so post this to main thread
    nsCOMPtr<nsIRunnable> caller =
        new NotifyObserversCaller(aSupports, aTopic, aData);
    NS_DispatchToMainThread(caller);
}

// Used by IPC code
namespace mozilla {

bool ProcessNextEvent()
{
    return nsAppShell::gAppShell->ProcessNextNativeEvent(true) ? true : false;
}

void NotifyEvent()
{
    nsAppShell::gAppShell->NotifyNativeEvent();
}

}
