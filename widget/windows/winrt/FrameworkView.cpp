/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FrameworkView.h"
#include "MetroAppShell.h"
#include "MetroWidget.h"
#include "mozilla/AutoRestore.h"
#include "MetroUtils.h"
#include "MetroApp.h"
#include "UIABridgePublic.h"
#include "KeyboardLayout.h"

// generated
#include "UIABridge.h"

using namespace mozilla;
using namespace mozilla::widget::winrt;
#ifdef ACCESSIBILITY
using namespace mozilla::a11y;
#endif
using namespace ABI::Windows::ApplicationModel;
using namespace ABI::Windows::ApplicationModel::Core;
using namespace ABI::Windows::ApplicationModel::Activation;
using namespace ABI::Windows::UI::Core;
using namespace ABI::Windows::UI::ViewManagement;
using namespace ABI::Windows::UI::Input;
using namespace ABI::Windows::Devices::Input;
using namespace ABI::Windows::System;
using namespace ABI::Windows::Foundation;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

namespace mozilla {
namespace widget {
namespace winrt {

// statics
bool FrameworkView::sKeyboardIsVisible = false;
Rect FrameworkView::sKeyboardRect;
HSTRING FrameworkView::sActivationURI = NULL;
ApplicationExecutionState FrameworkView::sPreviousExecutionState;
nsTArray<nsString>* sSettingsArray;

FrameworkView::FrameworkView(MetroApp* aMetroApp) :
  mDPI(-1.0f),
  mWidget(nullptr),
  mShuttingDown(false),
  mMetroApp(aMetroApp),
  mWindow(nullptr),
  mMetroInput(nullptr),
  mWinVisible(false),
  mWinActiveState(false)
{
  mActivated.value = 0;
  mWindowActivated.value = 0;
  mWindowVisibilityChanged.value = 0;
  mWindowSizeChanged.value = 0;
  mSoftKeyboardHidden.value = 0;
  mSoftKeyboardShown.value = 0;
  mDisplayPropertiesChanged.value = 0;
  mAutomationProviderRequested.value = 0;
  mDataTransferRequested.value = 0;
  mSearchQuerySubmitted.value = 0;
  mPlayToRequested.value = 0;
  mSettingsPane.value = 0;
  mPrintManager.value = 0;
  memset(&sKeyboardRect, 0, sizeof(Rect));
  sSettingsArray = new nsTArray<nsString>();
  LogFunction();
}

////////////////////////////////////////////////////
// IFrameworkView impl.

HRESULT
FrameworkView::Initialize(ICoreApplicationView* aAppView)
{
  LogFunction();
  if (mShuttingDown)
    return E_FAIL;

  aAppView->add_Activated(Callback<__FITypedEventHandler_2_Windows__CApplicationModel__CCore__CCoreApplicationView_Windows__CApplicationModel__CActivation__CIActivatedEventArgs_t>(
    this, &FrameworkView::OnActivated).Get(), &mActivated);

  //CoCreateInstance(CLSID_WICImagingFactory, nullptr,
  //                 CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&mWicFactory));
  return S_OK;
}

HRESULT
FrameworkView::Uninitialize()
{
  return S_OK;
}

HRESULT
FrameworkView::Load(HSTRING aEntryPoint)
{
  return S_OK;
}

// called by winrt on startup
HRESULT
FrameworkView::Run()
{
  LogFunction();

  // Initialize XPCOM, create mWidget and go! We get a
  // callback in MetroAppShell::Run, in which we kick
  // off normal browser execution / event dispatching.
  mMetroApp->Run();

  // Gecko is completely shut down at this point.
  WinUtils::Log("Exiting FrameworkView::Run()");

  WindowsDeleteString(sActivationURI);
  return S_OK;
}

HRESULT
FrameworkView::ActivateView()
{
  LogFunction();

  UpdateWidgetSizeAndPosition();

  nsIntRegion region(nsIntRect(0, 0, mWindowBounds.width, mWindowBounds.height));
  mWidget->Paint(region);

  // Activate the window, this kills the splash screen
  mWindow->Activate();

  ProcessLaunchArguments();
  AddEventHandlers();
  SetupContracts();

  return S_OK;
}

HRESULT
FrameworkView::SetWindow(ICoreWindow* aWindow)
{
  LogFunction();

  NS_ASSERTION(!mWindow, "Attempting to set a window on a view that already has a window!");
  NS_ASSERTION(aWindow, "Attempting to set a null window on a view!");

  mWindow = aWindow;
  UpdateLogicalDPI();
  return S_OK;
}

////////////////////////////////////////////////////
// FrameworkView impl.

void
FrameworkView::AddEventHandlers() {
  NS_ASSERTION(mWindow, "SetWindow must be called before AddEventHandlers!");
  NS_ASSERTION(mWidget, "SetWidget must be called before AddEventHAndlers!");

  mMetroInput = Make<MetroInput>(mWidget.Get(),
                                 mWindow.Get());

  mWindow->add_VisibilityChanged(Callback<__FITypedEventHandler_2_Windows__CUI__CCore__CCoreWindow_Windows__CUI__CCore__CVisibilityChangedEventArgs>(
    this, &FrameworkView::OnWindowVisibilityChanged).Get(), &mWindowVisibilityChanged);
  mWindow->add_Activated(Callback<__FITypedEventHandler_2_Windows__CUI__CCore__CCoreWindow_Windows__CUI__CCore__CWindowActivatedEventArgs_t>(
    this, &FrameworkView::OnWindowActivated).Get(), &mWindowActivated);
  mWindow->add_SizeChanged(Callback<__FITypedEventHandler_2_Windows__CUI__CCore__CCoreWindow_Windows__CUI__CCore__CWindowSizeChangedEventArgs_t>(
    this, &FrameworkView::OnWindowSizeChanged).Get(), &mWindowSizeChanged);

  mWindow->add_AutomationProviderRequested(Callback<__FITypedEventHandler_2_Windows__CUI__CCore__CCoreWindow_Windows__CUI__CCore__CAutomationProviderRequestedEventArgs_t>(
    this, &FrameworkView::OnAutomationProviderRequested).Get(), &mAutomationProviderRequested);

  HRESULT hr;
  ComPtr<ABI::Windows::Graphics::Display::IDisplayPropertiesStatics> dispProps;
  if (SUCCEEDED(GetActivationFactory(HStringReference(RuntimeClass_Windows_Graphics_Display_DisplayProperties).Get(), dispProps.GetAddressOf()))) {
    hr = dispProps->add_LogicalDpiChanged(Callback<ABI::Windows::Graphics::Display::IDisplayPropertiesEventHandler, FrameworkView>(
      this, &FrameworkView::OnLogicalDpiChanged).Get(), &mDisplayPropertiesChanged);
    LogHRESULT(hr);
  }

  ComPtr<ABI::Windows::UI::ViewManagement::IInputPaneStatics> inputStatic;
  if (SUCCEEDED(hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_UI_ViewManagement_InputPane).Get(), inputStatic.GetAddressOf()))) {
    ComPtr<ABI::Windows::UI::ViewManagement::IInputPane> inputPane;
    if (SUCCEEDED(inputStatic->GetForCurrentView(inputPane.GetAddressOf()))) {
      inputPane->add_Hiding(Callback<__FITypedEventHandler_2_Windows__CUI__CViewManagement__CInputPane_Windows__CUI__CViewManagement__CInputPaneVisibilityEventArgs_t>(
        this, &FrameworkView::OnSoftkeyboardHidden).Get(), &mSoftKeyboardHidden);
      inputPane->add_Showing(Callback<__FITypedEventHandler_2_Windows__CUI__CViewManagement__CInputPane_Windows__CUI__CViewManagement__CInputPaneVisibilityEventArgs_t>(
        this, &FrameworkView::OnSoftkeyboardShown).Get(), &mSoftKeyboardShown);
    }
  }
}

// Called by MetroApp
void
FrameworkView::Shutdown()
{
  LogFunction();
  mShuttingDown = true;

  if (mWindow && mWindowVisibilityChanged.value) {
    mWindow->remove_VisibilityChanged(mWindowVisibilityChanged);
    mWindow->remove_Activated(mWindowActivated);
    mWindow->remove_Closed(mWindowClosed);
    mWindow->remove_SizeChanged(mWindowSizeChanged);
    mWindow->remove_AutomationProviderRequested(mAutomationProviderRequested);
  }

  ComPtr<ABI::Windows::Graphics::Display::IDisplayPropertiesStatics> dispProps;
  if (mDisplayPropertiesChanged.value &&
      SUCCEEDED(GetActivationFactory(HStringReference(RuntimeClass_Windows_Graphics_Display_DisplayProperties).Get(), dispProps.GetAddressOf()))) {
    dispProps->remove_LogicalDpiChanged(mDisplayPropertiesChanged);
  }

  ComPtr<ABI::Windows::UI::ViewManagement::IInputPaneStatics> inputStatic;
  if (mSoftKeyboardHidden.value &&
      SUCCEEDED(GetActivationFactory(HStringReference(RuntimeClass_Windows_UI_ViewManagement_InputPane).Get(), inputStatic.GetAddressOf()))) {
    ComPtr<ABI::Windows::UI::ViewManagement::IInputPane> inputPane;
    if (SUCCEEDED(inputStatic->GetForCurrentView(inputPane.GetAddressOf()))) {
      inputPane->remove_Hiding(mSoftKeyboardHidden);
      inputPane->remove_Showing(mSoftKeyboardShown);
    }
  }

  if (mAutomationProvider) {
    ComPtr<IUIABridge> provider;
    mAutomationProvider.As(&provider);
    if (provider) {
      provider->Disconnect();
    }
  }
  mAutomationProvider = nullptr;

  mMetroInput = nullptr;
  delete sSettingsArray;
  sSettingsArray = nullptr;
  mWidget = nullptr;
  mMetroApp = nullptr;
  mWindow = nullptr;
}

void
FrameworkView::SetCursor(CoreCursorType aCursorType, DWORD aCustomId)
{
  if (mShuttingDown) {
    return;
  }
  NS_ASSERTION(mWindow, "SetWindow must be called before SetCursor!");
  ComPtr<ABI::Windows::UI::Core::ICoreCursorFactory> factory;
  AssertHRESULT(GetActivationFactory(HStringReference(RuntimeClass_Windows_UI_Core_CoreCursor).Get(), factory.GetAddressOf()));
  ComPtr<ABI::Windows::UI::Core::ICoreCursor> cursor;
  AssertHRESULT(factory->CreateCursor(aCursorType, aCustomId, cursor.GetAddressOf()));
  mWindow->put_PointerCursor(cursor.Get());
}

void
FrameworkView::ClearCursor()
{
  if (mShuttingDown) {
    return;
  }
  NS_ASSERTION(mWindow, "SetWindow must be called before ClearCursor!");
  mWindow->put_PointerCursor(nullptr);
}

void
FrameworkView::UpdateLogicalDPI()
{
  ComPtr<ABI::Windows::Graphics::Display::IDisplayPropertiesStatics> dispProps;
  HRESULT hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_Graphics_Display_DisplayProperties).Get(),
                                    dispProps.GetAddressOf());
  AssertHRESULT(hr);
  FLOAT value;
  AssertHRESULT(dispProps->get_LogicalDpi(&value));
  SetDpi(value);
}

