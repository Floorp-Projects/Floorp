/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaType.h"

namespace mozilla {
namespace media {

MediaType::MediaType()
{
  // Zero out our memory. We do it here, rather than in operator new,
  // in case this is allocated on the stack.
  memset(this, 0, sizeof(MediaType));
  lSampleSize = 1;
  bFixedSizeSamples = TRUE;
}


MediaType::MediaType(const MediaType& aMediaType)
{
  memset(this, 0, sizeof(MediaType));
  Assign(aMediaType);
}


MediaType::MediaType(const AM_MEDIA_TYPE* aMediaType)
{
  memset(this, 0, sizeof(MediaType));
  Assign(aMediaType);
}


MediaType::~MediaType()
{
  Clear();
}


void
MediaType::operator delete(void* ptr)
{
  CoTaskMemFree((void*)ptr);
}


void*
MediaType::operator new(size_t sz) throw()
{
  return CoTaskMemAlloc(sz);
}


void 
MediaType::Clear()
{
  if (cbFormat) {
    CoTaskMemFree(pbFormat);
    cbFormat = 0;
    pbFormat = NULL;
  }
  if (pUnk) {
    pUnk->Release();
    pUnk = NULL;
  }
  memset(this, 0, sizeof(MediaType));
}


HRESULT
MediaType::Assign(const AM_MEDIA_TYPE* aMediaType)
{
  if (!aMediaType)
    return E_POINTER;

  if (aMediaType == this)
    return S_OK;

  // Release old data...
  Clear();

  // Shallow copy.
  memcpy(this, aMediaType, sizeof(AM_MEDIA_TYPE));

  // Create deep copy of incoming data...
  if (cbFormat) {
    pbFormat = (BYTE*)CoTaskMemAlloc(cbFormat);
    if (!pbFormat)
      return E_OUTOFMEMORY;
    memcpy(pbFormat, aMediaType->pbFormat, cbFormat);
  }

  if (pUnk)
    pUnk->AddRef();

  return S_OK;
}


void
MediaType::Forget()
{
  cbFormat = 0;
  pbFormat = NULL;
  pUnk = NULL;
}


bool
MediaType::IsEqual(const AM_MEDIA_TYPE* aMediaType)
{
  if (!aMediaType)
    return false;
  
  if (aMediaType == this)
    return true;

  return IsEqualGUID(majortype, aMediaType->majortype) &&
         IsEqualGUID(subtype, aMediaType->subtype) &&
         IsEqualGUID(formattype, aMediaType->formattype) &&
         (cbFormat == aMediaType->cbFormat) &&
         ((cbFormat == 0) ||
            (pbFormat != NULL &&
            aMediaType->pbFormat != NULL &&
            (memcmp(pbFormat, aMediaType->pbFormat, cbFormat) == 0)));
}


bool
MediaType::MatchesPartial(const AM_MEDIA_TYPE* aMediaType) const
{

  if (!aMediaType)
    return false;

  if ((aMediaType->majortype != GUID_NULL) &&
      (majortype != aMediaType->majortype))
      return false;

  if ((aMediaType->subtype != GUID_NULL) &&
      (subtype != aMediaType->subtype))
      return false;

  if (aMediaType->formattype != GUID_NULL) {

    if (formattype != aMediaType->formattype)
      return false;

    if (cbFormat != aMediaType->cbFormat)
      return false;

    if ((cbFormat != 0) &&
        (memcmp(pbFormat, aMediaType->pbFormat, cbFormat) != 0))
      return false;
  }

  return true;
}


bool
MediaType::IsPartiallySpecified() const
{
  return (majortype == GUID_NULL) || (formattype == GUID_NULL);
}

BYTE*
MediaType::AllocFormatBuffer(SIZE_T aSize)
{
  pbFormat = static_cast<BYTE*>(CoTaskMemAlloc(aSize));
  return pbFormat;
}

}
}
