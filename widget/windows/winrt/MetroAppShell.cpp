/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MetroAppShell.h"
#include "nsXULAppAPI.h"
#include "mozilla/widget/AudioSession.h"
#include "MetroUtils.h"
#include "MetroApp.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "WinUtils.h"

using namespace mozilla::widget;
using namespace mozilla::widget::winrt;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::UI::Core;
using namespace ABI::Windows::Foundation;

namespace mozilla {
namespace widget {
namespace winrt {
extern ComPtr<MetroApp> sMetroApp;
} } }

const PRUnichar* kMetroAppShellEventId = L"nsAppShell:EventID";
static UINT sShellEventMsgID;
static ComPtr<ICoreWindowStatic> sCoreStatic;

MetroAppShell::~MetroAppShell()
{
  if (mEventWnd) {
    SendMessage(mEventWnd, WM_CLOSE, 0, 0);
  }
}

nsresult
MetroAppShell::Init()
{
  LogFunction();

  WNDCLASSW wc;
  HINSTANCE module = GetModuleHandle(NULL);
  sShellEventMsgID = RegisterWindowMessageW(kMetroAppShellEventId);

  const PRUnichar *const kWindowClass = L"nsAppShell:EventWindowClass";
  if (!GetClassInfoW(module, kWindowClass, &wc)) {
    wc.style         = 0;
    wc.lpfnWndProc   = EventWindowProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = module;
    wc.hIcon         = NULL;
    wc.hCursor       = NULL;
    wc.hbrBackground = (HBRUSH) NULL;
    wc.lpszMenuName  = (LPCWSTR) NULL;
    wc.lpszClassName = kWindowClass;
    RegisterClassW(&wc);
  }

  mEventWnd = CreateWindowW(kWindowClass, L"nsAppShell:EventWindow",
                           0, 0, 0, 10, 10, NULL, NULL, module, NULL);
  NS_ENSURE_STATE(mEventWnd);

  nsresult rv;
  nsCOMPtr<nsIObserverService> observerService = 
    do_GetService("@mozilla.org/observer-service;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    observerService->AddObserver(this, "dl-start", false);
    observerService->AddObserver(this, "dl-done", false);
    observerService->AddObserver(this, "dl-cancel", false);
    observerService->AddObserver(this, "dl-failed", false);
  }

  return nsBaseAppShell::Init();
}

// Called by appstartup->run in xre, which is initiated by a call to
// XRE_metroStartup in MetroApp. This call is on the metro main thread.
NS_IMETHODIMP
MetroAppShell::Run(void)
{
  LogFunction();
  nsresult rv = NS_OK;

  switch(XRE_GetProcessType()) {
    case  GeckoProcessType_Content:
    case GeckoProcessType_IPDLUnitTest:
      mozilla::widget::StartAudioSession();
      rv = nsBaseAppShell::Run();
      mozilla::widget::StopAudioSession();
    break;
    case  GeckoProcessType_Plugin:
      NS_WARNING("We don't support plugins currently.");
      // Just exit
      rv = NS_ERROR_NOT_IMPLEMENTED;
    break;
    case GeckoProcessType_Default:
      // Nothing to do, just return.
    break;
  }

  return rv;
}

static void
ProcessNativeEvents(CoreProcessEventsOption eventOption)
{
  HRESULT hr;
  if (!sCoreStatic) {
    hr = GetActivationFactory(HStringReference(L"Windows.UI.Core.CoreWindow").Get(), sCoreStatic.GetAddressOf());
    NS_ASSERTION(SUCCEEDED(hr), "GetActivationFactory failed?");
    AssertHRESULT(hr);
  }

  ComPtr<ICoreWindow> window;
  AssertHRESULT(sCoreStatic->GetForCurrentThread(window.GetAddressOf()));
  ComPtr<ICoreDispatcher> dispatcher;
  hr = window->get_Dispatcher(&dispatcher);
  NS_ASSERTION(SUCCEEDED(hr), "get_Dispatcher failed?");
  AssertHRESULT(hr);
  dispatcher->ProcessEvents(eventOption);
}

void
MetroAppShell::ProcessOneNativeEventIfPresent()
{
  ProcessNativeEvents(CoreProcessEventsOption::CoreProcessEventsOption_ProcessOneIfPresent);
}

void
MetroAppShell::ProcessAllNativeEventsPresent()
{
  ProcessNativeEvents(CoreProcessEventsOption::CoreProcessEventsOption_ProcessAllIfPresent);
}

bool
MetroAppShell::ProcessNextNativeEvent(bool mayWait)
{
  MSG msg;

  if (mayWait) {
    if (!WinUtils::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
      WinUtils::WaitForMessage();
    }
    ProcessOneNativeEventIfPresent();
    return true;
  }

  if (WinUtils::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
    ProcessOneNativeEventIfPresent();
    return true;
  }

  return false;
}

