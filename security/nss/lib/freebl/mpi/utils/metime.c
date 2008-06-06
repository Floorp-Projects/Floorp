/* 
 *  metime.c
 *
 * Modular exponentiation timing test
 *
 * $Id: metime.c,v 1.5 2004/04/27 23:04:37 gerv%gerv.net Exp $
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
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Netscape Communications Corporation
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
/* $Id: metime.c,v 1.5 2004/04/27 23:04:37 gerv%gerv.net Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#include "mpi.h"
#include "mpprime.h"

double clk_to_sec(clock_t start, clock_t stop);

int main(int argc, char *argv[])
{
  int          ix, num, prec = 8;
  unsigned int seed;
  clock_t      start, stop;
  double       sec;

  mp_int     a, m, c;

  if(getenv("SEED") != NULL)
    seed = abs(atoi(getenv("SEED")));
  else 
    seed = (unsigned int)time(NULL);

  if(argc < 2) {
    fprintf(stderr, "Usage: %s <num-tests> [<nbits>]\n", argv[0]);
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
  
  printf("Modular exponentiation timing test\n"
	 "Precision:  %d digits (%d bits)\n"
	 "# of tests: %d\n\n", prec, prec * DIGIT_BIT, num);

  mp_init_size(&a, prec);
  mp_init_size(&m, prec);
  mp_init_size(&c, prec);

  srand(seed);

  start = clock();
  for(ix = 0; ix < num; ix++) {

    mpp_random_size(&a, prec);
    mpp_random_size(&c, prec);
    mpp_random_size(&m, prec);
    /* set msb and lsb of m */
    DIGIT(&m,0) |= 1;
    DIGIT(&m, USED(&m)-1) |= (mp_digit)1 << (DIGIT_BIT - 1);
    if (mp_cmp(&a, &m) > 0)
      mp_sub(&a, &m, &a);

    mp_exptmod(&a, &c, &m, &c);
  }
  stop = clock();

  sec = clk_to_sec(start, stop);

  printf("Total:      %.3f seconds\n", sec);
  printf("Individual: %.3f seconds\n", sec / num);

  mp_clear(&c);
  mp_clear(&a);
  mp_clear(&m);

  return 0;
}

double clk_to_sec(clock_t start, clock_t stop)
{
  return (double)(stop - start) / CLOCKS_PER_SEC;
}

/*------------------------------------------------------------------------*/
/* HERE THERE BE DRAGONS                                                  */
