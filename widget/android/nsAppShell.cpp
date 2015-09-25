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
#include "nsINetworkLinkService.h"
#include "nsISpeculativeConnect.h"
#include "nsIURIFixup.h"
#include "nsCategoryManagerUtils.h"
#include "nsCDefaultURIFixup.h"
#include "nsToolkitCompsCID.h"

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

#include "mozilla/dom/ScreenOrientation.h"
#ifdef MOZ_GAMEPAD
#include "mozilla/dom/GamepadFunctions.h"
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

#ifdef DEBUG_ANDROID_EVENTS
#define EVLOG(args...)  ALOG(args)
#else
#define EVLOG(args...) do { } while (0)
#endif

using namespace mozilla;

PRLogModuleInfo *gWidgetLog = nullptr;

nsIGeolocationUpdate *gLocationCallback = nullptr;
nsAutoPtr<mozilla::AndroidGeckoEvent> gLastSizeChange;

nsAppShell *nsAppShell::gAppShell = nullptr;

NS_IMPL_ISUPPORTS_INHERITED(nsAppShell, nsBaseAppShell, nsIObserver)

class ThumbnailRunnable : public nsRunnable {
public:
    ThumbnailRunnable(nsIAndroidBrowserApp* aBrowserApp, int aTabId,
                       const nsTArray<nsIntPoint>& aPoints, RefCountedJavaObject* aBuffer):
        mBrowserApp(aBrowserApp), mPoints(aPoints), mTabId(aTabId), mBuffer(aBuffer) {}

    virtual nsresult Run() {
        const auto& buffer = jni::Object::Ref::From(mBuffer->GetObject());
        nsCOMPtr<nsIDOMWindow> domWindow;
        nsCOMPtr<nsIBrowserTab> tab;
        mBrowserApp->GetBrowserTab(mTabId, getter_AddRefs(tab));
        if (!tab) {
            widget::ThumbnailHelper::SendThumbnail(buffer, mTabId, false, false);
            return NS_ERROR_FAILURE;
        }

        tab->GetWindow(getter_AddRefs(domWindow));
        if (!domWindow) {
            widget::ThumbnailHelper::SendThumbnail(buffer, mTabId, false, false);
            return NS_ERROR_FAILURE;
        }

        NS_ASSERTION(mPoints.Length() == 1, "Thumbnail event does not have enough coordinates");

        bool shouldStore = true;
        nsresult rv = AndroidBridge::Bridge()->CaptureThumbnail(domWindow, mPoints[0].x, mPoints[0].y, mTabId, buffer, shouldStore);
        widget::ThumbnailHelper::SendThumbnail(buffer, mTabId, NS_SUCCEEDED(rv), shouldStore);
        return rv;
    }
private:
    nsCOMPtr<nsIAndroidBrowserApp> mBrowserApp;
    nsTArray<nsIntPoint> mPoints;
    int mTabId;
    nsRefPtr<RefCountedJavaObject> mBuffer;
};

class WakeLockListener final : public nsIDOMMozWakeLockListener {
private:
  ~WakeLockListener() {}

public:
  NS_DECL_ISUPPORTS;

