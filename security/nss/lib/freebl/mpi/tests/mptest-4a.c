/* 
 *  mptest4a - modular exponentiation speed test 
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

extern mp_err s_mp_pad();

int main(int argc, char *argv[])
{
  int        ix, num, prec = 8;
  unsigned   int d;
  instant_t  start, finish;
  time_t     seed;
  mp_int     a, m, c;

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
  }
  
  printf("Test 3a: Modular exponentiation timing test\n"
	 "Precision:  %d digits (%d bits)\n"
	 "# of tests: %d\n\n", prec, prec * DIGIT_BIT, num);

  mp_init_size(&a, prec);
  mp_init_size(&m, prec);
  mp_init_size(&c, prec);
  s_mp_pad(&a, prec);
  s_mp_pad(&m, prec);
  s_mp_pad(&c, prec);

  printf("Testing modular exponentiation ... \n");
  srand((unsigned int)seed);

  start = now();
  for(ix = 0; ix < num; ix++) {
    mpp_random(&a);
    mpp_random(&c);
    mpp_random(&m);
    mp_exptmod(&a, &c, &m, &c);
  }
  finish = now();

  d = (finish.sec - start.sec) * 1000000;
  d -= start.usec; d += finish.usec;

  printf("Total time elapsed:        %u usec\n", d);
  printf("Time per exponentiation:   %u usec (%.3f sec)\n", 
	 (d / num), (double)(d / num) / 1000000);

  mp_clear(&c);
  mp_clear(&a);
  mp_clear(&m);

  return 0;
}
