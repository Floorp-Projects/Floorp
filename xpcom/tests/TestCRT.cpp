/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCRT.h"
#include "nsString.h"
#include "plstr.h"
#include <stdlib.h>

namespace TestCRT {

// The return from strcmp etc is only defined to be postive, zero or
// negative. The magnitude of a non-zero return is irrelevant.
int sign(int val) {
    if (val == 0)
	return 0;
    else {
	if (val > 0)
	    return 1;
	else
	    return -1;
    }
}


// Verify that nsCRT versions of string comparison routines get the
// same answers as the native non-unicode versions. We only pass in
// iso-latin-1 strings, so the comparison must be valid.
static void Check(const char* s1, const char* s2, int n)
{
#ifdef DEBUG
  int clib =
#endif
    PL_strcmp(s1, s2);

#ifdef DEBUG
  int clib_n =
#endif
    PL_strncmp(s1, s2, n);

  nsAutoString t1,t2; 
  t1.AssignWithConversion(s1);
  t2.AssignWithConversion(s2);
  const char16_t* us1 = t1.get();
  const char16_t* us2 = t2.get();

#ifdef DEBUG
  int u2 =
#endif
    nsCRT::strcmp(us1, us2);

#ifdef DEBUG
  int u2_n =
#endif
    nsCRT::strncmp(us1, us2, n);

  NS_ASSERTION(sign(clib) == sign(u2), "strcmp");
  NS_ASSERTION(sign(clib_n) == sign(u2_n), "strncmp");
}

struct Test {
  const char* s1;
  const char* s2;
  int n;
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
};
#define NUM_TESTS int((sizeof(tests) / sizeof(tests[0])))

}

using namespace TestCRT;

int main()
{
  Test* tp = tests;
  for (int i = 0; i < NUM_TESTS; i++, tp++) {
    Check(tp->s1, tp->s2, tp->n);
  }

  return 0;
}
