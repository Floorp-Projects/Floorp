/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAppShell.h"

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "mozilla/Hal.h"
#include "nsExceptionHandler.h"
#include "nsIScreen.h"
#include "nsWindow.h"
#include "nsThreadUtils.h"
#include "nsIObserverService.h"
#include "nsIAppStartup.h"
#include "nsIGeolocationProvider.h"
#include "nsIDOMWakeLockListener.h"
#include "nsIPowerManagerService.h"
#include "nsISpeculativeConnect.h"
#include "nsIURIFixup.h"
#include "nsCategoryManagerUtils.h"
#include "mozilla/dom/GeolocationPosition.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/Components.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/Hal.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/Document.h"
#include "mozilla/intl/OSPreferences.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/java/GeckoAppShellNatives.h"
#include "mozilla/java/GeckoThreadNatives.h"
#include "mozilla/java/XPCOMEventTargetNatives.h"
#include "mozilla/widget/ScreenManager.h"
#include "prenv.h"

#include "AndroidBridge.h"
#include "AndroidBridgeUtilities.h"
#include "AndroidSurfaceTexture.h"
#include <android/log.h>
#include <pthread.h>
#include <wchar.h>

#ifdef MOZ_ANDROID_HISTORY
#  include "nsNetUtil.h"
#  include "nsIURI.h"
#  include "IHistory.h"
#endif

#ifdef MOZ_LOGGING
#  include "mozilla/Logging.h"
#endif

#include "AndroidAlerts.h"
#include "AndroidUiThread.h"
#include "GeckoBatteryManager.h"
#include "GeckoEditableSupport.h"
#include "GeckoNetworkManager.h"
#include "GeckoProcessManager.h"
#include "GeckoScreenOrientation.h"
#include "GeckoSystemStateListener.h"
#include "GeckoTelemetryDelegate.h"
#include "GeckoVRManager.h"
#include "ImageDecoderSupport.h"
#include "PrefsHelper.h"
#include "ScreenHelperAndroid.h"
#include "Telemetry.h"
#include "WebExecutorSupport.h"
#include "Base64UtilsSupport.h"
#include "WebAuthnTokenManager.h"

#ifdef DEBUG_ANDROID_EVENTS
#  define EVLOG(args...) ALOG(args)
#else
#  define EVLOG(args...) \
    do {                 \
    } while (0)
#endif

using namespace mozilla;
using namespace mozilla::widget;

nsIGeolocationUpdate* gLocationCallback = nullptr;

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
    : public java::GeckoThread::Natives<GeckoThreadSupport> {
  // When this number goes above 0, the app is paused. When less than or
  // equal to zero, the app is resumed.
  static int32_t sPauseCount;

 public:
  static void SpeculativeConnect(jni::String::Param aUriStr) {
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
    nsCOMPtr<nsIPrincipal> principal =
        BasePrincipal::CreateContentPrincipal(uri, attrs);
    specConn->SpeculativeConnect(uri, principal, nullptr);
  }

  static void OnPause() {
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

    // We really want to send a notification like profile-before-change,
    // but profile-before-change ends up shutting some things down instead
    // of flushing data
    Preferences* prefs = static_cast<Preferences*>(Preferences::GetService());
    if (prefs) {
      // Force a main thread blocking save
      prefs->SavePrefFileBlocking();
    }
  }

  static void OnResume() {
    MOZ_ASSERT(NS_IsMainThread());

    sPauseCount--;
    // If sPauseCount is now 0, we just crossed the threshold from "paused"
    // to "resumed", so we should notify observers and so on.
    if (sPauseCount != 0) {
      return;
    }

    // We didn't return from one of our own activities, so restore
    // to foreground status
    nsCOMPtr<nsIObserverService> obsServ =
        mozilla::services::GetObserverService();
    obsServ->NotifyObservers(nullptr, "application-foreground", nullptr);
  }

  static void CreateServices(jni::String::Param aCategory,
                             jni::String::Param aData) {
    MOZ_ASSERT(NS_IsMainThread());

    nsCString category(aCategory->ToCString());

    NS_CreateServicesFromCategory(category.get(),
                                  nullptr,  // aOrigin
                                  category.get(),
                                  aData ? aData->ToString().get() : nullptr);
  }

  static int64_t RunUiThreadCallback() { return RunAndroidUiTasks(); }

  static void ForceQuit() {
    nsCOMPtr<nsIAppStartup> appStartup = components::AppStartup::Service();

    if (appStartup) {
      bool userAllowedQuit = true;
      appStartup->Quit(nsIAppStartup::eForceQuit, 0, &userAllowedQuit);
    }
  }

  static void Crash() {
    printf_stderr("Intentionally crashing...\n");
    MOZ_CRASH("intentional crash");
  }
};

