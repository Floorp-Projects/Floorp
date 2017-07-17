/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HangAnnotations.h"
#include "ThreadHangStats.h"
#include "nsITelemetry.h"
#include "HangReports.h"
#include "jsapi.h"

namespace mozilla {
namespace Telemetry {

const char*
HangStack::InfallibleAppendViaBuffer(const char* aText, size_t aLength)
{
  MOZ_ASSERT(this->canAppendWithoutRealloc(1));
  // Include null-terminator in length count.
  MOZ_ASSERT(mBuffer.canAppendWithoutRealloc(aLength + 1));

  const char* const entry = mBuffer.end();
  mBuffer.infallibleAppend(aText, aLength);
  mBuffer.infallibleAppend('\0'); // Explicitly append null-terminator
  this->infallibleAppend(entry);
  return entry;
}

const char*
HangStack::AppendViaBuffer(const char* aText, size_t aLength)
{
  if (!this->reserve(this->length() + 1)) {
    return nullptr;
  }

  // Keep track of the previous buffer in case we need to adjust pointers later.
  const char* const prevStart = mBuffer.begin();
  const char* const prevEnd = mBuffer.end();

  // Include null-terminator in length count.
  if (!mBuffer.reserve(mBuffer.length() + aLength + 1)) {
    return nullptr;
  }

  if (prevStart != mBuffer.begin()) {
    // The buffer has moved; we have to adjust pointers in the stack.
    for (auto & entry : *this) {
      if (entry >= prevStart && entry < prevEnd) {
        // Move from old buffer to new buffer.
        entry += mBuffer.begin() - prevStart;
      }
    }
  }

  return InfallibleAppendViaBuffer(aText, aLength);
}

} // namespace Telemetry
} // namespace mozilla
