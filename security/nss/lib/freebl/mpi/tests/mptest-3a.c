/*
 * Simple test driver for MPI library
 *
 * Test 3a: Multiplication vs. squaring timing test
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include <time.h>

#include "mpi.h"
#include "mpprime.h"

int main(int argc, char *argv[])
{
  int        ix, num, prec = 8;
  double     d1, d2;
  clock_t    start, finish;
  time_t     seed;
  mp_int     a, c, d;

  seed = time(NULL);

  if(argc < 2) {
    fprintf(stderr, "Usage: %s <num-tests> [<precision>]\n", argv[0]);
    return 1;
  }

  if((num = atoi(argv[1])) < 0)
    num = -num;

  if(!num) {
    fprintf(stderr, "%s: must perform at least 1 test\n", argv[0]);
    return 1;
  }

  if(argc > 2) {
    if((prec = atoi(argv[2])) <= 0)
      prec = 8;
    else
      prec = (prec + (DIGIT_BIT - 1)) / DIGIT_BIT;
  }
  
  printf("Test 3a: Multiplication vs squaring timing test\n"
	 "Precision:  %d digits (%u bits)\n"
	 "# of tests: %d\n\n", prec, prec * DIGIT_BIT, num);

  mp_init_size(&a, prec);

  mp_init(&c); mp_init(&d);

  printf("Verifying accuracy ... \n");
  srand((unsigned int)seed);
  for(ix = 0; ix < num; ix++) {
    mpp_random_size(&a, prec);
    mp_mul(&a, &a, &c);
    mp_sqr(&a, &d);

    if(mp_cmp(&c, &d) != 0) {
      printf("Error!  Results not accurate:\n");
      printf("a = "); mp_print(&a, stdout); fputc('\n', stdout);
      printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);
      printf("d = "); mp_print(&d, stdout); fputc('\n', stdout);
      mp_sub(&c, &d, &d);
      printf("dif "); mp_print(&d, stdout); fputc('\n', stdout);
      mp_clear(&c); mp_clear(&d);
      mp_clear(&a);
      return 1;
    }
  }
  printf("Accuracy is confirmed for the %d test samples\n", num);
  mp_clear(&d);

  printf("Testing squaring ... \n");
  srand((unsigned int)seed);
  start = clock();
  for(ix = 0; ix < num; ix++) {
    mpp_random_size(&a, prec);
    mp_sqr(&a, &c);
  }
  finish = clock();

  d2 = (double)(finish - start) / CLOCKS_PER_SEC;

  printf("Testing multiplication ... \n");
  srand((unsigned int)seed);
  start = clock();
  for(ix = 0; ix < num; ix++) {
    mpp_random(&a);
    mp_mul(&a, &a, &c);
  }
  finish = clock();

  d1 = (double)(finish - start) / CLOCKS_PER_SEC;

  printf("Multiplication time: %.3f sec (%.3f each)\n", d1, d1 / num);
  printf("Squaring time:       %.3f sec (%.3f each)\n", d2, d2 / num);
  printf("Improvement:         %.2f%%\n", (1.0 - (d2 / d1)) * 100.0);

  mp_clear(&c);
  mp_clear(&a);

  return 0;
}
