/*
 *  exptmod.c
 *
 * Command line tool to perform modular exponentiation on arbitrary
 * precision integers.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpi.h"

int main(int argc, char *argv[])
{
  mp_int  a, b, m;
  mp_err  res;
  char   *str;
  int     len, rval = 0;

  if(argc < 3) {
    fprintf(stderr, "Usage: %s <a> <b> <m>\n", argv[0]);
    return 1;
  }

  mp_init(&a); mp_init(&b); mp_init(&m);
  mp_read_radix(&a, argv[1], 10);
  mp_read_radix(&b, argv[2], 10);
  mp_read_radix(&m, argv[3], 10);

  if((res = mp_exptmod(&a, &b, &m, &a)) != MP_OKAY) {
    fprintf(stderr, "%s: error: %s\n", argv[0], mp_strerror(res));
    rval = 1;
  } else {
    len = mp_radix_size(&a, 10);
    str = calloc(len, sizeof(char));
    mp_toradix(&a, str, 10);

    printf("%s\n", str);

    free(str);
  }

  mp_clear(&a); mp_clear(&b); mp_clear(&m);

  return rval;
}
