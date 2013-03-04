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

/*
 * Due to issues on older platforms with linking the winrt runtime lib we
 * can't have ref new winrt variables in the global scope. Everything should
 * be encapsulated in a class. See toolkit/library/nsDllMain for the details.
 */

namespace mozilla {
namespace widget {
namespace winrt {

// statics
bool FrameworkView::sKeyboardIsVisible = false;
Rect FrameworkView::sKeyboardRect;
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
  mPainting = false;
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
  LogFunction();
  mShuttingDown = true;
  mD2DWindowSurface = nullptr;
  delete sSettingsArray;
  sSettingsArray = nullptr;
  return S_OK;
}

HRESULT
FrameworkView::Load(HSTRING aEntryPoint)
{
  return S_OK;
}

HRESULT
FrameworkView::Run()
{
  LogFunction();

  // XPCOM is initialized here. mWidget is also created.
  mMetroApp->Initialize();

  if (mDeferredActivationEventArgs) {
    RunStartupArgs(mDeferredActivationEventArgs.Get());
    mDeferredActivationEventArgs = nullptr;
  }

  // Activate the window
  mWindow->Activate();

  UpdateWidgetSizeAndPosition();

  MetroUtils::GetViewState(mViewState);

  // Get the metro event dispatcher
  HRESULT hr = mWindow->get_Dispatcher(&mDispatcher);
  AssertRetHRESULT(hr, hr);

  // Needs mDispatcher
  AddEventHandlers();

  // Drop into the main metro event loop
  mDispatcher->ProcessEvents(ABI::Windows::UI::Core::CoreProcessEventsOption::CoreProcessEventsOption_ProcessUntilQuit);

  Log(L"Exiting FrameworkView::Run()");
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
  NS_ASSERTION(mDispatcher, "Must have a valid CoreDispatcher before "
                            "calling AddEventHAndlers!");

  mMetroInput = Make<MetroInput>(mWidget.Get(),
                                 mWindow.Get(),
                                 mDispatcher.Get());

  mWindow->add_VisibilityChanged(Callback<__FITypedEventHandler_2_Windows__CUI__CCore__CCoreWindow_Windows__CUI__CCore__CVisibilityChangedEventArgs>(
    this, &FrameworkView::OnWindowVisibilityChanged).Get(), &mWindowVisibilityChanged);
  mWindow->add_Activated(Callback<__FITypedEventHandler_2_Windows__CUI__CCore__CCoreWindow_Windows__CUI__CCore__CWindowActivatedEventArgs_t>(
    this, &FrameworkView::OnWindowActivated).Get(), &mWindowActivated);
  mWindow->add_Closed(Callback<__FITypedEventHandler_2_Windows__CUI__CCore__CCoreWindow_Windows__CUI__CCore__CCoreWindowEventArgs_t>(
    this, &FrameworkView::OnWindowClosed).Get(), &mWindowClosed);
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
FrameworkView::ShutdownXPCOM()
{
  mShuttingDown = true;
  mWidget = nullptr;
  ComPtr<IUIABridge> provider;
  if (mAutomationProvider) {
    mAutomationProvider.As(&provider);
    if (provider) {
      provider->Disconnect();
    }
  }
  mAutomationProvider = nullptr;
  mMetroInput = nullptr;
  Uninitialize();
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
  nsIntRect mozrect(0, 0, (uint32_t)ceil(mWindowBounds.Width),
                    (uint32_t)ceil(mWindowBounds.Height));
  aRect = mozrect;
}

void
FrameworkView::UpdateWidgetSizeAndPosition()
{
  if (mShuttingDown)
    return;

  NS_ASSERTION(mWindow, "SetWindow must be called before UpdateWidgetSizeAndPosition!");
  NS_ASSERTION(mWidget, "SetWidget must be called before UpdateWidgetSizeAndPosition!");

  mWidget->Move(0, 0);
  mWidget->Resize(0, 0, (uint32_t)ceil(mWindowBounds.Width),
                  (uint32_t)ceil(mWindowBounds.Height), true);
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
      mDPI = aDpi;
      // Often a DPI change implies a window size change.
      NS_ASSERTION(mWindow, "SetWindow must be called before SetDpi!");
      mWindow->get_Bounds(&mWindowBounds);
  }
}

void
FrameworkView::SetWidget(MetroWidget* aWidget)
{
  NS_ASSERTION(!mWidget, "Attempting to set a widget for a view that already has a widget!");
  NS_ASSERTION(aWidget, "Attempting to set a null widget for a view!");
  LogFunction();
  mWidget = aWidget;
  mWidget->FindMetroWindow();
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
  UpdateWidgetSizeAndPosition();
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
  // If we're on startup, we want to wait for FrameworkView::Run to run because
  // XPCOM is not initialized yet and and we can't use nsICommandLineRunner

  ApplicationExecutionState state;
  aArgs->get_PreviousExecutionState(&state);
  if (state != ApplicationExecutionState::ApplicationExecutionState_Terminated &&
      state != ApplicationExecutionState::ApplicationExecutionState_ClosedByUser &&
      state != ApplicationExecutionState::ApplicationExecutionState_NotRunning) {
    RunStartupArgs(aArgs);
  } else {
    mDeferredActivationEventArgs = aArgs;
  }
  return S_OK;
}

HRESULT
FrameworkView::OnSoftkeyboardHidden(IInputPane* aSender,
                                    IInputPaneVisibilityEventArgs* aArgs)
{
  LogFunction();
  if (mShuttingDown)
    return S_OK;
  sKeyboardIsVisible = false;
  memset(&sKeyboardRect, 0, sizeof(Rect));
  MetroUtils::FireObserver("metro_softkeyboard_hidden");
  return S_OK;
}

HRESULT
FrameworkView::OnSoftkeyboardShown(IInputPane* aSender,
                                   IInputPaneVisibilityEventArgs* aArgs)
{
  LogFunction();
  if (mShuttingDown)
    return S_OK;
  sKeyboardIsVisible = true;
  aSender->get_OccludedRect(&sKeyboardRect);
  MetroUtils::FireObserver("metro_softkeyboard_shown");
  return S_OK;
}

HRESULT
FrameworkView::OnWindowClosed(ICoreWindow* aSender, ICoreWindowEventArgs* aArgs)
{
  // this doesn't seem very reliable
  return S_OK;
}

void
FrameworkView::FireViewStateObservers()
{
  ApplicationViewState state;
  MetroUtils::GetViewState(state);
  if (state == mViewState) {
    return;
  }
  mViewState = state;
  nsAutoString name;
  switch (mViewState) {
    case ApplicationViewState_FullScreenLandscape:
      name.AssignLiteral("landscape");
    break;
    case ApplicationViewState_Filled:
      name.AssignLiteral("filled");
    break;
    case ApplicationViewState_Snapped:
      name.AssignLiteral("snapped");
    break;
    case ApplicationViewState_FullScreenPortrait:
      name.AssignLiteral("portrait");
    break;
    default:
      NS_WARNING("Unknown view state");
    return;
  };
  MetroUtils::FireObserver("metro_viewstate_changed", name.get());
}

HRESULT
FrameworkView::OnWindowSizeChanged(ICoreWindow* aSender, IWindowSizeChangedEventArgs* aArgs)
{
  LogFunction();

  if (mShuttingDown) {
    return S_OK;
  }

  NS_ASSERTION(mWindow, "SetWindow must be called before OnWindowSizeChanged!");
  mWindow->get_Bounds(&mWindowBounds);

  UpdateWidgetSizeAndPosition();
  FireViewStateObservers();
  return S_OK;
}

HRESULT
FrameworkView::OnWindowActivated(ICoreWindow* aSender, IWindowActivatedEventArgs* aArgs)
{
  LogFunction();
  if (mShuttingDown || !mWidget)
    return E_FAIL;
  CoreWindowActivationState state;
  aArgs->get_WindowActivationState(&state);
  mWinActiveState = !(state == CoreWindowActivationState::CoreWindowActivationState_Deactivated);
  SendActivationEvent();

  // Flush out all remaining events so base widget doesn't process other stuff
  // earlier which would lead to a white flash of a second at startup.
  MetroAppShell::ProcessAllNativeEventsPresent();

  return S_OK;
}

HRESULT
FrameworkView::OnLogicalDpiChanged(IInspectable* aSender)
{
  LogFunction();
  UpdateLogicalDPI();
  Render();
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
  if (!EnsureAutomationProviderCreated())
    return E_FAIL;
  aArgs->put_AutomationProvider(mAutomationProvider.Get());
  return S_OK;
}

} } }
