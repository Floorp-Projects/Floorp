/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "mozwrlbase.h"

#include <windows.system.h>
#include <windows.ui.core.h>
#include <Windows.ApplicationModel.core.h>
#include <Windows.ApplicationModel.h>
#include <Windows.Applicationmodel.Activation.h>

class MetroWidget;

namespace mozilla {
namespace widget {
namespace winrt {

class FrameworkView;

class MetroApp : public Microsoft::WRL::RuntimeClass<ABI::Windows::ApplicationModel::Core::IFrameworkViewSource>
{
  InspectableClass(L"MetroApp", TrustLevel::BaseTrust)

  typedef ABI::Windows::UI::Core::CoreDispatcherPriority CoreDispatcherPriority;
  typedef ABI::Windows::ApplicationModel::Activation::LaunchActivatedEventArgs LaunchActivatedEventArgs;
  typedef ABI::Windows::ApplicationModel::ISuspendingEventArgs ISuspendingEventArgs;
  typedef ABI::Windows::ApplicationModel::Core::IFrameworkView IFrameworkView;
  typedef ABI::Windows::ApplicationModel::Core::ICoreApplication ICoreApplication;

public:
  // IFrameworkViewSource
  STDMETHODIMP CreateView(IFrameworkView **viewProvider);

  // ICoreApplication event
  HRESULT OnSuspending(IInspectable* aSender, ISuspendingEventArgs* aArgs);
  HRESULT OnResuming(IInspectable* aSender, IInspectable* aArgs);

  // nsIWinMetroUtils tile related async callbacks
  HRESULT OnAsyncTileCreated(ABI::Windows::Foundation::IAsyncOperation<bool>* aOperation, AsyncStatus aStatus);

  void Initialize();
  void CoreExit();

  void ShutdownXPCOM();

  // Shared pointers between framework and widget
  static void SetBaseWidget(MetroWidget* aPtr);
  static void PostSuspendResumeProcessNotification(bool aIsSuspend);
  static void PostSleepWakeNotification(bool aIsSuspend);

private:
  EventRegistrationToken mSuspendEvent;
  EventRegistrationToken mResumeEvent;
};

} } }
