/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsCRT.h"
#include "nsString.h"
#include "plstr.h"
#include <stdlib.h>

// The return from strcmp etc is only defined to be postive, zero or
// negative. The magnitude of a non-zero return is irrelevant.
PRIntn sign(PRIntn val) {
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
// iso-latin-1 strings, so the comparision must be valid.
static void Check(const char* s1, const char* s2, PRIntn n)
{
  PRIntn clib = PL_strcmp(s1, s2);
  PRIntn clib_n = PL_strncmp(s1, s2, n);
  PRIntn clib_case = PL_strcasecmp(s1, s2);
  PRIntn clib_case_n = PL_strncasecmp(s1, s2, n);

  nsAutoString t1(s1), t2(s2);
  const PRUnichar* us1 = t1.GetUnicode();
  const PRUnichar* us2 = t2.GetUnicode();

  PRIntn u = nsCRT::strcmp(us1, s2);
  PRIntn u_n = nsCRT::strncmp(us1, s2, n);
  PRIntn u_case = nsCRT::strcasecmp(us1, s2);
  PRIntn u_case_n = nsCRT::strncasecmp(us1, s2, n);

  PRIntn u2 = nsCRT::strcmp(us1, us2);
  PRIntn u2_n = nsCRT::strncmp(us1, us2, n);
  PRIntn u2_case = nsCRT::strcasecmp(us1, us2);
  PRIntn u2_case_n = nsCRT::strncasecmp(us1, us2, n);

  NS_ASSERTION(sign(clib) == sign(u), "strcmp");
  NS_ASSERTION(sign(clib_n) == sign(u_n), "strncmp");
  NS_ASSERTION(sign(clib_case) == sign(u_case), "strcasecmp");
  NS_ASSERTION(sign(clib_case_n) == sign(u_case_n), "strncasecmp");

  NS_ASSERTION(sign(clib) == sign(u2), "strcmp");
  NS_ASSERTION(sign(clib_n) == sign(u2_n), "strncmp");
  NS_ASSERTION(sign(clib_case) == sign(u2_case), "strcasecmp");
  NS_ASSERTION(sign(clib_case_n) == sign(u2_case_n), "strncasecmp");
}

struct Test {
  const char* s1;
  const char* s2;
  PRIntn n;
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

int main()
{
  Test* tp = tests;
  for (int i = 0; i < NUM_TESTS; i++, tp++) {
    Check(tp->s1, tp->s2, tp->n);
  }

  return 0;
}
