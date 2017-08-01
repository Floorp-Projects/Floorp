/* -*- Mode: c++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAppShell.h"

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "mozilla/Hal.h"
#include "nsIScreen.h"
#include "nsIScreenManager.h"
#include "nsWindow.h"
#include "nsThreadUtils.h"
#include "nsICommandLineRunner.h"
#include "nsIObserverService.h"
#include "nsIAppStartup.h"
#include "nsIGeolocationProvider.h"
#include "nsCacheService.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMClientRectList.h"
#include "nsIDOMClientRect.h"
#include "nsIDOMWakeLockListener.h"
#include "nsIPowerManagerService.h"
#include "nsISpeculativeConnect.h"
#include "nsITabChild.h"
#include "nsIURIFixup.h"
#include "nsCategoryManagerUtils.h"
#include "nsCDefaultURIFixup.h"
#include "nsToolkitCompsCID.h"
#include "nsGeoPosition.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"
#include "mozilla/Hal.h"
#include "mozilla/dom/TabChild.h"
#include "prenv.h"

#include "AndroidBridge.h"
#include "AndroidBridgeUtilities.h"
#include "GeneratedJNINatives.h"
#include <android/log.h>
#include <pthread.h>
#include <wchar.h>

#include "GeckoProfiler.h"
#ifdef MOZ_ANDROID_HISTORY
#include "nsNetUtil.h"
#include "nsIURI.h"
#include "IHistory.h"
#endif

#ifdef MOZ_LOGGING
#include "mozilla/Logging.h"
#endif

#ifdef MOZ_CRASHREPORTER
#include "nsICrashReporter.h"
#include "nsExceptionHandler.h"
#endif

#include "AndroidAlerts.h"
#include "AndroidUiThread.h"
#include "ANRReporter.h"
#include "GeckoBatteryManager.h"
#include "GeckoNetworkManager.h"
#include "GeckoProcessManager.h"
#include "GeckoScreenOrientation.h"
#include "PrefsHelper.h"
#include "fennec/MemoryMonitor.h"
#include "fennec/Telemetry.h"
#include "fennec/ThumbnailHelper.h"

#ifdef DEBUG_ANDROID_EVENTS
#define EVLOG(args...)  ALOG(args)
#else
#define EVLOG(args...) do { } while (0)
#endif

using namespace mozilla;

nsIGeolocationUpdate *gLocationCallback = nullptr;

nsAppShell* nsAppShell::sAppShell;
StaticAutoPtr<Mutex> nsAppShell::sAppShellLock;

uint32_t nsAppShell::Queue::sLatencyCount[];
uint64_t nsAppShell::Queue::sLatencyTime[];

NS_IMPL_ISUPPORTS_INHERITED(nsAppShell, nsBaseAppShell, nsIObserver)

class WakeLockListener final : public nsIDOMMozWakeLockListener {
private:
  ~WakeLockListener() {}

public:
  NS_DECL_ISUPPORTS;

  nsresult Callback(const nsAString& topic, const nsAString& state) override {
    java::GeckoAppShell::NotifyWakeLockChanged(topic, state);
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(WakeLockListener, nsIDOMMozWakeLockListener)
nsCOMPtr<nsIPowerManagerService> sPowerManagerService = nullptr;
StaticRefPtr<WakeLockListener> sWakeLockListener;


class GeckoThreadSupport final
    : public java::GeckoThread::Natives<GeckoThreadSupport>
{
    // When this number goes above 0, the app is paused. When less than or
    // equal to zero, the app is resumed.
    static int32_t sPauseCount;

public:
    static void SpeculativeConnect(jni::String::Param aUriStr)
    {
        if (!NS_IsMainThread()) {
            // We will be on the main thread if the call was queued on the Java
            // side during startup. Otherwise, the call was not queued, which
            // means Gecko is already sufficiently loaded, and we don't really
            // care about speculative connections at this point.
            return;
        }

        nsCOMPtr<nsIIOService> ioServ = do_GetIOService();
        nsCOMPtr<nsISpeculativeConnect> specConn = do_QueryInterface(ioServ);
        if (!specConn) {
            return;
        }

        nsCOMPtr<nsIURI> uri = nsAppShell::ResolveURI(aUriStr->ToCString());
        if (!uri) {
            return;
        }

        OriginAttributes attrs;
        nsCOMPtr<nsIPrincipal> principal = BasePrincipal::CreateCodebasePrincipal(uri, attrs);
        specConn->SpeculativeConnect2(uri, principal, nullptr);
    }

    static void WaitOnGecko()
    {
        struct NoOpRunnable : Runnable
        {
            NoOpRunnable() : Runnable("NoOpRunnable") {}
            NS_IMETHOD Run() override { return NS_OK; }
        };

        struct NoOpEvent : nsAppShell::Event
        {
            void Run() override
            {
                // We cannot call NS_DispatchToMainThread from within
                // WaitOnGecko itself because the thread that is calling
                // WaitOnGecko may not be an nsThread, and may not be able to do
                // a sync dispatch.
                NS_DispatchToMainThread(do_AddRef(new NoOpRunnable()),
                                        NS_DISPATCH_SYNC);
            }
        };
        nsAppShell::SyncRunEvent(NoOpEvent());
    }

    static void OnPause()
    {
        MOZ_ASSERT(NS_IsMainThread());

        sPauseCount++;
        // If sPauseCount is now 1, we just crossed the threshold from "resumed"
        // "paused". so we should notify observers and so on.
        if (sPauseCount != 1) {
            return;
        }

        nsCOMPtr<nsIObserverService> obsServ =
            mozilla::services::GetObserverService();
        obsServ->NotifyObservers(nullptr, "application-background", nullptr);

        obsServ->NotifyObservers(nullptr, "memory-pressure", u"heap-minimize");

        // If we are OOM killed with the disk cache enabled, the entire
        // cache will be cleared (bug 105843), so shut down the cache here
        // and re-init on foregrounding
        if (nsCacheService::GlobalInstance()) {
            nsCacheService::GlobalInstance()->Shutdown();
        }

        // We really want to send a notification like profile-before-change,
        // but profile-before-change ends up shutting some things down instead
        // of flushing data
        Preferences* prefs = static_cast<Preferences *>(Preferences::GetService());
        if (prefs) {
            // Force a main thread blocking save
            prefs->SavePrefFileBlocking();
        }
    }

    static void OnResume()
    {
        MOZ_ASSERT(NS_IsMainThread());

        sPauseCount--;
        // If sPauseCount is now 0, we just crossed the threshold from "paused"
        // to "resumed", so we should notify observers and so on.
        if (sPauseCount != 0) {
            return;
        }

        // If we are OOM killed with the disk cache enabled, the entire
        // cache will be cleared (bug 105843), so shut down cache on backgrounding
        // and re-init here
        if (nsCacheService::GlobalInstance()) {
            nsCacheService::GlobalInstance()->Init();
        }

        // We didn't return from one of our own activities, so restore
        // to foreground status
        nsCOMPtr<nsIObserverService> obsServ =
            mozilla::services::GetObserverService();
        obsServ->NotifyObservers(nullptr, "application-foreground", nullptr);
    }

    static void CreateServices(jni::String::Param aCategory, jni::String::Param aData)
    {
        MOZ_ASSERT(NS_IsMainThread());

        nsCString category(aCategory->ToCString());

        NS_CreateServicesFromCategory(
                category.get(),
                nullptr, // aOrigin
                category.get(),
                aData ? aData->ToString().get() : nullptr);
    }

    static int64_t RunUiThreadCallback()
    {
        return RunAndroidUiTasks();
    }

    static void ForceQuit()
    {
        nsCOMPtr<nsIAppStartup> appStartup =
            do_GetService(NS_APPSTARTUP_CONTRACTID);

        if (appStartup) {
            appStartup->Quit(nsIAppStartup::eForceQuit);
        }

    }
};

int32_t GeckoThreadSupport::sPauseCount;


class GeckoAppShellSupport final
    : public java::GeckoAppShell::Natives<GeckoAppShellSupport>
{
public:
    static void ReportJavaCrash(const jni::Class::LocalRef& aCls,
                                jni::Throwable::Param aException,
                                jni::String::Param aStack)
    {
        if (!jni::ReportException(aCls.Env(), aException.Get(), aStack.Get())) {
            // Only crash below if crash reporter is initialized and annotation
            // succeeded. Otherwise try other means of reporting the crash in
            // Java.
            return;
        }

        MOZ_CRASH("Uncaught Java exception");
    }

    static void NotifyObservers(jni::String::Param aTopic,
                                jni::String::Param aData)
    {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(aTopic);

        nsCOMPtr<nsIObserverService> obsServ = services::GetObserverService();
        if (!obsServ) {
            return;
        }

        obsServ->NotifyObservers(nullptr, aTopic->ToCString().get(),
                                 aData ? aData->ToString().get() : nullptr);
    }

    static void OnSensorChanged(int32_t aType, float aX, float aY, float aZ,
                                float aW, int32_t aAccuracy, int64_t aTime)
    {
        AutoTArray<float, 4> values;

        switch (aType) {
        // Bug 938035, transfer HAL data for orientation sensor to meet w3c
        // spec, ex: HAL report alpha=90 means East but alpha=90 means West
        // in w3c spec
        case hal::SENSOR_ORIENTATION:
            values.AppendElement(360.0f - aX);
            values.AppendElement(-aY);
            values.AppendElement(-aZ);
            break;

        case hal::SENSOR_LINEAR_ACCELERATION:
        case hal::SENSOR_ACCELERATION:
        case hal::SENSOR_GYROSCOPE:
        case hal::SENSOR_PROXIMITY:
            values.AppendElement(aX);
            values.AppendElement(aY);
            values.AppendElement(aZ);
            break;

        case hal::SENSOR_LIGHT:
            values.AppendElement(aX);
            break;

        case hal::SENSOR_ROTATION_VECTOR:
        case hal::SENSOR_GAME_ROTATION_VECTOR:
            values.AppendElement(aX);
            values.AppendElement(aY);
            values.AppendElement(aZ);
            values.AppendElement(aW);
            break;

        default:
            __android_log_print(ANDROID_LOG_ERROR, "Gecko",
                                "Unknown sensor type %d", aType);
        }

        hal::SensorData sdata(hal::SensorType(aType), aTime, values,
                              hal::SensorAccuracyType(aAccuracy));
        hal::NotifySensorChange(sdata);
    }

    static void OnLocationChanged(double aLatitude, double aLongitude,
                                  double aAltitude, float aAccuracy,
                                  float aBearing, float aSpeed, int64_t aTime)
    {
        if (!gLocationCallback) {
            return;
        }

        RefPtr<nsIDOMGeoPosition> geoPosition(
                new nsGeoPosition(aLatitude, aLongitude, aAltitude, aAccuracy,
                                  aAccuracy, aBearing, aSpeed, aTime));
        gLocationCallback->Update(geoPosition);
    }

    static void NotifyUriVisited(jni::String::Param aUri)
    {
#ifdef MOZ_ANDROID_HISTORY
        nsCOMPtr<IHistory> history = services::GetHistoryService();
        nsCOMPtr<nsIURI> visitedURI;
        if (history &&
            NS_SUCCEEDED(NS_NewURI(getter_AddRefs(visitedURI),
                                   aUri->ToString()))) {
            history->NotifyVisited(visitedURI);
        }
#endif
    }

    static void NotifyAlertListener(jni::String::Param aName,
                                    jni::String::Param aTopic,
                                    jni::String::Param aCookie)
    {
        if (!aName || !aTopic || !aCookie) {
            return;
        }

        AndroidAlerts::NotifyListener(
                aName->ToString(), aTopic->ToCString().get(),
                aCookie->ToString().get());
    }
};

nsAppShell::nsAppShell()
    : mSyncRunFinished(*(sAppShellLock = new Mutex("nsAppShell")),
                       "nsAppShell.SyncRun")
    , mSyncRunQuit(false)
{
    {
        MutexAutoLock lock(*sAppShellLock);
        sAppShell = this;
    }

    if (!XRE_IsParentProcess()) {
        if (jni::IsAvailable()) {
            // Set the corresponding state in GeckoThread.
            java::GeckoThread::SetState(java::GeckoThread::State::RUNNING());
        }
        return;
    }

    if (jni::IsAvailable()) {
        // Initialize JNI and Set the corresponding state in GeckoThread.
        AndroidBridge::ConstructBridge();
        GeckoAppShellSupport::Init();
        GeckoThreadSupport::Init();
        mozilla::GeckoBatteryManager::Init();
        mozilla::GeckoNetworkManager::Init();
        mozilla::GeckoProcessManager::Init();
        mozilla::GeckoScreenOrientation::Init();
        mozilla::PrefsHelper::Init();
        nsWindow::InitNatives();

        if (jni::IsFennec()) {
            mozilla::ANRReporter::Init();
            mozilla::MemoryMonitor::Init();
            mozilla::widget::Telemetry::Init();
            mozilla::ThumbnailHelper::Init();
        }

        java::GeckoThread::SetState(java::GeckoThread::State::JNI_READY());

        CreateAndroidUiThread();
    }

    sPowerManagerService = do_GetService(POWERMANAGERSERVICE_CONTRACTID);

    if (sPowerManagerService) {
        sWakeLockListener = new WakeLockListener();
    } else {
        NS_WARNING("Failed to retrieve PowerManagerService, wakelocks will be broken!");
    }
}

nsAppShell::~nsAppShell()
{
    {
        // Release any thread waiting for a sync call to finish.
        MutexAutoLock lock(*sAppShellLock);
        sAppShell = nullptr;
        mSyncRunFinished.NotifyAll();
    }

    while (mEventQueue.Pop(/* mayWait */ false)) {
        NS_WARNING("Discarded event on shutdown");
    }

    if (sPowerManagerService) {
        sPowerManagerService->RemoveWakeLockListener(sWakeLockListener);

        sPowerManagerService = nullptr;
        sWakeLockListener = nullptr;
    }

    if (jni::IsAvailable() && XRE_IsParentProcess()) {
        DestroyAndroidUiThread();
        AndroidBridge::DeconstructBridge();
    }
}

