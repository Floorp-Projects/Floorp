/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/StandardInteger.h"
 
#include "prtypes.h"

// This test is NOT intended to be run.  It's a test to make sure
// PRInt{N} matches int{N}_t. If they don't match, we should get a
// compiler warning or error in main().

static void
ClearNSPRIntTypes(PRInt8 *a, PRInt16 *b, PRInt32 *c, PRInt64 *d)
{
  *a = 0; *b = 0; *c = 0; *d = 0;
}

static void
ClearStdIntTypes(int8_t *w, int16_t *x, int32_t *y, int64_t *z)
{
  *w = 0; *x = 0; *y = 0; *z = 0;
}

int
main()
{
  PRInt8 a; PRInt16 b; PRInt32 c; PRInt64 d;
  int8_t w; int16_t x; int32_t y; int64_t z;

  ClearNSPRIntTypes(&w, &x, &y, &z);
  ClearStdIntTypes(&a, &b, &c, &d);
  return 0;
}
