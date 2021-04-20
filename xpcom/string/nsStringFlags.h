/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStringFlags_h
#define nsStringFlags_h

#include <stdint.h>
#include "mozilla/TypedEnumBits.h"

namespace mozilla {
namespace detail {
// NOTE: these flags are declared public _only_ for convenience inside
// the string implementation.  And they are outside of the string
// class so that the type is the same for both narrow and wide
// strings.

// bits for mDataFlags
enum class StringDataFlags : uint16_t {
  // Some terminology:
  //
  //   "dependent buffer"    A dependent buffer is one that the string class
  //                         does not own.  The string class relies on some
  //                         external code to ensure the lifetime of the
  //                         dependent buffer.
  //
  //   "refcounted buffer"   A refcounted buffer is one that the string class
  //                         allocates.  When it allocates a refcounted string
  //                         buffer, it allocates some additional space at
  //                         the beginning of the buffer for additional
  //                         fields, including a reference count and a
  //                         buffer length.  See nsStringHeader.
  //
  //   "adopted buffer"      An adopted buffer is a raw string buffer
  //                         allocated on the heap (using moz_xmalloc)
  //                         of which the string class subsumes ownership.
  //
  // Some comments about the string data flags:
  //
  //   REFCOUNTED, OWNED, and INLINE are all mutually exlusive.  They
  //   indicate the allocation type of mData.  If none of these flags
  //   are set, then the string buffer is dependent.
  //
  //   REFCOUNTED, OWNED, or INLINE imply TERMINATED.  This is because
  //   the string classes always allocate null-terminated buffers, and
  //   non-terminated substrings are always dependent.
  //
  //   VOIDED implies TERMINATED, and moreover it implies that mData
  //   points to char_traits::sEmptyBuffer.  Therefore, VOIDED is
  //   mutually exclusive with REFCOUNTED, OWNED, and INLINE.
  //
  //   INLINE requires StringClassFlags::INLINE to be set on the type.

  // IsTerminated returns true
  TERMINATED = 1 << 0,

  // IsVoid returns true
  VOIDED = 1 << 1,

  // mData points to a heap-allocated, shareable, refcounted buffer
  REFCOUNTED = 1 << 2,

  // mData points to a heap-allocated, raw buffer
  OWNED = 1 << 3,

  // mData points to a writable, inline buffer
  INLINE = 1 << 4,

  // mData points to a string literal; DataFlags::TERMINATED will also be set
  LITERAL = 1 << 5,

  // used to check for invalid flags -- all bits above the last item
  INVALID_MASK = (uint16_t) ~((LITERAL << 1) - 1)
};

// bits for mClassFlags
enum class StringClassFlags : uint16_t {
  // |this|'s buffer is inline, and requires the type to be binary-compatible
  // with nsTAutoStringN
  INLINE = 1 << 0,
  // |this| requires its buffer is null-terminated
  NULL_TERMINATED = 1 << 1,
  // used to check for invalid flags -- all bits above the last item
  INVALID_MASK = (uint16_t) ~((NULL_TERMINATED << 1) - 1)
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(StringDataFlags)
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(StringClassFlags)

}  // namespace detail
}  // namespace mozilla

#endif