int32_t GeckoThreadSupport::sPauseCount;

class GeckoAppShellSupport final
    : public java::GeckoAppShell::Natives<GeckoAppShellSupport> {
 public:
  static void ReportJavaCrash(const jni::Class::LocalRef& aCls,
                              jni::Throwable::Param aException,
                              jni::String::Param aStack) {
    if (!jni::ReportException(aCls.Env(), aException.Get(), aStack.Get())) {
      // Only crash below if crash reporter is initialized and annotation
      // succeeded. Otherwise try other means of reporting the crash in
      // Java.
      return;
    }

    MOZ_CRASH("Uncaught Java exception");
  }

  static void NotifyObservers(jni::String::Param aTopic,
                              jni::String::Param aData) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aTopic);

    nsCOMPtr<nsIObserverService> obsServ = services::GetObserverService();
    if (!obsServ) {
      return;
    }

    obsServ->NotifyObservers(nullptr, aTopic->ToCString().get(),
                             aData ? aData->ToString().get() : nullptr);
  }

  static void AppendAppNotesToCrashReport(jni::String::Param aNotes) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aNotes);
    CrashReporter::AppendAppNotesToCrashReport(aNotes->ToCString());
  }

  static void OnSensorChanged(int32_t aType, float aX, float aY, float aZ,
                              float aW, int64_t aTime) {
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

    hal::SensorData sdata(hal::SensorType(aType), aTime, values);
    hal::NotifySensorChange(sdata);
  }

  static void OnLocationChanged(double aLatitude, double aLongitude,
                                double aAltitude, float aAccuracy,
                                float aAltitudeAccuracy, float aHeading,
                                float aSpeed, int64_t aTime) {
    if (!gLocationCallback) {
      return;
    }

    RefPtr<nsIDOMGeoPosition> geoPosition(
        new nsGeoPosition(aLatitude, aLongitude, aAltitude, aAccuracy,
                          aAltitudeAccuracy, aHeading, aSpeed, aTime));
    gLocationCallback->Update(geoPosition);
  }

  static void NotifyAlertListener(jni::String::Param aName,
                                  jni::String::Param aTopic,
                                  jni::String::Param aCookie) {
    if (!aName || !aTopic || !aCookie) {
      return;
    }

    widget::AndroidAlerts::NotifyListener(aName->ToString(),
                                          aTopic->ToCString().get(),
                                          aCookie->ToString().get());
  }
};

class XPCOMEventTargetWrapper final
    : public java::XPCOMEventTarget::Natives<XPCOMEventTargetWrapper> {
 public:
  // Wraps a java runnable into an XPCOM runnable and dispatches it to mTarget.
  void DispatchNative(mozilla::jni::Object::Param aJavaRunnable) {
    if (AppShutdown::GetCurrentShutdownPhase() >=
        ShutdownPhase::XPCOMShutdownThreads) {
      // No point in trying to dispatch this if we're already shutting down.
      return;
    }
    java::XPCOMEventTarget::JNIRunnable::GlobalRef r =
        java::XPCOMEventTarget::JNIRunnable::Ref::From(aJavaRunnable);
    mTarget->Dispatch(NS_NewRunnableFunction(
        "XPCOMEventTargetWrapper::DispatchNative",
        [runnable = std::move(r)]() { runnable->Run(); }));
  }

  bool IsOnCurrentThread() { return mTarget->IsOnCurrentThread(); }

  static void Init() {
    java::XPCOMEventTarget::Natives<XPCOMEventTargetWrapper>::Init();
    CreateWrapper(u"main"_ns, do_GetMainThread());
    if (XRE_IsParentProcess()) {
      CreateWrapper(u"launcher"_ns, ipc::GetIPCLauncher());
    }
  }

  static void CreateWrapper(mozilla::jni::String::Param aName,
                            nsCOMPtr<nsIEventTarget> aTarget) {
    auto java = java::XPCOMEventTarget::New();
    auto native = MakeUnique<XPCOMEventTargetWrapper>(aTarget.forget());
    AttachNative(java, std::move(native));

    java::XPCOMEventTarget::SetTarget(aName, java);
  }

  static void ResolveAndDispatchNative(mozilla::jni::String::Param aName,
                                       mozilla::jni::Object::Param aRunnable) {
    java::XPCOMEventTarget::ResolveAndDispatch(aName, aRunnable);
  }

  explicit XPCOMEventTargetWrapper(already_AddRefed<nsIEventTarget> aTarget)
      : mTarget(aTarget) {}

 private:
  nsCOMPtr<nsIEventTarget> mTarget;
};

