/*
 * Simple test driver for MPI library
 *
 * Test 5: Other number theoretic functions
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "mpi.h"

int main(int argc, char *argv[])
{
  mp_int   a, b, c, x, y;

  if(argc < 3) {
    fprintf(stderr, "Usage: %s <a> <b>\n", argv[0]);
    return 1;
  }

  printf("Test 5: Number theoretic functions\n\n");

  mp_init(&a);
  mp_init(&b);

  mp_read_radix(&a, argv[1], 10);
  mp_read_radix(&b, argv[2], 10);

  printf("a = "); mp_print(&a, stdout); fputc('\n', stdout);
  printf("b = "); mp_print(&b, stdout); fputc('\n', stdout);
  
  mp_init(&c);
  printf("\nc = (a, b)\n");

  mp_gcd(&a, &b, &c);
  printf("Euclid: c = "); mp_print(&c, stdout); fputc('\n', stdout);
/*
  mp_bgcd(&a, &b, &c);
  printf("Binary: c = "); mp_print(&c, stdout); fputc('\n', stdout);
*/
  mp_init(&x);
  mp_init(&y);
  printf("\nc = (a, b) = ax + by\n");

  mp_xgcd(&a, &b, &c, &x, &y);
  printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);
  printf("x = "); mp_print(&x, stdout); fputc('\n', stdout);
  printf("y = "); mp_print(&y, stdout); fputc('\n', stdout);

  printf("\nc = a^-1 (mod b)\n");
  if(mp_invmod(&a, &b, &c) == MP_UNDEF) {
    printf("a has no inverse mod b\n");
  } else {
    printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);
  }

  mp_clear(&y);
  mp_clear(&x);
  mp_clear(&c);
  mp_clear(&b);
  mp_clear(&a);

  return 0;
}
