/*
 *  isprime.c
 *
 *  Probabilistic primality tester command-line tool
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id: isprime.c,v 1.4 2012/04/25 14:49:54 gerv%gerv.net Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpi.h"
#include "mpprime.h"

#define  RM_TESTS       15   /* how many iterations of Rabin-Miller? */
#define  MINIMUM        1024 /* don't bother us with a < this        */

int      g_tests = RM_TESTS;
char    *g_prog = NULL;

int main(int argc, char *argv[])
{
  mp_int   a;
  mp_digit np = prime_tab_size; /* from mpprime.h */
  int      res = 0;

  g_prog = argv[0];

  if(argc < 2) {
    fprintf(stderr, "Usage: %s <a>, where <a> is a decimal integer\n"
	    "Use '0x' prefix for a hexadecimal value\n", g_prog);
    return 1;
  }

  /* Read number of tests from environment, if present */
  {
    char *tmp;

    if((tmp = getenv("RM_TESTS")) != NULL) {
      if((g_tests = atoi(tmp)) <= 0)
	g_tests = RM_TESTS;
    }
  }

  mp_init(&a);
  if(argv[1][0] == '0' && argv[1][1] == 'x')
    mp_read_radix(&a, argv[1] + 2, 16);
  else
    mp_read_radix(&a, argv[1], 10);

  if(mp_cmp_d(&a, MINIMUM) <= 0) {
    fprintf(stderr, "%s: please use a value greater than %d\n", 
	    g_prog, MINIMUM);
    mp_clear(&a);
    return 1;
  }

  /* Test for divisibility by small primes */
  if(mpp_divis_primes(&a, &np) != MP_NO) {
    printf("Not prime (divisible by small prime %d)\n", np);
    res = 2;
    goto CLEANUP;
  }

  /* Test with Fermat's test, using 2 as a witness */
  if(mpp_fermat(&a, 2) != MP_YES) {
    printf("Not prime (failed Fermat test)\n");
    res = 2;
    goto CLEANUP;
  }
 
  /* Test with Rabin-Miller probabilistic test */
  if(mpp_pprime(&a, g_tests) == MP_NO) {
      printf("Not prime (failed pseudoprime test)\n");
      res = 2;
      goto CLEANUP;
  }

  printf("Probably prime, 1 in 4^%d chance of false positive\n", g_tests);

CLEANUP:
  mp_clear(&a);
  
  return res;

}
