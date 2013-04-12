/*
 *  bbsrand.c
 *
 *  Test driver for routines in bbs_rand.h
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include "bbs_rand.h"

#define NUM_TESTS   100

int main(void)
{
  unsigned int   seed, result, ix;

  seed = time(NULL);
  bbs_srand((unsigned char *)&seed, sizeof(seed));

  for(ix = 0; ix < NUM_TESTS; ix++) {
    result = bbs_rand();
    
    printf("Test %3u: %08X\n", ix + 1, result);
  }

  return 0;
}
