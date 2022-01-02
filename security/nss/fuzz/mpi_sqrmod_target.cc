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

  INIT_THREE_NUMBERS

  // We can't divide by 0.
  if (mp_cmp_z(&b) == 0) {
    mp_clear(&a);
    mp_clear(&b);
    mp_clear(&c);
    BN_CTX_end(ctx);
    BN_CTX_free(ctx);
    return 0;
  }

  // Compare with OpenSSL square mod
  assert(mp_sqrmod(&a, &b, &c) == MP_OKAY);
  (void)BN_mod_sqr(C, A, B, ctx);
  check_equal(C, &c, max_size);

  CLEANUP_AND_RETURN_THREE
}
