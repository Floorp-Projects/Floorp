/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <type_traits>

#include "mozilla/Assertions.h"
#include "mozilla/TypeTraits.h"

using mozilla::IsDestructible;

class PublicDestructible {
 public:
  ~PublicDestructible();
};
class PrivateDestructible {
 private:
  ~PrivateDestructible();
};
class TrivialDestructible {};

static_assert(IsDestructible<PublicDestructible>::value,
              "public destructible class is destructible");
static_assert(!IsDestructible<PrivateDestructible>::value,
              "private destructible class is not destructible");
static_assert(IsDestructible<TrivialDestructible>::value,
              "trivial destructible class is destructible");

/*
 * Android's broken [u]intptr_t inttype macros are broken because its PRI*PTR
 * macros are defined as "ld", but sizeof(long) is 8 and sizeof(intptr_t)
 * is 4 on 32-bit Android. We redefine Android's PRI*PTR macros in
 * IntegerPrintfMacros.h and assert here that our new definitions match the
 * actual type sizes seen at compile time.
 */
#if defined(ANDROID) && !defined(__LP64__)
static_assert(std::is_same_v<int, intptr_t>,
              "emulated PRI[di]PTR definitions will be wrong");
static_assert(std::is_same_v<unsigned int, uintptr_t>,
              "emulated PRI[ouxX]PTR definitions will be wrong");
#endif

int main() { return 0; }
