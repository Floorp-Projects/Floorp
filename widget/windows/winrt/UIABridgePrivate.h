/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <windows.system.h>
#include <windows.ui.core.h>
#include <UIAutomation.h>
#include <UIAutomationCore.h>
#include <UIAutomationCoreApi.h>
#include <wrl.h>

#include "UIAAccessibilityBridge.h"

// generated
#include "UIABridge.h"

namespace mozilla {
namespace widget {
namespace winrt {

using namespace Microsoft::WRL;

// represents the root window to UIA
[uuid("D3EDD951-0715-4501-A8E5-25D97EF35D5A")]
class UIABridge : public RuntimeClass<RuntimeClassFlags<RuntimeClassType::WinRtClassicComMix>,
  IUIABridge,
  IUIAElement,
  IRawElementProviderSimple,
  IRawElementProviderFragment,
  IRawElementProviderFragmentRoot>
{
  typedef ABI::Windows::UI::Core::ICoreWindow ICoreWindow;

  InspectableClass(L"IUIABridge", BaseTrust);

public:
  UIABridge() :
    mConnected(false),
    mHasFocus(false)
  {}

  // IUIABridge
#if defined(x86_64)
  IFACEMETHODIMP Init(IInspectable* view, IInspectable* window, __int64 inner);
#else
  IFACEMETHODIMP Init(IInspectable* view, IInspectable* window, __int32 inner);
#endif
  IFACEMETHODIMP Disconnect();

  // IUIAElement
#if defined(x86_64)
  IFACEMETHODIMP SetFocusInternal(__int64 aAccessible);
#else
  IFACEMETHODIMP SetFocusInternal(__int32 aAccessible);
#endif
  IFACEMETHODIMP CheckFocus(int x, int y);
  IFACEMETHODIMP ClearFocus();
  IFACEMETHODIMP HasFocus(VARIANT_BOOL * hasFocus);

  // IRawElementProviderFragmentRoot
  IFACEMETHODIMP ElementProviderFromPoint(double x, double y, IRawElementProviderFragment ** retVal);
  IFACEMETHODIMP GetFocus(IRawElementProviderFragment ** retVal);

  // IRawElementProviderFragment
  IFACEMETHODIMP Navigate(NavigateDirection direction, IRawElementProviderFragment ** retVal);
  IFACEMETHODIMP GetRuntimeId(SAFEARRAY ** retVal);
  IFACEMETHODIMP get_BoundingRectangle(UiaRect * retVal);
  IFACEMETHODIMP GetEmbeddedFragmentRoots(SAFEARRAY ** retVal);
  IFACEMETHODIMP SetFocus();
  IFACEMETHODIMP get_FragmentRoot(IRawElementProviderFragmentRoot * * retVal);

  // IRawElementProviderSimple
  IFACEMETHODIMP get_ProviderOptions(ProviderOptions * retVal);
  IFACEMETHODIMP GetPatternProvider(PATTERNID iid, IUnknown * * retVal);
  IFACEMETHODIMP GetPropertyValue(PROPERTYID idProp, VARIANT * retVal );
  IFACEMETHODIMP get_HostRawElementProvider(IRawElementProviderSimple ** retVal);

protected:
  bool Connected();

private:
  bool mConnected;
  Microsoft::WRL::ComPtr<ICoreWindow> mWindow;
#if defined(ACCESSIBILITY)
  nsRefPtr<AccessibilityBridge> mBridge;
#endif
  bool mHasFocus;
};

[uuid("4438135F-F624-43DE-A417-275CE7A1A0CD")]
class UIATextElement : public RuntimeClass<RuntimeClassFlags<RuntimeClassType::WinRtClassicComMix>,
  IUIAElement,
  IRawElementProviderSimple,
  IRawElementProviderFragment,
  ITextProvider,
  IValueProvider >
{
  typedef ABI::Windows::Foundation::Rect Rect;

  InspectableClass(L"UIATextElement", BaseTrust);

public:
  UIATextElement() :
    mHasFocus(false)
  {}

  // IUIAElement
#if defined(x86_64)
  IFACEMETHODIMP SetFocusInternal(__int64 aAccessible);
#else
  IFACEMETHODIMP SetFocusInternal(__int32 aAccessible);
#endif
  IFACEMETHODIMP CheckFocus(int x, int y);
  IFACEMETHODIMP ClearFocus();
  IFACEMETHODIMP HasFocus(VARIANT_BOOL * hasFocus);

  // IRawElementProviderFragment
  IFACEMETHODIMP Navigate(NavigateDirection direction, IRawElementProviderFragment ** retVal);
  IFACEMETHODIMP GetRuntimeId(SAFEARRAY ** retVal);
  IFACEMETHODIMP get_BoundingRectangle(UiaRect * retVal);
  IFACEMETHODIMP GetEmbeddedFragmentRoots(SAFEARRAY ** retVal);
  IFACEMETHODIMP SetFocus();
  IFACEMETHODIMP get_FragmentRoot(IRawElementProviderFragmentRoot * * retVal);

  // IRawElementProviderSimple
  IFACEMETHODIMP get_ProviderOptions(ProviderOptions * retVal);
  IFACEMETHODIMP GetPatternProvider(PATTERNID iid, IUnknown * * retVal);
  IFACEMETHODIMP GetPropertyValue(PROPERTYID idProp, VARIANT * retVal );
  IFACEMETHODIMP get_HostRawElementProvider(IRawElementProviderSimple ** retVal);

  // ITextProvider
  IFACEMETHODIMP GetSelection(SAFEARRAY * *pRetVal);
  IFACEMETHODIMP GetVisibleRanges(SAFEARRAY * *pRetVal);
  IFACEMETHODIMP RangeFromChild(IRawElementProviderSimple *childElement, ITextRangeProvider **pRetVal);
  IFACEMETHODIMP RangeFromPoint(UiaPoint point, ITextRangeProvider **pRetVal);
  IFACEMETHODIMP get_DocumentRange(ITextRangeProvider **pRetVal);
  IFACEMETHODIMP get_SupportedTextSelection(SupportedTextSelection *pRetVal);

  // IValueProvider
  IFACEMETHODIMP SetValue(LPCWSTR val);
  IFACEMETHODIMP get_Value(BSTR *pRetVal);
  IFACEMETHODIMP get_IsReadOnly(BOOL *pRetVal);

  void SetIndexID(int id) {
    mIndexID = id;
  }

private:
  int mIndexID;
  Rect mBounds;
  bool mHasFocus;
};

} } }
