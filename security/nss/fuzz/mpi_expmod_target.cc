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

  auto modulus = get_modulus(data, size, ctx);
  // Compare with OpenSSL exp mod
  m1 = &std::get<1>(modulus);
  // The exponent b (B) can get really big. Make it smaller if necessary.
  if (MP_USED(&b) > 100) {
    size_t shift = (MP_USED(&b) - 100) * MP_DIGIT_BIT;
    mp_div_2d(&b, shift, &b, nullptr);
    BN_rshift(B, B, shift);
  }
  check_equal(A, &a, max_size);
  check_equal(B, &b, max_size);
  check_equal(std::get<0>(modulus), m1, 3 * max_size);
  assert(mp_exptmod(&a, &b, m1, &c) == MP_OKAY);
  (void)BN_mod_exp(C, A, B, std::get<0>(modulus), ctx);
  check_equal(C, &c, 2 * max_size);

  CLEANUP_AND_RETURN
}
