/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. 
 */

#include <mpi.h>
#include <mprio.h>

#include "mutest.h"
#include "prio/client.h"
#include "prio/server.h"
#include "prio/server.c"

void mu_test__eval_poly (void) 
{
  SECStatus rv = SECSuccess;
  PrioConfig cfg = NULL;
  MPArray coeffs = NULL;
  mp_int eval_at, out;

  MP_DIGITS (&eval_at) = NULL;
  MP_DIGITS (&out) = NULL;

  P_CHECKA (cfg = PrioConfig_newTest (54));
  P_CHECKA (coeffs = MPArray_new (3));

  mp_set (&coeffs->data[0], 2);
  mp_set (&coeffs->data[1], 8);
  mp_set (&coeffs->data[2], 3);

  MP_CHECKC (mp_init (&eval_at));
  MP_CHECKC (mp_init (&out));
  mp_set (&eval_at, 7);

  const int val = 3*7*7 + 8*7 + 2;
  mu_check (poly_eval (&out, coeffs, &eval_at, cfg) == SECSuccess);
  mu_check (mp_cmp_d (&out, val) == 0);

cleanup:
  mu_check (rv == SECSuccess);
  mp_clear (&out);
  mp_clear (&eval_at);
  MPArray_clear (coeffs);
  PrioConfig_clear (cfg);
}

void 
mu_test__verify_new (void)
{
  SECStatus rv = SECSuccess;
  PublicKey pkA = NULL;
  PublicKey pkB = NULL;
  PrivateKey skA = NULL;
  PrivateKey skB = NULL;
  PrioConfig cfg = NULL;
  PrioServer sA = NULL;
  PrioServer sB = NULL;
  PrioVerifier vA = NULL;
  PrioVerifier vB = NULL;
  unsigned char *for_a = NULL;
  unsigned char *for_b = NULL;

  mp_int fR, gR, hR;
  MP_DIGITS (&fR) = NULL;
  MP_DIGITS (&gR) = NULL;
  MP_DIGITS (&hR) = NULL;

  PrioPRGSeed seed;
  P_CHECKC (PrioPRGSeed_randomize (&seed));

  P_CHECKC (Keypair_new (&skA, &pkA));
  P_CHECKC (Keypair_new (&skB, &pkB));
  P_CHECKA (cfg = PrioConfig_new (214, pkA, pkB, 
        (unsigned char *)"testbatch", 9));

  const int ndata = PrioConfig_numDataFields (cfg);
  {
  bool data_items[ndata];
  for (int i=0; i < ndata; i++) {
    // Arbitrary data
    data_items[i] = (i % 3 == 1) || (i % 5 == 3);
  }

  P_CHECKA (sA = PrioServer_new (cfg, 0, skA, seed));
  P_CHECKA (sB = PrioServer_new (cfg, 1, skB, seed));

  unsigned int aLen, bLen;
  P_CHECKC (PrioClient_encode (cfg, data_items, &for_a, &aLen, &for_b, &bLen));

  MP_CHECKC (mp_init (&fR));
  MP_CHECKC (mp_init (&gR));
  MP_CHECKC (mp_init (&hR));

  P_CHECKA (vA = PrioVerifier_new (sA));
  P_CHECKA (vB = PrioVerifier_new (sB));
  P_CHECKC (PrioVerifier_set_data (vA, for_a, aLen));
  P_CHECKC (PrioVerifier_set_data (vB, for_b, bLen));

  PrioPacketClient pA = vA->clientp;
  PrioPacketClient pB = vB->clientp;
  MP_CHECKC (mp_addmod (&pA->f0_share, &pB->f0_share, &cfg->modulus, &fR));
  MP_CHECKC (mp_addmod (&pA->g0_share, &pB->g0_share, &cfg->modulus, &gR));
  MP_CHECKC (mp_addmod (&pA->h0_share, &pB->h0_share, &cfg->modulus, &hR));

  MP_CHECKC (mp_mulmod (&fR, &gR, &cfg->modulus, &fR));
  mu_check (mp_cmp (&fR, &hR) == 0);


  MP_CHECKC (mp_addmod (&vA->share_fR, &vB->share_fR, &cfg->modulus, &fR));
  MP_CHECKC (mp_addmod (&vA->share_gR, &vB->share_gR, &cfg->modulus, &gR));
  MP_CHECKC (mp_addmod (&vA->share_hR, &vB->share_hR, &cfg->modulus, &hR));

  MP_CHECKC (mp_mulmod (&fR, &gR, &cfg->modulus, &fR));

  //puts ("fR");
  //mp_print (&fR, stdout);
  //puts ("hR");
  //mp_print (&hR, stdout);
  mu_check (mp_cmp (&fR, &hR) == 0);
  }

cleanup:
  mu_check (rv == SECSuccess);

  if (for_a) free (for_a);
  if (for_b) free (for_b);

  mp_clear (&fR);
  mp_clear (&gR);
  mp_clear (&hR);

  PrioVerifier_clear (vA);
  PrioVerifier_clear (vB);

  PrioServer_clear (sA);
  PrioServer_clear (sB);
  PrioConfig_clear (cfg);

  PublicKey_clear (pkA);
  PublicKey_clear (pkB);
  PrivateKey_clear (skA);
  PrivateKey_clear (skB);

}

