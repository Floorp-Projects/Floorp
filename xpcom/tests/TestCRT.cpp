/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

  nsAutoString t1,t2; 
  t1.AssignWithConversion(s1);
  t2.AssignWithConversion(s2);
  const PRUnichar* us1 = t1.get();
  const PRUnichar* us2 = t2.get();

  PRIntn u2 = nsCRT::strcmp(us1, us2);
  PRIntn u2_n = nsCRT::strncmp(us1, us2, n);

  NS_ASSERTION(sign(clib) == sign(u2), "strcmp");
  NS_ASSERTION(sign(clib_n) == sign(u2_n), "strncmp");
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
