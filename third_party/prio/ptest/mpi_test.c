/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. 
 */

#include <mpi.h>

#include "mutest.h"


void 
mu_test_mpi__add (void) 
{
  mp_int a;
  mp_int b;
  mp_int c;

  mu_check (mp_init (&a) == MP_OKAY);
  mu_check (mp_init (&b) == MP_OKAY);
  mu_check (mp_init (&c) == MP_OKAY);

  mp_set (&a, 10);
  mp_set (&b, 7);
  mp_add (&a, &b, &c);

  mp_set (&a, 17);
  mu_check (mp_cmp (&a, &c) == 0);

  mp_clear (&a);
  mp_clear (&b);
  mp_clear (&c);
}



