/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"
#include "mpprime.h"
#include <sys/types.h>
#include <time.h>

#define MAX_PREC (4096 / MP_DIGIT_BIT)

mp_err identity_test(void)
{
  mp_size       preca, precb;
  mp_err        res;
  mp_int        a, b;
  mp_int        t1, t2, t3, t4, t5;

  preca = (rand() % MAX_PREC) + 1;
  precb = (rand() % MAX_PREC) + 1;

  MP_DIGITS(&a) = 0;
  MP_DIGITS(&b) = 0;
  MP_DIGITS(&t1) = 0;
  MP_DIGITS(&t2) = 0;
  MP_DIGITS(&t3) = 0;
  MP_DIGITS(&t4) = 0;
  MP_DIGITS(&t5) = 0;

  MP_CHECKOK( mp_init(&a)  );
  MP_CHECKOK( mp_init(&b)  );
  MP_CHECKOK( mp_init(&t1) );
  MP_CHECKOK( mp_init(&t2) );
  MP_CHECKOK( mp_init(&t3) );
  MP_CHECKOK( mp_init(&t4) );
  MP_CHECKOK( mp_init(&t5) );

  MP_CHECKOK( mpp_random_size(&a, preca) );
  MP_CHECKOK( mpp_random_size(&b, precb) );

  if (mp_cmp(&a, &b) < 0)
    mp_exch(&a, &b);

  MP_CHECKOK( mp_mod(&a, &b, &t1) );       /* t1 = a%b */
  MP_CHECKOK( mp_div(&a, &b, &t2, NULL) ); /* t2 = a/b */
  MP_CHECKOK( mp_mul(&b, &t2, &t3) );      /* t3 = (a/b)*b */
  MP_CHECKOK( mp_add(&t1, &t3, &t4) );     /* t4 = a%b + (a/b)*b */
  MP_CHECKOK( mp_sub(&t4, &a, &t5) );      /* t5 = a%b + (a/b)*b - a */
  if (mp_cmp_z(&t5) != 0) {
    res = MP_UNDEF;
    goto CLEANUP;
  }

CLEANUP:
  mp_clear(&t5);
  mp_clear(&t4);
  mp_clear(&t3);
  mp_clear(&t2);
  mp_clear(&t1);
  mp_clear(&b);
  mp_clear(&a);
  return res;
}

int
main(void)
{
  unsigned int  seed = (unsigned int)time(NULL);
  unsigned long count = 0;
  mp_err        res;

  srand(seed);

  while (MP_OKAY == (res = identity_test())) {
     if ((++count % 100) == 0)
       fputc('.', stderr);
  }

  fprintf(stderr, "\ntest failed, err %d\n", res);
  return res;
}
