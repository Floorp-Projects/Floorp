/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include "BaseFilter.h"
#include "BasePin.h"

namespace mozilla {
namespace media {

// Used by BaseFilter to enumerate pins on a DirectShow filter.
// If the number of pins on the filter changes while an EnumPins
// is enumerating pins on a filter, methods start failing with 
// return value VFW_E_ENUM_OUT_OF_SYNC. Really only an issue when
// doing dynamic filter reconnections.
//
// Implements:
//  * IEnumPins
//  * IUnknown
//
class DECLSPEC_UUID("0e9924bd-1cb8-48ad-ba31-2fb831b162be")
  EnumPins : public IEnumPins
{
public:

  EnumPins(BaseFilter* aFilter)
    : mFilter(aFilter)
    , mRefCnt(0)
  {
    Reset();
  }
  EnumPins(EnumPins* aEnumPins)
    : mFilter(aEnumPins->mFilter)
    , mNumPins(aEnumPins->mNumPins)
    , mPinIdx(aEnumPins->mPinIdx)
    , mRefCnt(0)
  {
  }


  virtual ~EnumPins() {}

  STDMETHODIMP QueryInterface(REFIID aIId, void **aInterface)
  {
    if (!aInterface) {
      return E_POINTER;
    }

    if (aIId == IID_IEnumPins) {
      *aInterface = static_cast<IEnumPins*>(this);
      AddRef();
      return S_OK;
    }

    return E_NOINTERFACE;
  }
  STDMETHODIMP_(ULONG) AddRef()
  {
    return ::InterlockedIncrement(&mRefCnt);
  }

  STDMETHODIMP_(ULONG) Release()
  {
    unsigned long newRefCnt = ::InterlockedDecrement(&mRefCnt);

    if (!newRefCnt) {
      delete this;
    }

    return newRefCnt;
  }

  // IEnumPins methods.
  STDMETHODIMP Next(ULONG aNumPins,
                    IPin** aPinArray,
                    ULONG* aNumFetched)
  {
    if (!aPinArray)
      return E_POINTER;
  
    if (!aNumFetched && aNumPins != 1)
      return E_INVALIDARG;
  
    if (IsOutOfSync())
      return VFW_E_ENUM_OUT_OF_SYNC;
    
    unsigned int numFetched = 0;
  
    while (numFetched < aNumPins && mPinIdx < mNumPins) {
      IPinPtr pin = mFilter->GetPin(mPinIdx);
      aPinArray[numFetched] = pin;
      assert(aPinArray[numFetched] != NULL);
      if (!aPinArray[numFetched]) {
        // Failure, release all pins.
        for (unsigned int i=0; i<numFetched; i++) {
          NS_IF_RELEASE(aPinArray[i]);
        }
        return VFW_E_ENUM_OUT_OF_SYNC;
      }
      NS_IF_ADDREF(aPinArray[numFetched]);
      mPinIdx++;
      numFetched++;
    }
  
    if (aNumFetched)
      *aNumFetched = numFetched;
  
  
    return (aNumPins == numFetched) ? S_OK : S_FALSE;
  }

  STDMETHODIMP Skip(ULONG aNumPins)
  {
    if (IsOutOfSync())
      return VFW_E_ENUM_OUT_OF_SYNC;
  
    if ((mPinIdx + aNumPins) > mNumPins)
      return S_FALSE;
  
    mPinIdx += aNumPins;
  
    return S_OK;
  }

  STDMETHODIMP Reset()
  {
    mNumPins = mFilter->GetPinCount();
    mPinIdx = 0;
    return S_OK;
  }

  STDMETHODIMP Clone(IEnumPins** aEnum)
  {
    if (!aEnum)
      return E_POINTER;
  
    if (IsOutOfSync())
      return VFW_E_ENUM_OUT_OF_SYNC;
  
    EnumPins *p = new EnumPins(this);

    *aEnum = p;
    NS_IF_ADDREF(p);
  
    return S_OK;
  }
private:

  bool IsOutOfSync() {
    return mNumPins != mFilter->GetPinCount();
  }

  BaseFilterPtr mFilter;
  unsigned int mNumPins;
  unsigned int mPinIdx;
  unsigned long mRefCnt;
};

_COM_SMARTPTR_TYPEDEF(IMediaEventSink, __uuidof(IMediaEventSink));

BaseFilter::BaseFilter(const wchar_t* aName,
                       REFCLSID aClsID)
  : mClsId(aClsID)
  , mState(State_Stopped)
  , mLock("BaseFilter::mLock")
  , mRefCnt(0)
{
  mName = aName;
  assert(mName == aName);
}


STDMETHODIMP BaseFilter::QueryInterface(REFIID riid,
                                        void** aInterface)
{
  if (!aInterface) {
    return E_POINTER;
  }

  if (riid == IID_IBaseFilter || riid == IID_IUnknown) {
    *aInterface = static_cast<IBaseFilter*>(this);
  } else if (riid == IID_IMediaFilter) {
    *aInterface = static_cast<IMediaFilter*>(this);
  } else if (riid == IID_IPersist) {
    *aInterface = static_cast<IPersist*>(this);
  } else {
    *aInterface = NULL;
    return E_NOINTERFACE;
  }

  AddRef();
  return S_OK;
}


STDMETHODIMP
BaseFilter::GetClassID(CLSID* aClsId)
{
  if (!aClsId)
    return E_POINTER;
  *aClsId = mClsId;
  return NOERROR;
}


STDMETHODIMP
BaseFilter::GetState(DWORD, FILTER_STATE* aState)
{
  if (!aState)
    return E_POINTER;
  *aState = mState;
  return S_OK;
}


STDMETHODIMP
BaseFilter::SetSyncSource(IReferenceClock* aClock)
{
  CriticalSectionAutoEnter monitor(mLock);
  mClock = aClock;
  return NOERROR;
}


STDMETHODIMP
BaseFilter::GetSyncSource(IReferenceClock** aClock)
{
  if (!aClock)
    return E_POINTER;

  CriticalSectionAutoEnter monitor(mLock);

  if (mClock) {
    // returning an interface... addref it...
    mClock->AddRef();
  }
  *aClock = mClock;
  return NOERROR;
}


// Stop deactivates any active, connected pins on this filter.
STDMETHODIMP
BaseFilter::Stop()
{
  CriticalSectionAutoEnter monitor(mLock);
  HRESULT retval = S_OK;

  if (mState == State_Stopped)
    return S_OK;

  int numPins = GetPinCount();
  for (int i = 0; i < numPins; i++) {

    BasePin* pin = GetPin(i);
    if (NULL == pin) {
      continue;
    }

    if (pin->IsConnected()) {
      HRESULT hr = pin->Inactive();
      if (FAILED(hr) && SUCCEEDED(retval)) {
        retval = hr;
      }
    }
  }

  mState = State_Stopped;

  return retval;
}


// Moves the filter into the paused state. If the graph is stopped, notify all
// the connected pins that they're active.
STDMETHODIMP
BaseFilter::Pause()
{
  CriticalSectionAutoEnter monitor(mLock);

  if (mState == State_Stopped) {
    int numPins = GetPinCount();
    for (int i = 0; i < numPins; i++) {

      BasePin* pin = GetPin(i);
      if (NULL == pin) {
        break;
      }

      if (pin->IsConnected()) {
        HRESULT hr = pin->Active();
        if (FAILED(hr)) {
          return hr;
        }
      }
    }
  }

  mState = State_Paused;

  return S_OK;
}


// Moves the filter into a running state. aStartTime is the offset to be added
// to the samples' stream time in order for the samples to appear in stream time.
STDMETHODIMP
BaseFilter::Run(REFERENCE_TIME aStartTime)
{
  CriticalSectionAutoEnter monitor(mLock);

  mStartTime = aStartTime;

  if (mState == State_Running) {
    return S_OK;
  }

  // First pause the filter if it's stopped.
  if (mState == State_Stopped) {
    HRESULT hr = Pause();
    if (FAILED(hr)) {
      return hr;
    }
  }

  // Start all connected pins.
  int numPins = GetPinCount();
  for (int i = 0; i < numPins; i++) {

    BasePin* pin = GetPin(i);
    assert(pin != NULL);
    if (!pin) {
      continue;
    }

    if (pin->IsConnected()) {
      HRESULT hr = pin->Run(aStartTime);
      if (FAILED(hr)) {
        return hr;
      }
    }
  }

  mState = State_Running;
  return S_OK;
}

STDMETHODIMP
BaseFilter::EnumPins(IEnumPins** aEnum)
{
  if (!aEnum)
    return E_POINTER;

  *aEnum = new mozilla::media::EnumPins(this);
  if (!(*aEnum))
    return E_OUTOFMEMORY;

  NS_IF_ADDREF(*aEnum);

  return S_OK;
}


STDMETHODIMP
BaseFilter::FindPin(LPCWSTR aId,
                    IPin** aPin)
{
  if (!aPin)
    return E_POINTER;

  *aPin = NULL;

  CriticalSectionAutoEnter monitor(mLock);
  int numPins = GetPinCount();
  for (int i = 0; i < numPins; i++) {
    BasePin* pin = GetPin(i);
    if (NULL == pin) {
      assert(pin != NULL);
      return VFW_E_NOT_FOUND;
    }

    if (!pin->Name().compare(aId)) {
      // Found a pin with a matching name, AddRef() and return it.
      *aPin = pin;
      NS_IF_ADDREF(pin);
      return S_OK;
    }
  }

  return VFW_E_NOT_FOUND;
}


STDMETHODIMP
BaseFilter::QueryFilterInfo(FILTER_INFO* aInfo)
{
  if (!aInfo)
    return E_POINTER;

  if (!mName.empty()) {
    StringCchCopyW(aInfo->achName, NUMELMS(aInfo->achName), mName.data());
  } else {
    aInfo->achName[0] = L'\0';
  }
  aInfo->pGraph = mGraph;
  NS_IF_ADDREF(mGraph);

  return S_OK;
}


STDMETHODIMP
BaseFilter::JoinFilterGraph(IFilterGraph* aGraph,
                              LPCWSTR aName)
{
  CriticalSectionAutoEnter monitor(mLock);
  mGraph = aGraph;
  IMediaEventSinkPtr sink = mGraph;
  // Store weak ref to event sink. Graph holds strong reg to us, so if we hold
  // strong ref to its event sink we'll create a cycle.
  mEventSink = sink;
  if (aGraph) {
    mName = aName;
  } else {
    mName.resize(0);
  }
  return S_OK;
}


HRESULT
BaseFilter::NotifyEvent(long aEventCode,
                          LONG_PTR aEventParam1,
                          LONG_PTR aEventParam2)
{
  IMediaEventSink* sink = mEventSink;
  if (sink) {
    if (EC_COMPLETE == aEventCode) {
      aEventParam2 = (LONG_PTR)(static_cast<IBaseFilter*>(this));
    }
    return sink->Notify(aEventCode, aEventParam1, aEventParam2);
  }
  return E_NOTIMPL;
}

STDMETHODIMP_(ULONG) 
BaseFilter::AddRef()
{
  return ::InterlockedIncrement(&mRefCnt);
}

STDMETHODIMP_(ULONG)
BaseFilter::Release()
{
  unsigned long newRefCnt = ::InterlockedDecrement(&mRefCnt);

  if (!newRefCnt) {
    delete this;
  }

  return newRefCnt;
}

}
}