void
nsAppShell::NotifyNativeEvent()
{
    mEventQueue.Signal();
}

void
nsAppShell::RecordLatencies()
{
    if (!mozilla::Telemetry::CanRecordExtended()) {
        return;
    }

    const mozilla::Telemetry::HistogramID timeIDs[] = {
        mozilla::Telemetry::HistogramID::FENNEC_LOOP_UI_LATENCY,
        mozilla::Telemetry::HistogramID::FENNEC_LOOP_OTHER_LATENCY
    };

    static_assert(ArrayLength(Queue::sLatencyCount) == Queue::LATENCY_COUNT,
                  "Count array length mismatch");
    static_assert(ArrayLength(Queue::sLatencyTime) == Queue::LATENCY_COUNT,
                  "Time array length mismatch");
    static_assert(ArrayLength(timeIDs) == Queue::LATENCY_COUNT,
                  "Time ID array length mismatch");

    for (size_t i = 0; i < Queue::LATENCY_COUNT; i++) {
        if (!Queue::sLatencyCount[i]) {
            continue;
        }

        const uint64_t time = Queue::sLatencyTime[i] / 1000ull /
                              Queue::sLatencyCount[i];
        if (time) {
            mozilla::Telemetry::Accumulate(
                    timeIDs[i], uint32_t(std::min<uint64_t>(UINT32_MAX, time)));
        }

        // Reset latency counts.
        Queue::sLatencyCount[i] = 0;
        Queue::sLatencyTime[i] = 0;
    }
}

