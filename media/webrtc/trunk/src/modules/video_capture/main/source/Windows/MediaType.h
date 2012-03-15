/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_MEDIATYPE_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_MEDIATYPE_H_

#include "dshow.h"

namespace mozilla {
namespace media {

// Wrapper for AM_MEDIA_TYPE. Can be used for a drop in replacement
// for AM_MEDIA_TYPE. Manages memory of the AM_MEDIA_TYPE structure.
// This class must be instantiated/destroyed using its custom new/detele
// operators.
class MediaType : public AM_MEDIA_TYPE
{
public:
  
  MediaType();
  MediaType(const AM_MEDIA_TYPE* aMediaType);
  MediaType(const MediaType& aMediaType);

  ~MediaType();

  // Deep copies |aMediaType| into this instance, freeing memory of this
  // instance first.
  HRESULT Assign(const AM_MEDIA_TYPE* aMediaType);
  HRESULT Assign(const AM_MEDIA_TYPE& aMediaType) {
    return Assign(&aMediaType);
  }

  // Checks if |aMediaType|'s majortype, subtype, and format
  // blocks exactly match.
  bool IsEqual(const AM_MEDIA_TYPE* aMediaType);
  bool IsEqual(const AM_MEDIA_TYPE& aMediaType) {
    return IsEqual(&aMediaType);
  }

  // Checks if |aMediaType|'s majortype, subtype, and format
  // blocks partially match. Null fields are assumed to match.
  bool MatchesPartial(const AM_MEDIA_TYPE* aMediaType) const;
  bool MatchesPartial(const AM_MEDIA_TYPE& aMediaType) const {
    return MatchesPartial(&aMediaType);
  }

  // Returns true if media type isn't fully specified, i.e. it doesn't
  // have both a major and sub media type.
  bool IsPartiallySpecified() const;

  // Release all memory for held by pointers in this media type
  // (the format buffer, and the ref count on pUnk).
  void Clear();

  // Blanks all pointers to memory allocated on the free store.
  // Call this when ownership of that memory passes to another object.
  void Forget();

  const GUID* Type() const
  {
    return &majortype;
  }
  void SetType(const GUID* aType)
  {
    majortype = *aType;
  }

  const GUID* Subtype() const
  {
    return &subtype;
  }
  void SetSubtype(const GUID* aType)
  {
    subtype = *aType;
  }

  const GUID* FormatType() const
  {
    return &formattype;
  }
  void SetFormatType(const GUID* aType)
  {
    formattype = *aType;
  }

  BOOL TemporalCompression() const { return bTemporalCompression; }
  void SetTemporalCompression(BOOL aCompression)
  {
    bTemporalCompression = aCompression;
  }

  ULONG SampleSize() const { return lSampleSize; }
  void SetSampleSize(ULONG aSampleSize)
  {
    lSampleSize = aSampleSize;
  }

  BYTE* AllocFormatBuffer(SIZE_T aSize);
  BYTE* Format() const { return pbFormat; }

  // Custom new/delete. This object *must* be allocated using
  // CoTaskMemAlloc(), and freed using CoTaskMemFree(). These new/delete
  // functions ensure that.
  void operator delete(void* ptr);
  void* operator new(size_t sz) throw();

  operator AM_MEDIA_TYPE* () {
    return static_cast<AM_MEDIA_TYPE*>(this);
  }

private:
  // Don't allow assignment using operator=, use Assign() instead.
  MediaType& operator=(const MediaType&);
  MediaType& operator=(const AM_MEDIA_TYPE&);
};

}
}

#endif
