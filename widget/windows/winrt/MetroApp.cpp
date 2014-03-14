/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MetroApp.h"
#include "MetroWidget.h"
#include "mozilla/widget/AudioSession.h"
#include "nsIRunnable.h"
#include "MetroUtils.h"
#include "MetroAppShell.h"
#include "nsICommandLineRunner.h"
#include "FrameworkView.h"
#include "nsAppDirectoryServiceDefs.h"
#include "GeckoProfiler.h"
#include <shellapi.h>

using namespace ABI::Windows::ApplicationModel;
using namespace ABI::Windows::ApplicationModel::Core;
using namespace ABI::Windows::UI::Core;
using namespace ABI::Windows::System;
using namespace ABI::Windows::Foundation;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace mozilla::widget;

// Metro specific XRE methods we call from here on an
// appropriate thread.
extern nsresult XRE_metroStartup(bool runXREMain);
extern void XRE_metroShutdown();

static const char* gGeckoThreadName = "GeckoMain";

#ifdef PR_LOGGING
extern PRLogModuleInfo* gWindowsLog;
#endif

namespace mozilla {
namespace widget {
namespace winrt {

ComPtr<FrameworkView> sFrameworkView;
ComPtr<MetroApp> sMetroApp;
ComPtr<ICoreApplication> sCoreApp;
bool MetroApp::sGeckoShuttingDown = false;

////////////////////////////////////////////////////
// IFrameworkViewSource impl.

// Called after CoreApplication::Run(app)
HRESULT
MetroApp::CreateView(ABI::Windows::ApplicationModel::Core::IFrameworkView **aViewProvider)
{
  // This entry point is called on the metro main thread, but the thread won't
  // be recognized as such until after Run() is called below. XPCOM has not
  // gone through startup at this point.

  // Note that we create the view which creates our native window for us. The
  // gecko widget gets created by gecko, and the two get hooked up later in
  // MetroWidget::Create().

  LogFunction();

  sFrameworkView = Make<FrameworkView>(this);
  sFrameworkView.Get()->AddRef();
  *aViewProvider = sFrameworkView.Get();
  return !sFrameworkView ? E_FAIL : S_OK;
}

////////////////////////////////////////////////////
// MetroApp impl.

void
MetroApp::Run()
{
  LogThread();

  // Name this thread for debugging and register it with the profiler
  // as the main gecko thread.
  char aLocal;
  PR_SetCurrentThreadName(gGeckoThreadName);
  profiler_register_thread(gGeckoThreadName, &aLocal);

  HRESULT hr;
  hr = sCoreApp->add_Suspending(Callback<__FIEventHandler_1_Windows__CApplicationModel__CSuspendingEventArgs_t>(
    this, &MetroApp::OnSuspending).Get(), &mSuspendEvent);
  AssertHRESULT(hr);

  hr = sCoreApp->add_Resuming(Callback<__FIEventHandler_1_IInspectable_t>(
    this, &MetroApp::OnResuming).Get(), &mResumeEvent);
  AssertHRESULT(hr);

  WinUtils::Log("Calling XRE_metroStartup.");
  nsresult rv = XRE_metroStartup(true);
  WinUtils::Log("Exiting XRE_metroStartup.");
  if (NS_FAILED(rv)) {
    WinUtils::Log("XPCOM startup initialization failed, bailing. rv=%X", rv);
    CoreExit();
  }
}

// Free all xpcom related resources before calling the xre shutdown call.
// Must be called on the metro main thread. Currently called from appshell.
void
MetroApp::Shutdown()
{
  LogThread();

  if (sCoreApp) {
    sCoreApp->remove_Suspending(mSuspendEvent);
    sCoreApp->remove_Resuming(mResumeEvent);
  }

  if (sFrameworkView) {
    sFrameworkView->Shutdown();
  }

  MetroApp::sGeckoShuttingDown = true;

  // Shut down xpcom
  XRE_metroShutdown();

  // Unhook this thread from the profiler
  profiler_unregister_thread();
}

// Request a shutdown of the application
void
MetroApp::CoreExit()
{
  LogFunction();
  HRESULT hr;
  ComPtr<ICoreApplicationExit> coreExit;
  HStringReference className(RuntimeClass_Windows_ApplicationModel_Core_CoreApplication);
  hr = GetActivationFactory(className.Get(), coreExit.GetAddressOf());
  NS_ASSERTION(SUCCEEDED(hr), "Activation of ICoreApplicationExit");
  if (SUCCEEDED(hr)) {
    coreExit->Exit();
  }
}

void
MetroApp::ActivateBaseView()
{
  if (sFrameworkView) {
    sFrameworkView->ActivateView();
  }
}

/*
 * TBD: when we support multiple widgets, we'll need a way to sync up the view
 * created in CreateView with the widget gecko creates. Currently we only have
 * one view (sFrameworkView) and one main widget.
 */
void
MetroApp::SetWidget(MetroWidget* aPtr)
{
  LogThread();

  NS_ASSERTION(aPtr, "setting null base widget?");

  // Both of these calls AddRef the ptr we pass in
  aPtr->SetView(sFrameworkView.Get());
  sFrameworkView->SetWidget(aPtr);
}

////////////////////////////////////////////////////
// MetroApp events

HRESULT
MetroApp::OnSuspending(IInspectable* aSender, ISuspendingEventArgs* aArgs)
{
  LogThread();
  PostSuspendResumeProcessNotification(true);
  return S_OK;
}

HRESULT
MetroApp::OnResuming(IInspectable* aSender, IInspectable* aArgs)
{
  LogThread();
  PostSuspendResumeProcessNotification(false);
  return S_OK;
}

HRESULT
MetroApp::OnAsyncTileCreated(ABI::Windows::Foundation::IAsyncOperation<bool>* aOperation,
                             AsyncStatus aStatus)
{
  WinUtils::Log("Async operation status: %d", aStatus);
  MetroUtils::FireObserver("metro_on_async_tile_created");
  return S_OK;
}

// static
void
MetroApp::PostSuspendResumeProcessNotification(const bool aIsSuspend)
{
  static bool isSuspend = false;
  if (isSuspend == aIsSuspend) {
    return;
  }
  isSuspend = aIsSuspend;
  MetroUtils::FireObserver(aIsSuspend ? "suspend_process_notification" :
                                        "resume_process_notification");
}

// static
void
MetroApp::PostSleepWakeNotification(const bool aIsSleep)
{
  static bool isSleep = false;
  if (isSleep == aIsSleep) {
    return;
  }
  isSleep = aIsSleep;
  MetroUtils::FireObserver(aIsSleep ? "sleep_notification" :
                                      "wake_notification");
}

} } }


