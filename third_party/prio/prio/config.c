/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <mprio.h>
#include <stdlib.h>

#include "config.h"
#include "mparray.h"
#include "params.h"
#include "rand.h"
#include "util.h"

// The PrioConfig object stores "2^k-th roots of unity" modulo
// the prime modulus we use for all arithmetic. We use
// these roots to perform fast FFT-style polynomial
// interpolation and evaluation.
//
// In particular, we use a prime modulus p such that
//    p = (2^k)q + 1.
// The roots are integers such that r^{2^k} = 1 mod p.
static SECStatus
initialize_roots(MPArray arr, const char values[], bool inverted)
{
  // TODO: Read in only the number of roots of unity we need.
  // Right now we read in all 4096 roots whether or not we use
  // them all.
  MP_CHECK(mp_read_radix(&arr->data[0], &values[0], 16));
  unsigned int len = arr->len;
  unsigned int n_chars = len * RootWidth;

  if (n_chars != sizeof(Roots)) {
    return SECFailure;
  }

  if (inverted) {
    for (unsigned int i = n_chars - RootWidth, j = 1; i > 0;
         i -= RootWidth, j++) {
      MP_CHECK(mp_read_radix(&arr->data[j], &values[i], 16));
    }
  } else {
    for (unsigned int i = RootWidth, j = 1; i < n_chars; i += RootWidth, j++) {
      MP_CHECK(mp_read_radix(&arr->data[j], &values[i], 16));
    }
  }

  return SECSuccess;
}

int
PrioConfig_maxDataFields(void)
{
  const int n_roots = 1 << Generator2Order;
  return (n_roots >> 1) - 1;
}

PrioConfig
PrioConfig_new(int n_fields, PublicKey server_a, PublicKey server_b,
               const unsigned char* batch_id, unsigned int batch_id_len)
{
  SECStatus rv = SECSuccess;
  PrioConfig cfg = malloc(sizeof(*cfg));
  if (!cfg)
    return NULL;

  cfg->batch_id = NULL;
  cfg->batch_id_len = batch_id_len;
  cfg->server_a_pub = server_a;
  cfg->server_b_pub = server_b;
  cfg->num_data_fields = n_fields;
  cfg->n_roots = 1 << Generator2Order;
  MP_DIGITS(&cfg->modulus) = NULL;
  MP_DIGITS(&cfg->inv2) = NULL;
  cfg->roots = NULL;
  cfg->rootsInv = NULL;

  P_CHECKCB(cfg->n_roots > 1);
  P_CHECKCB(cfg->num_data_fields <= PrioConfig_maxDataFields());

  P_CHECKA(cfg->batch_id = malloc(batch_id_len));
  strncpy((char*)cfg->batch_id, (char*)batch_id, batch_id_len);

  MP_CHECKC(mp_init(&cfg->modulus));
  MP_CHECKC(mp_read_radix(&cfg->modulus, Modulus, 16));

  // Compute  2^{-1} modulo M
  MP_CHECKC(mp_init(&cfg->inv2));
  mp_set(&cfg->inv2, 2);
  MP_CHECKC(mp_invmod(&cfg->inv2, &cfg->modulus, &cfg->inv2));

  P_CHECKA(cfg->roots = MPArray_new(cfg->n_roots));
  P_CHECKA(cfg->rootsInv = MPArray_new(cfg->n_roots));
  MP_CHECKC(initialize_roots(cfg->roots, Roots, /*inverted=*/false));
  MP_CHECKC(initialize_roots(cfg->rootsInv, Roots, /*inverted=*/true));

cleanup:
  if (rv != SECSuccess) {
    PrioConfig_clear(cfg);
    return NULL;
  }

  return cfg;
}

PrioConfig
PrioConfig_newTest(int nFields)
{
  return PrioConfig_new(nFields, NULL, NULL, (unsigned char*)"testBatch", 9);
}

void
PrioConfig_clear(PrioConfig cfg)
{
  if (!cfg)
    return;
  if (cfg->batch_id)
    free(cfg->batch_id);
  MPArray_clear(cfg->roots);
  MPArray_clear(cfg->rootsInv);
  mp_clear(&cfg->modulus);
  mp_clear(&cfg->inv2);
  free(cfg);
}

int
PrioConfig_numDataFields(const_PrioConfig cfg)
{
  return cfg->num_data_fields;
}

SECStatus
Prio_init(void)
{
  return rand_init();
}

void
Prio_clear(void)
{
  rand_clear();
}

int
PrioConfig_hPoints(const_PrioConfig cfg)
{
  const int mul_gates = cfg->num_data_fields + 1;
  const int N = next_power_of_two(mul_gates);
  return N;
}
