/*
 * mptest-4b.c
 *
 * Test speed of a large modular exponentiation of a primitive element
 * modulo a prime.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#include <sys/time.h>

#include "mpi.h"
#include "mpprime.h"

char *g_prime = 
  "34BD53C07350E817CCD49721020F1754527959C421C1533244769D4CF060A8B1C3DA"
  "25094BE723FB1E2369B55FEEBBE0FAC16425161BF82684062B5EC5D7D47D1B23C117"
  "0FA19745E44A55E148314E582EB813AC9EE5126295E2E380CACC2F6D206B293E5ED9"
  "23B54EE961A8C69CD625CE4EC38B70C649D7F014432AEF3A1C93";
char *g_gen = "5";

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

extern mp_err s_mp_pad();

int main(int argc, char *argv[])
{
  instant_t    start, finish;
  mp_int       prime, gen, expt, res;
  unsigned int ix, diff;
  int          num;

  srand(time(NULL));

  if(argc < 2) {
    fprintf(stderr, "Usage: %s <num-tests>\n", argv[0]);
    return 1;
  }

  if((num = atoi(argv[1])) < 0)
    num = -num;

  if(num == 0)
    ++num;

  mp_init(&prime); mp_init(&gen); mp_init(&res);
  mp_read_radix(&prime, g_prime, 16);
  mp_read_radix(&gen, g_gen, 16);

  mp_init_size(&expt, USED(&prime) - 1);
  s_mp_pad(&expt, USED(&prime) - 1);

  printf("Testing %d modular exponentations ... \n", num);

  start = now();
  for(ix = 0; ix < num; ix++) {
    mpp_random(&expt);
    mp_exptmod(&gen, &expt, &prime, &res);
  }
  finish = now();

  diff = (finish.sec - start.sec) * 1000000;
  diff += finish.usec; diff -= start.usec;

  printf("%d operations took %u usec (%.3f sec)\n",
	 num, diff, (double)diff / 1000000.0);
  printf("That is %.3f sec per operation.\n",
	 ((double)diff / 1000000.0) / num);

  mp_clear(&expt);
  mp_clear(&res);
  mp_clear(&gen);
  mp_clear(&prime);

  return 0;
}
