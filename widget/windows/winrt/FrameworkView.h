/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "MetroWidget.h"
#include "MetroInput.h"
#include "gfxWindowsPlatform.h"
#include "nsDataHashtable.h"

#include "mozwrlbase.h"

#include <windows.system.h>
#include <Windows.ApplicationModel.core.h>
#include <Windows.ApplicationModel.h>
#include <Windows.Applicationmodel.Activation.h>
#include <Windows.ApplicationModel.search.h>
#include <windows.ui.core.h>
#include <windows.ui.viewmanagement.h>
#include <windows.ui.applicationsettings.h>
#include <windows.ui.popups.h>
#include <windows.graphics.printing.h>
#include <windows.graphics.display.h>
#include <windows.media.playto.h>
#include <d2d1_1.h>

namespace mozilla {
namespace widget {
namespace winrt {

class MetroApp;

class FrameworkView : public Microsoft::WRL::RuntimeClass<ABI::Windows::ApplicationModel::Core::IFrameworkView>
{
  InspectableClass(L"FrameworkView", TrustLevel::BaseTrust)

  typedef mozilla::layers::LayerManager LayerManager;

  typedef ABI::Windows::Foundation::Rect Rect;
  typedef ABI::Windows::UI::Core::IWindowSizeChangedEventArgs IWindowSizeChangedEventArgs;
  typedef ABI::Windows::UI::Core::ICoreWindowEventArgs ICoreWindowEventArgs;
  typedef ABI::Windows::UI::Core::IWindowActivatedEventArgs IWindowActivatedEventArgs;
  typedef ABI::Windows::UI::Core::IAutomationProviderRequestedEventArgs IAutomationProviderRequestedEventArgs;
  typedef ABI::Windows::UI::Core::ICoreWindow ICoreWindow;
  typedef ABI::Windows::UI::Core::ICoreDispatcher ICoreDispatcher;
  typedef ABI::Windows::UI::Core::IVisibilityChangedEventArgs IVisibilityChangedEventArgs;
  typedef ABI::Windows::UI::ViewManagement::IInputPaneVisibilityEventArgs IInputPaneVisibilityEventArgs;
  typedef ABI::Windows::UI::ViewManagement::IInputPane IInputPane;
  typedef ABI::Windows::UI::ViewManagement::ApplicationViewState ApplicationViewState;
  typedef ABI::Windows::UI::ApplicationSettings::ISettingsPane ISettingsPane;
  typedef ABI::Windows::UI::ApplicationSettings::ISettingsPaneCommandsRequestedEventArgs ISettingsPaneCommandsRequestedEventArgs;
  typedef ABI::Windows::UI::Popups::IUICommand IUICommand;
  typedef ABI::Windows::ApplicationModel::Activation::ILaunchActivatedEventArgs ILaunchActivatedEventArgs;
  typedef ABI::Windows::ApplicationModel::Activation::IActivatedEventArgs IActivatedEventArgs;
  typedef ABI::Windows::ApplicationModel::Activation::ISearchActivatedEventArgs ISearchActivatedEventArgs;
  typedef ABI::Windows::ApplicationModel::Activation::IFileActivatedEventArgs IFileActivatedEventArgs;
  typedef ABI::Windows::ApplicationModel::Core::ICoreApplicationView ICoreApplicationView;
  typedef ABI::Windows::ApplicationModel::DataTransfer::IDataTransferManager IDataTransferManager;
  typedef ABI::Windows::ApplicationModel::DataTransfer::IDataRequestedEventArgs IDataRequestedEventArgs;
  typedef ABI::Windows::ApplicationModel::Search::ISearchPane ISearchPane;
  typedef ABI::Windows::ApplicationModel::Search::ISearchPaneQuerySubmittedEventArgs ISearchPaneQuerySubmittedEventArgs;
  typedef ABI::Windows::Media::PlayTo::IPlayToManager IPlayToManager;
  typedef ABI::Windows::Media::PlayTo::IPlayToSourceRequestedEventArgs IPlayToSourceRequestedEventArgs;
  typedef ABI::Windows::Graphics::Printing::IPrintManager IPrintManager;
  typedef ABI::Windows::Graphics::Printing::IPrintTaskRequestedEventArgs IPrintTaskRequestedEventArgs;
  typedef ABI::Windows::Graphics::Printing::IPrintTaskSourceRequestedArgs IPrintTaskSourceRequestedArgs;

public:
  FrameworkView(MetroApp* aMetroApp);