nsAppShell::nsAppShell()
    : mSyncRunFinished(*(sAppShellLock = new Mutex("nsAppShell")),
                       "nsAppShell.SyncRun"),
      mSyncRunQuit(false) {
  {
    MutexAutoLock lock(*sAppShellLock);
    sAppShell = this;
  }

  hal::Init();

  if (!XRE_IsParentProcess()) {
    if (jni::IsAvailable()) {
      GeckoThreadSupport::Init();
      GeckoAppShellSupport::Init();
      XPCOMEventTargetWrapper::Init();
      mozilla::widget::Telemetry::Init();
      mozilla::widget::GeckoTelemetryDelegate::Init();

      // Set the corresponding state in GeckoThread.
      java::GeckoThread::SetState(java::GeckoThread::State::RUNNING());
    }
    return;
  }

  if (jni::IsAvailable()) {
    ScreenManager& screenManager = ScreenManager::GetSingleton();
    screenManager.SetHelper(mozilla::MakeUnique<ScreenHelperAndroid>());

    // Initialize JNI and Set the corresponding state in GeckoThread.
    AndroidBridge::ConstructBridge();
    GeckoAppShellSupport::Init();
    GeckoThreadSupport::Init();
    XPCOMEventTargetWrapper::Init();
    mozilla::GeckoBatteryManager::Init();
    mozilla::GeckoNetworkManager::Init();
    mozilla::GeckoProcessManager::Init();
    mozilla::GeckoScreenOrientation::Init();
    mozilla::GeckoSystemStateListener::Init();
    mozilla::PrefsHelper::Init();
    mozilla::widget::Telemetry::Init();
    mozilla::widget::ImageDecoderSupport::Init();
    mozilla::widget::WebExecutorSupport::Init();
    mozilla::widget::Base64UtilsSupport::Init();
    nsWindow::InitNatives();
    mozilla::gl::AndroidSurfaceTexture::Init();
    mozilla::WebAuthnTokenManager::Init();
    mozilla::widget::GeckoTelemetryDelegate::Init();

    java::GeckoThread::SetState(java::GeckoThread::State::JNI_READY());

    CreateAndroidUiThread();
  }

  sPowerManagerService = do_GetService(POWERMANAGERSERVICE_CONTRACTID);

  if (sPowerManagerService) {
    sWakeLockListener = new WakeLockListener();
  } else {
    NS_WARNING(
        "Failed to retrieve PowerManagerService, wakelocks will be broken!");
  }
}

nsAppShell::~nsAppShell() {
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

  hal::Shutdown();

  if (jni::IsAvailable() && XRE_IsParentProcess()) {
    DestroyAndroidUiThread();
    AndroidBridge::DeconstructBridge();
  }
}

void nsAppShell::NotifyNativeEvent() { mEventQueue.Signal(); }

nsresult nsAppShell::Init() {
  nsresult rv = nsBaseAppShell::Init();
  nsCOMPtr<nsIObserverService> obsServ =
      mozilla::services::GetObserverService();
  if (obsServ) {
    obsServ->AddObserver(this, "browser-delayed-startup-finished", false);
    obsServ->AddObserver(this, "geckoview-startup-complete", false);
    obsServ->AddObserver(this, "profile-after-change", false);
    obsServ->AddObserver(this, "quit-application", false);
    obsServ->AddObserver(this, "quit-application-granted", false);

    if (XRE_IsParentProcess()) {
      obsServ->AddObserver(this, "chrome-document-loaded", false);
    } else {
      obsServ->AddObserver(this, "content-document-global-created", false);
      obsServ->AddObserver(this, "geckoview-content-global-transferred", false);
    }
  }

  if (sPowerManagerService)
    sPowerManagerService->AddWakeLockListener(sWakeLockListener);

  return rv;
}

