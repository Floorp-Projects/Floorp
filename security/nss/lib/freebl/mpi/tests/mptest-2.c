/*
 * Simple test driver for MPI library
 *
 * Test 2: Basic addition and subtraction test
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
  mp_int a, b, c;

  if(argc < 3) {
    fprintf(stderr, "Usage: %s <a> <b>\n", argv[0]);
    return 1;
  }

  printf("Test 2: Basic addition and subtraction\n\n");

  mp_init(&a);
  mp_init(&b);

  mp_read_radix(&a, argv[1], 10);
  mp_read_radix(&b, argv[2], 10);
  printf("a = "); mp_print(&a, stdout); fputc('\n', stdout);
  printf("b = "); mp_print(&b, stdout); fputc('\n', stdout);
  
  mp_init(&c);
  printf("c = a + b\n");

  mp_add(&a, &b, &c);
  printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);
  
  printf("c = a - b\n");
  
  mp_sub(&a, &b, &c);
  printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);  

  mp_clear(&c);
  mp_clear(&b);
  mp_clear(&a);

  return 0;
}