#define PREFNAME_COALESCE_TOUCHES "dom.event.touch.coalescing.enabled"
static const char* kObservedPrefs[] = {
  PREFNAME_COALESCE_TOUCHES,
  nullptr
};

nsresult
nsAppShell::Init()
{
    nsresult rv = nsBaseAppShell::Init();
    nsCOMPtr<nsIObserverService> obsServ =
        mozilla::services::GetObserverService();
    if (obsServ) {
        obsServ->AddObserver(this, "browser-delayed-startup-finished", false);
        obsServ->AddObserver(this, "profile-after-change", false);
        obsServ->AddObserver(this, "tab-child-created", false);
        obsServ->AddObserver(this, "quit-application", false);
        obsServ->AddObserver(this, "quit-application-granted", false);
        obsServ->AddObserver(this, "xpcom-shutdown", false);

        if (XRE_IsParentProcess()) {
            obsServ->AddObserver(this, "chrome-document-loaded", false);
        }
    }

    if (sPowerManagerService)
        sPowerManagerService->AddWakeLockListener(sWakeLockListener);

    Preferences::AddStrongObservers(this, kObservedPrefs);
    mAllowCoalescingTouches = Preferences::GetBool(PREFNAME_COALESCE_TOUCHES, true);
    return rv;
}

