/*
 * Test whether to include squaring code given the current settings
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the MPI Arbitrary Precision Integer Arithmetic
 * library.
 *
 * The Initial Developer of the Original Code is Michael J. Fromberger.
 * Portions created by Michael J. Fromberger are 
 * Copyright (C) 1997, 1998, 1999, 2000 Michael J. Fromberger. 
 * All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable
 * instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the GPL.
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>

#define MP_SQUARE 1  /* make sure squaring code is included */

#include "mpi.h"
#include "mpprime.h"

int main(int argc, char *argv[])
{
  int           ntests, prec, ix;
  unsigned int  seed;
  clock_t       start, stop;
  double        multime, sqrtime;
  mp_int        a, c;

  seed = (unsigned int)time(NULL);

  if(argc < 3) {
    fprintf(stderr, "Usage: %s <ntests> <nbits>\n", argv[0]);
    return 1;
  }

  if((ntests = abs(atoi(argv[1]))) == 0) {
    fprintf(stderr, "%s: must request at least 1 test.\n", argv[0]);
    return 1;
  }
  if((prec = abs(atoi(argv[2]))) < CHAR_BIT) {
    fprintf(stderr, "%s: must request at least %d bits.\n", argv[0],
	    CHAR_BIT);
    return 1;
  }

  prec = (prec + (DIGIT_BIT - 1)) / DIGIT_BIT;

  mp_init_size(&a, prec);
  mp_init_size(&c, 2 * prec);

  /* Test multiplication by self */
  srand(seed);
  start = clock();
  for(ix = 0; ix < ntests; ix++) {
    mpp_random_size(&a, prec);
    mp_mul(&a, &a, &c);
  }
  stop = clock();

  multime = (double)(stop - start) / CLOCKS_PER_SEC;

  /* Test squaring */
  srand(seed);
  start = clock();
  for(ix = 0; ix < ntests; ix++) {
    mpp_random_size(&a, prec);
    mp_sqr(&a, &c);
  }
  stop = clock();

  sqrtime = (double)(stop - start) / CLOCKS_PER_SEC;

  printf("Multiply: %.4f\n", multime);
  printf("Square:   %.4f\n", sqrtime);
  if(multime < sqrtime) {
    printf("Speedup:  %.1f%%\n", 100.0 * (1.0 - multime / sqrtime));
    printf("Prefer:   multiply\n");
  } else {
    printf("Speedup:  %.1f%%\n", 100.0 * (1.0 - sqrtime / multime));
    printf("Prefer:   square\n");
  }

  mp_clear(&a); mp_clear(&c);
  return 0;

}
