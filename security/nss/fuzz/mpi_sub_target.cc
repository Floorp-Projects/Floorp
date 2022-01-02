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

  // Compare with OpenSSL subtraction
  assert(mp_sub(&a, &b, &c) == MP_OKAY);
  (void)BN_sub(C, A, B);
  check_equal(C, &c, max_size);

  // Check a - b == a + -b
  mp_neg(&b, &b);
  assert(mp_add(&a, &b, &r) == MP_OKAY);
  bool eq = mp_cmp(&r, &c) == 0;
  if (!eq) {
    char rC[max_size], cC[max_size], aC[max_size], bC[max_size];
    mp_tohex(&r, rC);
    mp_tohex(&c, cC);
    mp_tohex(&a, aC);
    mp_tohex(&b, bC);
    std::cout << "a = " << std::hex << aC << std::endl;
    std::cout << "-b = " << std::hex << bC << std::endl;
    std::cout << "a - b = " << std::hex << cC << std::endl;
    std::cout << "a + -b = " << std::hex << rC << std::endl;
  }
  assert(eq);

  CLEANUP_AND_RETURN
}