NS_IMETHODIMP
nsAppShell::Observe(nsISupports* aSubject,
                    const char* aTopic,
                    const char16_t* aData)
{
    bool removeObserver = false;

    if (!strcmp(aTopic, "xpcom-shutdown")) {
        {
            // Release any thread waiting for a sync call to finish.
            mozilla::MutexAutoLock shellLock(*sAppShellLock);
            mSyncRunQuit = true;
            mSyncRunFinished.NotifyAll();
        }
        // We need to ensure no observers stick around after XPCOM shuts down
        // or we'll see crashes, as the app shell outlives XPConnect.
        mObserversHash.Clear();
        return nsBaseAppShell::Observe(aSubject, aTopic, aData);

    } else if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) &&
               aData &&
               nsDependentString(aData).Equals(NS_LITERAL_STRING(PREFNAME_COALESCE_TOUCHES))) {
        mAllowCoalescingTouches = Preferences::GetBool(PREFNAME_COALESCE_TOUCHES, true);
        return NS_OK;

    } else if (!strcmp(aTopic, "browser-delayed-startup-finished")) {
        NS_CreateServicesFromCategory("browser-delayed-startup-finished", nullptr,
                                      "browser-delayed-startup-finished");

    } else if (!strcmp(aTopic, "profile-after-change")) {
        if (jni::IsAvailable()) {
            // See if we want to force 16-bit color before doing anything
            if (Preferences::GetBool("gfx.android.rgb16.force", false)) {
                java::GeckoAppShell::SetScreenDepthOverride(16);
            }

            java::GeckoThread::SetState(
                    java::GeckoThread::State::PROFILE_READY());

            // Gecko on Android follows the Android app model where it never
            // stops until it is killed by the system or told explicitly to
            // quit. Therefore, we should *not* exit Gecko when there is no
            // window or the last window is closed. nsIAppStartup::Quit will
            // still force Gecko to exit.
            nsCOMPtr<nsIAppStartup> appStartup =
                do_GetService(NS_APPSTARTUP_CONTRACTID);
            if (appStartup) {
                appStartup->EnterLastWindowClosingSurvivalArea();
            }
        }
        removeObserver = true;

    } else if (!strcmp(aTopic, "chrome-document-loaded")) {
        // Set the global ready state and enable the window event dispatcher
        // for this particular GeckoView.
        nsCOMPtr<nsIDocument> doc = do_QueryInterface(aSubject);
        MOZ_ASSERT(doc);
        nsCOMPtr<nsIWidget> widget =
            WidgetUtils::DOMWindowToWidget(doc->GetWindow());

        // `widget` may be one of several different types in the parent
        // process, including the Android nsWindow, PuppetWidget, etc. To
        // ensure that we only accept the Android nsWindow, we check that the
        // widget is a top-level window and that its NS_NATIVE_WIDGET value is
        // non-null, which is not the case for non-native widgets like
        // PuppetWidget.
        if (widget &&
            widget->WindowType() == nsWindowType::eWindowType_toplevel &&
            widget->GetNativeData(NS_NATIVE_WIDGET) == widget) {
            if (jni::IsAvailable()) {
                // When our first window has loaded, assume any JS
                // initialization has run and set Gecko to ready.
                java::GeckoThread::CheckAndSetState(
                        java::GeckoThread::State::PROFILE_READY(),
                        java::GeckoThread::State::RUNNING());
            }
            const auto window = static_cast<nsWindow*>(widget.get());
            window->EnableEventDispatcher();
        }
    } else if (!strcmp(aTopic, "quit-application")) {
        if (jni::IsAvailable()) {
            const bool restarting =
                    aData && NS_LITERAL_STRING("restart").Equals(aData);
            java::GeckoThread::SetState(
                    restarting ?
                    java::GeckoThread::State::RESTARTING() :
                    java::GeckoThread::State::EXITING());
        }
        removeObserver = true;

    } else if (!strcmp(aTopic, "quit-application-granted")) {
        if (jni::IsAvailable()) {
            // We are told explicitly to quit, perhaps due to
            // nsIAppStartup::Quit being called. We should release our hold on
            // nsIAppStartup and let it continue to quit.
            nsCOMPtr<nsIAppStartup> appStartup =
                do_GetService(NS_APPSTARTUP_CONTRACTID);
            if (appStartup) {
                appStartup->ExitLastWindowClosingSurvivalArea();
            }
        }
        removeObserver = true;

    } else if (!strcmp(aTopic, "nsPref:changed")) {
        if (jni::IsAvailable()) {
            mozilla::PrefsHelper::OnPrefChange(aData);
        }

    } else if (!strcmp(aTopic, "tab-child-created")) {
        // Associate the PuppetWidget of the newly-created TabChild with a
        // GeckoEditableChild instance.
        MOZ_ASSERT(!XRE_IsParentProcess());

        dom::ContentChild* contentChild = dom::ContentChild::GetSingleton();
        nsCOMPtr<nsITabChild> ptabChild = do_QueryInterface(aSubject);
        NS_ENSURE_TRUE(contentChild && ptabChild, NS_OK);

        // Get the content/tab ID in order to get the correct
        // IGeckoEditableParent object, which GeckoEditableChild uses to
        // communicate with the parent process.
        const auto tabChild = static_cast<dom::TabChild*>(ptabChild.get());
        const uint64_t contentId = contentChild->GetID();
        const uint64_t tabId = tabChild->GetTabId();
        NS_ENSURE_TRUE(contentId && tabId, NS_OK);

        auto editableParent = java::GeckoServiceChildProcess::GetEditableParent(
                contentId, tabId);
        NS_ENSURE_TRUE(editableParent, NS_OK);

        RefPtr<widget::PuppetWidget> widget(tabChild->WebWidget());
        auto editableChild = java::GeckoEditableChild::New(editableParent);
        NS_ENSURE_TRUE(widget && editableChild, NS_OK);

        RefPtr<GeckoEditableSupport> editableSupport =
                new GeckoEditableSupport(editableChild);

        // Tell PuppetWidget to use our listener for IME operations.
        widget->SetNativeTextEventDispatcherListener(editableSupport);
    }

    if (removeObserver) {
        nsCOMPtr<nsIObserverService> obsServ =
            mozilla::services::GetObserverService();
        if (obsServ) {
            obsServ->RemoveObserver(this, aTopic);
        }
    }
    return NS_OK;
}