  // IFrameworkView Methods
  STDMETHODIMP Initialize(ICoreApplicationView* aAppView);
  STDMETHODIMP SetWindow(ICoreWindow* aWindow);
  STDMETHODIMP Load(HSTRING aEntryPoint);
  STDMETHODIMP Run();
  STDMETHODIMP Uninitialize();

  HRESULT ActivateView();

  // Public apis for MetroWidget
  float GetDPI() { return mDPI; }
  ICoreWindow* GetCoreWindow() { return mWindow.Get(); }
  void SetWidget(MetroWidget* aWidget);
  MetroWidget* GetWidget() { return mWidget.Get(); }
  void GetBounds(nsIntRect &aRect);
  void SetCursor(ABI::Windows::UI::Core::CoreCursorType aCursorType, DWORD aCustomId = 0);
  void ClearCursor();
  bool IsEnabled() const;
  bool IsVisible() const;

  // Activation apis for nsIWinMetroUtils
  static int GetPreviousExecutionState() { return sPreviousExecutionState; }
  static void GetActivationURI(nsAString &aActivationURI) {
    unsigned int length;
    aActivationURI = WindowsGetStringRawBuffer(sActivationURI, &length);
  }

  // Soft keyboard info for nsIWinMetroUtils
  static bool IsKeyboardVisible() { return sKeyboardIsVisible; }
  static ABI::Windows::Foundation::Rect KeyboardVisibleRect() { return sKeyboardRect; }

  // MetroApp apis
  void SetupContracts();
  void Shutdown();

  // MetroContracts settings panel enumerator entry
  void AddSetting(ISettingsPaneCommandsRequestedEventArgs* aArgs, uint32_t aId,
                  Microsoft::WRL::Wrappers::HString& aSettingName);
protected:
  // Event Handlers
  HRESULT OnActivated(ICoreApplicationView* aApplicationView,
                      IActivatedEventArgs* aArgs);
  HRESULT OnWindowVisibilityChanged(ICoreWindow* aCoreWindow,
                                    IVisibilityChangedEventArgs* aArgs);

  HRESULT OnWindowSizeChanged(ICoreWindow* aSender,
                              IWindowSizeChangedEventArgs* aArgs);
  HRESULT OnWindowActivated(ICoreWindow* aSender,
                            IWindowActivatedEventArgs* aArgs);
  HRESULT OnLogicalDpiChanged(IInspectable* aSender);

  HRESULT OnAutomationProviderRequested(ICoreWindow* aSender,
                                        IAutomationProviderRequestedEventArgs* aArgs);

  HRESULT OnSoftkeyboardHidden(IInputPane* aSender,
                               IInputPaneVisibilityEventArgs* aArgs);
  HRESULT OnSoftkeyboardShown(IInputPane* aSender,
                              IInputPaneVisibilityEventArgs* aArgs);

