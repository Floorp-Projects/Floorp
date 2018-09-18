/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <mprio.h>

#include "rand.h"
#include "share.h"
#include "util.h"

SECStatus
share_int(const struct prio_config* cfg, const mp_int* src, mp_int* shareA,
          mp_int* shareB)
{
  SECStatus rv;
  P_CHECK(rand_int(shareA, &cfg->modulus));
  MP_CHECK(mp_submod(src, shareA, &cfg->modulus, shareB));

  return rv;
}

BeaverTriple
BeaverTriple_new(void)
{
  BeaverTriple triple = malloc(sizeof *triple);
  if (!triple)
    return NULL;

  MP_DIGITS(&triple->a) = NULL;
  MP_DIGITS(&triple->b) = NULL;
  MP_DIGITS(&triple->c) = NULL;

  SECStatus rv = SECSuccess;
  MP_CHECKC(mp_init(&triple->a));
  MP_CHECKC(mp_init(&triple->b));
  MP_CHECKC(mp_init(&triple->c));

cleanup:
  if (rv != SECSuccess) {
    BeaverTriple_clear(triple);
    return NULL;
  }
  return triple;
}

void
BeaverTriple_clear(BeaverTriple triple)
{
  if (!triple)
    return;
  mp_clear(&triple->a);
  mp_clear(&triple->b);
  mp_clear(&triple->c);
  free(triple);
}

SECStatus
BeaverTriple_set_rand(const struct prio_config* cfg,
                      struct beaver_triple* triple_1,
                      struct beaver_triple* triple_2)
{
  SECStatus rv = SECSuccess;

  // TODO: Can shorten this code using share_int()

  // We need that
  //   (a1 + a2)(b1 + b2) = c1 + c2   (mod p)
  P_CHECK(rand_int(&triple_1->a, &cfg->modulus));
  P_CHECK(rand_int(&triple_1->b, &cfg->modulus));
  P_CHECK(rand_int(&triple_2->a, &cfg->modulus));
  P_CHECK(rand_int(&triple_2->b, &cfg->modulus));

  // We are trying to be a little clever here to avoid the use of temp
  // variables.

  // c1 = a1 + a2
  MP_CHECK(mp_addmod(&triple_1->a, &triple_2->a, &cfg->modulus, &triple_1->c));

  // c2 = b1 + b2
  MP_CHECK(mp_addmod(&triple_1->b, &triple_2->b, &cfg->modulus, &triple_2->c));

  // c1 = c1 * c2 = (a1 + a2) (b1 + b2)
  MP_CHECK(mp_mulmod(&triple_1->c, &triple_2->c, &cfg->modulus, &triple_1->c));

  // Set c2 to random blinding value
  MP_CHECK(rand_int(&triple_2->c, &cfg->modulus));

  // c1 = c1 - c2
  MP_CHECK(mp_submod(&triple_1->c, &triple_2->c, &cfg->modulus, &triple_1->c));

  // Now we should have random tuples satisfying:
  //   (a1 + a2) (b1 + b2) = c1 + c2

  return rv;
}

bool
BeaverTriple_areEqual(const_BeaverTriple t1, const_BeaverTriple t2)
{
  return (mp_cmp(&t1->a, &t2->a) == 0 && mp_cmp(&t1->b, &t2->b) == 0 &&
          mp_cmp(&t1->c, &t2->c) == 0);
}
