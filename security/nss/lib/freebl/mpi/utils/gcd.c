/*
 *  gcd.c
 *
 *  Greatest common divisor
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpi.h"

char     *g_prog = NULL;

void print_mp_int(mp_int *mp, FILE *ofp);

int main(int argc, char *argv[]) 
{
  mp_int   a, b, x, y;
  mp_err   res;
  int      ext = 0;

  g_prog = argv[0];

  if(argc < 3) {
    fprintf(stderr, "Usage: %s <a> <b>\n", g_prog);
    return 1;
  }

  mp_init(&a); mp_read_radix(&a, argv[1], 10);
  mp_init(&b); mp_read_radix(&b, argv[2], 10);

  /* If we were called 'xgcd', compute x, y so that g = ax + by */
  if(strcmp(g_prog, "xgcd") == 0) {
    ext = 1;
    mp_init(&x); mp_init(&y);
  }

  if(ext) {
    if((res = mp_xgcd(&a, &b, &a, &x, &y)) != MP_OKAY) {
      fprintf(stderr, "%s: error: %s\n", g_prog, mp_strerror(res));
      mp_clear(&a); mp_clear(&b);
      mp_clear(&x); mp_clear(&y);
      return 1;
    }
  } else {
    if((res = mp_gcd(&a, &b, &a)) != MP_OKAY) {
      fprintf(stderr, "%s: error: %s\n", g_prog,
	      mp_strerror(res));
      mp_clear(&a); mp_clear(&b);
      return 1;
    }
  }

  print_mp_int(&a, stdout); 
  if(ext) {
    fputs("x = ", stdout); print_mp_int(&x, stdout); 
    fputs("y = ", stdout); print_mp_int(&y, stdout); 
  }

  mp_clear(&a); mp_clear(&b);

  if(ext) {
    mp_clear(&x);
    mp_clear(&y);
  }

  return 0;

}

void print_mp_int(mp_int *mp, FILE *ofp)
{
  char  *buf;
  int    len;

  len = mp_radix_size(mp, 10);
  buf = calloc(len, sizeof(char));
  mp_todecimal(mp, buf);
  fprintf(ofp, "%s\n", buf);
  free(buf);

}
