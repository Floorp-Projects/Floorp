/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MetroAppShell.h"
#include "nsXULAppAPI.h"
#include "mozilla/widget/AudioSession.h"
#include "MetroUtils.h"
#include "MetroApp.h"
#include "FrameworkView.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/AutoRestore.h"
#include "WinUtils.h"
#include "nsIAppStartup.h"
#include "nsToolkitCompsCID.h"

using namespace mozilla;
using namespace mozilla::widget;
using namespace mozilla::widget::winrt;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::UI::Core;
using namespace ABI::Windows::Foundation;

// ProcessNextNativeEvent message wait timeout, see bug 907410.
#define MSG_WAIT_TIMEOUT 250

namespace mozilla {
namespace widget {
namespace winrt {
extern ComPtr<MetroApp> sMetroApp;
extern ComPtr<FrameworkView> sFrameworkView;
} } }

namespace mozilla {
namespace widget {
// pulled from win32 app shell
extern UINT sAppShellGeckoMsgId;
} }

static ComPtr<ICoreWindowStatic> sCoreStatic;
static bool sIsDispatching = false;
static bool sWillEmptyThreadQueue = false;
static bool sEmptyingThreadQueue = false;

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
  HINSTANCE module = GetModuleHandle(nullptr);

  const PRUnichar *const kWindowClass = L"nsAppShell:EventWindowClass";
  if (!GetClassInfoW(module, kWindowClass, &wc)) {
    wc.style         = 0;
    wc.lpfnWndProc   = EventWindowProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = module;
    wc.hIcon         = nullptr;
    wc.hCursor       = nullptr;
    wc.hbrBackground = (HBRUSH) nullptr;
    wc.lpszMenuName  = (LPCWSTR) nullptr;
    wc.lpszClassName = kWindowClass;
    RegisterClassW(&wc);
  }

  mEventWnd = CreateWindowW(kWindowClass, L"nsAppShell:EventWindow",
                           0, 0, 0, 10, 10, nullptr, nullptr, module, nullptr);
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

HRESULT SHCreateShellItemArrayFromShellItemDynamic(IShellItem *psi, REFIID riid, void **ppv)
{
  HMODULE shell32DLL = LoadLibraryW(L"shell32.dll");
  if (!shell32DLL) {
    return E_FAIL;
  }

  typedef BOOL (WINAPI* SHFn)(IShellItem *psi, REFIID riid, void **ppv);

  HRESULT hr = E_FAIL;
  SHFn SHCreateShellItemArrayFromShellItemDynamicPtr =
    (SHFn)GetProcAddress(shell32DLL, "SHCreateShellItemArrayFromShellItem");
  FreeLibrary(shell32DLL);
  if (SHCreateShellItemArrayFromShellItemDynamicPtr) {
    hr = SHCreateShellItemArrayFromShellItemDynamicPtr(psi, riid, ppv);
  }

  FreeLibrary(shell32DLL);
  return hr;
}

BOOL
WinLaunchDeferredMetroFirefox()
{
  // Create an instance of the Firefox Metro DEH which is used to launch the browser
  const CLSID CLSID_FirefoxMetroDEH = {0x5100FEC1,0x212B, 0x4BF5 ,{0x9B,0xF8, 0x3E,0x65, 0x0F,0xD7,0x94,0xA3}};

  nsRefPtr<IExecuteCommand> executeCommand;
  HRESULT hr = CoCreateInstance(CLSID_FirefoxMetroDEH,
                                nullptr,
                                CLSCTX_LOCAL_SERVER,
                                IID_IExecuteCommand,
                                getter_AddRefs(executeCommand));
  if (FAILED(hr))
    return FALSE;

  // Get the currently running exe path
  WCHAR exePath[MAX_PATH + 1] = { L'\0' };
  if (!::GetModuleFileNameW(0, exePath, MAX_PATH))
    return FALSE;

  // Convert the path to a long path since GetModuleFileNameW returns the path
  // that was used to launch Firefox which is not necessarily a long path.
  if (!::GetLongPathNameW(exePath, exePath, MAX_PATH))
    return FALSE;

  // Create an IShellItem for the current browser path
  nsRefPtr<IShellItem> shellItem;
  hr = WinUtils::SHCreateItemFromParsingName(exePath, nullptr, IID_IShellItem,
                                             getter_AddRefs(shellItem));
  if (FAILED(hr))
    return FALSE;

  // Convert to an IShellItemArray which is used for the path to launch
  nsRefPtr<IShellItemArray> shellItemArray;
  hr = SHCreateShellItemArrayFromShellItemDynamic(shellItem, IID_IShellItemArray, getter_AddRefs(shellItemArray));
  if (FAILED(hr))
    return FALSE;

  // Set the path to launch and parameters needed
  nsRefPtr<IObjectWithSelection> selection;
  hr = executeCommand->QueryInterface(IID_IObjectWithSelection, getter_AddRefs(selection));
  if (FAILED(hr))
    return FALSE;
  hr = selection->SetSelection(shellItemArray);
  if (FAILED(hr))
    return FALSE;

  hr = executeCommand->SetParameters(L"--metro-restart");
  if (FAILED(hr))
    return FALSE;

  // Run the default browser through the DEH
  hr = executeCommand->Execute();
  return SUCCEEDED(hr);
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
    case GeckoProcessType_Default: {
      mozilla::widget::StartAudioSession();
      sFrameworkView->ActivateView();
      rv = nsBaseAppShell::Run();
      mozilla::widget::StopAudioSession();

      nsCOMPtr<nsIAppStartup> appStartup (do_GetService(NS_APPSTARTUP_CONTRACTID));
      bool restarting;
      if (appStartup && NS_SUCCEEDED(appStartup->GetRestarting(&restarting)) && restarting) {
        if (!WinLaunchDeferredMetroFirefox()) {
          NS_WARNING("Couldn't deferred launch Metro Firefox.");
        }
      }

      // This calls XRE_metroShutdown() in xre. This will also destroy
      // MessagePump.
      sMetroApp->ShutdownXPCOM();

      // This will free the real main thread in CoreApplication::Run()
      // once winrt cleans up this thread.
      sMetroApp->CoreExit();
    }
    break;
  }

  return rv;
}