void
FrameworkView::GetBounds(nsIntRect &aRect)
{
  // May be called by compositor thread
  if (mShuttingDown) {
    return;
  }
  aRect = mWindowBounds;
}

void
FrameworkView::UpdateWidgetSizeAndPosition()
{
  if (mShuttingDown)
    return;

  NS_ASSERTION(mWindow, "SetWindow must be called before UpdateWidgetSizeAndPosition!");
  NS_ASSERTION(mWidget, "SetWidget must be called before UpdateWidgetSizeAndPosition!");

  UpdateBounds();
  mWidget->Move(0, 0);
  mWidget->Resize(0, 0, mWindowBounds.width, mWindowBounds.height, true);
  mWidget->SizeModeChanged();
}

bool
FrameworkView::IsEnabled() const
{
  return mWinActiveState;
}

bool
FrameworkView::IsVisible() const
{
  // we could check the wnd in MetroWidget for this, but
  // generally we don't let nsIWidget control visibility
  // or activation.
  return mWinVisible;
}

void FrameworkView::SetDpi(float aDpi)
{
  if (aDpi != mDPI) {
    LogFunction();

    mDPI = aDpi;

    // notify the widget that dpi has changed
    if (mWidget) {
      mWidget->ChangedDPI();
      UpdateBounds();
    }
  }
}

