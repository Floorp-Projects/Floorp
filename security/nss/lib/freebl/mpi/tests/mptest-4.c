/*
 * Simple test driver for MPI library
 *
 * Test 4: Modular arithmetic tests
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id: mptest-4.c,v 1.4 2012/04/25 14:49:53 gerv%gerv.net Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "mpi.h"

int main(int argc, char *argv[])
{
  int      ix;
  mp_int   a, b, c, m;
  mp_digit r;

  if(argc < 4) {
    fprintf(stderr, "Usage: %s <a> <b> <m>\n", argv[0]);
    return 1;
  }

  printf("Test 4: Modular arithmetic\n\n");

  mp_init(&a);
  mp_init(&b);
  mp_init(&m);

  mp_read_radix(&a, argv[1], 10);
  mp_read_radix(&b, argv[2], 10);
  mp_read_radix(&m, argv[3], 10);
  printf("a = "); mp_print(&a, stdout); fputc('\n', stdout);
  printf("b = "); mp_print(&b, stdout); fputc('\n', stdout);
  printf("m = "); mp_print(&m, stdout); fputc('\n', stdout);
  
  mp_init(&c);
  printf("\nc = a (mod m)\n");

  mp_mod(&a, &m, &c);
  printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);

  printf("\nc = b (mod m)\n");

  mp_mod(&b, &m, &c);
  printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);

  printf("\nc = b (mod 1853)\n");

  mp_mod_d(&b, 1853, &r);
  printf("c = %04X\n", r);

  printf("\nc = (a + b) mod m\n");

  mp_addmod(&a, &b, &m, &c);
  printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);

  printf("\nc = (a - b) mod m\n");

  mp_submod(&a, &b, &m, &c);
  printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);

  printf("\nc = (a * b) mod m\n");

  mp_mulmod(&a, &b, &m, &c);
  printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);

  printf("\nc = (a ** b) mod m\n");

  mp_exptmod(&a, &b, &m, &c);
  printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);

  printf("\nIn-place modular squaring test:\n");
  for(ix = 0; ix < 5; ix++) {
    printf("a = (a * a) mod m   a = ");
    mp_sqrmod(&a, &m, &a);
    mp_print(&a, stdout);
    fputc('\n', stdout);
  }
  

  mp_clear(&c);
  mp_clear(&m);
  mp_clear(&b);
  mp_clear(&a);

  return 0;
}
