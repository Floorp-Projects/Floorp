/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UIABridge.h"
#include "MetroUtils.h"
#include "UIABridgePrivate.h"

#include <wrl.h>
#include <OAIdl.h>
#include <windows.graphics.display.h>

#ifdef ACCESSIBILITY
using namespace mozilla::a11y;
#endif
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::UI;
using namespace ABI::Windows::UI::Core;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::System;

//#define DEBUG_BRIDGE
#if !defined(DEBUG_BRIDGE)
#undef LogThread
#undef LogFunction
#undef Log
#define LogThread() 
#define LogFunction()
#define Log(...)
#endif

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
 const type name = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}
MIDL_DEFINE_GUID(IID, IID_IUIABridge,0x343159D8,0xB1E9,0x4464,0x82,0xFC,0xB1,0x2C,0x7A,0x47,0x3C,0xF1);

namespace mozilla {
namespace widget {
namespace winrt {

#define ProviderOptions_UseClientCoordinates (ProviderOptions)0x100

static int gIDIndex = 2;
static ComPtr<IUIABridge> gProviderRoot = nullptr;
static ComPtr<IUIAElement> gElement = nullptr;

HRESULT
UIABridge_CreateInstance(IInspectable **retVal)
{
  LogFunction();
  HRESULT hr = E_OUTOFMEMORY;
  *retVal = nullptr;
  ComPtr<UIABridge> spProvider = Make<UIABridge>();
  if (spProvider != nullptr &&
      SUCCEEDED(hr = spProvider.Get()->QueryInterface(IID_PPV_ARGS(retVal))) &&
      SUCCEEDED(hr = spProvider.Get()->QueryInterface(IID_PPV_ARGS(&gProviderRoot)))) {
    return S_OK;
  }
  return hr;
}

HRESULT
UIATextElement_CreateInstance(IRawElementProviderFragmentRoot* aRoot)
{
  LogFunction();
  HRESULT hr = E_OUTOFMEMORY;
  ComPtr<UIATextElement> spProvider = Make<UIATextElement>();
  if (spProvider != nullptr &&
      SUCCEEDED(hr = spProvider.Get()->QueryInterface(IID_PPV_ARGS(&gElement)))) {
    spProvider->SetIndexID(gIDIndex++);
    return S_OK;
  }
  return hr;
}

// IUIABridge

HRESULT
UIABridge::Init(IInspectable* aView, IInspectable* aWindow, LONG_PTR aInnerPtr)
{
  LogFunction();
  NS_ASSERTION(aView, "invalid framework view pointer");
  NS_ASSERTION(aWindow, "invalid window pointer");
  NS_ASSERTION(aInnerPtr, "invalid Accessible pointer");

#if defined(ACCESSIBILITY)
  // init AccessibilityBridge and connect to accessibility
  mAccBridge = new AccessibilityBridge();
  if (!mAccBridge->Init(CastToUnknown(), (Accessible*)aInnerPtr)) {
    return E_FAIL;
  }

  aWindow->QueryInterface(IID_PPV_ARGS(&mWindow));

  if (FAILED(UIATextElement_CreateInstance(this)))
    return E_FAIL;

  mAccessible = (Accessible*)aInnerPtr;

  return S_OK;
#endif
  return E_FAIL;
}

HRESULT
UIABridge::Disconnect()
{
  LogFunction();
#if defined(ACCESSIBILITY)
  mAccBridge->Disconnect();
  mAccessible = nullptr;
#endif
  mWindow = nullptr;
  return S_OK;
}

bool
UIABridge::Connected()
{
  return !!mAccessible;
}

// IUIAElement

HRESULT
UIABridge::SetFocusInternal(LONG_PTR aAccessible)
{
  return S_OK;
}

HRESULT
UIABridge::ClearFocus()
{
  return S_OK;
}

static void
DumpChildInfo(nsCOMPtr<nsIAccessible>& aChild)
{
#ifdef DEBUG
  if (!aChild) {
    return;
  }
  nsString str;
  aChild->GetName(str);
  Log(L"name: %s", str.BeginReading());
  aChild->GetDescription(str);
  Log(L"description: %s", str.BeginReading());
#endif
}

static bool
ChildHasFocus(nsCOMPtr<nsIAccessible>& aChild)
{
  Accessible* access = (Accessible*)aChild.get();
  Log(L"Focus element flags: %d %d %d",
    ((access->NativeState() & mozilla::a11y::states::EDITABLE) > 0),
    ((access->NativeState() & mozilla::a11y::states::FOCUSABLE) > 0),
    ((access->NativeState() & mozilla::a11y::states::READONLY) > 0));
  return (((access->NativeState() & mozilla::a11y::states::EDITABLE) > 0) &&
           ((access->NativeState() & mozilla::a11y::states::READONLY) == 0));
}

// Accessibility calls here to let us know about focus related changes.
// The only event we are concerned with is lost focus, so that we can
// signal UIA that the focus on our child has been lost.
HRESULT
UIABridge::FocusChangeEvent()
{
  LogFunction();

  if (!Connected()) {
    return UIA_E_ELEMENTNOTAVAILABLE;
  }

  nsCOMPtr<nsIAccessible> child;
  nsresult rv = mAccessible->GetFocusedChild(getter_AddRefs(child));
  if (!child) {
    return S_OK;
  }

  DumpChildInfo(child);

  if (!ChildHasFocus(child)) {
    ComPtr<IUIAElement> element;
    gElement.As(&element);
    if (!element) {
      return S_OK;
    }

    element->ClearFocus();
    UiaRaiseAutomationEvent(this, UIA_AutomationFocusChangedEventId);
  }

  return S_OK;
}

// IRawElementProviderFragmentRoot

HRESULT
UIABridge::ElementProviderFromPoint(double x, double y, IRawElementProviderFragment ** retVal)
{
  LogFunction();
  *retVal = nullptr;
  if (!Connected()) {
    return UIA_E_ELEMENTNOTAVAILABLE;
  }
  gElement.Get()->QueryInterface(IID_PPV_ARGS(retVal));
  return S_OK;
}

// Windows calls this looking for the current focus element. Windows
// will call here before accessible sends up any observer events through
// the accessibility bridge, so update child focus information.
HRESULT
UIABridge::GetFocus(IRawElementProviderFragment ** retVal)
{
  LogFunction();
  if (!Connected()) {
    return UIA_E_ELEMENTNOTAVAILABLE;
  }

  nsCOMPtr<nsIAccessible> child;
  nsresult rv = mAccessible->GetFocusedChild(getter_AddRefs(child));
  if (!child) {
    return S_OK;
  }

  DumpChildInfo(child);

  ComPtr<IUIAElement> element;
  gElement.As(&element);
  if (!element) {
    return S_OK;
  }

  if (!ChildHasFocus(child)) {
    element->ClearFocus();
  } else {
    element->SetFocusInternal((LONG_PTR)child.get());
    element.Get()->QueryInterface(IID_PPV_ARGS(retVal));
  }

  return S_OK;
}

// IRawElementProviderFragment

HRESULT
UIABridge::Navigate(NavigateDirection direction, IRawElementProviderFragment ** retVal)
{
  LogFunction();
  if (!Connected()) {
    return UIA_E_ELEMENTNOTAVAILABLE;
  }
  *retVal = nullptr;

  switch(direction) {
    case NavigateDirection_Parent:
    break;
    case NavigateDirection_NextSibling:
    break;
    case NavigateDirection_PreviousSibling:
    break;
    case NavigateDirection_FirstChild:
    gElement.Get()->QueryInterface(IID_PPV_ARGS(retVal));
    break;
    case NavigateDirection_LastChild:
    gElement.Get()->QueryInterface(IID_PPV_ARGS(retVal));
    break;
  }

  // For the other directions (parent, next, previous) the default of nullptr is correct
  return S_OK;
}

HRESULT
UIABridge::GetRuntimeId(SAFEARRAY ** retVal)
{
  if (!Connected()) {
    return UIA_E_ELEMENTNOTAVAILABLE;
  }

  int runtimeId[2] = { UiaAppendRuntimeId, 1 }; // always 1
  *retVal = SafeArrayCreateVector(VT_I4, 0, ARRAYSIZE(runtimeId));
  if (*retVal != nullptr) {
    for (long index = 0; index < ARRAYSIZE(runtimeId); ++index) {
      SafeArrayPutElement(*retVal, &index, &runtimeId[index]);
    }
  } else {
    return E_OUTOFMEMORY;
  }
  return S_OK;
}

HRESULT
UIABridge::get_BoundingRectangle(UiaRect * retVal)
{
  LogFunction();
  if (!Connected() || !mWindow) {
    return UIA_E_ELEMENTNOTAVAILABLE;
  }

  // returns logical pixels
  Rect bounds;
  mWindow->get_Bounds(&bounds);

  // we need to return physical pixels
  retVal->left = MetroUtils::LogToPhys(bounds.X);
  retVal->top = MetroUtils::LogToPhys(bounds.Y);
  retVal->width = MetroUtils::LogToPhys(bounds.Width);
  retVal->height = MetroUtils::LogToPhys(bounds.Height);

  return S_OK;
}

HRESULT
UIABridge::GetEmbeddedFragmentRoots(SAFEARRAY ** retVal)
{
  if (!Connected()) {
    return UIA_E_ELEMENTNOTAVAILABLE;
  }
  // doesn't apply according to msdn.
  *retVal = nullptr;
  return S_OK;
}

HRESULT
UIABridge::SetFocus()
{
  LogFunction();
  if (!Connected()) {
    return UIA_E_ELEMENTNOTAVAILABLE;
  }
  return S_OK;
}

HRESULT
UIABridge::get_FragmentRoot(IRawElementProviderFragmentRoot ** retVal)
{
  // we are the fragment root. Our children return us for this call.
  return QueryInterface(IID_PPV_ARGS(retVal));
}

// IRawElementProviderSimple

HRESULT
UIABridge::get_ProviderOptions(ProviderOptions * pRetVal)
{
  if (!Connected()) {
    return E_FAIL;
  }
  *pRetVal = ProviderOptions_ServerSideProvider | 
             ProviderOptions_UseComThreading | 
             ProviderOptions_UseClientCoordinates;
  return S_OK;
}

HRESULT
UIABridge::GetPatternProvider(PATTERNID patternId, IUnknown **ppRetVal)
{
  LogFunction();
  Log(L"UIABridge::GetPatternProvider=%d", patternId);

  // The root window doesn't support any specific pattern
  *ppRetVal = nullptr;

  return S_OK;
}

HRESULT
UIABridge::GetPropertyValue(PROPERTYID idProp, VARIANT * pRetVal)
{
  LogFunction();
  pRetVal->vt = VT_EMPTY;

  // native hwnd
  if (idProp == 30020) {
    return S_OK;
  }

  Log(L"UIABridge::GetPropertyValue: idProp=%d", idProp);

  if (!Connected()) {
    return E_FAIL;
  }

  switch (idProp) {
    case UIA_AutomationIdPropertyId:
      pRetVal->bstrVal = SysAllocString(L"MozillaAccessibilityBridge0001");
      pRetVal->vt = VT_BSTR;
      break;

    case UIA_ControlTypePropertyId:
      pRetVal->vt = VT_I4;
      pRetVal->lVal = UIA_WindowControlTypeId;
      break;

    case UIA_IsKeyboardFocusablePropertyId:
    case UIA_IsContentElementPropertyId:
    case UIA_IsControlElementPropertyId:
    case UIA_IsEnabledPropertyId:
      pRetVal->boolVal = VARIANT_TRUE;
      pRetVal->vt = VT_BOOL;
      break;

    case UIA_HasKeyboardFocusPropertyId:
      pRetVal->vt = VT_BOOL;
      pRetVal->boolVal = VARIANT_FALSE;
      break;

    case UIA_NamePropertyId:
      pRetVal->bstrVal = SysAllocString(L"MozillaAccessibilityBridge");
      pRetVal->vt = VT_BSTR;
      break;

    case UIA_IsPasswordPropertyId:
      pRetVal->vt = VT_BOOL;
      pRetVal->boolVal = VARIANT_FALSE;
      break;

    case UIA_NativeWindowHandlePropertyId:
    break;

    default:
      Log(L"UIABridge: Unhandled property");
      break;
  }
  return S_OK;
}

HRESULT
UIABridge::get_HostRawElementProvider(IRawElementProviderSimple **ppRetVal)
{
  // We only have this in the root bridge - this is our parent ICoreWindow.
  *ppRetVal = nullptr;
  if (mWindow != nullptr) {
    IInspectable *pHostAsInspectable = nullptr;
    if (SUCCEEDED(mWindow->get_AutomationHostProvider(&pHostAsInspectable))) {
      pHostAsInspectable->QueryInterface(ppRetVal);
      pHostAsInspectable->Release();
    }
  }
  return S_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Element

HRESULT
UIATextElement::SetFocusInternal(LONG_PTR aAccessible)
{
  LogFunction();
#if defined(ACCESSIBILITY)
  NS_ASSERTION(mAccessItem, "Bad accessible pointer");
  mAccessItem = (nsIAccessible*)aAccessible;
  return S_OK;
#endif
  return E_FAIL;
}

HRESULT
UIATextElement::ClearFocus()
{
  LogFunction();
  mAccessItem = nullptr;
  return S_OK;
}

// IRawElementProviderFragment

HRESULT
UIATextElement::Navigate(NavigateDirection direction, IRawElementProviderFragment ** retVal)
{
  LogFunction();

  *retVal = nullptr;
  switch(direction) {
    case NavigateDirection_Parent:
    gProviderRoot.Get()->QueryInterface(IID_PPV_ARGS(retVal));
    break;
    case NavigateDirection_NextSibling:
    break;
    case NavigateDirection_PreviousSibling:
    break;
    case NavigateDirection_FirstChild:
    break;
    case NavigateDirection_LastChild:
    break;
  }
  return S_OK;
}

HRESULT
UIATextElement::GetRuntimeId(SAFEARRAY ** retVal)
{
  int runtimeId[2] = { UiaAppendRuntimeId, mIndexID };
  *retVal = SafeArrayCreateVector(VT_I4, 0, ARRAYSIZE(runtimeId));
  if (*retVal != nullptr) {
    for (long index = 0; index < ARRAYSIZE(runtimeId); ++index) {
      SafeArrayPutElement(*retVal, &index, &runtimeId[index]);
    }
  } else {
    return E_OUTOFMEMORY;
  }
  return S_OK;
}

HRESULT
UIATextElement::get_BoundingRectangle(UiaRect * retVal)
{
  LogFunction();
  
  if (!mAccessItem) {
    return UIA_E_ELEMENTNOTAVAILABLE;
  }

  // bounds are in physical pixels
  int32_t docX = 0, docY = 0, docWidth = 0, docHeight = 0;
  mAccessItem->GetBounds(&docX, &docY, &docWidth, &docHeight);
  retVal->left = (float)docX;
  retVal->top = (float)docY;
  retVal->width = (float)docWidth;
  retVal->height = (float)docHeight;
  return S_OK;
}

HRESULT
UIATextElement::GetEmbeddedFragmentRoots(SAFEARRAY ** retVal)
{
  *retVal = nullptr;
  return S_OK;
}

HRESULT
UIATextElement::SetFocus()
{
  LogFunction();
  return S_OK;
}

HRESULT
UIATextElement::get_FragmentRoot(IRawElementProviderFragmentRoot ** retVal)
{
  return gProviderRoot.Get()->QueryInterface(IID_PPV_ARGS(retVal));
}

// IRawElementProviderSimple

HRESULT
UIATextElement::get_ProviderOptions(ProviderOptions * pRetVal)
{
  *pRetVal = ProviderOptions_ServerSideProvider | 
             ProviderOptions_UseComThreading | 
             ProviderOptions_UseClientCoordinates;
  return S_OK;
}

HRESULT
UIATextElement::GetPatternProvider(PATTERNID patternId, IUnknown **ppRetVal)
{
  LogFunction();
  Log(L"UIATextElement::GetPatternProvider=%d", patternId);
  
  // UIA_ValuePatternId - 10002
  // UIA_TextPatternId  - 10014
  // UIA_TextChildPatternId - 10029

  *ppRetVal = nullptr;
  if (patternId == UIA_TextPatternId) {
    Log(L"** TextPattern requested from element.");
    *ppRetVal = static_cast<ITextProvider*>(this);
    AddRef();
    return S_OK;
  } else if (patternId == UIA_ValuePatternId) {
    Log(L"** ValuePattern requested from element.");
    *ppRetVal = static_cast<IValueProvider*>(this);
    AddRef();
    return S_OK;
  }

  return S_OK;
}

HRESULT
UIATextElement::GetPropertyValue(PROPERTYID idProp, VARIANT * pRetVal)
{
  LogFunction();
  pRetVal->vt = VT_EMPTY;

  // native hwnd
  if (idProp == 30020) {
    return S_OK;
  }

  Log(L"UIATextElement::GetPropertyValue: idProp=%d", idProp);

  switch (idProp) {
    case UIA_AutomationIdPropertyId:
      pRetVal->bstrVal = SysAllocString(L"MozillaDocument0001");
      pRetVal->vt = VT_BSTR;
      break;

    case UIA_ControlTypePropertyId:
      pRetVal->vt = VT_I4;
      pRetVal->lVal = UIA_DocumentControlTypeId;
      break;

    case UIA_IsTextPatternAvailablePropertyId:
    case UIA_IsKeyboardFocusablePropertyId:
    case UIA_IsContentElementPropertyId:
    case UIA_IsControlElementPropertyId:
    case UIA_IsEnabledPropertyId:
      pRetVal->boolVal = VARIANT_TRUE;
      pRetVal->vt = VT_BOOL;
      break;

    case UIA_LocalizedControlTypePropertyId:
    case UIA_LabeledByPropertyId:
      break;

    case UIA_HasKeyboardFocusPropertyId: 
    {
      if (mAccessItem) {
        uint32_t state, extraState;
        if (NS_SUCCEEDED(mAccessItem->GetState(&state, &extraState)) &&
            (state & nsIAccessibleStates::STATE_FOCUSED)) {
          pRetVal->vt = VT_BOOL;
          pRetVal->boolVal = VARIANT_TRUE;
          return S_OK;
        }
      }
      pRetVal->vt = VT_BOOL;
      pRetVal->boolVal = VARIANT_FALSE;
      break;
    }

    case UIA_NamePropertyId:
      pRetVal->bstrVal = SysAllocString(L"MozillaDocument");
      pRetVal->vt = VT_BSTR;
      break;

    case UIA_IsPasswordPropertyId:
      pRetVal->vt = VT_BOOL;
      pRetVal->boolVal = VARIANT_FALSE;
      break;

    default:
      Log(L"UIATextElement: Unhandled property");
      break;
  }
  return S_OK;
}

HRESULT
UIATextElement::get_HostRawElementProvider(IRawElementProviderSimple **ppRetVal)
{
  *ppRetVal = nullptr;
  return S_OK;
}

// ITextProvider

HRESULT
UIATextElement::GetSelection(SAFEARRAY * *pRetVal)
{
  LogFunction();
  return E_NOTIMPL;
}

HRESULT
UIATextElement::GetVisibleRanges(SAFEARRAY * *pRetVal)
{
  LogFunction();
  return E_NOTIMPL;
}

HRESULT
UIATextElement::RangeFromChild(IRawElementProviderSimple *childElement, ITextRangeProvider **pRetVal)
{
  LogFunction();
  return E_NOTIMPL;
}

HRESULT
UIATextElement::RangeFromPoint(UiaPoint point, ITextRangeProvider **pRetVal)
{
  LogFunction();
  return E_NOTIMPL;
}

HRESULT
UIATextElement::get_DocumentRange(ITextRangeProvider **pRetVal)
{
  LogFunction();
  return E_NOTIMPL;
}

HRESULT
UIATextElement::get_SupportedTextSelection(SupportedTextSelection *pRetVal)
{
  LogFunction();
  return E_NOTIMPL;
}

// IValueProvider

IFACEMETHODIMP
UIATextElement::SetValue(LPCWSTR val)
{
  LogFunction();
  return E_NOTIMPL;
}

IFACEMETHODIMP
UIATextElement::get_Value(BSTR *pRetVal)
{
  LogFunction();
  return E_NOTIMPL;
}

IFACEMETHODIMP
UIATextElement::get_IsReadOnly(BOOL *pRetVal)
{
  LogFunction();
  *pRetVal = FALSE;
  return S_OK;
}

} } }
