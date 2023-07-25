/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "InkCollector.h"
#include "nsDebug.h"
#include "nsISupportsUtils.h"

// Msinkaut_i.c and Msinkaut.h should both be included
// https://msdn.microsoft.com/en-us/library/windows/desktop/ms695519.aspx
#include <msinkaut_i.c>

mozilla::StaticAutoPtr<InkCollector> InkCollector::sInkCollector;

InkCollector::~InkCollector() {
  Shutdown();
  MOZ_ASSERT(!mCookie && !mEnabled && !mComInitialized && !mMarshaller &&
             !mInkCollector && !mConnectionPoint && !mInkCollectorEvent);
}

void InkCollector::Initialize() {
  // Possibly, we can use mConnectionPoint for checking,
  // But if errors exist (perhaps COM object is unavailable),
  // Initialize() will be called more times.
  static bool sInkCollectorCreated = false;
  if (sInkCollectorCreated) {
    return;
  }
  sInkCollectorCreated = true;

  // COM could get uninitialized due to previous initialization.
  mComInitialized = SUCCEEDED(::CoInitialize(nullptr));

  // Set up instance of InkCollectorEvent.
  mInkCollectorEvent = new InkCollectorEvent();

  // Set up a free threaded marshaler.
  if (FAILED(::CoCreateFreeThreadedMarshaler(mInkCollectorEvent,
                                             getter_AddRefs(mMarshaller)))) {
    return;
  }

  // Create the ink collector.
  if (FAILED(::CoCreateInstance(CLSID_InkCollector, NULL, CLSCTX_INPROC_SERVER,
                                IID_IInkCollector,
                                getter_AddRefs(mInkCollector)))) {
    return;
  }

  // Set up connection between sink and InkCollector.
  RefPtr<IConnectionPointContainer> connPointContainer;

  // Get the connection point container.
  if (SUCCEEDED(mInkCollector->QueryInterface(
          IID_IConnectionPointContainer, getter_AddRefs(connPointContainer)))) {
    // Find the connection point for Ink Collector events.
    if (SUCCEEDED(connPointContainer->FindConnectionPoint(
            __uuidof(_IInkCollectorEvents),
            getter_AddRefs(mConnectionPoint)))) {
      // Hook up sink to connection point.
      if (SUCCEEDED(mConnectionPoint->Advise(mInkCollectorEvent, &mCookie))) {
        OnInitialize();
      }
    }
  }
}

void InkCollector::Shutdown() {
  Enable(false);
  if (mConnectionPoint) {
    // Remove the connection of the sink to the Ink Collector.
    mConnectionPoint->Unadvise(mCookie);
    mCookie = 0;
    mConnectionPoint = nullptr;
  }
  mInkCollector = nullptr;
  mMarshaller = nullptr;
  mInkCollectorEvent = nullptr;

  // Let uninitialization get handled in a place where it got inited.
  if (mComInitialized) {
    CoUninitialize();
    mComInitialized = false;
  }
}

void InkCollector::OnInitialize() {
  // Suppress all events to do not allow performance decreasing.
  // https://msdn.microsoft.com/en-us/library/ms820347.aspx
  mInkCollector->SetEventInterest(InkCollectorEventInterest::ICEI_AllEvents,
                                  VARIANT_FALSE);

  // Sets a value that indicates whether an object or control has interest in a
  // specified event.
  mInkCollector->SetEventInterest(
      InkCollectorEventInterest::ICEI_CursorOutOfRange, VARIANT_TRUE);

  // If the MousePointer property is set to IMP_Custom and the MouseIcon
  // property is NULL, Then the ink collector no longer handles mouse cursor
  // settings.
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ms700686.aspx
  mInkCollector->put_MouseIcon(nullptr);
  mInkCollector->put_MousePointer(InkMousePointer::IMP_Custom);

  // This mode allows an ink collector to collect ink from any tablet attached
  // to the Tablet PC. The Boolean value that indicates whether to use the mouse
  // as an input device. If TRUE, the mouse is used for input.
  // https://msdn.microsoft.com/en-us/library/ms820346.aspx
  mInkCollector->SetAllTabletsMode(VARIANT_FALSE);

  // Sets the value that specifies whether ink is rendered as it is drawn.
  // VARIANT_TRUE to render ink as it is drawn on the display.
  // VARIANT_FALSE to not have the ink appear on the display as strokes are
  // made.
  // https://msdn.microsoft.com/en-us/library/windows/desktop/dd314598.aspx
  mInkCollector->put_DynamicRendering(VARIANT_FALSE);

  // Set AutoRedraw to false to prevent repainting the ink when the window is
  // invalidated.
  mInkCollector->put_AutoRedraw(VARIANT_FALSE);
}

