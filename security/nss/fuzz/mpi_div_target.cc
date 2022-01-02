/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This target fuzzes NSS mpi against openssl bignum.
 * It therefore requires openssl to be installed.
 */

#include "mpi_helper.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // We require at least size 3 to get two integers from Data.
  if (size < 3) {
    return 0;
  }
  INIT_FOUR_NUMBERS

  // We can't divide by 0.
  if (mp_cmp_z(&b) == 0) {
    CLEANUP_AND_RETURN
  }

  // Compare with OpenSSL division
  assert(mp_div(&a, &b, &c, &r) == MP_OKAY);
  BN_div(C, R, A, B, ctx);
  check_equal(C, &c, max_size);
  check_equal(R, &r, max_size);

  // Check c * b + r == a
  assert(mp_mul(&c, &b, &c) == MP_OKAY);
  assert(mp_add(&c, &r, &c) == MP_OKAY);
  assert(mp_cmp(&c, &a) == 0);

  CLEANUP_AND_RETURN
}
