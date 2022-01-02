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

  // Compare with OpenSSL mod
  assert(mp_mod(&a, &b, &c) == MP_OKAY);
  (void)BN_mod(C, A, B, ctx);
  check_equal(C, &c, max_size);

  // Check a mod b = a - floor(a / b) * b
  assert(mp_div(&a, &b, &r, nullptr) == MP_OKAY);
  assert(mp_mul(&r, &b, &r) == MP_OKAY);
  assert(mp_sub(&a, &r, &r) == MP_OKAY);
  assert(mp_cmp(&c, &r) == 0);

  CLEANUP_AND_RETURN
}
