/*
 * pi.c
 *
 * Compute pi to an arbitrary number of digits.  Uses Machin's formula,
 * like everyone else on the planet:
 * 
 *    pi = 16 * arctan(1/5) - 4 * arctan(1/239)
 *
 * This is pretty effective for up to a few thousand digits, but it
 * gets pretty slow after that.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#include "mpi.h"

mp_err arctan(mp_digit mul, mp_digit x, mp_digit prec, mp_int *sum);

int main(int argc, char *argv[])
{
  mp_err       res;
  mp_digit     ndigits;
  mp_int       sum1, sum2;
  clock_t      start, stop;
  int          out = 0;

  /* Make the user specify precision on the command line */
  if(argc < 2) {
    fprintf(stderr, "Usage: %s <num-digits>\n", argv[0]);
    return 1;
  }

  if((ndigits = abs(atoi(argv[1]))) == 0) {
    fprintf(stderr, "%s: you must request at least 1 digit\n", argv[0]);
    return 1;
  }

  start = clock();
  mp_init(&sum1); mp_init(&sum2);

  /* sum1 = 16 * arctan(1/5)  */
  if((res = arctan(16, 5, ndigits, &sum1)) != MP_OKAY) {
    fprintf(stderr, "%s: arctan: %s\n", argv[0], mp_strerror(res));
    out = 1; goto CLEANUP;
  }

  /* sum2 = 4 * arctan(1/239) */
  if((res = arctan(4, 239, ndigits, &sum2)) != MP_OKAY) {
    fprintf(stderr, "%s: arctan: %s\n", argv[0], mp_strerror(res));
    out = 1; goto CLEANUP;
  }

  /* pi = sum1 - sum2         */
  if((res = mp_sub(&sum1, &sum2, &sum1)) != MP_OKAY) {
    fprintf(stderr, "%s: mp_sub: %s\n", argv[0], mp_strerror(res));
    out = 1; goto CLEANUP;
  }
  stop = clock();

  /* Write the output in decimal */
  {
    char  *buf = malloc(mp_radix_size(&sum1, 10));

    if(buf == NULL) {
      fprintf(stderr, "%s: out of memory\n", argv[0]);
      out = 1; goto CLEANUP;
    }
    mp_todecimal(&sum1, buf);
    printf("%s\n", buf);
    free(buf);
  }

  fprintf(stderr, "Computation took %.2f sec.\n", 
	  (double)(stop - start) / CLOCKS_PER_SEC);

 CLEANUP:
  mp_clear(&sum1);
  mp_clear(&sum2);

  return out;

}

/* Compute sum := mul * arctan(1/x), to 'prec' digits of precision */
mp_err arctan(mp_digit mul, mp_digit x, mp_digit prec, mp_int *sum)
{
  mp_int   t, v;
  mp_digit q = 1, rd;
  mp_err   res;
  int      sign = 1;

  prec += 3;  /* push inaccuracies off the end */

  mp_init(&t); mp_set(&t, 10);
  mp_init(&v); 
  if((res = mp_expt_d(&t, prec, &t)) != MP_OKAY ||  /* get 10^prec    */
     (res = mp_mul_d(&t, mul, &t)) != MP_OKAY ||    /* ... times mul  */
     (res = mp_mul_d(&t, x, &t)) != MP_OKAY)        /* ... times x    */
    goto CLEANUP;

  /*
    The extra multiplication by x in the above takes care of what
    would otherwise have to be a special case for 1 / x^1 during the
    first loop iteration.  A little sneaky, but effective.

    We compute arctan(1/x) by the formula:

         1     1       1       1
	 - - ----- + ----- - ----- + ...
	 x   3 x^3   5 x^5   7 x^7

    We multiply through by 'mul' beforehand, which gives us a couple
    more iterations and more precision
   */

  x *= x; /* works as long as x < sqrt(RADIX), which it is here */

  mp_zero(sum);

  do {
    if((res = mp_div_d(&t, x, &t, &rd)) != MP_OKAY)
      goto CLEANUP;

    if(sign < 0 && rd != 0)
      mp_add_d(&t, 1, &t);

    if((res = mp_div_d(&t, q, &v, &rd)) != MP_OKAY)
      goto CLEANUP;

    if(sign < 0 && rd != 0)
      mp_add_d(&v, 1, &v);

    if(sign > 0)
      res = mp_add(sum, &v, sum);
    else
      res = mp_sub(sum, &v, sum);

    if(res != MP_OKAY)
      goto CLEANUP;

    sign *= -1;
    q += 2;

  } while(mp_cmp_z(&t) != 0);

  /* Chop off inaccurate low-order digits */
  mp_div_d(sum, 1000, sum, NULL);

 CLEANUP:
  mp_clear(&v);
  mp_clear(&t);

  return res;
}

/*------------------------------------------------------------------------*/
/* HERE THERE BE DRAGONS                                                  */
