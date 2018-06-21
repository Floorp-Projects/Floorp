/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. 
 */

#include <mpi.h>

#include "mutest.h"
#include "prio/prg.h"
#include "prio/util.h"

void
mu_test__prg_simple (void) 
{
  SECStatus rv = SECSuccess;
  PrioPRGSeed key;
  PRG prg = NULL;

  P_CHECKC (PrioPRGSeed_randomize (&key)); 
  P_CHECKA (prg = PRG_new (key)); 

cleanup:
  mu_check (rv == SECSuccess);
  PRG_clear (prg);
}

void
mu_test__prg_repeat (void) 
{
  SECStatus rv = SECSuccess;
  const int buflen = 10000;
  unsigned char buf1[buflen];
  unsigned char buf2[buflen];

  PrioPRGSeed key;
  PRG prg1 = NULL;
  PRG prg2 = NULL;

  buf1[3] = 'a';
  buf2[3] = 'b';

  P_CHECKC (PrioPRGSeed_randomize (&key)); 
  P_CHECKA (prg1 = PRG_new (key)); 
  P_CHECKA (prg2 = PRG_new (key)); 

  P_CHECKC (PRG_get_bytes (prg1, buf1, buflen));
  P_CHECKC (PRG_get_bytes (prg2, buf2, buflen));

  bool all_zero = true;
  for (int i=0; i<buflen; i++) {
    mu_check (buf1[i] == buf2[i]);
    if (buf1[i]) all_zero = false;
  }
  mu_check (!all_zero);

cleanup:
  mu_check (rv == SECSuccess);
  PRG_clear (prg1);
  PRG_clear (prg2);
}


void
mu_test__prg_repeat_int (void) 
{
  SECStatus rv = SECSuccess;
  const int tries = 10000;
  mp_int max;
  mp_int out1;
  mp_int out2;
  MP_DIGITS (&max) = NULL;
  MP_DIGITS (&out1) = NULL;
  MP_DIGITS (&out2) = NULL;

  PrioPRGSeed key;
  PRG prg1 = NULL;
  PRG prg2 = NULL;

  P_CHECKC (PrioPRGSeed_randomize (&key)); 
  P_CHECKA (prg1 = PRG_new (key)); 
  P_CHECKA (prg2 = PRG_new (key)); 

  MP_CHECKC (mp_init (&max)); 
  MP_CHECKC (mp_init (&out1)); 
  MP_CHECKC (mp_init (&out2)); 

  for (int i=0; i<tries; i++) { 
    mp_set (&max, i+1);
    P_CHECKC (PRG_get_int (prg1, &out1, &max));
    P_CHECKC (PRG_get_int (prg2, &out2, &max));
    mu_check (mp_cmp (&out1, &out2) == 0);
  }

cleanup:
  mu_check (rv == SECSuccess);
  PRG_clear (prg1);
  PRG_clear (prg2);
  mp_clear (&max);
  mp_clear (&out1);
  mp_clear (&out2);
}

void 
test_prg_once (int limit)
{
  SECStatus rv = SECSuccess;
  PrioPRGSeed key;
  mp_int max;
  mp_int out;
  PRG prg = NULL;

  MP_DIGITS (&max) = NULL;
  MP_DIGITS (&out) = NULL;

  P_CHECKC (PrioPRGSeed_randomize (&key)); 
  P_CHECKA (prg = PRG_new (key)); 

  MP_CHECKC (mp_init (&max)); 
  MP_CHECKC (mp_init (&out)); 

  mp_set (&max, limit);

  P_CHECKC (PRG_get_int (prg, &out, &max));
  mu_check (mp_cmp_d (&out, limit) == -1);
  mu_check (mp_cmp_z (&out) > -1);

cleanup:
  mu_check (rv == SECSuccess);
  mp_clear (&max);
  mp_clear (&out);
  PRG_clear (prg);
}

void 
mu_test_prg__multiple_of_8 (void) 
{
  test_prg_once (256);
  test_prg_once (256*256);
}


void 
mu_test_prg__near_multiple_of_8 (void) 
{
  test_prg_once (256+1);
  test_prg_once (256*256+1);
}