// Sets a value that specifies whether the InkCollector object collects pen
// input. This property must be set to FALSE before setting or calling specific
// properties and methods of the object.
// https://msdn.microsoft.com/en-us/library/windows/desktop/ms701721.aspx
void InkCollector::Enable(bool aNewState) {
  if (aNewState != mEnabled) {
    if (mInkCollector) {
      if (SUCCEEDED(mInkCollector->put_Enabled(aNewState ? VARIANT_TRUE
                                                         : VARIANT_FALSE))) {
        mEnabled = aNewState;
      } else {
        NS_WARNING("InkCollector did not change status successfully");
      }
    } else {
      NS_WARNING("InkCollector should be exist");
    }
  }
}

HWND InkCollector::GetTarget() { return mTargetWindow; }

void InkCollector::SetTarget(HWND aTargetWindow) {
  NS_ASSERTION(aTargetWindow, "aTargetWindow should be exist");
  if (aTargetWindow && (aTargetWindow != mTargetWindow)) {
    Initialize();
    if (mInkCollector) {
      Enable(false);
      if (SUCCEEDED(mInkCollector->put_hWnd((LONG_PTR)aTargetWindow))) {
        mTargetWindow = aTargetWindow;
      } else {
        NS_WARNING("InkCollector did not change window property successfully");
      }
      Enable(true);
    }
  }
}

void InkCollector::ClearTarget() {
  if (mTargetWindow && mInkCollector) {
    Enable(false);
    if (SUCCEEDED(mInkCollector->put_hWnd(0))) {
      mTargetWindow = 0;
    } else {
      NS_WARNING("InkCollector did not clear window property successfully");
    }
  }
}

uint16_t InkCollector::GetPointerId() { return mPointerId; }

void InkCollector::SetPointerId(uint16_t aPointerId) {
  mPointerId = aPointerId;
}

void InkCollector::ClearPointerId() { mPointerId = 0; }

// The display and the digitizer have quite different properties.
// The display has CursorMustTouch, the mouse pointer alway touches the display
// surface. The digitizer lists Integrated and HardProximity. When the stylus is
// in the proximity of the tablet its movements are also detected. An external
// tablet will only list HardProximity.
bool InkCollectorEvent::IsHardProximityTablet(IInkTablet* aTablet) const {
  if (aTablet) {
    TabletHardwareCapabilities caps;
    if (SUCCEEDED(aTablet->get_HardwareCapabilities(&caps))) {
      return (TabletHardwareCapabilities::THWC_HardProximity & caps);
    }
  }
  return false;
}

HRESULT __stdcall InkCollectorEvent::QueryInterface(REFIID aRiid,
                                                    void** aObject) {
  // Validate the input
  if (!aObject) {
    return E_POINTER;
  }
  HRESULT result = E_NOINTERFACE;
  // This object supports IUnknown/IDispatch/IInkCollectorEvents
  if ((IID_IUnknown == aRiid) || (IID_IDispatch == aRiid) ||
      (DIID__IInkCollectorEvents == aRiid)) {
    *aObject = this;
    // AddRef should be called when we give info about interface
    NS_ADDREF_THIS();
    result = S_OK;
  }
  return result;
}

HRESULT InkCollectorEvent::Invoke(DISPID aDispIdMember, REFIID /*aRiid*/,
                                  LCID /*aId*/, WORD /*wFlags*/,
                                  DISPPARAMS* aDispParams,
                                  VARIANT* /*aVarResult*/,
                                  EXCEPINFO* /*aExcepInfo*/,
                                  UINT* /*aArgErr*/) {
  switch (aDispIdMember) {
    case DISPID_ICECursorOutOfRange: {
      if (aDispParams && aDispParams->cArgs) {
        CursorOutOfRange(
            static_cast<IInkCursor*>(aDispParams->rgvarg[0].pdispVal));
      }
      break;
    }
  }
  return S_OK;
}

void InkCollectorEvent::CursorOutOfRange(IInkCursor* aCursor) const {
  IInkTablet* curTablet = nullptr;
  if (FAILED(aCursor->get_Tablet(&curTablet))) {
    return;
  }
  // All events should be suppressed except
  // from tablets with hard proximity.
  if (!IsHardProximityTablet(curTablet)) {
    return;
  }
  // Notify current target window.
  if (HWND targetWindow = InkCollector::sInkCollector->GetTarget()) {
    ::SendMessage(targetWindow, MOZ_WM_PEN_LEAVES_HOVER_OF_DIGITIZER, 0, 0);
  }
}