void
FrameworkView::UpdateBounds()
{
  if (!mWidget)
    return;

  RECT winRect;
  GetClientRect(mWidget->GetICoreWindowHWND(), &winRect);

  mWindowBounds = nsIntRect(winRect.left,
                            winRect.top,
                            winRect.right - winRect.left,
                            winRect.bottom - winRect.top);
}

void
FrameworkView::SetWidget(MetroWidget* aWidget)
{
  NS_ASSERTION(!mWidget, "Attempting to set a widget for a view that already has a widget!");
  NS_ASSERTION(aWidget, "Attempting to set a null widget for a view!");
  LogFunction();
  mWidget = aWidget;
  mWidget->FindMetroWindow();
  UpdateBounds();
}

////////////////////////////////////////////////////
// Event handlers

void
FrameworkView::SendActivationEvent() 
{
  if (mShuttingDown) {
    return;
  }
  NS_ASSERTION(mWindow, "SetWindow must be called before SendActivationEvent!");
  mWidget->Activated(mWinActiveState);
  if (mWinActiveState) {
    UpdateWidgetSizeAndPosition();
  }
  EnsureAutomationProviderCreated();
}

HRESULT
FrameworkView::OnWindowVisibilityChanged(ICoreWindow* aWindow,
                                         IVisibilityChangedEventArgs* aArgs)
{
  // If we're visible, or we can't determine if we're visible, just store
  // that we are visible.
  boolean visible;
  mWinVisible = FAILED(aArgs->get_Visible(&visible)) || visible;
  return S_OK;
}

