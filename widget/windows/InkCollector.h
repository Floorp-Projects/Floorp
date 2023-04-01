/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef InkCollector_h__
#define InkCollector_h__

#include <objbase.h>
#include <msinkaut.h>
#include "mozilla/StaticPtr.h"

#define MOZ_WM_PEN_LEAVES_HOVER_OF_DIGITIZER WM_USER + 0x83

class InkCollectorEvent final : public _IInkCollectorEvents {
 public:
  // IUnknown
  HRESULT __stdcall QueryInterface(REFIID aRiid, void** aObject);
  virtual ULONG STDMETHODCALLTYPE AddRef() { return ++mRefCount; }
  virtual ULONG STDMETHODCALLTYPE Release() {
    MOZ_ASSERT(mRefCount);
    if (!--mRefCount) {
      delete this;
      return 0;
    }
    return mRefCount;
  }

 protected:
  // IDispatch
  STDMETHOD(GetTypeInfoCount)(UINT* aInfo) { return E_NOTIMPL; }
  STDMETHOD(GetTypeInfo)(UINT aInfo, LCID aId, ITypeInfo** aTInfo) {
    return E_NOTIMPL;
  }
  STDMETHOD(GetIDsOfNames)
  (REFIID aRiid, LPOLESTR* aStrNames, UINT aNames, LCID aId, DISPID* aDispId) {
    return E_NOTIMPL;
  }
  STDMETHOD(Invoke)
  (DISPID aDispIdMember, REFIID aRiid, LCID aId, WORD wFlags,
   DISPPARAMS* aDispParams, VARIANT* aVarResult, EXCEPINFO* aExcepInfo,
   UINT* aArgErr);

  // InkCollectorEvent
  void CursorOutOfRange(IInkCursor* aCursor) const;
  bool IsHardProximityTablet(IInkTablet* aTablet) const;

 private:
  uint32_t mRefCount = 0;

  ~InkCollectorEvent() = default;
};

class InkCollector {
 public:
  ~InkCollector();
  void Shutdown();

  HWND GetTarget();
  void SetTarget(HWND aTargetWindow);
  void ClearTarget();

  uint16_t GetPointerId();  // 0 shows that there is no existing pen.
  void SetPointerId(uint16_t aPointerId);
  void ClearPointerId();

  static mozilla::StaticAutoPtr<InkCollector> sInkCollector;

 protected:
  void Initialize();
  void OnInitialize();
  void Enable(bool aNewState);

 private:
  RefPtr<IUnknown> mMarshaller;
  RefPtr<IInkCollector> mInkCollector;
  RefPtr<IConnectionPoint> mConnectionPoint;
  RefPtr<InkCollectorEvent> mInkCollectorEvent;

  HWND mTargetWindow = 0;
  DWORD mCookie = 0;
  bool mComInitialized = false;
  bool mEnabled = false;

  // This value holds the previous pointerId of the pen, and is used by the
  // nsWindow when processing a MOZ_WM_PEN_LEAVES_HOVER_OF_DIGITIZER which
  // indicates that a pen leaves the digitizer.

  // TODO: If we move our implementation to window pointer input messages, then
  // we no longer need this value, since the pointerId can be retrieved from the
  // window message, please refer to
  // https://msdn.microsoft.com/en-us/library/windows/desktop/hh454916(v=vs.85).aspx

  // NOTE: The pointerId of a pen shouldn't be 0 on a Windows platform, since 0
  // is reserved of the mouse, please refer to
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ms703320(v=vs.85).aspx
  uint16_t mPointerId = 0;
};

#endif  // InkCollector_h__
