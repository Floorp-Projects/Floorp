/*
 * Simple test driver for MPI library
 *
 * Test 5a: Greatest common divisor speed test, binary vs. Euclid
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>

#include <sys/time.h>

#include "mpi.h"
#include "mpprime.h"

typedef struct {
  unsigned int  sec;
  unsigned int  usec;
} instant_t;

instant_t now(void)
{
  struct timeval clk;
  instant_t      res;

  res.sec = res.usec = 0;

  if(gettimeofday(&clk, NULL) != 0)
    return res;

  res.sec = clk.tv_sec;
  res.usec = clk.tv_usec;

  return res;
}

#define PRECISION 16

int main(int argc, char *argv[])
{
  int          ix, num, prec = PRECISION;
  mp_int       a, b, c, d;
  instant_t    start, finish;
  time_t       seed;
  unsigned int d1, d2;

  seed = time(NULL);

  if(argc < 2) {
    fprintf(stderr, "Usage: %s <num-tests>\n", argv[0]);
    return 1;
  }

  if((num = atoi(argv[1])) < 0)
    num = -num;

  printf("Test 5a: Euclid vs. Binary, a GCD speed test\n\n"
	 "Number of tests: %d\n"
	 "Precision:       %d digits\n\n", num, prec);

  mp_init_size(&a, prec);
  mp_init_size(&b, prec);
  mp_init(&c);
  mp_init(&d);

  printf("Verifying accuracy ... \n");
  srand((unsigned int)seed);
  for(ix = 0; ix < num; ix++) {
    mpp_random_size(&a, prec);
    mpp_random_size(&b, prec);

    mp_gcd(&a, &b, &c);
    mp_bgcd(&a, &b, &d);

    if(mp_cmp(&c, &d) != 0) {
      printf("Error!  Results not accurate:\n");
      printf("a = "); mp_print(&a, stdout); fputc('\n', stdout);
      printf("b = "); mp_print(&b, stdout); fputc('\n', stdout);
      printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);
      printf("d = "); mp_print(&d, stdout); fputc('\n', stdout);

      mp_clear(&a); mp_clear(&b); mp_clear(&c); mp_clear(&d);
      return 1;
    }
  }
  mp_clear(&d);
  printf("Accuracy confirmed for the %d test samples\n", num);

  printf("Testing Euclid ... \n");
  srand((unsigned int)seed);
  start = now();
  for(ix = 0; ix < num; ix++) {
    mpp_random_size(&a, prec);
    mpp_random_size(&b, prec);
    mp_gcd(&a, &b, &c);

  }
  finish = now();

  d1 = (finish.sec - start.sec) * 1000000;
  d1 -= start.usec; d1 += finish.usec;

  printf("Testing binary ... \n");
  srand((unsigned int)seed);
  start = now();
  for(ix = 0; ix < num; ix++) {
    mpp_random_size(&a, prec);
    mpp_random_size(&b, prec);
    mp_bgcd(&a, &b, &c);
  }
  finish = now();

  d2 = (finish.sec - start.sec) * 1000000;
  d2 -= start.usec; d2 += finish.usec;

  printf("Euclidean algorithm time: %u usec\n", d1);
  printf("Binary algorithm time:    %u usec\n", d2);
  printf("Improvement:              %.2f%%\n",
         (1.0 - ((double)d2 / (double)d1)) * 100.0);

  mp_clear(&c);
  mp_clear(&b);
  mp_clear(&a);

  return 0;
}