void 
mu_test_prg__odd (void) 
{
  test_prg_once (39);
  test_prg_once (123);
  test_prg_once (993123);
}

void 
mu_test_prg__large (void) 
{
  test_prg_once (1231239933);
}

void 
mu_test_prg__bit(void) 
{
  test_prg_once (1);
  for (int i = 0; i < 100; i++)
    test_prg_once (2);
}

void 
test_prg_distribution (int limit) 
{
  int bins[limit];
  SECStatus rv = SECSuccess;
  PrioPRGSeed key;
  mp_int max;
  mp_int out;
  PRG prg = NULL;

  MP_DIGITS (&max) = NULL;
  MP_DIGITS (&out) = NULL;

  P_CHECKC (PrioPRGSeed_randomize (&key)); 
  P_CHECKA (prg = PRG_new (key)); 

  MP_CHECKC (mp_init (&max)); 
  MP_CHECKC (mp_init (&out)); 

  mp_set (&max, limit);

  for (int i = 0; i < limit; i++) {
    bins[i] = 0;
  }

  for (int i = 0; i < limit*limit; i++) {
    P_CHECKC (PRG_get_int (prg, &out, &max));
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
  PRG_clear (prg);
}


void 
mu_test__prg_distribution123 (void) 
{
  test_prg_distribution(123);
}

void 
mu_test__prg_distribution257 (void) 
{
  test_prg_distribution(257);
}

void 
mu_test__prg_distribution259 (void) 
{
  test_prg_distribution(259);
}

void 
test_prg_distribution_large (mp_int *max) 
{
  const int limit = 16;
  int bins[limit];
  SECStatus rv = SECSuccess;
  PrioPRGSeed key;
  mp_int out;
  PRG prg = NULL;

  MP_DIGITS (&out) = NULL;

  P_CHECKC (PrioPRGSeed_randomize (&key)); 
  P_CHECKA (prg = PRG_new (key)); 

  MP_CHECKC (mp_init (&out)); 

  for (int i = 0; i < limit; i++) {
    bins[i] = 0;
  }

  for (int i = 0; i < 100*limit*limit; i++) {
    MP_CHECKC (PRG_get_int (prg, &out, max));
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
  PRG_clear (prg);
}

void 
mu_test__prg_distribution_large (void) 
{
  SECStatus rv = SECSuccess;
  mp_int max;
  MP_DIGITS (&max) = NULL;
  MP_CHECKC (mp_init (&max));

  char bytes[] = "FF1230985198451798EDC8123";
  MP_CHECKC (mp_read_radix (&max, bytes, 16));
  test_prg_distribution_large (&max);

cleanup:
  mu_check (rv == SECSuccess);
  mp_clear (&max);
}


void
mu_test__prg_share_arr (void)
{
  SECStatus rv = SECSuccess;
  PrioConfig cfg = NULL;
  MPArray arr = NULL;
  MPArray arr_share = NULL;
  PRG prg = NULL;
  PrioPRGSeed seed;

  P_CHECKA (cfg = PrioConfig_newTest (72));
  P_CHECKC (PrioPRGSeed_randomize (&seed));
  P_CHECKA (arr = MPArray_new (10));
  P_CHECKA (arr_share = MPArray_new (10));
  P_CHECKA (prg = PRG_new (seed));

  for (int i=0; i<10; i++) {
    mp_set (&arr->data[i], i);
  }

  P_CHECKC (PRG_share_array (prg, arr_share, arr, cfg));

  // Reset PRG 
  PRG_clear (prg);
  P_CHECKA (prg = PRG_new (seed));

  // Read pseudorandom values into arr
  P_CHECKC (PRG_get_array (prg, arr, &cfg->modulus));


  for (int i=0; i<10; i++) {
    MP_CHECKC (mp_addmod (&arr->data[i], &arr_share->data[i], 
          &cfg->modulus, &arr->data[i]));
    mu_check (mp_cmp_d (&arr->data[i], i) == 0);
  }

cleanup:
  mu_check (rv == SECSuccess);

  PRG_clear (prg);
  MPArray_clear (arr);
  MPArray_clear (arr_share);
  PrioConfig_clear (cfg);
}
