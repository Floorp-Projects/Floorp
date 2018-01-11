/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCRT.h"
#include "nsString.h"
#include "plstr.h"
#include <stdlib.h>
#include "gtest/gtest.h"

namespace TestCRT {

// The return from strcmp etc is only defined to be postive, zero or
// negative. The magnitude of a non-zero return is irrelevant.
int sign(int val) {
  if (val == 0) {
    return 0;
  } else {
    if (val > 0) {
      return 1;
    } else {
      return -1;
    }
  }
}


// Verify that nsCRT versions of string comparison routines get the
// same answers as the native non-unicode versions. We only pass in
// iso-latin-1 strings, so the comparison must be valid.
static void Check(const char* s1, const char* s2, size_t n)
{
  bool longerThanN = strlen(s1) > n || strlen(s2) > n;

  int clib = PL_strcmp(s1, s2);
  int clib_n = PL_strncmp(s1, s2, n);

  if (!longerThanN) {
    EXPECT_EQ(sign(clib), sign(clib_n));
  }

  nsAutoString t1,t2;
  CopyASCIItoUTF16(s1, t1);
  CopyASCIItoUTF16(s2, t2);
  const char16_t* us1 = t1.get();
  const char16_t* us2 = t2.get();

  int u2, u2_n;
  u2 = nsCRT::strcmp(us1, us2);

  EXPECT_EQ(sign(clib), sign(u2));

  u2 = NS_strcmp(us1, us2);
  u2_n = NS_strncmp(us1, us2, n);

  EXPECT_EQ(sign(clib), sign(u2));
  EXPECT_EQ(sign(clib_n), sign(u2_n));
}

struct Test {
  const char* s1;
  const char* s2;
  size_t n;
};

static Test tests[] = {
  { "foo", "foo", 3 },
  { "foo", "fo", 3 },

  { "foo", "bar", 3 },
  { "foo", "ba", 3 },

  { "foo", "zap", 3 },
  { "foo", "za", 3 },

  { "bar", "foo", 3 },
  { "bar", "fo", 3 },

  { "bar", "foo", 3 },
  { "bar", "fo", 3 },

  { "foo", "foobar", 3 },
  { "foobar", "foo", 3 },
  { "foobar", "foozap", 3 },
  { "foozap", "foobar", 3 },
};
#define NUM_TESTS int((sizeof(tests) / sizeof(tests[0])))

TEST(CRT, main)
{
  TestCRT::Test* tp = tests;
  for (int i = 0; i < NUM_TESTS; i++, tp++) {
    Check(tp->s1, tp->s2, tp->n);
  }
}

} // namespace TestCRT