  nsresult Callback(const nsAString& topic, const nsAString& state) override {
    widget::GeckoAppShell::NotifyWakeLockChanged(topic, state);
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(WakeLockListener, nsIDOMMozWakeLockListener)
nsCOMPtr<nsIPowerManagerService> sPowerManagerService = nullptr;
StaticRefPtr<WakeLockListener> sWakeLockListener;

namespace {

already_AddRefed<nsIURI>
ResolveURI(const nsCString& uriStr)
{
    nsCOMPtr<nsIIOService> ioServ = do_GetIOService();
    nsCOMPtr<nsIURI> uri;

    if (NS_SUCCEEDED(ioServ->NewURI(uriStr, nullptr,
                                    nullptr, getter_AddRefs(uri)))) {
        return uri.forget();
    }

    nsCOMPtr<nsIURIFixup> fixup = do_GetService(NS_URIFIXUP_CONTRACTID);
    if (fixup && NS_SUCCEEDED(
            fixup->CreateFixupURI(uriStr, 0, nullptr, getter_AddRefs(uri)))) {
        return uri.forget();
    }
    return nullptr;
}

} // namespace

class GeckoThreadNatives final
    : public widget::GeckoThread::Natives<GeckoThreadNatives>
{
public:
    static void SpeculativeConnect(jni::String::Param uriStr)
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

        nsCOMPtr<nsIURI> uri = ResolveURI(nsCString(uriStr));
        if (!uri) {
            return;
        }
        specConn->SpeculativeConnect(uri, nullptr);
    }
};

nsAppShell::nsAppShell()
    : mQueuedViewportEvent(nullptr)
{
    gAppShell = this;

    if (!XRE_IsParentProcess()) {
        return;
    }

    if (jni::IsAvailable()) {
        // Initialize JNI and Set the corresponding state in GeckoThread.
        AndroidBridge::ConstructBridge();
        GeckoThreadNatives::Init();
        mozilla::ANRReporter::Init();
        nsWindow::InitNatives();

        widget::GeckoThread::SetState(widget::GeckoThread::State::JNI_READY());
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
    while (mEventQueue.Pop(/* mayWait */ false)) {
        NS_WARNING("Discarded event on shutdown");
    }

    gAppShell = nullptr;

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
    if (!gWidgetLog)
        gWidgetLog = PR_NewLogModule("Widget");

    nsresult rv = nsBaseAppShell::Init();
    nsCOMPtr<nsIObserverService> obsServ =
        mozilla::services::GetObserverService();
    if (obsServ) {
        obsServ->AddObserver(this, "browser-delayed-startup-finished", false);
        obsServ->AddObserver(this, "profile-after-change", false);
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
    if (!strcmp(aTopic, "xpcom-shutdown")) {
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
            widget::GeckoThread::SetState(
                    widget::GeckoThread::State::PROFILE_READY());

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
        nsCOMPtr<nsIObserverService> obsServ =
            mozilla::services::GetObserverService();
        if (obsServ) {
            obsServ->RemoveObserver(this, "profile-after-change");
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

class nsAppShell::LegacyGeckoEvent : public Event
{
    mozilla::UniquePtr<AndroidGeckoEvent> ae;

public:
    LegacyGeckoEvent(AndroidGeckoEvent* e) : ae(e) {}

    void Run() override;
    void PostTo(mozilla::LinkedList<Event>& queue) override;

    mozilla::HangMonitor::ActivityType ActivityType() const
    {
        return ae->IsInputEvent() ? mozilla::HangMonitor::kUIActivity
                                  : mozilla::HangMonitor::kGeneralActivity;
    }
};

void
nsAppShell::PostEvent(AndroidGeckoEvent* event)
{
    mEventQueue.Post(mozilla::MakeUnique<LegacyGeckoEvent>(event));
}

void
nsAppShell::LegacyGeckoEvent::Run()
{
    const mozilla::UniquePtr<AndroidGeckoEvent>& curEvent = ae;

    EVLOG("nsAppShell: event %p %d", (void*)curEvent.get(), curEvent->Type());

    switch (curEvent->Type()) {
    case AndroidGeckoEvent::NATIVE_POKE:
        nsAppShell::gAppShell->NativeEventCallback();
        break;

    case AndroidGeckoEvent::SENSOR_EVENT: {
        nsAutoTArray<float, 4> values;
        mozilla::hal::SensorType type = (mozilla::hal::SensorType) curEvent->Flags();

        switch (type) {
          // Bug 938035, transfer HAL data for orientation sensor to meet w3c
          // spec, ex: HAL report alpha=90 means East but alpha=90 means West
          // in w3c spec
          case hal::SENSOR_ORIENTATION:
            values.AppendElement(360 -curEvent->X());
            values.AppendElement(-curEvent->Y());
            values.AppendElement(-curEvent->Z());
            break;
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

        case hal::SENSOR_ROTATION_VECTOR:
        case hal::SENSOR_GAME_ROTATION_VECTOR:
            values.AppendElement(curEvent->X());
            values.AppendElement(curEvent->Y());
            values.AppendElement(curEvent->Z());
            values.AppendElement(curEvent->W());
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

    case AndroidGeckoEvent::PROCESS_OBJECT: {

      switch (curEvent->Action()) {
      case AndroidGeckoEvent::ACTION_OBJECT_LAYER_CLIENT:
        AndroidBridge::Bridge()->SetLayerClient(
                widget::GeckoLayerClient::Ref::From(curEvent->Object().wrappedObject()));
        break;
      }
      break;
    }

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

    case AndroidGeckoEvent::APP_BACKGROUNDING: {
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
            // reset the crash loop state
            nsCOMPtr<nsIPrefBranch> prefBranch;
            prefs->GetBranch("browser.sessionstore.", getter_AddRefs(prefBranch));
            if (prefBranch)
                prefBranch->SetIntPref("recent_crashes", 0);

            prefs->SavePrefFile(nullptr);
        }
        break;
    }

    case AndroidGeckoEvent::APP_FOREGROUNDING: {
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
        break;
    }

    case AndroidGeckoEvent::THUMBNAIL: {
        if (!nsAppShell::gAppShell->mBrowserApp)
            break;

        int32_t tabId = curEvent->MetaState();
        const nsTArray<nsIntPoint>& points = curEvent->Points();
        RefCountedJavaObject* buffer = curEvent->ByteBuffer();
        nsRefPtr<ThumbnailRunnable> sr = new ThumbnailRunnable(nsAppShell::gAppShell->mBrowserApp, tabId, points, buffer);
        MessageLoop::current()->PostIdleTask(FROM_HERE, NewRunnableMethod(sr.get(), &ThumbnailRunnable::Run));
        break;
    }

    case AndroidGeckoEvent::ZOOMEDVIEW: {
        if (!nsAppShell::gAppShell->mBrowserApp)
            break;
        int32_t tabId = curEvent->MetaState();
        const nsTArray<nsIntPoint>& points = curEvent->Points();
        float scaleFactor = (float) curEvent->X();
        nsRefPtr<RefCountedJavaObject> javaBuffer = curEvent->ByteBuffer();
        const auto& mBuffer = jni::Object::Ref::From(javaBuffer->GetObject());

        nsCOMPtr<nsIDOMWindow> domWindow;
        nsCOMPtr<nsIBrowserTab> tab;
        nsAppShell::gAppShell->mBrowserApp->GetBrowserTab(tabId, getter_AddRefs(tab));
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

    case AndroidGeckoEvent::VIEWPORT:
    case AndroidGeckoEvent::BROADCAST: {
        {
            MonitorAutoLock lock(nsAppShell::gAppShell->mEventQueue.mMonitor);
            nsAppShell::gAppShell->mQueuedViewportEvent = nullptr;
        }

        if (curEvent->Characters().Length() == 0)
            break;

        nsCOMPtr<nsIObserverService> obsServ =
            mozilla::services::GetObserverService();

        const NS_ConvertUTF16toUTF8 topic(curEvent->Characters());
        const nsPromiseFlatString& data = PromiseFlatString(curEvent->CharactersExtra());

        obsServ->NotifyObservers(nullptr, topic.get(), data.get());
        break;
    }

    case AndroidGeckoEvent::TELEMETRY_UI_SESSION_STOP: {
        if (curEvent->Characters().Length() == 0)
            break;

        nsCOMPtr<nsIUITelemetryObserver> obs;
        nsAppShell::gAppShell->mBrowserApp->GetUITelemetryObserver(getter_AddRefs(obs));
        if (!obs)
            break;

        obs->StopSession(
                nsString(curEvent->Characters()).get(),
                nsString(curEvent->CharactersExtra()).get(),
                curEvent->Time()
                );
        break;
    }

    case AndroidGeckoEvent::TELEMETRY_UI_SESSION_START: {
        if (curEvent->Characters().Length() == 0)
            break;

        nsCOMPtr<nsIUITelemetryObserver> obs;
        nsAppShell::gAppShell->mBrowserApp->GetUITelemetryObserver(getter_AddRefs(obs));
        if (!obs)
            break;

        obs->StartSession(
                nsString(curEvent->Characters()).get(),
                curEvent->Time()
                );
        break;
    }

    case AndroidGeckoEvent::TELEMETRY_UI_EVENT: {
        if (curEvent->Data().Length() == 0)
            break;

        nsCOMPtr<nsIUITelemetryObserver> obs;
        nsAppShell::gAppShell->mBrowserApp->GetUITelemetryObserver(getter_AddRefs(obs));
        if (!obs)
            break;

        obs->AddEvent(
                nsString(curEvent->Data()).get(),
                nsString(curEvent->Characters()).get(),
                curEvent->Time(),
                nsString(curEvent->CharactersExtra()).get()
                );
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
        nsresult rv = cmdline->Init(4, argv, nullptr, nsICommandLine::STATE_REMOTE_AUTO);
        if (NS_SUCCEEDED(rv))
            cmdline->Run();
        free(uri);
        if (flag)
            free(flag);
        break;
    }

    case AndroidGeckoEvent::SIZE_CHANGED: {
        // store the last resize event to dispatch it to new windows with a FORCED_RESIZE event
        if (curEvent.get() != gLastSizeChange) {
            gLastSizeChange = AndroidGeckoEvent::CopyResizeEvent(curEvent.get());
        }
        nsWindow::OnGlobalAndroidEvent(curEvent.get());
        break;
    }

    case AndroidGeckoEvent::VISITED: {
#ifdef MOZ_ANDROID_HISTORY
        nsCOMPtr<IHistory> history = services::GetHistoryService();
        nsCOMPtr<nsIURI> visitedURI;
        if (history &&
            NS_SUCCEEDED(NS_NewURI(getter_AddRefs(visitedURI),
                                   nsString(curEvent->Characters())))) {
            history->NotifyVisited(visitedURI);
        }
#endif
        break;
    }

    case AndroidGeckoEvent::NETWORK_CHANGED: {
        hal::NotifyNetworkChange(hal::NetworkInformation(curEvent->ConnectionType(),
                                                         curEvent->IsWifi(),
                                                         curEvent->DHCPGateway()));
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
        int32_t colorDepth, pixelDepth;
        int16_t angle;
        dom::ScreenOrientationInternal orientation;
        nsCOMPtr<nsIScreen> screen;

        screenMgr->GetPrimaryScreen(getter_AddRefs(screen));
        screen->GetRect(&rect.x, &rect.y, &rect.width, &rect.height);
        screen->GetColorDepth(&colorDepth);
        screen->GetPixelDepth(&pixelDepth);
        orientation =
            static_cast<dom::ScreenOrientationInternal>(curEvent->ScreenOrientation());
        angle = curEvent->ScreenAngle();

        hal::NotifyScreenConfigurationChange(
            hal::ScreenConfiguration(rect, orientation, angle, colorDepth, pixelDepth));
        break;
    }

    case AndroidGeckoEvent::CALL_OBSERVER:
    {
        nsCOMPtr<nsIObserver> observer;
        nsAppShell::gAppShell->mObserversHash.Get(curEvent->Characters(), getter_AddRefs(observer));

        if (observer) {
            observer->Observe(nullptr, NS_ConvertUTF16toUTF8(curEvent->CharactersExtra()).get(),
                              nsString(curEvent->Data()).get());
        } else {
            ALOG("Call_Observer event: Observer was not found!");
        }

        break;
    }

    case AndroidGeckoEvent::REMOVE_OBSERVER:
        nsAppShell::gAppShell->mObserversHash.Remove(curEvent->Characters());
        break;

    case AndroidGeckoEvent::ADD_OBSERVER:
        nsAppShell::gAppShell->AddObserver(curEvent->Characters(), curEvent->Observer());
        break;

    case AndroidGeckoEvent::LOW_MEMORY:
        // TODO hook in memory-reduction stuff for different levels here
        if (curEvent->MetaState() >= AndroidGeckoEvent::MEMORY_PRESSURE_MEDIUM) {
            nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
            if (os) {
                os->NotifyObservers(nullptr,
                                    "memory-pressure",
                                    MOZ_UTF16("low-memory"));
            }
        }
        break;

    case AndroidGeckoEvent::NETWORK_LINK_CHANGE:
    {
        nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
        if (os) {
            os->NotifyObservers(nullptr,
                                NS_NETWORK_LINK_TOPIC,
                                nsString(curEvent->Characters()).get());
        }
        break;
    }

    case AndroidGeckoEvent::TELEMETRY_HISTOGRAM_ADD:
        Telemetry::Accumulate(NS_ConvertUTF16toUTF8(curEvent->Characters()).get(),
                              curEvent->Count());
        break;

    case AndroidGeckoEvent::GAMEPAD_ADDREMOVE: {
#ifdef MOZ_GAMEPAD
            if (curEvent->Action() == AndroidGeckoEvent::ACTION_GAMEPAD_ADDED) {
            int svc_id = dom::GamepadFunctions::AddGamepad("android",
                                                           dom::GamepadMappingType::Standard,
                                                           dom::kStandardGamepadButtons,
                                                           dom::kStandardGamepadAxes);
                widget::GeckoAppShell::GamepadAdded(curEvent->ID(),
                                                    svc_id);
            } else if (curEvent->Action() == AndroidGeckoEvent::ACTION_GAMEPAD_REMOVED) {
            dom::GamepadFunctions::RemoveGamepad(curEvent->ID());
        }
#endif
        break;
    }

    case AndroidGeckoEvent::GAMEPAD_DATA: {
#ifdef MOZ_GAMEPAD
            int id = curEvent->ID();
            if (curEvent->Action() == AndroidGeckoEvent::ACTION_GAMEPAD_BUTTON) {
            dom::GamepadFunctions::NewButtonEvent(id, curEvent->GamepadButton(),
                                     curEvent->GamepadButtonPressed(),
                                     curEvent->GamepadButtonValue());
            } else if (curEvent->Action() == AndroidGeckoEvent::ACTION_GAMEPAD_AXES) {
                int valid = curEvent->Flags();
                const nsTArray<float>& values = curEvent->GamepadValues();
                for (unsigned i = 0; i < values.Length(); i++) {
                    if (valid & (1<<i)) {
                    dom::GamepadFunctions::NewAxisMoveEvent(id, i, values[i]);
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

    if (curEvent->AckNeeded()) {
        widget::GeckoAppShell::AcknowledgeEvent();
    }

    EVLOG("nsAppShell: -- done event %p %d", (void*)curEvent.get(), curEvent->Type());
}

void
nsAppShell::LegacyGeckoEvent::PostTo(mozilla::LinkedList<Event>& queue)
{
    {
        // set this to true when inserting events that we can coalesce
        // viewport events across. this is effectively maintaining a whitelist
        // of events that are unaffected by viewport changes.
        bool allowCoalescingNextViewport = false;

        EVLOG("nsAppShell::PostEvent %p %d", ae, ae->Type());
        switch (ae->Type()) {
        case AndroidGeckoEvent::COMPOSITOR_CREATE:
        case AndroidGeckoEvent::COMPOSITOR_PAUSE:
        case AndroidGeckoEvent::COMPOSITOR_RESUME:
            // Give priority to these events, but maintain their order wrt each other.
            {
                Event* event = queue.getFirst();
                while (event && event->HasSameTypeAs(this)) {
                    const auto& e = static_cast<LegacyGeckoEvent*>(event)->ae;
                    if (e->Type() != AndroidGeckoEvent::COMPOSITOR_CREATE
                            && e->Type() != AndroidGeckoEvent::COMPOSITOR_PAUSE
                            && e->Type() != AndroidGeckoEvent::COMPOSITOR_RESUME) {
                        break;
                    }
                    event = event->getNext();
                }
                if (event) {
                    event->setPrevious(this);
                } else {
                    queue.insertFront(this);
                }
                EVLOG("nsAppShell: Inserting compositor event %d to maintain priority order", ae->Type());
            }
            break;

        case AndroidGeckoEvent::VIEWPORT:
            if (nsAppShell::gAppShell->mQueuedViewportEvent) {
                // drop the previous viewport event now that we have a new one
                EVLOG("nsAppShell: Dropping old viewport event at %p in favour of new VIEWPORT event %p",
                      nsAppShell::gAppShell->mQueuedViewportEvent, ae);
                // Delete the event and remove from list.
                delete nsAppShell::gAppShell->mQueuedViewportEvent;
            }
            nsAppShell::gAppShell->mQueuedViewportEvent = this;
            allowCoalescingNextViewport = true;

            queue.insertBack(this);
            break;

        case AndroidGeckoEvent::MOTION_EVENT:
        case AndroidGeckoEvent::APZ_INPUT_EVENT:
            if (nsAppShell::gAppShell->mAllowCoalescingTouches) {
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

        case AndroidGeckoEvent::NATIVE_POKE:
            allowCoalescingNextViewport = true;
            // fall through

        default:
            queue.insertBack(this);
            break;
        }

        // if the event wasn't on our whitelist then reset mQueuedViewportEvent
        // so that we don't coalesce future viewport events into the last viewport
        // event we added
        if (!allowCoalescingNextViewport) {
            nsAppShell::gAppShell->mQueuedViewportEvent = nullptr;
        }
    }
}

void
nsAppShell::ResendLastResizeEvent(nsWindow* aDest) {
    if (gLastSizeChange) {
        nsWindow::OnGlobalAndroidEvent(gLastSizeChange);
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
    if (!nsAppShell::gAppShell) {
        return false;
    }

    return nsAppShell::gAppShell->ProcessNextNativeEvent(true) ? true : false;
}

void NotifyEvent()
{
    if (nsAppShell::gAppShell) {
        nsAppShell::gAppShell->NotifyNativeEvent();
    }
}

}
