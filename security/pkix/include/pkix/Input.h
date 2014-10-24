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

#include <cstring>

#include "pkix/nullptr.h"
#include "pkix/Result.h"
#include "stdint.h"

namespace mozilla { namespace pkix {

class Reader;

// An Input is a safety-oriented immutable weak reference to a array of bytes
// of a known size. The data can only be legally accessed by constructing a
// Reader object, which guarantees all accesses to the data are memory safe.
// Neither Input not Reader provide any facilities for modifying the data
// they reference.
//
// Inputs are small and should usually be passed by value, not by reference,
// though for inline functions the distinction doesn't matter:
//
//    Result GoodExample(Input input);
//    Result BadExample(const Input& input);
//    Result WorseExample(const uint8_t* input, size_t len);
//
// Note that in the example, GoodExample has the same performance
// characteristics as WorseExample, but with much better safety guarantees.
class Input
{
public:
  typedef uint16_t size_type;

  // This constructor is useful for inputs that are statically known to be of a
  // fixed size, e.g.:
  //
  //   static const uint8_t EXPECTED_BYTES[] = { 0x00, 0x01, 0x02 };
  //   const Input expected(EXPECTED_BYTES);
  //
  // This is equivalent to (and preferred over):
  //
  //   static const uint8_t EXPECTED_BYTES[] = { 0x00, 0x01, 0x02 };
  //   Input expected;
  //   Result rv = expected.Init(EXPECTED_BYTES, sizeof EXPECTED_BYTES);
  template <size_type N>
  explicit Input(const uint8_t (&data)[N])
    : data(data)
    , len(N)
  {
  }

  // Construct a valid, empty, Init-able Input.
  Input()
    : data(nullptr)
    , len(0u)
  {
  }

  // Initialize the input. data must be non-null and len must be less than
  // 65536. Init may not be called more than once.
  Result Init(const uint8_t* data, size_t len)
  {
    if (this->data) {
      // already initialized
      return Result::FATAL_ERROR_INVALID_ARGS;
    }
    if (!data || len > 0xffffu) {
      // input too large
      return Result::ERROR_BAD_DER;
    }

    this->data = data;
    this->len = len;

    return Success;
  }

  // Initialize the input to be equivalent to the given input. Init may not be
  // called more than once.
  //
  // This is basically operator=, but it wasn't given that name because
  // normally callers do not check the result of operator=, and normally
  // operator= can be used multiple times.
  Result Init(Input other)
  {
    return Init(other.data, other.len);
  }

  // Returns the length of the input.
  //
  // Having the return type be size_type instead of size_t avoids the need for
  // callers to ensure that the result is small enough.
  size_type GetLength() const { return static_cast<size_type>(len); }

  // Don't use this. It is here because we have some "friend" functions that we
  // don't want to declare in this header file.
  const uint8_t* UnsafeGetData() const { return data; }

private:
  const uint8_t* data;
  size_t len;

  void operator=(const Input&) /* = delete */; // Use Init instead.
};

inline bool
InputsAreEqual(const Input& a, const Input& b)
{
  return a.GetLength() == b.GetLength() &&
         !std::memcmp(a.UnsafeGetData(), b.UnsafeGetData(), a.GetLength());
}

// An Reader is a cursor/iterator through the contents of an Input, designed to
// maximize safety during parsing while minimizing the performance cost of that
// safety. In particular, all methods do strict bounds checking to ensure
// buffer overflows are impossible, and they are all inline so that the
// compiler can coalesce as many of those checks together as possible.
//
// In general, Reader allows for one byte of lookahead and no backtracking.
// However, the Match* functions internally may have more lookahead.
class Reader
{
public:
  Reader()
    : input(nullptr)
    , end(nullptr)
  {
  }

  explicit Reader(Input input)
    : input(input.UnsafeGetData())
    , end(input.UnsafeGetData() + input.GetLength())
  {
  }

  Result Init(Input input)
  {
    if (this->input) {
      return Result::FATAL_ERROR_INVALID_ARGS;
    }
    this->input = input.UnsafeGetData();
    this->end = input.UnsafeGetData() + input.GetLength();
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

  template <Input::size_type N>
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
    input = end;
    return true;
  }

  bool MatchRest(Input toMatch)
  {
    // Normally we use EnsureLength which compares (input + len < end), but
    // here we want to be sure that there is nothing following the matched
    // bytes
    size_t remaining = static_cast<size_t>(end - input);
    if (toMatch.GetLength() != remaining) {
      return false;
    }
    if (std::memcmp(input, toMatch.UnsafeGetData(), remaining)) {
      return false;
    }
    input = end;
    return true;
  }

  Result Skip(Input::size_type len)
  {
    Result rv = EnsureLength(len);
    if (rv != Success) {
      return rv;
    }
    input += len;
    return Success;
  }

  Result Skip(Input::size_type len, Reader& skipped)
  {
    Result rv = EnsureLength(len);
    if (rv != Success) {
      return rv;
    }
    rv = skipped.Init(input, len);
    if (rv != Success) {
      return rv;
    }
    input += len;
    return Success;
  }

  Result Skip(Input::size_type len, Input& skipped)
  {
    Result rv = EnsureLength(len);
    if (rv != Success) {
      return rv;
    }
    rv = skipped.Init(input, len);
    if (rv != Success) {
      return rv;
    }
    input += len;
    return Success;
  }

  void SkipToEnd()
  {
    input = end;
  }

  Result EnsureLength(Input::size_type len)
  {
    if (static_cast<size_t>(end - input) < len) {
      return Result::ERROR_BAD_DER;
    }
    return Success;
  }

  bool AtEnd() const { return input == end; }

  class Mark
  {
  private:
    friend class Reader;
    Mark(const Reader& input, const uint8_t* mark) : input(input), mark(mark) { }
    const Reader& input;
    const uint8_t* const mark;
    void operator=(const Mark&) /* = delete */;
  };

  Mark GetMark() const { return Mark(*this, input); }

  Result GetInput(const Mark& mark, /*out*/ Input& item)
  {
    if (&mark.input != this || mark.mark > input) {
      return NotReached("invalid mark", Result::FATAL_ERROR_INVALID_ARGS);
    }
    return item.Init(mark.mark,
                     static_cast<Input::size_type>(input - mark.mark));
  }

private:
  Result Init(const uint8_t* data, Input::size_type len)
  {
    if (input) {
      // already initialized
      return Result::FATAL_ERROR_INVALID_ARGS;
    }
    input = data;
    end = data + len;
    return Success;
  }

  const uint8_t* input;
  const uint8_t* end;

  Reader(const Reader&) /* = delete */;
  void operator=(const Reader&) /* = delete */;
};

} } // namespace mozilla::pkix

#endif // mozilla_pkix__Input_h
