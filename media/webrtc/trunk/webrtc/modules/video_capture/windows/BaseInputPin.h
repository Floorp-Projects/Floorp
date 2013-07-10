/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_BASEINPUTPIN_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_BASEINPUTPIN_H_


#include "BasePin.h"

namespace mozilla {
namespace media {

_COM_SMARTPTR_TYPEDEF(IMemAllocator, __uuidof(IMemAllocator));

class BaseInputPin : public BasePin
                   , public IMemInputPin
{
protected:
  BaseInputPin(
    const wchar_t* aObjectName,
    BaseFilter *aFilter,
    CriticalSection *aLock,
    HRESULT *aHR,
    const wchar_t* aName)
    : BasePin(aFilter, aLock, aName, PINDIR_INPUT)
    , mFlushing(false)
    , mReadOnly(false)
  {
    *aHR = S_OK;
  }

  STDMETHODIMP BeginFlush();
  STDMETHODIMP EndFlush();
  STDMETHODIMP QueryInterface(REFIID aIId, void **aInterface);
  STDMETHODIMP_(ULONG) AddRef() { return mFilter->AddRef(); }
  STDMETHODIMP_(ULONG) Release() { return mFilter->Release(); }
  
  STDMETHODIMP GetAllocator(IMemAllocator **aAllocator);
  STDMETHODIMP NotifyAllocator(IMemAllocator *aAllocator, BOOL aReadOnly);
  STDMETHODIMP GetAllocatorRequirements(ALLOCATOR_PROPERTIES *aProps);
  STDMETHODIMP Receive(IMediaSample *aSample);
  STDMETHODIMP ReceiveMultiple(IMediaSample **aSamples, long aSampleCount, long *aProcessedCount);
  STDMETHODIMP ReceiveCanBlock();
protected:
  bool mFlushing;
  bool mReadOnly;
  IMemAllocatorPtr mAllocator;
};

}
}

#endif