  HRESULT OnDataShareRequested(IDataTransferManager*, IDataRequestedEventArgs* aArgs);
  HRESULT OnSearchQuerySubmitted(ISearchPane* aPane, ISearchPaneQuerySubmittedEventArgs* aArgs);
  HRESULT OnSettingsCommandsRequested(ISettingsPane* aPane, ISettingsPaneCommandsRequestedEventArgs* aArgs);
  HRESULT OnPlayToSourceRequested(IPlayToManager* aPane, IPlayToSourceRequestedEventArgs* aArgs);
  HRESULT OnSettingsCommandInvoked(IUICommand* aCommand);
  HRESULT OnPrintTaskRequested(IPrintManager* aMgr, IPrintTaskRequestedEventArgs* aArgs);
  HRESULT OnPrintTaskSourceRequested(IPrintTaskSourceRequestedArgs* aArgs);

protected:
  void SetDpi(float aDpi);
  void UpdateWidgetSizeAndPosition();
  void PerformURILoad(Microsoft::WRL::Wrappers::HString& aString);
  void PerformSearch(Microsoft::WRL::Wrappers::HString& aQuery);
  void PerformURILoadOrSearch(Microsoft::WRL::Wrappers::HString& aString);
  bool EnsureAutomationProviderCreated();
  void SearchActivated(Microsoft::WRL::ComPtr<ISearchActivatedEventArgs>& aArgs, bool aStartup);
  void FileActivated(Microsoft::WRL::ComPtr<IFileActivatedEventArgs>& aArgs, bool aStartup);
  void LaunchActivated(Microsoft::WRL::ComPtr<ILaunchActivatedEventArgs>& aArgs, bool aStartup);
  void ProcessActivationArgs(IActivatedEventArgs* aArgs, bool aStartup);
  void UpdateForWindowSizeChange();
  void SendActivationEvent();
  void UpdateLogicalDPI();
  void FireViewStateObservers();
  void ProcessLaunchArguments();
  void UpdateBounds();

  // Printing and preview
  void CreatePrintControl(IPrintDocumentPackageTarget* aDocPackageTarget, 
                          D2D1_PRINT_CONTROL_PROPERTIES* aPrintControlProperties);
  HRESULT ClosePrintControl();
  void PrintPage(uint32_t aPageNumber, D2D1_RECT_F aImageableArea,
                 D2D1_SIZE_F aPageSize, IStream* aPagePrintTicketStream);
  void AddEventHandlers();

private:
  EventRegistrationToken mActivated;
  EventRegistrationToken mWindowActivated;
  EventRegistrationToken mWindowVisibilityChanged;
  EventRegistrationToken mWindowClosed;
  EventRegistrationToken mWindowSizeChanged;
  EventRegistrationToken mSoftKeyboardHidden;
  EventRegistrationToken mSoftKeyboardShown;
  EventRegistrationToken mDisplayPropertiesChanged;
  EventRegistrationToken mAutomationProviderRequested;
  EventRegistrationToken mDataTransferRequested;
  EventRegistrationToken mSearchQuerySubmitted;
  EventRegistrationToken mPlayToRequested;
  EventRegistrationToken mSettingsPane;
  EventRegistrationToken mPrintManager;

private:
  nsIntRect mWindowBounds; // in device-pixel coordinates
  float mDPI;
  bool mShuttingDown;
  nsAutoString mActivationCommandLine;
  Microsoft::WRL::ComPtr<IInspectable> mAutomationProvider;
  //Microsoft::WRL::ComPtr<ID2D1PrintControl> mD2DPrintControl;
  // Private critical section protects D2D device context for on-screen
  // rendering from that for print/preview in the different thread.
  //Microsoft::WRL::ComPtr<IWICImagingFactory2> mWicFactory;
  Microsoft::WRL::ComPtr<MetroApp> mMetroApp;
  Microsoft::WRL::ComPtr<ICoreWindow> mWindow;
  Microsoft::WRL::ComPtr<MetroWidget> mWidget;
  Microsoft::WRL::ComPtr<MetroInput> mMetroInput;
  bool mWinVisible;
  bool mWinActiveState;

  static bool sKeyboardIsVisible;
  static Rect sKeyboardRect;
  static HSTRING sActivationURI;
  static ABI::Windows::ApplicationModel::Activation::ApplicationExecutionState sPreviousExecutionState;
};

} } }
