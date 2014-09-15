/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2013 Mozilla Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef mozilla_pkix__Input_h
#define mozilla_pkix__Input_h

#include "pkix/nullptr.h"
#include "pkix/Result.h"
#include "stdint.h"

namespace mozilla { namespace pkix {

// Expect* functions advance the input mark and return Success if the input
// matches the given criteria; they fail with the input mark in an undefined
// state if the input does not match the criteria.
//
// Match* functions advance the input mark and return true if the input matches
// the given criteria; they return false without changing the input mark if the
// input does not match the criteria.
//
// Skip* functions unconditionally advance the input mark and return Success if
// they are able to do so; otherwise they fail with the input mark in an
// undefined state.
class Input
{
public:
  Input()
    : input(nullptr)
    , end(nullptr)
  {
  }

  Result Init(const uint8_t* data, size_t len)
  {
    if (input) {
      // already initialized
      return Fail(SEC_ERROR_INVALID_ARGS);
    }
    if (!data || len > 0xffffu) {
      // input too large
      return Fail(SEC_ERROR_BAD_DER);
    }

    // XXX: this->input = input bug was not caught by tests! Why not?
    //      this->end = end bug was not caught by tests! Why not?
    this->input = data;
    this->end = data + len;

    return Success;
  }

  Result Expect(const uint8_t* expected, uint16_t expectedLen)
  {
    Result rv = EnsureLength(expectedLen);
    if (rv != Success) {
      return rv;
    }
    if (memcmp(input, expected, expectedLen)) {
      return Fail(SEC_ERROR_BAD_DER);
    }
    input += expectedLen;
    return Success;
  }

  bool Peek(uint8_t expectedByte) const
  {
    return input < end && *input == expectedByte;
  }

  Result Read(uint8_t& out)
  {
    Result rv = EnsureLength(1);
    if (rv != Success) {
      return rv;
    }
    out = *input++;
    return Success;
  }

  Result Read(uint16_t& out)
  {
    Result rv = EnsureLength(2);
    if (rv != Success) {
      return rv;
    }
    out = *input++;
    out <<= 8u;
    out |= *input++;
    return Success;
  }

  template <uint16_t N>
  bool MatchRest(const uint8_t (&toMatch)[N])
  {
    // Normally we use EnsureLength which compares (input + len < end), but
    // here we want to be sure that there is nothing following the matched
    // bytes
    if (static_cast<size_t>(end - input) != N) {
      return false;
    }
    if (memcmp(input, toMatch, N)) {
      return false;
    }
    input += N;
    return true;
  }

  template <uint16_t N>
  bool MatchTLV(uint8_t tag, uint16_t len, const uint8_t (&value)[N])
  {
    static_assert(N <= 127, "buffer larger than largest length supported");
    if (len > N) {
      PR_NOT_REACHED("overflow prevented dynamically instead of statically");
      return false;
    }
    uint16_t totalLen = 2u + len;
    if (EnsureLength(totalLen) != Success) {
      return false;
    }
    if (*input != tag) {
      return false;
    }
    if (*(input + 1) != len) {
      return false;
    }
    if (memcmp(input + 2, value, len)) {
      return false;
    }
    input += totalLen;
    return true;
  }

  Result Skip(uint16_t len)
  {
    Result rv = EnsureLength(len);
    if (rv != Success) {
      return rv;
    }
    input += len;
    return Success;
  }

  Result Skip(uint16_t len, Input& skippedInput)
  {
    Result rv = EnsureLength(len);
    if (rv != Success) {
      return rv;
    }
    rv = skippedInput.Init(input, len);
    if (rv != Success) {
      return rv;
    }
    input += len;
    return Success;
  }

  Result Skip(uint16_t len, SECItem& skippedItem)
  {
    Result rv = EnsureLength(len);
    if (rv != Success) {
      return rv;
    }
    skippedItem.type = siBuffer;
    skippedItem.data = const_cast<uint8_t*>(input);
    skippedItem.len = len;
    input += len;
    return Success;
  }

  void SkipToEnd()
  {
    input = end;
  }

  Result EnsureLength(uint16_t len)
  {
    if (static_cast<size_t>(end - input) < len) {
      return Fail(SEC_ERROR_BAD_DER);
    }
    return Success;
  }

  bool AtEnd() const { return input == end; }

  class Mark
  {
  private:
    friend class Input;
    Mark(const Input& input, const uint8_t* mark) : input(input), mark(mark) { }
    const Input& input;
    const uint8_t* const mark;
    void operator=(const Mark&) /* = delete */;
  };

  Mark GetMark() const { return Mark(*this, input); }

  Result GetSECItem(SECItemType type, const Mark& mark, /*out*/ SECItem& item)
  {
    if (&mark.input != this || mark.mark > input) {
      PR_NOT_REACHED("invalid mark");
      return Fail(SEC_ERROR_INVALID_ARGS);
    }
    item.type = type;
    item.data = const_cast<uint8_t*>(mark.mark);
    item.len = static_cast<decltype(item.len)>(input - mark.mark);
    return Success;
  }

private:
  const uint8_t* input;
  const uint8_t* end;

  Input(const Input&) /* = delete */;
  void operator=(const Input&) /* = delete */;
};

} } // namespace mozilla::pkix

#endif // mozilla_pkix__Input_h
