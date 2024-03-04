/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set et cin ts=4 sw=2 sts=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ContentRange_h__
#define ContentRange_h__

#include "mozilla/Maybe.h"
#include "nsString.h"
#include "nsISupportsImpl.h"

// nsIBaseChannel subclasses may support range headers when accessed via
// Fetch or XMLHTTPRequest, even if they are not HTTP assets and so do not
// normally have access to headers (such as the blob URLs). The ContentRange
// class helps to encapsulate much of the common logic involved in parsing,
// storing, validating, and writing out Content-Range headers.

namespace mozilla::net {

class ContentRange {
 private:
  ~ContentRange() = default;

  uint64_t mStart{0};
  uint64_t mEnd{0};
  uint64_t mSize{0};

 public:
  uint64_t Start() const { return mStart; }
  uint64_t End() const { return mEnd; }
  uint64_t Size() const { return mSize; }
  bool IsValid() const { return mStart < mSize; }
  ContentRange(uint64_t aStart, uint64_t aEnd, uint64_t aSize)
      : mStart(aStart), mEnd(aEnd), mSize(aSize) {}
  ContentRange(const nsACString& aRangeHeader, uint64_t aSize);
  void AsHeader(nsACString& aOutString) const;

  NS_INLINE_DECL_REFCOUNTING(ContentRange)
};

}  // namespace mozilla::net

#endif  // ContentRange_h__