NS_IMETHODIMP
nsAppShell::Exit(void) {
  {
    // Release any thread waiting for a sync call to finish.
    mozilla::MutexAutoLock shellLock(*sAppShellLock);
    mSyncRunQuit = true;
    mSyncRunFinished.NotifyAll();
  }
  // We need to ensure no observers stick around after XPCOM shuts down
  // or we'll see crashes, as the app shell outlives XPConnect.
  mObserversHash.Clear();
  return nsBaseAppShell::Exit();
}

NS_IMETHODIMP
nsAppShell::Observe(nsISupports* aSubject, const char* aTopic,
                    const char16_t* aData) {
  bool removeObserver = false;

  if (!strcmp(aTopic, "browser-delayed-startup-finished")) {
    NS_CreateServicesFromCategory("browser-delayed-startup-finished", nullptr,
                                  "browser-delayed-startup-finished");
  } else if (!strcmp(aTopic, "geckoview-startup-complete")) {
    if (jni::IsAvailable()) {
      java::GeckoThread::CheckAndSetState(
          java::GeckoThread::State::PROFILE_READY(),
          java::GeckoThread::State::RUNNING());
    }
  } else if (!strcmp(aTopic, "profile-after-change")) {
    if (jni::IsAvailable()) {
      java::GeckoThread::SetState(java::GeckoThread::State::PROFILE_READY());

      // Gecko on Android follows the Android app model where it never
      // stops until it is killed by the system or told explicitly to
      // quit. Therefore, we should *not* exit Gecko when there is no
      // window or the last window is closed. nsIAppStartup::Quit will
      // still force Gecko to exit.
      nsCOMPtr<nsIAppStartup> appStartup = components::AppStartup::Service();
      if (appStartup) {
        appStartup->EnterLastWindowClosingSurvivalArea();
      }
    }
    removeObserver = true;

  } else if (!strcmp(aTopic, "chrome-document-loaded")) {
    // Set the global ready state and enable the window event dispatcher
    // for this particular GeckoView.
    nsCOMPtr<dom::Document> doc = do_QueryInterface(aSubject);
    MOZ_ASSERT(doc);
    if (const RefPtr<nsWindow> window = nsWindow::From(doc->GetWindow())) {
      window->OnGeckoViewReady();
    }
  } else if (!strcmp(aTopic, "quit-application")) {
    if (jni::IsAvailable()) {
      const bool restarting = aData && u"restart"_ns.Equals(aData);
      java::GeckoThread::SetState(restarting
                                      ? java::GeckoThread::State::RESTARTING()
                                      : java::GeckoThread::State::EXITING());
    }
    removeObserver = true;

  } else if (!strcmp(aTopic, "quit-application-granted")) {
    if (jni::IsAvailable()) {
      // We are told explicitly to quit, perhaps due to
      // nsIAppStartup::Quit being called. We should release our hold on
      // nsIAppStartup and let it continue to quit.
      nsCOMPtr<nsIAppStartup> appStartup = components::AppStartup::Service();
      if (appStartup) {
        appStartup->ExitLastWindowClosingSurvivalArea();
      }
    }
    removeObserver = true;

  } else if (!strcmp(aTopic, "nsPref:changed")) {
    if (jni::IsAvailable()) {
      mozilla::PrefsHelper::OnPrefChange(aData);
    }

  } else if (!strcmp(aTopic, "content-document-global-created")) {
    // Associate the PuppetWidget of the newly-created BrowserChild with a
    // GeckoEditableChild instance.
    MOZ_ASSERT(!XRE_IsParentProcess());

    nsCOMPtr<mozIDOMWindowProxy> domWindow = do_QueryInterface(aSubject);
    MOZ_ASSERT(domWindow);
    nsCOMPtr<nsIWidget> domWidget = widget::WidgetUtils::DOMWindowToWidget(
        nsPIDOMWindowOuter::From(domWindow));
    NS_ENSURE_TRUE(domWidget, NS_OK);

    widget::GeckoEditableSupport::SetOnBrowserChild(
        domWidget->GetOwningBrowserChild());

  } else if (!strcmp(aTopic, "geckoview-content-global-transferred")) {
    // We're transferring to a new GeckoEditableParent, so notify the
    // existing GeckoEditableChild instance associated with the docshell.
    nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(aSubject);
    widget::GeckoEditableSupport::SetOnBrowserChild(
        dom::BrowserChild::GetFrom(docShell));
  } else {
    return nsBaseAppShell::Observe(aSubject, aTopic, aData);
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

bool nsAppShell::ProcessNextNativeEvent(bool mayWait) {
  EVLOG("nsAppShell::ProcessNextNativeEvent %d", mayWait);

  AUTO_PROFILER_LABEL("nsAppShell::ProcessNextNativeEvent", OTHER);

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

      AUTO_PROFILER_LABEL("nsAppShell::ProcessNextNativeEvent:Wait", IDLE);
      mozilla::BackgroundHangMonitor().NotifyWait();

      curEvent = mEventQueue.Pop(/* mayWait */ true);
    }
  }

  if (!curEvent) return false;

  mozilla::BackgroundHangMonitor().NotifyActivity();

  curEvent->Run();
  return true;
}

