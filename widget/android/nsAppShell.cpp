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
#include "nsIURIFixup.h"
#include "nsCategoryManagerUtils.h"
#include "nsCDefaultURIFixup.h"
#include "nsToolkitCompsCID.h"
#include "nsGeoPosition.h"

#include "mozilla/Services.h"
#include "mozilla/Preferences.h"
#include "mozilla/Hal.h"
#include "prenv.h"

#include "AndroidBridge.h"
#include "AndroidBridgeUtilities.h"
#include "GeneratedJNINatives.h"
#include <android/log.h>
#include <pthread.h>
#include <wchar.h>

#ifdef MOZ_GAMEPAD
#include "mozilla/dom/GamepadPlatformService.h"
#include "mozilla/dom/Gamepad.h"
#endif

#include "GeckoProfiler.h"
#ifdef MOZ_ANDROID_HISTORY
#include "nsNetUtil.h"
#include "nsIURI.h"
#include "IHistory.h"
#endif

#ifdef MOZ_LOGGING
#include "mozilla/Logging.h"
#endif

#include "ANRReporter.h"
#include "GeckoNetworkManager.h"
#include "GeckoScreenOrientation.h"
#include "PrefsHelper.h"
#include "ThumbnailHelper.h"

#ifdef DEBUG_ANDROID_EVENTS
#define EVLOG(args...)  ALOG(args)
#else
#define EVLOG(args...) do { } while (0)
#endif

using namespace mozilla;
typedef mozilla::dom::GamepadPlatformService GamepadPlatformService;

nsIGeolocationUpdate *gLocationCallback = nullptr;

nsAppShell* nsAppShell::sAppShell;
StaticAutoPtr<Mutex> nsAppShell::sAppShellLock;

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
    , public UsesGeckoThreadProxy
{
    static uint32_t sPauseCount;

public:
    template<typename Functor>
    static void OnNativeCall(Functor&& aCall)
    {
        if (aCall.IsTarget(&SpeculativeConnect) ||
            aCall.IsTarget(&WaitOnGecko)) {

            aCall();
            return;
        }
        return UsesGeckoThreadProxy::OnNativeCall(aCall);
    }

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
        specConn->SpeculativeConnect(uri, nullptr);
    }

    static void WaitOnGecko()
    {
        struct NoOpEvent : nsAppShell::Event {
            void Run() override {}
        };
        nsAppShell::SyncRunEvent(NoOpEvent());
    }

    static void OnPause()
    {
        MOZ_ASSERT(NS_IsMainThread());

        if ((++sPauseCount) > 1) {
            // Already paused.
            return;
        }

        nsCOMPtr<nsIObserverService> obsServ =
            mozilla::services::GetObserverService();
        obsServ->NotifyObservers(nullptr, "application-background", nullptr);

        NS_NAMED_LITERAL_STRING(minimize, "heap-minimize");
        obsServ->NotifyObservers(nullptr, "memory-pressure", minimize.get());

        // If we are OOM killed with the disk cache enabled, the entire
        // cache will be cleared (bug 105843), so shut down the cache here
        // and re-init on foregrounding
        if (nsCacheService::GlobalInstance()) {
            nsCacheService::GlobalInstance()->Shutdown();
        }

        // We really want to send a notification like profile-before-change,
        // but profile-before-change ends up shutting some things down instead
        // of flushing data
        nsIPrefService* prefs = Preferences::GetService();
        if (prefs) {
            prefs->SavePrefFile(nullptr);
        }
    }

    static void OnResume()
    {
        MOZ_ASSERT(NS_IsMainThread());

        if (!sPauseCount || (--sPauseCount) > 0) {
            // Still paused.
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
        nsCString category(aCategory->ToCString());

        NS_CreateServicesFromCategory(
                category.get(),
                nullptr, // aOrigin
                category.get(),
                aData ? aData->ToString().get() : nullptr);
    }
};

uint32_t GeckoThreadSupport::sPauseCount;


