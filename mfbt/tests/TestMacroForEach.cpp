/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/MacroForEach.h"

#define HELPER_IDENTITY(x) x
#define HELPER_IDENTITY_PLUS(x) x +
static_assert(MOZ_FOR_EACH(HELPER_IDENTITY_PLUS, (), (10)) 0 == 10, "");
static_assert(MOZ_FOR_EACH(HELPER_IDENTITY_PLUS, (), (1, 1, 1)) 0 == 3, "");
static_assert(MOZ_FOR_EACH_SEPARATED(HELPER_IDENTITY, (+),
                                     (), (10)) == 10, "");
static_assert(MOZ_FOR_EACH_SEPARATED(HELPER_IDENTITY, (+),
                                     (), (1, 1, 1)) == 3, "");

#define HELPER_ONE_PLUS(x) HELPER_IDENTITY_PLUS(1)
static_assert(MOZ_FOR_EACH(HELPER_ONE_PLUS, (), ()) 0 == 0, "");

#define HELPER_DEFINE_VAR(x) const int test1_##x = x;
MOZ_FOR_EACH(HELPER_DEFINE_VAR, (), (10, 20))
static_assert(test1_10 == 10 && test1_20 == 20, "");

#define HELPER_DEFINE_VAR2(k, x) const int test2_##x = k + x;
MOZ_FOR_EACH(HELPER_DEFINE_VAR2, (5,), (10, 20))
static_assert(test2_10 == 15 && test2_20 == 25, "");

#define HELPER_DEFINE_PARAM(t, n) t n
constexpr int
test(MOZ_FOR_EACH_SEPARATED(HELPER_DEFINE_PARAM, (,), (int,), (a, b, c)))
{
  return a + b + c;
}
static_assert(test(1, 2, 3) == 6, "");

int
main()
{
#define HELPER_IDENTITY_COMMA(k1, k2, x) k1, k2, x,
  const int a[] = {
    MOZ_FOR_EACH(HELPER_IDENTITY_COMMA, (1, 2,), (10, 20, 30))
  };
  MOZ_RELEASE_ASSERT(a[0] == 1 && a[1] == 2 && a[2] == 10 &&
                     a[3] == 1 && a[4] == 2 && a[5] == 20 &&
                     a[6] == 1 && a[7] == 2 && a[8] == 30,
                     "MOZ_FOR_EACH args enumerated in incorrect order");
  return 0;
}
