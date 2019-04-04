/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <mpi.h>

#include "mparray.h"

struct prio_config
{
  int num_data_fields;
  unsigned char* batch_id;
  unsigned int batch_id_len;

  PublicKey server_a_pub;
  PublicKey server_b_pub;

  mp_int modulus;
  mp_int inv2;

  int n_roots;
  mp_int generator;
};

int PrioConfig_hPoints(const_PrioConfig cfg);

#endif /* __CONFIG_H__ */
