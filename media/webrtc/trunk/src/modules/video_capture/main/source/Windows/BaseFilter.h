/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_BASEFILTER_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_BASEFILTER_H_

#include "dshow.h"
#include <comdef.h>
#include "DShowTools.h"

#include <string> 
 
namespace mozilla {
namespace media {

_COM_SMARTPTR_TYPEDEF(IReferenceClock, __uuidof(IReferenceClock));
 
class BasePin;

// Base class for a data source filter which supplies media streams
// for rendering in DirectShow.
//
// Implements:
// * IBaseFilter
// * IUnknown
//

class DECLSPEC_UUID("4debd354-b0c6-44ab-93cf-49f64ed36ab8")
BaseFilter : public IBaseFilter
{
  friend class BasePin;
public:
  // The maximum number of buffers that this samples allocator will request.
  // Allocators are negotiated between filters. The size of the buffers is
  // determined in subclasses. We must have enough buffers so that we can read
  // up to the first keyframe before we run out of buffers.
  static const unsigned int sMaxNumBuffers = 256;

  // Zero all fields on alloc.
  void* operator new(size_t sz) throw() {
    void* rv = ::operator new(sz);
    if (rv) {
      memset(rv, 0, sz);
    }
    return rv;
  }
  void operator delete(void* ptr) {
    ::operator delete(ptr);
  }

  // |aName| is the debug name of the filter (shows up in GraphEdit).
  // |aClsID| is the clsid of the filter.
  BaseFilter(const wchar_t* aName, REFCLSID aClsID);

  ~BaseFilter() {}

  STDMETHODIMP QueryInterface(REFIID aIId, void **aInterface);
  STDMETHODIMP_(ULONG) AddRef();
  STDMETHODIMP_(ULONG) Release();

  // IPersist
  STDMETHODIMP GetClassID(CLSID* aClsID);

  // IMediaFilter methods.

  // Retrieves the state of the filter (running, stopped, or paused).
  STDMETHODIMP GetState(DWORD aTimeout, FILTER_STATE* aState);

  // Sets the reference clock for the filter or the filter graph.
  STDMETHODIMP SetSyncSource(IReferenceClock* aClock);

  // Retrieves the current reference clock.
  STDMETHODIMP GetSyncSource(IReferenceClock** aClock);

  // Stops the filter.
  STDMETHODIMP Stop();

  // Pauses the filter.
  STDMETHODIMP Pause();

  // Runs the filter.
  STDMETHODIMP Run(REFERENCE_TIME aStartTime);

  // IBaseFilter methods.

  // Enumerates the pins on this filter.
  STDMETHODIMP EnumPins(IEnumPins** aEnum);

  // Retrieves the pin with the specified identifier.
  STDMETHODIMP FindPin(LPCWSTR aId, IPin** aPin);

  // Retrieves information about the filter.
  STDMETHODIMP QueryFilterInfo(FILTER_INFO* aInfo);

  // Notifies the filter that it has joined or left the filter graph.
  STDMETHODIMP JoinFilterGraph(IFilterGraph* aGraph, LPCWSTR aName);

  // Retrieves a string containing vendor information.
  // Dfault implementation returns E_NOTIMPL.
  STDMETHODIMP QueryVendorInfo(LPWSTR* aVendorInfo) { return E_NOTIMPL; }

  // Helper methods.
  HRESULT NotifyEvent(long aEventCode,
  LONG_PTR aEventParam1,
  LONG_PTR aEventParam2);

  // Returns the number of pins on this filter. This must be implemented
  // in subclasses.
  virtual int GetPinCount() = 0;

  // Returns the n'th pin. This must be implemented in subclasses.
  virtual BasePin* GetPin(int n) = 0;

  // Called when the filter's graph is about to be paused.
  virtual void AboutToPause() { }

protected:
  // Current state, running, paused, etc.
  FILTER_STATE mState;

  // Reference clock of the graph. This provides an monotonically incrementing
  // clock which the graph may use to timestamp frames.
  IReferenceClockPtr mClock;

  // The offset of stream time to the reference clock.
  REFERENCE_TIME mStartTime;

  // This filter's class ID.
  CLSID mClsId;

  // Filter lock used for serialization.
  CriticalSection mLock;

  // Graph to which this filter belongs. This must be a weak reference,
  // as the graphs holds a strong reference to each filter it contains.
  // This is set when the graph calls to JoinFilterGraph(), and unset
  // by a call to JoinFilterGraph(NULL) when the filter leaves the graph.
  IFilterGraph* mGraph;

  // Event sink for notifications. This is QI'd of mGraph in JoinFilterGraph(),
  // and must be a weak reference for the same reasons as mGraph must.
  IMediaEventSink* mEventSink;

  // Name assigned to this filter.
  std::wstring mName;

  // IUnknown ref counting.
  unsigned long mRefCnt;
};

_COM_SMARTPTR_TYPEDEF(BaseFilter, __uuidof(BaseFilter));

}
}

#endif