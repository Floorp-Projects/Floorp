/*
 * makeprime.c
 *
 * A simple prime generator function (and test driver).  Prints out the
 * first prime it finds greater than or equal to the starting value.
 *
 * Usage: makeprime <start>
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
