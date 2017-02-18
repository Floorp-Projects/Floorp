/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This target fuzzes NSS mpi against openssl bignum.
 * It therefore requires openssl to be installed.
 */

#include "mpi_helper.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // We require at least size 2 to get an integers from Data.
  if (size < 2) {
    return 0;
  }
  mp_int a, c, r;
  BN_CTX *ctx = BN_CTX_new();
  BN_CTX_start(ctx);
  BIGNUM *A = BN_CTX_get(ctx);
  BIGNUM *C = BN_CTX_get(ctx);
  assert(mp_init(&a) == MP_OKAY);
  assert(mp_init(&c) == MP_OKAY);
  assert(mp_init(&r) == MP_OKAY);
  size_t max_size = 4 * size + 1;
  parse_input(data, size, A, &a);

  // Compare with OpenSSL sqr
  assert(mp_sqr(&a, &c) == MP_OKAY);
  (void)BN_sqr(C, A, ctx);
  check_equal(C, &c, max_size);

  // Check a * a == a**2
  assert(mp_mul(&a, &a, &r) == MP_OKAY);
  bool eq = mp_cmp(&r, &c) == 0;
  if (!eq) {
    char rC[max_size], cC[max_size], aC[max_size];
    mp_tohex(&r, rC);
    mp_tohex(&c, cC);
    mp_tohex(&a, aC);
    std::cout << "a = " << std::hex << aC << std::endl;
    std::cout << "a * a = " << std::hex << cC << std::endl;
    std::cout << "a ** 2 = " << std::hex << rC << std::endl;
  }
  assert(eq);
  mp_clear(&a);
  mp_clear(&c);
  mp_clear(&r);
  BN_CTX_end(ctx);
  BN_CTX_free(ctx);

  return 0;
}