static bool
IsBackgroundSessionClosedStartup()
{
  int argc;
  LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  bool backgroundSessionClosed = argc > 1 && !wcsicmp(argv[1], L"-BackgroundSessionClosed");
  LocalFree(argv);
  return backgroundSessionClosed;
}

bool
XRE_MetroCoreApplicationRun()
{
  HRESULT hr;
  LogThread();

  using namespace mozilla::widget::winrt;

  sMetroApp = Make<MetroApp>();

  HStringReference className(RuntimeClass_Windows_ApplicationModel_Core_CoreApplication);
  hr = GetActivationFactory(className.Get(), sCoreApp.GetAddressOf());
  if (FAILED(hr)) {
    LogHRESULT(hr);
    return false;
  }

  // Perform any cleanup for unclean shutdowns here, such as when the background session
  // is closed via the appbar on the left when outside of Metro.  Windows restarts the
  // process solely for cleanup reasons.
  if (IsBackgroundSessionClosedStartup() && SUCCEEDED(XRE_metroStartup(false))) {

    // Whether or  not to use sessionstore depends on if the bak exists.  Since host process
    // shutdown isn't a crash we shouldn't restore sessionstore.
    nsCOMPtr<nsIFile> sessionBAK;
    if (NS_FAILED(NS_GetSpecialDirectory("ProfDS", getter_AddRefs(sessionBAK)))) {
      return false;
    }

    sessionBAK->AppendNative(nsDependentCString("sessionstore.bak"));
    bool exists;
    if (NS_SUCCEEDED(sessionBAK->Exists(&exists)) && exists) {
      sessionBAK->Remove(false);
    }
    return false;
  }

  hr = sCoreApp->Run(sMetroApp.Get());

  WinUtils::Log("Exiting CoreApplication::Run");

  sCoreApp = nullptr;
  sMetroApp = nullptr;

  return true;
}

