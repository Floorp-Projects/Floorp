/*
 *  prng.c
 *
 *  Command-line pseudo-random number generator
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>

#ifdef __OS2__
#include <types.h>
#include <process.h>
#else
#include <unistd.h>
#endif

#include "bbs_rand.h"

int main(int argc, char *argv[])
{
  unsigned char *seed;
  unsigned int   ix, num = 1;
  pid_t          pid;
  
  if(argc > 1) {
    num = atoi(argv[1]);
    if(num <= 0) 
      num = 1;
  }

  pid = getpid();
  srand(time(NULL) * (unsigned int)pid);

  /* Not a perfect seed, but not bad */
  seed = malloc(bbs_seed_size);
  for(ix = 0; ix < bbs_seed_size; ix++) {
    seed[ix] = rand() % UCHAR_MAX;
  }

  bbs_srand(seed, bbs_seed_size);
  memset(seed, 0, bbs_seed_size);
  free(seed);

  while(num-- > 0) {
    ix = bbs_rand();

    printf("%u\n", ix);
  }

  return 0;

}