HRESULT
FrameworkView::OnActivated(ICoreApplicationView* aApplicationView,
                           IActivatedEventArgs* aArgs)
{
  LogFunction();

  if (mShuttingDown) {
    return S_OK;
  }

  aArgs->get_PreviousExecutionState(&sPreviousExecutionState);
  bool startup = sPreviousExecutionState == ApplicationExecutionState::ApplicationExecutionState_Terminated ||
                 sPreviousExecutionState == ApplicationExecutionState::ApplicationExecutionState_ClosedByUser ||
                 sPreviousExecutionState == ApplicationExecutionState::ApplicationExecutionState_NotRunning;
  ProcessActivationArgs(aArgs, startup);
  return S_OK;
}

HRESULT
FrameworkView::OnSoftkeyboardHidden(IInputPane* aSender,
                                    IInputPaneVisibilityEventArgs* aArgs)
{
  LogFunction();
  sKeyboardIsVisible = false;
  memset(&sKeyboardRect, 0, sizeof(Rect));
  MetroUtils::FireObserver("metro_softkeyboard_hidden");
  aArgs->put_EnsuredFocusedElementInView(true);
  return S_OK;
}

HRESULT
FrameworkView::OnSoftkeyboardShown(IInputPane* aSender,
                                   IInputPaneVisibilityEventArgs* aArgs)
{
  LogFunction();
  sKeyboardIsVisible = true;
  aSender->get_OccludedRect(&sKeyboardRect);
  MetroUtils::FireObserver("metro_softkeyboard_shown");
  aArgs->put_EnsuredFocusedElementInView(true);
  return S_OK;
}

HRESULT
FrameworkView::OnWindowSizeChanged(ICoreWindow* aSender, IWindowSizeChangedEventArgs* aArgs)
{
  LogFunction();
  UpdateWidgetSizeAndPosition();
  return S_OK;
}

HRESULT
FrameworkView::OnWindowActivated(ICoreWindow* aSender, IWindowActivatedEventArgs* aArgs)
{
  LogFunction();
  if (!mWidget) {
    return S_OK;
  }
  CoreWindowActivationState state;
  aArgs->get_WindowActivationState(&state);
  mWinActiveState = !(state == CoreWindowActivationState::CoreWindowActivationState_Deactivated);
  SendActivationEvent();
  return S_OK;
}

HRESULT
FrameworkView::OnLogicalDpiChanged(IInspectable* aSender)
{
  LogFunction();
  UpdateLogicalDPI();
  if (mWidget) {
    mWidget->Invalidate();
  }
  return S_OK;
}

bool
FrameworkView::EnsureAutomationProviderCreated()
{
#ifdef ACCESSIBILITY
  if (!mWidget || mShuttingDown)
    return false;

  if (mAutomationProvider) {
    return true;
  }

  Accessible *rootAccessible = mWidget->GetRootAccessible();
  if (rootAccessible) {
    IInspectable* inspectable;
    HRESULT hr;
    AssertRetHRESULT(hr = UIABridge_CreateInstance(&inspectable), hr); // Addref
    IUIABridge* bridge = nullptr;
    inspectable->QueryInterface(IID_IUIABridge, (void**)&bridge); // Addref
    if (bridge) {
      bridge->Init(this, mWindow.Get(), (ULONG)rootAccessible);
      mAutomationProvider = inspectable;
      inspectable->Release();
      return true;
    }
  }
#endif
  return false;
}

HRESULT
FrameworkView::OnAutomationProviderRequested(ICoreWindow* aSender,
                                             IAutomationProviderRequestedEventArgs* aArgs)
{
  LogFunction();
  if (!EnsureAutomationProviderCreated())
    return E_FAIL;
  HRESULT hr = aArgs->put_AutomationProvider(mAutomationProvider.Get());
  if (FAILED(hr)) {
    WinUtils::Log("put failed? %X", hr);
  }
  return S_OK;
}

} } }
