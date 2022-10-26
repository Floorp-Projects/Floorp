/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StreamBufferSink_h
#define mozilla_StreamBufferSink_h

#include <cstddef>
#include "mozilla/Span.h"
#include "nsString.h"

namespace mozilla {

class StreamBufferSink {
 public:
  virtual mozilla::Span<char> Data() = 0;

  nsDependentCSubstring Slice(size_t aOffset) {
    return nsDependentCSubstring(Data().First(aOffset));
  }

  virtual ~StreamBufferSink() = default;
};

}  // namespace mozilla

#endif  // mozilla_StreamBufferSink_h