void
verify_full (int tweak)
{
  SECStatus rv = SECSuccess;
  PublicKey pkA = NULL;
  PublicKey pkB = NULL;
  PrivateKey skA = NULL;
  PrivateKey skB = NULL;
  PrioConfig cfg = NULL;
  PrioServer sA = NULL;
  PrioServer sB = NULL;
  PrioVerifier vA = NULL;
  PrioVerifier vB = NULL;
  PrioPacketVerify1 p1A = NULL;
  PrioPacketVerify1 p1B = NULL;
  PrioPacketVerify2 p2A = NULL;
  PrioPacketVerify2 p2B = NULL;
  unsigned char *for_a = NULL;
  unsigned char *for_b = NULL;

  mp_int fR, gR, hR;
  MP_DIGITS (&fR) = NULL;
  MP_DIGITS (&gR) = NULL;
  MP_DIGITS (&hR) = NULL;

  PrioPRGSeed seed;
  P_CHECKC (PrioPRGSeed_randomize (&seed));

  P_CHECKC (Keypair_new (&skA, &pkA));
  P_CHECKC (Keypair_new (&skB, &pkB));
  P_CHECKA (cfg = PrioConfig_new (47, pkA, pkB, (unsigned char *)"test4", 5));

  const int ndata = PrioConfig_numDataFields (cfg);
  {
  bool data_items[ndata];
  for (int i=0; i < ndata; i++) {
    // Arbitrary data
    data_items[i] = (i % 3 == 1) || (i % 5 == 3);
  }

  P_CHECKA (sA = PrioServer_new (cfg, 0, skA, seed));
  P_CHECKA (sB = PrioServer_new (cfg, 1, skB, seed));

  unsigned int aLen, bLen;
  P_CHECKC (PrioClient_encode (cfg, data_items, &for_a, &aLen, &for_b, &bLen));

  if (tweak == 5) {
    for_a[3] = 3;
    for_a[4] = 4;
  }

  P_CHECKA (vA = PrioVerifier_new (sA));
  P_CHECKA (vB = PrioVerifier_new (sB));
  P_CHECKC (PrioVerifier_set_data (vA, for_a, aLen));
  P_CHECKC (PrioVerifier_set_data (vB, for_b, bLen));

  if (tweak == 3) {
    mp_add_d (&vA->share_fR, 1, &vA->share_fR);
  }

  if (tweak == 4) {
    mp_add_d (&vB->share_gR, 1, &vB->share_gR);
  }

  P_CHECKA (p1A = PrioPacketVerify1_new ());
  P_CHECKA (p1B = PrioPacketVerify1_new ());

  P_CHECKC (PrioPacketVerify1_set_data (p1A, vA));
  P_CHECKC (PrioPacketVerify1_set_data (p1B, vB));

  if (tweak == 1) {
    mp_add_d (&p1B->share_d, 1, &p1B->share_d);
  }

  P_CHECKA (p2A = PrioPacketVerify2_new ());
  P_CHECKA (p2B = PrioPacketVerify2_new ());
  P_CHECKC (PrioPacketVerify2_set_data (p2A, vA, p1A, p1B));
  P_CHECKC (PrioPacketVerify2_set_data (p2B, vB, p1A, p1B));

  if (tweak == 2) {
    mp_add_d (&p2A->share_out, 1, &p2B->share_out);
  }

  int shouldBe = tweak ? SECFailure : SECSuccess;
  mu_check (PrioVerifier_isValid (vA, p2A, p2B) == shouldBe);
  mu_check (PrioVerifier_isValid (vB, p2A, p2B) == shouldBe);
  }

cleanup:
  if (!tweak) {
    mu_check (rv == SECSuccess);
  }

  if (for_a) free (for_a);
  if (for_b) free (for_b);

  PrioPacketVerify2_clear (p2A);
  PrioPacketVerify2_clear (p2B);

  PrioPacketVerify1_clear (p1A);
  PrioPacketVerify1_clear (p1B);

  PrioVerifier_clear (vA);
  PrioVerifier_clear (vB);

  PrioServer_clear (sA);
  PrioServer_clear (sB);
  PrioConfig_clear (cfg);

  PublicKey_clear (pkA);
  PublicKey_clear (pkB);
  PrivateKey_clear (skA);
  PrivateKey_clear (skB);
}

void 
mu_test__verify_full_good (void)
{
  verify_full (0);
}

void 
mu_test__verify_full_bad1 (void)
{
  verify_full (1);
}

void 
mu_test__verify_full_bad2 (void)
{
  verify_full (2);
}

void 
mu_test__verify_full_bad3 (void)
{
  verify_full (3);
}

void 
mu_test__verify_full_bad4 (void)
{
  verify_full (4);
}

void 
mu_test__verify_full_bad5 (void)
{
  verify_full (5);
}
