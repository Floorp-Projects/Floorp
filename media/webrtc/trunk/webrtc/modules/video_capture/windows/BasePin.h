/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_BASEPIN_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_BASEPIN_H_


#include "BaseFilter.h"
#include "MediaType.h"

#include "dshow.h"
#include "strmif.h"
#include <string>

namespace mozilla {
namespace media {

_COM_SMARTPTR_TYPEDEF(IPin, __uuidof(IPin));


// Base class for DirectShow filter pins.
//
// Implements:
//  * IPin
//  * IQualityControl
//  * IUnknown
//
class DECLSPEC_UUID("199669c6-672a-4130-b13e-57aa830eae55")
  BasePin
  : public IPin
  , public IQualityControl
{
public:

  BasePin(BaseFilter* aFilter,
          CriticalSection* aLock,
          const wchar_t* aName, 
          PIN_DIRECTION aDirection);

  virtual ~BasePin() {}

  // Reference count of the pin is actually stored on the owning filter.
  // So don't AddRef() the filter from the pin, else you'll create a cycle. 
  STDMETHODIMP QueryInterface(REFIID aIId, void **aInterface);
  STDMETHODIMP_(ULONG) AddRef() { return mFilter->AddRef(); }
  STDMETHODIMP_(ULONG) Release() { return mFilter->Release(); }

  // IPin overrides.

  // Connects the pin to another pin. The pmt parameter can be NULL or a
  // partial media type.
  STDMETHODIMP Connect(IPin* aReceivePin,
                       const AM_MEDIA_TYPE* aMediaType);

  //Accepts a connection from another pin.
  STDMETHODIMP ReceiveConnection(IPin* aConnector,
                                 const AM_MEDIA_TYPE* aMediaType);

  // Breaks the current pin connection.
  STDMETHODIMP Disconnect();

  // Retrieves the pin connected to this pin.
  STDMETHODIMP ConnectedTo(IPin** aPin);

  // Retrieves the media type for the current pin connection.
  STDMETHODIMP ConnectionMediaType(AM_MEDIA_TYPE* aMediaType);

  // Retrieves information about the pin, such as the name, the owning filter,
  // and the direction.
  STDMETHODIMP QueryPinInfo(PIN_INFO* aInfo);

  // Retrieves the direction of the pin (input or output).
  STDMETHODIMP QueryDirection(PIN_DIRECTION* aDirection);

  // Retrieves the pin identifier.
  STDMETHODIMP QueryId(LPWSTR* Id);

 	// Determines whether the pin accepts a specified media type.
  STDMETHODIMP QueryAccept(const AM_MEDIA_TYPE* aMediaType);

  // Enumerates the pin's preferred media types.
  STDMETHODIMP EnumMediaTypes(IEnumMediaTypes** aEnum);

  // Retrieves the pins that are connected internally to this pin
  // (within the filter).
  STDMETHODIMP QueryInternalConnections(IPin** apPin,
                                        ULONG* aPin);

  // Notifies the pin that no additional data is expected.
  STDMETHODIMP EndOfStream(void);

  // IPin::BeginFlush() and IPin::EndFlush() are still pure virtual,
  // and must be implemented in a subclass.

  // Notifies the pin that media samples received after this call
  // are grouped as a segment.
  STDMETHODIMP NewSegment(
    REFERENCE_TIME aStartTime,
    REFERENCE_TIME aStopTime,
    double aRate);



  // IQualityControl overrides.
 
  // Notifies the recipient that a quality change is requested.
  STDMETHODIMP Notify(IBaseFilter * aSender, Quality aQuality);

  // Sets the IQualityControl object that will receive quality messages.
  STDMETHODIMP SetSink(IQualityControl* aQualitySink);



  // Other methods.

  // Sets the media type of the connection.
  virtual HRESULT SetMediaType(const MediaType *aMediaType);

  // check if the pin can support this specific proposed type and format
  virtual HRESULT CheckMediaType(const MediaType *) = 0;

  // This is called to release any resources needed for a connection.
  virtual HRESULT BreakConnect();

  // Called when we've made a connection to another pin. Returning failure
  // triggers the caller to break the connection. Subclasses may want to
  // override this.
  virtual HRESULT CompleteConnect(IPin *pReceivePin);

  // Checks if this pin can connect to |aPin|. We expect sub classes to
  // override this method to support their own needs. Default implementation
  // simply checks that the directions of the pins do not match.
  virtual HRESULT CheckConnect(IPin *);

  // Check if our filter is currently stopped
  BOOL IsStopped() {
    return mFilter->mState == State_Stopped;
  };

  // Moves pin to active state (running or paused). Subclasses will
  // override to prepare to handle data.
  virtual HRESULT Active(void);

  // Moves pin into inactive state (stopped). Releases resources associated
  // with handling data. Subclasses should override this.
  virtual HRESULT Inactive(void);

  // Called when Run() is called on the parent filter. Subclasses may want to
  // override this.
  virtual HRESULT Run(REFERENCE_TIME aStartTime);

  // Gets the supported media types for this pin.
  virtual HRESULT GetMediaType(int aIndex, MediaType *aMediaType);

  //  Access name.
  const std::wstring& Name() { return mName; };

  bool IsConnected() { return mConnectedPin != NULL; }

  IPin* GetConnected() { return mConnectedPin; }

protected:

  // The pin's name, as returned by QueryPinInfo().
  std::wstring mName;
  
  // Event sink for quality messages.
  IQualityControl *mQualitySink;

  // The pin which this one is connected to.
  IPinPtr mConnectedPin;

  // Direction of data flow through this pin.
  PIN_DIRECTION mDirection;

  // Media type of the pin's connection.
  MediaType mMediaType;

  // Our state lock. All state should be accessed while this is locked.
  mozilla::CriticalSection *mLock;

  // Our owning filter.
  BaseFilter *mFilter;
 
  // This pin attempts to connect to |aPin| with media type |aMediaType|.
  // If |aMediaType| is fully specified, we must attempt to connect with
  // that, else we just enumerate our types, then the other pin's type and
  // try them, filtering them using |aMediaType| if it's paritally
  // specificed. Used by Connect().
  HRESULT AttemptConnection(IPin* aPin, const MediaType* aMediaType);

  // Tries to form a connection using all media types in the enumeration.
  HRESULT TryMediaTypes(IPin *aPin,
                        const MediaType *aMediaType,
                        IEnumMediaTypes *aEnum);
};

_COM_SMARTPTR_TYPEDEF(BasePin, __uuidof(BasePin));

}
}

#endif