class GeckoAppShellSupport final
    : public java::GeckoAppShell::Natives<GeckoAppShellSupport>
    , public UsesGeckoThreadProxy
{
public:
    template<typename Functor>
    static void OnNativeCall(Functor&& aCall)
    {
        if (aCall.IsTarget(&SyncNotifyObservers)) {
            aCall();
            return;
        }
        return UsesGeckoThreadProxy::OnNativeCall(aCall);
    }

    static void SyncNotifyObservers(jni::String::Param aTopic,
                                    jni::String::Param aData)
    {
        MOZ_RELEASE_ASSERT(NS_IsMainThread());
        NotifyObservers(aTopic, aData);
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
        return;
    }

    if (jni::IsAvailable()) {
        // Initialize JNI and Set the corresponding state in GeckoThread.
        AndroidBridge::ConstructBridge();
        GeckoAppShellSupport::Init();
        GeckoThreadSupport::Init();
        mozilla::ANRReporter::Init();
        mozilla::GeckoNetworkManager::Init();
        mozilla::GeckoScreenOrientation::Init();
        mozilla::PrefsHelper::Init();
        mozilla::ThumbnailHelper::Init();
        nsWindow::InitNatives();

        java::GeckoThread::SetState(java::GeckoThread::State::JNI_READY());
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
        MutexAutoLock lock(*sAppShellLock);
        sAppShell = nullptr;
    }

    while (mEventQueue.Pop(/* mayWait */ false)) {
        NS_WARNING("Discarded event on shutdown");
    }

    if (sPowerManagerService) {
        sPowerManagerService->RemoveWakeLockListener(sWakeLockListener);

        sPowerManagerService = nullptr;
        sWakeLockListener = nullptr;
    }

    if (jni::IsAvailable()) {
        AndroidBridge::DeconstructBridge();
    }
}

