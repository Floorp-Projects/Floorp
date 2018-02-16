/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/WrappingOperations.h"

#include <stdint.h>

using mozilla::WrapToSigned;

// NOTE: In places below |-FOO_MAX - 1| is used instead of |-FOO_MIN| because
//       in C++ numeric literals are full expressions -- the |-| in a negative
//       number is technically separate.  So with most compilers that limit
//       |int| to the signed 32-bit range, something like |-2147483648| is
//       operator-() applied to an *unsigned* expression.  And MSVC, at least,
//       warns when you do that.  (The operation is well-defined, but it likely
//       doesn't do what was intended.)  So we do the usual workaround for this
//       (see your local copy of <stdint.h> for a likely demo of this), writing
//       it out by negating the max value and subtracting 1.

static_assert(WrapToSigned(uint8_t(17)) == 17,
              "no wraparound should work, 8-bit");
static_assert(WrapToSigned(uint8_t(128)) == -128,
              "works for 8-bit numbers, wraparound low end");
static_assert(WrapToSigned(uint8_t(128 + 7)) == -128 + 7,
              "works for 8-bit numbers, wraparound mid");
static_assert(WrapToSigned(uint8_t(128 + 127)) == -128 + 127,
              "works for 8-bit numbers, wraparound high end");

static_assert(WrapToSigned(uint16_t(12345)) == 12345,
              "no wraparound should work, 16-bit");
static_assert(WrapToSigned(uint16_t(32768)) == -32768,
              "works for 16-bit numbers, wraparound low end");
static_assert(WrapToSigned(uint16_t(32768 + 42)) == -32768 + 42,
              "works for 16-bit numbers, wraparound mid");
static_assert(WrapToSigned(uint16_t(32768 + 32767)) == -32768 + 32767,
              "works for 16-bit numbers, wraparound high end");

static_assert(WrapToSigned(uint32_t(8675309)) == 8675309,
              "no wraparound should work, 32-bit");
static_assert(WrapToSigned(uint32_t(2147483648)) == -2147483647 - 1,
              "works for 32-bit numbers, wraparound low end");
static_assert(WrapToSigned(uint32_t(2147483648 + 42)) == -2147483647 - 1 + 42,
              "works for 32-bit numbers, wraparound mid");
static_assert(WrapToSigned(uint32_t(2147483648 + 2147483647)) ==
                -2147483647 - 1 + 2147483647,
              "works for 32-bit numbers, wraparound high end");

static_assert(WrapToSigned(uint64_t(4152739164)) == 4152739164,
              "no wraparound should work, 64-bit");
static_assert(WrapToSigned(uint64_t(9223372036854775808ULL)) == -9223372036854775807LL - 1,
              "works for 64-bit numbers, wraparound low end");
static_assert(WrapToSigned(uint64_t(9223372036854775808ULL + 8005552368LL)) ==
                -9223372036854775807LL - 1 + 8005552368LL,
              "works for 64-bit numbers, wraparound mid");
static_assert(WrapToSigned(uint64_t(9223372036854775808ULL + 9223372036854775807ULL)) ==
                -9223372036854775807LL - 1 + 9223372036854775807LL,
              "works for 64-bit numbers, wraparound high end");

int
main()
{
  return 0;
}
