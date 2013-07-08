/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include "BaseInputPin.h"
#include <vector>

using namespace std;

namespace mozilla {
namespace media {

STDMETHODIMP 
BaseInputPin::QueryInterface(REFIID aIId, void **aInterface)
{
  if (!aInterface) {
    return E_POINTER;
  }

  if (aIId == IID_IMemInputPin) {
    *aInterface = static_cast<IMemInputPin*>(this);
    AddRef();
    return S_OK;
  }

  return BasePin::QueryInterface(aIId, aInterface);
}

STDMETHODIMP
BaseInputPin::BeginFlush()
{
  CriticalSectionAutoEnter lock(*mLock);
  assert(!mFlushing);
  mFlushing = true;
  return S_OK;
}

STDMETHODIMP
BaseInputPin::EndFlush()
{
  CriticalSectionAutoEnter lock(*mLock);
  assert(mFlushing);
  mFlushing = false;
  return S_OK;
}

STDMETHODIMP
BaseInputPin::GetAllocatorRequirements(ALLOCATOR_PROPERTIES*)
{
    return E_NOTIMPL;
}

STDMETHODIMP
BaseInputPin::ReceiveCanBlock()
{
  return S_OK;
}

STDMETHODIMP
BaseInputPin::Receive(IMediaSample* aSample)
{
  if (mFlushing) {
    return S_FALSE;
  }

  return S_OK;
}

STDMETHODIMP
BaseInputPin::ReceiveMultiple(IMediaSample **aSamples, long aSampleCount, long *aProcessedCount)
{
  HRESULT hr = S_OK;
  *aProcessedCount = 0;
  while (aSampleCount-- > 0) {
    hr = Receive(aSamples[*aProcessedCount]);

    // Refuse when S_FALSE
    if (hr != S_OK) {
        break;
    }
    (*aProcessedCount)++;
  }
  return hr;
}

STDMETHODIMP
BaseInputPin::GetAllocator(IMemAllocator **aAllocator)
{
  // It appears DirectShow will create an allocator for us in this case.
  return VFW_E_NO_ALLOCATOR;
}

STDMETHODIMP
BaseInputPin::NotifyAllocator(IMemAllocator *aAllocator, BOOL aReadOnly)
{
  mAllocator = aAllocator;
  mReadOnly = aReadOnly;
  return S_OK;
}

}
}
