/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __SHARE_H__
#define __SHARE_H__

#include <mpi.h>

#include "config.h"

struct beaver_triple
{
  mp_int a;
  mp_int b;
  mp_int c;
};

typedef struct beaver_triple* BeaverTriple;
typedef const struct beaver_triple* const_BeaverTriple;

/*
 * Use secret sharing to split the int src into two shares.
 * The mp_ints must be initialized.
 */
SECStatus share_int(const_PrioConfig cfg, const mp_int* src, mp_int* shareA,
                    mp_int* shareB);

/*
 * Prio uses Beaver triples to implement one step of the
 * client data validation routine. A Beaver triple is just
 * a sharing of random values a, b, c such that
 *    a * b = c
 */
BeaverTriple BeaverTriple_new(void);
void BeaverTriple_clear(BeaverTriple t);

SECStatus BeaverTriple_set_rand(const_PrioConfig cfg, BeaverTriple triple_a,
                                BeaverTriple triple_b);

bool BeaverTriple_areEqual(const_BeaverTriple t1, const_BeaverTriple t2);

#endif /* __SHARE_H__ */
