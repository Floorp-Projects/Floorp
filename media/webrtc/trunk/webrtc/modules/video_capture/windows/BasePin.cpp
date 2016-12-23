/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include "nsAutoPtr.h"
#include "BasePin.h"

namespace mozilla {
namespace media {

// Implements IEnumMediaTypes for nsBasePin::EnumMediaTypes().
// Does not support dynamic media types.
//
// Implements:
//  * IEnumMediaTypes
//  * IUnknown
//
class DECLSPEC_UUID("4de7a03c-6c3f-4314-949a-ee7e1ad05083")
  EnumMediaTypes
    : public IEnumMediaTypes
{
public:

  explicit EnumMediaTypes(BasePin* aPin)
    : mPin(aPin)
    , mIndex(0)
    , mRefCnt(0)
  {
  }

  explicit EnumMediaTypes(EnumMediaTypes* aEnum)
    : mPin(aEnum->mPin)
    , mIndex(aEnum->mIndex)
    , mRefCnt(0)
  {
  }

  STDMETHODIMP QueryInterface(REFIID aIId, void **aInterface)
  {
    if (!aInterface) {
      return E_POINTER;
    }

    if (aIId == IID_IEnumMediaTypes) {
      *aInterface = static_cast<IEnumMediaTypes*>(this);
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

  // IEnumMediaTypes
  STDMETHODIMP Next(ULONG aCount,
                    AM_MEDIA_TYPE ** aMediaTypes,
                    ULONG * aNumFetched)
  {
    if (!aMediaTypes)
      return E_POINTER;
  
    if (aNumFetched) {
      *aNumFetched = 0;
    } else if (aCount > 1) {
      // !aNumFetched && aCount>1, we can't report how many media types we read.
      return E_INVALIDARG;
    }
  
    unsigned int numFetched = 0;
    while (numFetched < aCount) {
      // Get the media type
      MediaType mediaType;
      HRESULT hr = mPin->GetMediaType(mIndex++, &mediaType);
      if (hr != S_OK) {
        break;
      }
  
      // Create a copy of media type on the free store.
      MediaType* m = new MediaType(mediaType);
      if (!m)
        return E_OUTOFMEMORY;
      aMediaTypes[numFetched] = m;
      if (!aMediaTypes[numFetched])
        break;
  
      numFetched++;
    }
  
    if (aNumFetched)
      *aNumFetched = numFetched;
  
    return (numFetched == aCount) ? S_OK : S_FALSE;
  }

  STDMETHODIMP Skip(ULONG aCount)
  {
    if (aCount == 0)
      return S_OK;
  
    // Advance the media type index by |aCount|.
    mIndex += aCount;
  
    // See if the new index is *past* the end by querying for a media type.
    MediaType mediaType;
    HRESULT hr = mPin->GetMediaType(mIndex-1, &mediaType);
  
    // Forget mediaTypes pointers, so nsMediaType destructor it won't release them.
    mediaType.Forget();
  
    return (hr != S_OK) ? S_OK : S_FALSE;
  }
  
  STDMETHODIMP Reset()
  {
    mIndex = 0;
    return S_OK;
  }

  STDMETHODIMP Clone(IEnumMediaTypes **aClone)
  {
    if (!aClone)
      return E_POINTER;
  
    EnumMediaTypes* e = new EnumMediaTypes(this);
    if (!e)
      return E_OUTOFMEMORY;
  
    e->AddRef();
  
    *aClone = static_cast<IEnumMediaTypes*>(e);
  
    return S_OK;
  }

private:

  // Pin whose media types we are iterating.
  BasePinPtr mPin;

  // Index of our "iterator" through mPin's media types.
  int mIndex;

  unsigned long mRefCnt;

};
_COM_SMARTPTR_TYPEDEF(IEnumMediaTypes, __uuidof(IEnumMediaTypes));

BasePin::BasePin(BaseFilter* aFilter,
                 CriticalSection* aLock,
                 const wchar_t* aName,
                 PIN_DIRECTION aDirection)
  : mFilter(aFilter)
  , mLock(aLock)
  , mName(aName)
  , mDirection(aDirection)
  , mQualitySink(NULL)
{
  assert(aFilter != NULL);
  assert(aLock != NULL);
}


STDMETHODIMP
BasePin::QueryInterface(REFIID aIID, void ** aInterface)
{
  if (!aInterface) {
    return E_POINTER;
  }

  if (aIID == IID_IPin || aIID == IID_IUnknown) {
    *aInterface = static_cast<IPin*>(this);
  } else if (aIID == IID_IQualityControl) {
    *aInterface = static_cast<IQualityControl*>(this);
  } else {
    *aInterface = NULL;
    return E_NOINTERFACE;
  }

  AddRef();
  return S_OK;
}


STDMETHODIMP
BasePin::QueryPinInfo(PIN_INFO * aInfo)
{
  if (!aInfo)
    return E_POINTER;

  memset(aInfo, 0, sizeof(PIN_INFO));

  aInfo->pFilter = mFilter;
  if (mFilter) {
    mFilter->AddRef();
  }

  if (!mName.empty()) {
    // Copy at most (max_buffer_size - sizeof(WCHAR)). The -1 is there to
    // ensure we always have a null terminator.
    unsigned int len = PR_MIN((MAX_PIN_NAME-1)*sizeof(WCHAR), (sizeof(WCHAR)*mName.length()));
    memcpy(aInfo->achName, mName.data(), len);
  }

  aInfo->dir = mDirection;

  return NOERROR;
}


STDMETHODIMP
BasePin::QueryDirection(PIN_DIRECTION * aPinDir)
{
  if (!aPinDir)
    return E_POINTER;
  *aPinDir = mDirection;
  return S_OK;
}


STDMETHODIMP
BasePin::QueryId(LPWSTR * aId)
{
  if (!aId)
    return E_POINTER;

  *aId = NULL;

  WCHAR* str = NULL;

  // Length not including null-terminator.
  unsigned int sz = mName.length() * sizeof(WCHAR); 

  // Allocate memory for new string copy, plus 1 for null-terminator.
  str = (LPWSTR)CoTaskMemAlloc(sz + sizeof(WCHAR));
  if (!str)
    return E_OUTOFMEMORY;

  // Copy name.
  memcpy(str, mName.data(), sz);

  // Set null-terminator.
  str[mName.length()] = 0;

  *aId = str;

  return S_OK;
}


STDMETHODIMP
BasePin::EnumMediaTypes(IEnumMediaTypes **aEnum)
{
  if (!aEnum)
    return E_POINTER;

  *aEnum = new mozilla::media::EnumMediaTypes(this);

  if (*aEnum == NULL)
    return E_OUTOFMEMORY;

  // Must addref, caller's responsibility to release.
  NS_ADDREF(*aEnum);

  return S_OK;
}


// Base class returns an error; we expect sub-classes to override this.
HRESULT
BasePin::GetMediaType(int, MediaType*)
{
  return E_UNEXPECTED;
}


STDMETHODIMP
BasePin::QueryAccept(const AM_MEDIA_TYPE *aMediaType)
{
  if (!aMediaType)
    return E_POINTER;

  // Defer to subclasses CheckMediaType() function. Map all errors to S_FALSE,
  // so that we match the spec for QueryAccept().

  if (FAILED(CheckMediaType((MediaType*)aMediaType)))
    return S_FALSE;

  return S_OK;
}


HRESULT
BasePin::Active(void)
{
  return S_OK;
}


HRESULT
BasePin::Run(REFERENCE_TIME)
{
  return S_OK;
}


HRESULT
BasePin::Inactive(void)
{
  return S_OK;
}


STDMETHODIMP
BasePin::EndOfStream(void)
{
  return S_OK;
}


STDMETHODIMP
BasePin::SetSink(IQualityControl* aQualitySink)
{
  CriticalSectionAutoEnter monitor(*mLock);
  mQualitySink = aQualitySink;
  return S_OK;
}


STDMETHODIMP
BasePin::Notify(IBaseFilter *, Quality)
{
  return E_NOTIMPL;
}


STDMETHODIMP
BasePin::NewSegment(REFERENCE_TIME aStartTime,
                      REFERENCE_TIME aStopTime,
                      double aRate)
{
  return E_NOTIMPL;
}


// This pin attempts to connect to |aPin| with media type |aMediaType|.
// If |aMediaType| is fully specified, we must attempt to connect with
// that, else we just enumerate our types, then the other pin's type and
// try them, filtering them using |aMediaType| if it's paritally
// specificed. Used by Connect().
STDMETHODIMP
BasePin::Connect(IPin * aPin,
                   const AM_MEDIA_TYPE* aMediaType)
{
  if (!aPin)
    return E_POINTER;

  CriticalSectionAutoEnter monitor(*mLock);

  if (IsConnected())
    return VFW_E_ALREADY_CONNECTED;

  // Can't connect when filter is not stopped.
  if (!IsStopped())
    return VFW_E_NOT_STOPPED;

  // Find a muatually acceptable media type. First try the media type
  // suggested, then try our media types, then the other pin's media types.

  const MediaType* mediaType = reinterpret_cast<const MediaType*>(aMediaType);

  if (aMediaType && !mediaType->IsPartiallySpecified()) {
    // Media type is supplied and not partially specified, we must try to
    // connect with that.
    return AttemptConnection(aPin, mediaType);
  }

  // Try this pin's media types...
  IEnumMediaTypesPtr enumMediaTypes;
  HRESULT hr = EnumMediaTypes(&enumMediaTypes);
  assert(SUCCEEDED(hr));
  if (enumMediaTypes) {
    hr = TryMediaTypes(aPin, mediaType, enumMediaTypes);
    if (SUCCEEDED(hr))
      return S_OK;
  }

  // Can't connect with our media types, try other pins types...
  enumMediaTypes = NULL;
  hr = aPin->EnumMediaTypes(&enumMediaTypes);
  assert(SUCCEEDED(hr));
  if (enumMediaTypes) {
    hr = TryMediaTypes(aPin, mediaType, enumMediaTypes);
    if (SUCCEEDED(hr))
      return S_OK;
  }

  // Nothing connects.
  return VFW_E_NO_ACCEPTABLE_TYPES;
}


// Try the media types of |aEnum| when connecting to |aPin| with type limiter
// of |aFilter|. If |aFilter| is specified, we will only attempt to connect
// with media types from |aEnum| which partially match |aFilter|.
HRESULT
BasePin::TryMediaTypes(IPin *aPin,
                         const MediaType *aFilter,
                         IEnumMediaTypes *aEnum)
{
  // Reset enumerator.
  HRESULT hr = aEnum->Reset();
  if (FAILED(hr))
    return hr;

  while (true) {
    AM_MEDIA_TYPE* amt = NULL;
    ULONG mediaCount = 0;
    HRESULT hr = aEnum->Next(1, &amt, &mediaCount);
    if (hr != S_OK) // No more media types...
      return VFW_E_NO_ACCEPTABLE_TYPES;

    assert(mediaCount == 1 && hr == S_OK);

    MediaType* mediaType = reinterpret_cast<MediaType*>(amt);
    if (!aFilter || mediaType->MatchesPartial(aFilter)) {
      // Either there's no limiter, or we partially match it,
      // attempt connection.
      if (SUCCEEDED(AttemptConnection(aPin, mediaType)))
        return S_OK;
    }
  }
}


// Checks if this pin can connect to |aPin|. We expect sub classes to
// override this method to support their own needs.
HRESULT
BasePin::CheckConnect(IPin* aPin)
{
  PIN_DIRECTION otherPinsDirection;
  aPin->QueryDirection(&otherPinsDirection);

  // Can't connect pins with same direction together...
  if (otherPinsDirection == mDirection) {
    return VFW_E_INVALID_DIRECTION;
  }

  return S_OK;
}


// Called when we've made a connection to another pin. Returning failure
// triggers the caller to break the connection. Subclasses may want to
// override this.
HRESULT
BasePin::CompleteConnect(IPin *)
{
  return S_OK;
}


// Sets the media type of the connection.
HRESULT
BasePin::SetMediaType(const MediaType *aMediaType)
{
  return mMediaType.Assign(aMediaType);
}


STDMETHODIMP
BasePin::QueryInternalConnections(IPin**, ULONG*)
{
  return E_NOTIMPL;
}


// This is called to release any resources needed for a connection.
HRESULT
BasePin::BreakConnect()
{
  return S_OK;
}


STDMETHODIMP
BasePin::Disconnect()
{
  CriticalSectionAutoEnter monitor(*mLock);

  // Can't disconnect a non-stopped filter.
  if (!IsStopped())
    return VFW_E_NOT_STOPPED;

  if (!IsConnected())
     return S_FALSE;

  HRESULT hr = BreakConnect();
  mConnectedPin = NULL;
  return hr;
}


// Attempt to connect this pin to |aPin| using given media type.
HRESULT
BasePin::AttemptConnection(IPin* aPin,
                             const MediaType* aMediaType)
{
  CriticalSectionAutoEnter monitor(*mLock);

  // Ensure we can connect to the other pin. Gives subclasses a chance
  // to prevent connection.
  HRESULT hr = CheckConnect(aPin);
  if (FAILED(hr)) {
    BreakConnect();
    return hr;
  }

  // Ensure we can connect with this media type. This gives subclasses a
  // chance to abort the connection.
  hr = CheckMediaType(aMediaType);
  if (FAILED(hr))
    return hr;

  hr = SetMediaType(aMediaType);
  if (FAILED(hr))
    return hr;

  // Ask the other pin if it will accept a connection with our media type.
  hr = aPin->ReceiveConnection(static_cast<IPin*>(this), aMediaType);
  if (FAILED(hr))
    return hr;

  // Looks good so far, give subclass one final chance to refuse connection...
  mConnectedPin = aPin;
  hr = CompleteConnect(aPin);

  if (FAILED(hr)) {
    // Subclass refused the connection, inform the other pin that we're
    // disconnecting, and break the connection.
    aPin->Disconnect();
    BreakConnect();
    mConnectedPin = NULL;
    mMediaType.Clear();
    return VFW_E_TYPE_NOT_ACCEPTED;
  }

  // Otherwise, we're all good!
  return S_OK;
}


STDMETHODIMP
BasePin::ReceiveConnection(IPin * aPin,
                             const AM_MEDIA_TYPE *aMediaType)
{
  if (!aPin)
    return E_POINTER;

  if (!aMediaType)
    E_POINTER;

  CriticalSectionAutoEnter monitor(*mLock);

  if (IsConnected())
    return VFW_E_ALREADY_CONNECTED;

  if (!IsStopped())
    return VFW_E_NOT_STOPPED;

  HRESULT hr = CheckConnect(aPin);
  if (FAILED(hr)) {
    BreakConnect();
    return hr;
  }

  // See if subclass supports the specified media type.
  const MediaType* mediaType = reinterpret_cast<const MediaType*>(aMediaType);
  hr = CheckMediaType(mediaType);
  if (FAILED(hr)) {
    BreakConnect();
    return hr;
  }

  // Supported, set it.
  hr = SetMediaType(mediaType);
  if (FAILED(hr))
    return hr;

  // Complete the connection.
  mConnectedPin = aPin;
  // Give the subclass one last chance to refuse the connection.
  hr = CompleteConnect(aPin);
  if (FAILED(hr)) {
    // Subclass refused connection, fail...
    mConnectedPin = NULL;
    BreakConnect();
    return hr;
  }

  // It's all good, we're connected.
  return S_OK;
}


STDMETHODIMP
BasePin::ConnectedTo(IPin** aPin)
{
  if (!aPin)
    return E_POINTER;

  if (!IsConnected())
    return VFW_E_NOT_CONNECTED;

  *aPin = mConnectedPin;
  (*aPin)->AddRef();

  return S_OK;
}


STDMETHODIMP
BasePin::ConnectionMediaType(AM_MEDIA_TYPE *aMediaType)
{
  if (!aMediaType)
    return E_POINTER;

  CriticalSectionAutoEnter monitor(*mLock);

  if (IsConnected()) {
    reinterpret_cast<MediaType*>(aMediaType)->Assign(&mMediaType);
    return S_OK;
  } else {
    memset(aMediaType, 0, sizeof(AM_MEDIA_TYPE));
    return VFW_E_NOT_CONNECTED;
  }
}

}
}
