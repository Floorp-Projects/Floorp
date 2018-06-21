/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. 
 */

#include <mpi.h>

#include "mutest.h"
#include "prio/rand.h"
#include "prio/util.h"

void 
mu_test__util_msb_mast (void)
{
  mu_check (msb_mask (0x01) == 0x01);
  mu_check (msb_mask (0x02) == 0x03);
  mu_check (msb_mask (0x0C) == 0x0F);
  mu_check (msb_mask (0x1C) == 0x1F);
  mu_check (msb_mask (0xFF) == 0xFF);
}

void 
test_rand_once (int limit)
{
  mp_int max;
  mp_int out;

  mu_check (mp_init (&max) == MP_OKAY);
  mu_check (mp_init (&out) == MP_OKAY);

  mp_set (&max, limit);

  mu_check (rand_int (&out, &max)  == MP_OKAY);
  mu_check (mp_cmp_d (&out, limit) == -1);
  mu_check (mp_cmp_z (&out) > -1);

  mp_clear (&max);
  mp_clear (&out);
}

void 
mu_test_rand__multiple_of_8 (void) 
{
  test_rand_once (256);
  test_rand_once (256*256);
}


void 
mu_test_rand__near_multiple_of_8 (void) 
{
  test_rand_once (256+1);
  test_rand_once (256*256+1);
}

void 
mu_test_rand__odd (void) 
{
  test_rand_once (39);
  test_rand_once (123);
  test_rand_once (993123);
}

void 
mu_test_rand__large (void) 
{
  test_rand_once (1231239933);
}

void 
mu_test_rand__bit(void) 
{
  test_rand_once (1);
  for (int i = 0; i < 100; i++)
    test_rand_once (2);
}

void 
test_rand_distribution (int limit) 
{
  SECStatus rv = SECSuccess;
  int bins[limit];

  mp_int max;
  mp_int out;

  MP_DIGITS (&max) = NULL;
  MP_DIGITS (&out) = NULL;

  MP_CHECKC (mp_init (&max));
  MP_CHECKC (mp_init (&out));

  mp_set (&max, limit);

  for (int i = 0; i < limit; i++) {
    bins[i] = 0;
  }

  for (int i = 0; i < limit*limit; i++) {
    mu_check (rand_int (&out, &max)  == MP_OKAY);
    mu_check (mp_cmp_d (&out, limit) == -1);
    mu_check (mp_cmp_z (&out) > -1);

    unsigned char ival[2] = {0x00, 0x00};
    MP_CHECKC (mp_to_fixlen_octets (&out, ival, 2));
    if (ival[1] + 256*ival[0] < limit) {
      bins[ival[1] + 256*ival[0]] += 1;
    } else {
      mu_check (false);
    }
  }

  for (int i = 0; i < limit; i++) {
    mu_check (bins[i] > limit/2);
  }

cleanup:
  mu_check (rv == SECSuccess);
  mp_clear (&max);
  mp_clear (&out);
}


void 
mu_test__rand_distribution123 (void) 
{
  test_rand_distribution(123);
}

void 
mu_test__rand_distribution257 (void) 
{
  test_rand_distribution(257);
}

void 
mu_test__rand_distribution259 (void) 
{
  test_rand_distribution(259);
}

void 
test_rand_distribution_large (mp_int *max) 
{
  SECStatus rv = SECSuccess;
  int limit = 16;
  int bins[limit];

  mp_int out;
  MP_DIGITS (&out) = NULL;
  MP_CHECKC (mp_init (&out));

  for (int i = 0; i < limit; i++) {
    bins[i] = 0;
  }

  for (int i = 0; i < 100*limit*limit; i++) {
    MP_CHECKC (rand_int (&out, max));
    mu_check (mp_cmp (&out, max) == -1);
    mu_check (mp_cmp_z (&out) > -1);

    unsigned long res; 
    MP_CHECKC (mp_mod_d (&out, limit, &res));
    bins[res] += 1;
  }

  for (int i = 0; i < limit; i++) {
    mu_check (bins[i] > limit/2);
  }

cleanup:
  mu_check (rv == SECSuccess);
  mp_clear (&out);
}

void 
mu_test__rand_distribution_large (void) 
{
  SECStatus rv = SECSuccess;
  mp_int max;
  MP_DIGITS (&max) = NULL;
  MP_CHECKC (mp_init (&max));

  char bytes[] = "FF1230985198451798EDC8123";
  MP_CHECKC (mp_read_radix (&max, bytes, 16));
  test_rand_distribution_large (&max);

cleanup:
  mu_check (rv == SECSuccess);
  mp_clear (&max);
}