bool
nsAppShell::ProcessNextNativeEvent(bool mayWait)
{
    EVLOG("nsAppShell::ProcessNextNativeEvent %d", mayWait);

    AUTO_PROFILER_LABEL("nsAppShell::ProcessNextNativeEvent", EVENTS);

    mozilla::UniquePtr<Event> curEvent;

    {
        curEvent = mEventQueue.Pop(/* mayWait */ false);

        if (!curEvent && mayWait) {
            // This processes messages in the Android Looper. Note that we only
            // get here if the normal Gecko event loop has been awoken
            // (bug 750713). Looper messages effectively have the lowest
            // priority because we only process them before we're about to
            // wait for new events.
            if (jni::IsAvailable() && XRE_IsParentProcess() &&
                    AndroidBridge::Bridge()->PumpMessageLoop()) {
                return true;
            }

            AUTO_PROFILER_LABEL("nsAppShell::ProcessNextNativeEvent:Wait",
                                EVENTS);
            mozilla::HangMonitor::Suspend();

            curEvent = mEventQueue.Pop(/* mayWait */ true);
        }
    }

    if (!curEvent)
        return false;

    mozilla::HangMonitor::NotifyActivity(curEvent->ActivityType());

    curEvent->Run();
    return true;
}

void
nsAppShell::SyncRunEvent(Event&& event,
                         UniquePtr<Event>(*eventFactory)(UniquePtr<Event>&&))
{
    // Perform the call on the Gecko thread in a separate lambda, and wait
    // on the monitor on the current thread.
    MOZ_ASSERT(!NS_IsMainThread());

    // This is the lock to check that app shell is still alive,
    // and to wait on for the sync call to complete.
    mozilla::MutexAutoLock shellLock(*sAppShellLock);
    nsAppShell* const appShell = sAppShell;

    if (MOZ_UNLIKELY(!appShell)) {
        // Post-shutdown.
        return;
    }

    bool finished = false;
    auto runAndNotify = [&event, &finished] {
        nsAppShell* const appShell = nsAppShell::Get();
        if (MOZ_UNLIKELY(!appShell || appShell->mSyncRunQuit)) {
            return;
        }
        event.Run();
        finished = true;
        mozilla::MutexAutoLock shellLock(*sAppShellLock);
        appShell->mSyncRunFinished.NotifyAll();
    };

    UniquePtr<Event> runAndNotifyEvent = mozilla::MakeUnique<
            LambdaEvent<decltype(runAndNotify)>>(mozilla::Move(runAndNotify));

    if (eventFactory) {
        runAndNotifyEvent = (*eventFactory)(mozilla::Move(runAndNotifyEvent));
    }

    appShell->mEventQueue.Post(mozilla::Move(runAndNotifyEvent));

    while (!finished && MOZ_LIKELY(sAppShell && !sAppShell->mSyncRunQuit)) {
        appShell->mSyncRunFinished.Wait();
    }
}

