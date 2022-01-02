/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This target fuzzes NSS mpi against openssl bignum.
 * It therefore requires openssl to be installed.
 */

#include "mpi_helper.h"
#include "mpprime.h"

#include <algorithm>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // We require at least size 4 to get everything we need from data.
  if (size < 4) {
    return 0;
  }

  INIT_THREE_NUMBERS

  // Make a prime of length size.
  int count = 0;
  mp_err res = MP_NO;
  // mpp_make_prime is so slow :( use something smaller than size.
  int primeLen = std::max(static_cast<int>(size / 4), 3);
  uint8_t bp[primeLen];
  memcpy(bp, data, primeLen);
  do {
    bp[0] |= 0x80;            /* set high-order bit */
    bp[primeLen - 1] |= 0x01; /* set low-order bit  */
    ++count;
    assert(mp_read_unsigned_octets(&b, bp, primeLen) == MP_OKAY);
  } while ((res = mpp_make_prime(&b, primeLen * 8, PR_FALSE)) != MP_YES &&
           count < 10);
  if (res != MP_YES) {
    return 0;
  }

  // Use the same prime in OpenSSL B
  char tmp[max_size];
  mp_toradix(&b, tmp, 16);
  int tmpLen;
  assert((tmpLen = BN_hex2bn(&B, tmp)) != 0);

  // Compare with OpenSSL invmod
  res = mp_invmod(&a, &b, &c);
  BIGNUM *X = BN_mod_inverse(C, A, B, ctx);
  if (res != MP_OKAY) {
    // In case we couldn't compute the inverse, OpenSSL shouldn't be able to
    // either.
    assert(X == nullptr);
  } else {
    check_equal(C, &c, max_size);

    // Check a * c mod b == 1
    assert(mp_mulmod(&a, &c, &b, &c) == MP_OKAY);
    bool eq = mp_cmp_d(&c, 1) == 0;
    if (!eq) {
      char cC[max_size];
      mp_tohex(&c, cC);
      std::cout << "c = " << std::hex << cC << std::endl;
    }
    assert(eq);
  }

  CLEANUP_AND_RETURN_THREE
}