// Results from a call to appstartup->quit, which fires a final nsAppExitEvent
// event which calls us here. This is on the metro main thread. We want to
// call xpcom shutdown here, but we need to wait until the runnable that fires
// this is off the stack. See NativeEventCallback below.
NS_IMETHODIMP
MetroAppShell::Exit(void)
{
  LogFunction();
  mExiting = true;
  return NS_OK;
}

void
MetroAppShell::NativeCallback()
{
  NS_ASSERTION(NS_IsMainThread(), "Native callbacks must be on the metro main thread");
  NativeEventCallback();

  // Handle shutdown after Exit() is called and unwinds.
  if (mExiting) {
    // shutdown fires events, don't recurse
    static bool sShutdown = false;
    if (sShutdown)
      return;
    sShutdown = true;
    if (sMetroApp) {
      // This calls XRE_metroShutdown() in xre
      sMetroApp->ShutdownXPCOM();
      // This will free the real main thread in CoreApplication::Run()
      // once winrt cleans up this thread.
      sMetroApp->CoreExit();
    }
  }
}

// static
LRESULT CALLBACK
MetroAppShell::EventWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  if (uMsg == sShellEventMsgID) {
    MetroAppShell *as = reinterpret_cast<MetroAppShell *>(lParam);
    as->NativeCallback();
    NS_RELEASE(as);
    return TRUE;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void
MetroAppShell::ScheduleNativeEventCallback()
{
  NS_ADDREF_THIS();
  PostMessage(mEventWnd, sShellEventMsgID, 0, reinterpret_cast<LPARAM>(this));
}

void
MetroAppShell::DoProcessMoreGeckoEvents()
{
  ScheduleNativeEventCallback();
}

static HANDLE
PowerCreateRequestDyn(REASON_CONTEXT *context)
{
  typedef HANDLE (WINAPI * PowerCreateRequestPtr)(REASON_CONTEXT *context);
  static HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
  static PowerCreateRequestPtr powerCreateRequest =
    (PowerCreateRequestPtr)GetProcAddress(kernel32, "PowerCreateRequest");
  if (!powerCreateRequest)
    return INVALID_HANDLE_VALUE;
  return powerCreateRequest(context);
}

static BOOL
PowerClearRequestDyn(HANDLE powerRequest, POWER_REQUEST_TYPE requestType)
{
  typedef BOOL (WINAPI * PowerClearRequestPtr)(HANDLE powerRequest, POWER_REQUEST_TYPE requestType);
  static HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
  static PowerClearRequestPtr powerClearRequest =
    (PowerClearRequestPtr)GetProcAddress(kernel32, "PowerClearRequest");
  if (!powerClearRequest)
    return FALSE;
  return powerClearRequest(powerRequest, requestType);
}

static BOOL
PowerSetRequestDyn(HANDLE powerRequest, POWER_REQUEST_TYPE requestType)
{
  typedef BOOL (WINAPI * PowerSetRequestPtr)(HANDLE powerRequest, POWER_REQUEST_TYPE requestType);
  static HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
  static PowerSetRequestPtr powerSetRequest =
    (PowerSetRequestPtr)GetProcAddress(kernel32, "PowerSetRequest");
  if (!powerSetRequest)
    return FALSE;
  return powerSetRequest(powerRequest, requestType);
}

NS_IMETHODIMP
MetroAppShell::Observe(nsISupports *subject, const char *topic,
                       const PRUnichar *data)
{
    NS_ENSURE_ARG_POINTER(topic);
    if (!strcmp(topic, "dl-start")) {
      if (mPowerRequestCount++ == 0) {
        Log("Download started - Disallowing suspend");
        REASON_CONTEXT context;
        context.Version = POWER_REQUEST_CONTEXT_VERSION;
        context.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
        context.Reason.SimpleReasonString = L"downloading";
        mPowerRequest.own(PowerCreateRequestDyn(&context));
        PowerSetRequestDyn(mPowerRequest, PowerRequestExecutionRequired);
      }
      return NS_OK;
    } else if (!strcmp(topic, "dl-done") ||
               !strcmp(topic, "dl-cancel") ||
               !strcmp(topic, "dl-failed")) {
      if (--mPowerRequestCount == 0 && mPowerRequest) {
        Log("All downloads ended - Allowing suspend");
        PowerClearRequestDyn(mPowerRequest, PowerRequestExecutionRequired); 
        mPowerRequest.reset();
      }
      return NS_OK;
    }

    return nsBaseAppShell::Observe(subject, topic, data);
}