void
nsAppShell::NotifyNativeEvent()
{
    mEventQueue.Signal();
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
        obsServ->AddObserver(this, "chrome-document-loaded", false);
        obsServ->AddObserver(this, "quit-application-granted", false);
        obsServ->AddObserver(this, "xpcom-shutdown", false);
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
        if (jni::IsAvailable()) {
            // Our first window has loaded, assume any JS initialization has run.
            java::GeckoThread::CheckAndSetState(
                    java::GeckoThread::State::PROFILE_READY(),
                    java::GeckoThread::State::RUNNING());
        }
        removeObserver = true;

    } else if (!strcmp(aTopic, "quit-application-granted")) {
        if (jni::IsAvailable()) {
            java::GeckoThread::SetState(
                    java::GeckoThread::State::EXITING());

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

    PROFILER_LABEL("nsAppShell", "ProcessNextNativeEvent",
        js::ProfileEntry::Category::EVENTS);

    mozilla::UniquePtr<Event> curEvent;

    {
        curEvent = mEventQueue.Pop(/* mayWait */ false);

        if (!curEvent && mayWait) {
            // This processes messages in the Android Looper. Note that we only
            // get here if the normal Gecko event loop has been awoken
            // (bug 750713). Looper messages effectively have the lowest
            // priority because we only process them before we're about to
            // wait for new events.
            if (jni::IsAvailable() &&
                    AndroidBridge::Bridge()->PumpMessageLoop()) {
                return true;
            }

            PROFILER_LABEL("nsAppShell", "ProcessNextNativeEvent::Wait",
                js::ProfileEntry::Category::EVENTS);
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
        mozilla::MutexAutoLock shellLock(*sAppShellLock);
        nsAppShell* const appShell = sAppShell;
        if (MOZ_UNLIKELY(!appShell || appShell->mSyncRunQuit)) {
            return;
        }
        event.Run();
        finished = true;
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

class nsAppShell::LegacyGeckoEvent : public Event
{
    mozilla::UniquePtr<AndroidGeckoEvent> ae;

public:
    LegacyGeckoEvent(AndroidGeckoEvent* e) : ae(e) {}

    void Run() override;
    void PostTo(mozilla::LinkedList<Event>& queue) override;

    Event::Type ActivityType() const override
    {
        return ae->IsInputEvent() ? mozilla::HangMonitor::kUIActivity
                                  : mozilla::HangMonitor::kGeneralActivity;
    }
};

void
nsAppShell::PostEvent(AndroidGeckoEvent* event)
{
    mozilla::MutexAutoLock lock(*sAppShellLock);
    if (!sAppShell) {
        return;
    }
    sAppShell->mEventQueue.Post(mozilla::MakeUnique<LegacyGeckoEvent>(event));
}

void
nsAppShell::LegacyGeckoEvent::Run()
{
    const mozilla::UniquePtr<AndroidGeckoEvent>& curEvent = ae;

    EVLOG("nsAppShell: event %p %d", (void*)curEvent.get(), curEvent->Type());

    switch (curEvent->Type()) {
    case AndroidGeckoEvent::ZOOMEDVIEW: {
        if (!nsAppShell::Get()->mBrowserApp)
            break;
        int32_t tabId = curEvent->MetaState();
        const nsTArray<nsIntPoint>& points = curEvent->Points();
        float scaleFactor = (float) curEvent->X();
        RefPtr<RefCountedJavaObject> javaBuffer = curEvent->ByteBuffer();
        const auto& mBuffer = jni::ByteBuffer::Ref::From(javaBuffer->GetObject());

        nsCOMPtr<mozIDOMWindowProxy> domWindow;
        nsCOMPtr<nsIBrowserTab> tab;
        nsAppShell::Get()->mBrowserApp->GetBrowserTab(tabId, getter_AddRefs(tab));
        if (!tab) {
            NS_ERROR("Can't find tab!");
            break;
        }
        tab->GetWindow(getter_AddRefs(domWindow));
        if (!domWindow) {
            NS_ERROR("Can't find dom window!");
            break;
        }
        NS_ASSERTION(points.Length() == 2, "ZoomedView event does not have enough coordinates");
        nsIntRect r(points[0].x, points[0].y, points[1].x, points[1].y);
        AndroidBridge::Bridge()->CaptureZoomedView(domWindow, r, mBuffer, scaleFactor);
        break;
    }

    case AndroidGeckoEvent::VIEWPORT: {
        if (curEvent->Characters().Length() == 0)
            break;

        nsCOMPtr<nsIObserverService> obsServ =
            mozilla::services::GetObserverService();

        const NS_ConvertUTF16toUTF8 topic(curEvent->Characters());

        obsServ->NotifyObservers(nullptr, topic.get(), curEvent->CharactersExtra().get());
        break;
    }

    case AndroidGeckoEvent::TELEMETRY_UI_SESSION_STOP: {
        if (!nsAppShell::Get()->mBrowserApp)
            break;
        if (curEvent->Characters().Length() == 0)
            break;

        nsCOMPtr<nsIUITelemetryObserver> obs;
        nsAppShell::Get()->mBrowserApp->GetUITelemetryObserver(getter_AddRefs(obs));
        if (!obs)
            break;

        obs->StopSession(
                curEvent->Characters().get(),
                curEvent->CharactersExtra().get(),
                curEvent->Time()
                );
        break;
    }

    case AndroidGeckoEvent::TELEMETRY_UI_SESSION_START: {
        if (!nsAppShell::Get()->mBrowserApp)
            break;
        if (curEvent->Characters().Length() == 0)
            break;

        nsCOMPtr<nsIUITelemetryObserver> obs;
        nsAppShell::Get()->mBrowserApp->GetUITelemetryObserver(getter_AddRefs(obs));
        if (!obs)
            break;

        obs->StartSession(
                curEvent->Characters().get(),
                curEvent->Time()
                );
        break;
    }

    case AndroidGeckoEvent::TELEMETRY_UI_EVENT: {
        if (!nsAppShell::Get()->mBrowserApp)
            break;
        if (curEvent->Data().Length() == 0)
            break;

        nsCOMPtr<nsIUITelemetryObserver> obs;
        nsAppShell::Get()->mBrowserApp->GetUITelemetryObserver(getter_AddRefs(obs));
        if (!obs)
            break;

        obs->AddEvent(
                curEvent->Data().get(),
                curEvent->Characters().get(),
                curEvent->Time(),
                curEvent->CharactersExtra().get()
                );
        break;
    }

    case AndroidGeckoEvent::CALL_OBSERVER:
    {
        nsCOMPtr<nsIObserver> observer;
        nsAppShell::Get()->mObserversHash.Get(curEvent->Characters(), getter_AddRefs(observer));

        if (observer) {
            observer->Observe(nullptr, NS_ConvertUTF16toUTF8(curEvent->CharactersExtra()).get(),
                              curEvent->Data().get());
        } else {
            ALOG("Call_Observer event: Observer was not found!");
        }

        break;
    }

    case AndroidGeckoEvent::REMOVE_OBSERVER:
        nsAppShell::Get()->mObserversHash.Remove(curEvent->Characters());
        break;

    case AndroidGeckoEvent::ADD_OBSERVER:
        nsAppShell::Get()->AddObserver(curEvent->Characters(), curEvent->Observer());
        break;

    case AndroidGeckoEvent::LOW_MEMORY:
        // TODO hook in memory-reduction stuff for different levels here
        if (curEvent->MetaState() >= AndroidGeckoEvent::MEMORY_PRESSURE_MEDIUM) {
            nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
            if (os) {
                os->NotifyObservers(nullptr, "memory-pressure", u"low-memory");
            }
        }
        break;

    case AndroidGeckoEvent::TELEMETRY_HISTOGRAM_ADD:
        // If the extras field is not empty then this is a keyed histogram.
        if (!curEvent->CharactersExtra().IsVoid()) {
            Telemetry::Accumulate(NS_ConvertUTF16toUTF8(curEvent->Characters()).get(),
                                  NS_ConvertUTF16toUTF8(curEvent->CharactersExtra()),
                                  curEvent->Count());
        } else {
            Telemetry::Accumulate(NS_ConvertUTF16toUTF8(curEvent->Characters()).get(),
                                  curEvent->Count());
        }
        break;

    case AndroidGeckoEvent::GAMEPAD_ADDREMOVE: {
#ifdef MOZ_GAMEPAD
            RefPtr<GamepadPlatformService> service;
            service = GamepadPlatformService::GetParentService();
            if (!service) {
              break;
            }
            if (curEvent->Action() == AndroidGeckoEvent::ACTION_GAMEPAD_ADDED) {
              int svc_id = service->AddGamepad("android",
                                               dom::GamepadMappingType::Standard,
                                               dom::kStandardGamepadButtons,
                                               dom::kStandardGamepadAxes);
              java::GeckoAppShell::GamepadAdded(curEvent->ID(), svc_id);
            } else if (curEvent->Action() == AndroidGeckoEvent::ACTION_GAMEPAD_REMOVED) {
              service->RemoveGamepad(curEvent->ID());
            }
#endif
        break;
    }

    case AndroidGeckoEvent::GAMEPAD_DATA: {
#ifdef MOZ_GAMEPAD
            int id = curEvent->ID();
            RefPtr<GamepadPlatformService> service;
            service = GamepadPlatformService::GetParentService();
            if (!service) {
              break;
            }
            if (curEvent->Action() == AndroidGeckoEvent::ACTION_GAMEPAD_BUTTON) {
              service->NewButtonEvent(id, curEvent->GamepadButton(),
                                      curEvent->GamepadButtonPressed(),
                                      curEvent->GamepadButtonValue());
            } else if (curEvent->Action() == AndroidGeckoEvent::ACTION_GAMEPAD_AXES) {
                int valid = curEvent->Flags();
                const nsTArray<float>& values = curEvent->GamepadValues();
                for (unsigned i = 0; i < values.Length(); i++) {
                    if (valid & (1<<i)) {
                      service->NewAxisMoveEvent(id, i, values[i]);
                    }
                }
            }
#endif
        break;
    }
    case AndroidGeckoEvent::NOOP:
        break;

    default:
        nsWindow::OnGlobalAndroidEvent(curEvent.get());
        break;
    }

    EVLOG("nsAppShell: -- done event %p %d", (void*)curEvent.get(), curEvent->Type());
}

void
nsAppShell::LegacyGeckoEvent::PostTo(mozilla::LinkedList<Event>& queue)
{
    {
        EVLOG("nsAppShell::PostEvent %p %d", ae, ae->Type());
        switch (ae->Type()) {
        case AndroidGeckoEvent::VIEWPORT:
            // Coalesce a previous viewport event with this one, while
            // allowing coalescing to happen across native callback events.
            for (Event* event = queue.getLast(); event;
                    event = event->getPrevious())
            {
                if (event->HasSameTypeAs(this) &&
                        static_cast<LegacyGeckoEvent*>(event)->ae->Type()
                            == AndroidGeckoEvent::VIEWPORT) {
                    // Found a previous viewport event; remove it.
                    delete event;
                    break;
                }
                NativeCallbackEvent callbackEvent(nullptr);
                if (event->HasSameTypeAs(&callbackEvent)) {
                    // Allow coalescing viewport events across callback events.
                    continue;
                }
                // End of search for viewport events to coalesce.
                break;
            }
            queue.insertBack(this);
            break;

        case AndroidGeckoEvent::MOTION_EVENT:
        case AndroidGeckoEvent::APZ_INPUT_EVENT:
            if (sAppShell->mAllowCoalescingTouches) {
                Event* const event = queue.getLast();
                if (event && event->HasSameTypeAs(this) && ae->CanCoalesceWith(
                        static_cast<LegacyGeckoEvent*>(event)->ae.get())) {

                    // consecutive motion-move events; drop the last one before adding the new one
                    EVLOG("nsAppShell: Dropping old move event at %p in favour of new move event %p", event, ae);
                    // Delete the event and remove from list.
                    delete event;
                }
            }
            queue.insertBack(this);
            break;

        default:
            queue.insertBack(this);
            break;
        }
    }
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