bool nsAppShell::SyncRunEvent(
    Event&& event, UniquePtr<Event> (*eventFactory)(UniquePtr<Event>&&),
    const TimeDuration timeout) {
  // Perform the call on the Gecko thread in a separate lambda, and wait
  // on the monitor on the current thread.
  MOZ_ASSERT(!NS_IsMainThread());

  // This is the lock to check that app shell is still alive,
  // and to wait on for the sync call to complete.
  mozilla::MutexAutoLock shellLock(*sAppShellLock);
  nsAppShell* const appShell = sAppShell;

  if (MOZ_UNLIKELY(!appShell)) {
    // Post-shutdown.
    return false;
  }

  bool finished = false;
  auto runAndNotify = [&event, &finished] {
    nsAppShell* const appShell = nsAppShell::Get();
    if (MOZ_UNLIKELY(!appShell || appShell->mSyncRunQuit)) {
      return false;
    }
    event.Run();
    finished = true;
    mozilla::MutexAutoLock shellLock(*sAppShellLock);
    appShell->mSyncRunFinished.NotifyAll();
    return finished;
  };

  UniquePtr<Event> runAndNotifyEvent =
      mozilla::MakeUnique<LambdaEvent<decltype(runAndNotify)>>(
          std::move(runAndNotify));

  if (eventFactory) {
    runAndNotifyEvent = (*eventFactory)(std::move(runAndNotifyEvent));
  }

  appShell->mEventQueue.Post(std::move(runAndNotifyEvent));

  while (!finished && MOZ_LIKELY(sAppShell && !sAppShell->mSyncRunQuit)) {
    appShell->mSyncRunFinished.Wait(timeout);
  }

  return finished;
}

already_AddRefed<nsIURI> nsAppShell::ResolveURI(const nsCString& aUriStr) {
  nsCOMPtr<nsIIOService> ioServ = do_GetIOService();
  nsCOMPtr<nsIURI> uri;

  if (NS_SUCCEEDED(
          ioServ->NewURI(aUriStr, nullptr, nullptr, getter_AddRefs(uri)))) {
    return uri.forget();
  }

  nsCOMPtr<nsIURIFixup> fixup = components::URIFixup::Service();
  nsCOMPtr<nsIURIFixupInfo> fixupInfo;
  if (fixup &&
      NS_SUCCEEDED(fixup->GetFixupURIInfo(aUriStr, nsIURIFixup::FIXUP_FLAG_NONE,
                                          getter_AddRefs(fixupInfo))) &&
      NS_SUCCEEDED(fixupInfo->GetPreferredURI(getter_AddRefs(uri)))) {
    return uri.forget();
  }
  return nullptr;
}

nsresult nsAppShell::AddObserver(const nsAString& aObserverKey,
                                 nsIObserver* aObserver) {
  NS_ASSERTION(aObserver != nullptr,
               "nsAppShell::AddObserver: aObserver is null!");
  mObserversHash.InsertOrUpdate(aObserverKey, aObserver);
  return NS_OK;
}

// Used by IPC code
namespace mozilla {

bool ProcessNextEvent() {
  nsAppShell* const appShell = nsAppShell::Get();
  if (!appShell) {
    return false;
  }

  return appShell->ProcessNextNativeEvent(true) ? true : false;
}

void NotifyEvent() {
  nsAppShell* const appShell = nsAppShell::Get();
  if (!appShell) {
    return;
  }
  appShell->NotifyNativeEvent();
}

}  // namespace mozilla
