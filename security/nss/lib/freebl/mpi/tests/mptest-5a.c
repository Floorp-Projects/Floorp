/*
 * Simple test driver for MPI library
 *
 * Test 5a: Greatest common divisor speed test, binary vs. Euclid
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the MPI Arbitrary Precision Integer Arithmetic library.
 *
 * The Initial Developer of the Original Code is
 * Michael J. Fromberger.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
