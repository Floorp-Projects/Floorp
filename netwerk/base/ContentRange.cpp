/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentRange.h"
#include "nsContentUtils.h"

mozilla::net::ContentRange::ContentRange(const nsACString& aRangeHeader,
                                         uint64_t aSize)
    : mStart(0), mEnd(0), mSize(0) {
  auto parsed = nsContentUtils::ParseSingleRangeRequest(aRangeHeader, true);
  // https://fetch.spec.whatwg.org/#ref-for-simple-range-header-value%E2%91%A1
  // If rangeValue is failure, then return a network error.
  if (!parsed) {
    return;
  }

  // Sanity check: ParseSingleRangeRequest should handle these two cases.
  // If rangeEndValue and rangeStartValue are null, then return failure.
  MOZ_ASSERT(parsed->Start().isSome() || parsed->End().isSome());
  // If rangeStartValue and rangeEndValue are numbers, and rangeStartValue
  // is greater than rangeEndValue, then return failure.
  MOZ_ASSERT(parsed->Start().isNothing() || parsed->End().isNothing() ||
             *parsed->Start() <= *parsed->End());

  // https://fetch.spec.whatwg.org/#ref-for-simple-range-header-value%E2%91%A1
  // If rangeStart is null:
  if (parsed->Start().isNothing()) {
    // Set rangeStart to fullLength − rangeEnd.
    mStart = aSize - *parsed->End();

    // Set rangeEnd to rangeStart + rangeEnd − 1.
    mEnd = mStart + *parsed->End() - 1;

    // Otherwise:
  } else {
    // If rangeStart is greater than or equal to fullLength, then return a
    // network error.
    if (*parsed->Start() >= aSize) {
      return;
    }
    mStart = *parsed->Start();

    // If rangeEnd is null or rangeEnd is greater than or equal to fullLength,
    // then set rangeEnd to fullLength − 1.
    if (parsed->End().isNothing() || *parsed->End() >= aSize) {
      mEnd = aSize - 1;
    } else {
      mEnd = *parsed->End();
    }
  }
  mSize = aSize;
}

void mozilla::net::ContentRange::AsHeader(nsACString& aOutString) const {
  aOutString.Assign("bytes "_ns);
  aOutString.AppendInt(mStart);
  aOutString.AppendLiteral("-");
  aOutString.AppendInt(mEnd);
  aOutString.AppendLiteral("/");
  aOutString.AppendInt(mSize);
}
