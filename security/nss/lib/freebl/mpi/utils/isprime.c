/*
 *  isprime.c
 *
 *  Probabilistic primality tester command-line tool
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
