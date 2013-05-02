/*
 *  basecvt.c
 *
 *  Convert integer values specified on the command line from one input
 *  base to another.  Accepts input and output bases between 2 and 36
 *  inclusive.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpi.h"

#define IBASE     10
#define OBASE     16
#define USAGE     "Usage: %s ibase obase [value]\n"
#define MAXBASE   64
#define MINBASE   2

int main(int argc, char *argv[])
{
  int    ix, ibase = IBASE, obase = OBASE;
  mp_int val;

  ix = 1;
  if(ix < argc) {
    ibase = atoi(argv[ix++]);
    
    if(ibase < MINBASE || ibase > MAXBASE) {
      fprintf(stderr, "%s: input radix must be between %d and %d inclusive\n",
	      argv[0], MINBASE, MAXBASE);
      return 1;
    }
  }
  if(ix < argc) {
    obase = atoi(argv[ix++]);

    if(obase < MINBASE || obase > MAXBASE) {
      fprintf(stderr, "%s: output radix must be between %d and %d inclusive\n",
	      argv[0], MINBASE, MAXBASE);
      return 1;
    }
  }

  mp_init(&val);
  while(ix < argc) {
    char  *out;
    int    outlen;

    mp_read_radix(&val, argv[ix++], ibase);

    outlen = mp_radix_size(&val, obase);
    out = calloc(outlen, sizeof(char));
    mp_toradix(&val, out, obase);

    printf("%s\n", out);
    free(out);
  }

  mp_clear(&val);

  return 0;
}
