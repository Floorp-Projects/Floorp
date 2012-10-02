/*
 * makeprime.c
 *
 * A simple prime generator function (and test driver).  Prints out the
 * first prime it finds greater than or equal to the starting value.
 *
 * Usage: makeprime <start>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id: makeprime.c,v 1.4 2012/04/25 14:49:54 gerv%gerv.net Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* These two must be included for make_prime() to work */

#include "mpi.h"
#include "mpprime.h"

/*
  make_prime(p, nr)

  Find the smallest prime integer greater than or equal to p, where
  primality is verified by 'nr' iterations of the Rabin-Miller
  probabilistic primality test.  The caller is responsible for
  generating the initial value of p.

  Returns MP_OKAY if a prime has been generated, otherwise the error
  code indicates some other problem.  The value of p is clobbered; the
  caller should keep a copy if the value is needed.  
 */
mp_err   make_prime(mp_int *p, int nr);

/* The main() is not required -- it's just a test driver */
int main(int argc, char *argv[])
{
  mp_int    start;
  mp_err    res;

  if(argc < 2) {
    fprintf(stderr, "Usage: %s <start-value>\n", argv[0]);
    return 1;
  }
	    
  mp_init(&start);
  if(argv[1][0] == '0' && tolower(argv[1][1]) == 'x') {
    mp_read_radix(&start, argv[1] + 2, 16);
  } else {
    mp_read_radix(&start, argv[1], 10);
  }
  mp_abs(&start, &start);

  if((res = make_prime(&start, 5)) != MP_OKAY) {
    fprintf(stderr, "%s: error: %s\n", argv[0], mp_strerror(res));
    mp_clear(&start);

    return 1;

  } else {
    char  *buf = malloc(mp_radix_size(&start, 10));

    mp_todecimal(&start, buf);
    printf("%s\n", buf);
    free(buf);
    
    mp_clear(&start);

    return 0;
  }
  
} /* end main() */

/*------------------------------------------------------------------------*/

mp_err   make_prime(mp_int *p, int nr)
{
  mp_err  res;

  if(mp_iseven(p)) {
    mp_add_d(p, 1, p);
  }

  do {
    mp_digit   which = prime_tab_size;

    /*  First test for divisibility by a few small primes */
    if((res = mpp_divis_primes(p, &which)) == MP_YES)
      continue;
    else if(res != MP_NO)
      goto CLEANUP;

    /* If that passes, try one iteration of Fermat's test */
    if((res = mpp_fermat(p, 2)) == MP_NO)
      continue;
    else if(res != MP_YES)
      goto CLEANUP;

    /* If that passes, run Rabin-Miller as often as requested */
    if((res = mpp_pprime(p, nr)) == MP_YES)
      break;
    else if(res != MP_NO)
      goto CLEANUP;
      
  } while((res = mp_add_d(p, 2, p)) == MP_OKAY);

 CLEANUP:
  return res;

} /* end make_prime() */

/*------------------------------------------------------------------------*/
/* HERE THERE BE DRAGONS                                                  */