already_AddRefed<nsIURI>
nsAppShell::ResolveURI(const nsCString& aUriStr)
{
    nsCOMPtr<nsIIOService> ioServ = do_GetIOService();
    nsCOMPtr<nsIURI> uri;

    if (NS_SUCCEEDED(ioServ->NewURI(aUriStr, nullptr,
                                    nullptr, getter_AddRefs(uri)))) {
        return uri.forget();
    }

    nsCOMPtr<nsIURIFixup> fixup = do_GetService(NS_URIFIXUP_CONTRACTID);
    if (fixup && NS_SUCCEEDED(
            fixup->CreateFixupURI(aUriStr, 0, nullptr, getter_AddRefs(uri)))) {
        return uri.forget();
    }
    return nullptr;
}

nsresult
nsAppShell::AddObserver(const nsAString &aObserverKey, nsIObserver *aObserver)
{
    NS_ASSERTION(aObserver != nullptr, "nsAppShell::AddObserver: aObserver is null!");
    mObserversHash.Put(aObserverKey, aObserver);
    return NS_OK;
}

// Used by IPC code
namespace mozilla {

bool ProcessNextEvent()
{
    nsAppShell* const appShell = nsAppShell::Get();
    if (!appShell) {
        return false;
    }

    return appShell->ProcessNextNativeEvent(true) ? true : false;
}

void NotifyEvent()
{
    nsAppShell* const appShell = nsAppShell::Get();
    if (!appShell) {
        return;
    }
    appShell->NotifyNativeEvent();
}

}
