/*
 * mptest-4b.c
 *
 * Test speed of a large modular exponentiation of a primitive element
 * modulo a prime.
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