// Called in certain cases where we have async input events in the thread
// queue and need to make sure they get dispatched before the stack unwinds.
void // static
MetroAppShell::MarkEventQueueForPurge()
{
  sWillEmptyThreadQueue = true;

  // If we're dispatching native events, wait until the dispatcher is
  // off the stack.
  if (sIsDispatching) {
    return;
  }

  // Safe to process pending events now
  DispatchAllGeckoEvents();
}

// static
void
MetroAppShell::DispatchAllGeckoEvents()
{
  if (!sWillEmptyThreadQueue) {
    return;
  }

  NS_ASSERTION(NS_IsMainThread(), "DispatchAllXPCOMEvents should be called on the main thread");

  sWillEmptyThreadQueue = false;

  AutoRestore<bool> dispatching(sEmptyingThreadQueue);
  sEmptyingThreadQueue = true;
  nsIThread *thread = NS_GetCurrentThread();
  NS_ProcessPendingEvents(thread, 0);
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

// static
bool
MetroAppShell::ProcessOneNativeEventIfPresent()
{
  if (sIsDispatching) {
    // Calling into ProcessNativeEvents is harmless, but won't actually process any
    // native events. So we log here so we can spot this and get a handle on the
    // corner cases where this can happen.
    WinUtils::Log("WARNING: Reentrant call into process events detected, returning early.");
    return false;
  }

  {
    AutoRestore<bool> dispatching(sIsDispatching);
    sIsDispatching = true;
    ProcessNativeEvents(CoreProcessEventsOption::CoreProcessEventsOption_ProcessOneIfPresent);
  }

  DispatchAllGeckoEvents();

  return !!HIWORD(::GetQueueStatus(MOZ_QS_ALLEVENT));
}

bool
MetroAppShell::ProcessNextNativeEvent(bool mayWait)
{
  // NS_ProcessPendingEvents will process thread events *and* call
  // nsBaseAppShell::OnProcessNextEvent to process native events. However
  // we do not want native events getting dispatched while we are in
  // DispatchAllGeckoEvents.
  if (sEmptyingThreadQueue) {
    return false;
  }

  if (ProcessOneNativeEventIfPresent()) {
    return true;
  }
  if (mayWait) {
    DWORD result = ::MsgWaitForMultipleObjectsEx(0, nullptr, MSG_WAIT_TIMEOUT,
                                                 MOZ_QS_ALLEVENT,
                                                 MWMO_INPUTAVAILABLE|MWMO_ALERTABLE);
    NS_WARN_IF_FALSE(result != WAIT_FAILED, "Wait failed");
  }
  return ProcessOneNativeEventIfPresent();
}

void
MetroAppShell::NativeCallback()
{
  NS_ASSERTION(NS_IsMainThread(), "Native callbacks must be on the metro main thread");
  NativeEventCallback();
}

// static
LRESULT CALLBACK
MetroAppShell::EventWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  if (uMsg == sAppShellGeckoMsgId) {
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
  PostMessage(mEventWnd, sAppShellGeckoMsgId, 0, reinterpret_cast<LPARAM>(this));
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
        WinUtils::Log("Download started - Disallowing suspend");
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
        WinUtils::Log("All downloads ended - Allowing suspend");
        PowerClearRequestDyn(mPowerRequest, PowerRequestExecutionRequired); 
        mPowerRequest.reset();
      }
      return NS_OK;
    }

    return nsBaseAppShell::Observe(subject, topic, data);
}
