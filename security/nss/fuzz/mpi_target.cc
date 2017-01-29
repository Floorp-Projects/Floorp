/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This target fuzzes NSS mpi against openssl bignum.
 * It therefore requires openssl to be installed.
 */

#include <algorithm>
#include <iostream>
#include <string>

#include "hasht.h"
#include "mpi.h"
#include "shared.h"

#include <openssl/bn.h>

#define CLEAR_NUMS \
  mp_zero(&c);     \
  BN_zero(C);      \
  mp_zero(&r);     \
  BN_zero(R);

// Check that the two numbers are equal.
void check_equal(BIGNUM *b, mp_int *m, size_t max_size) {
  char *bnBc = BN_bn2hex(b);
  char mpiMc[max_size];
  mp_tohex(m, mpiMc);
  std::string bnA(bnBc);
  std::string mpiA(mpiMc);
  OPENSSL_free(bnBc);
  // We have to strip leading zeros from bignums, ignoring the sign.
  if (bnA.at(0) != '-') {
    bnA.erase(0, std::min(bnA.find_first_not_of('0'), bnA.size() - 1));
  } else if (bnA.at(1) == '0') {
    bnA.erase(1, std::min(bnA.find_first_not_of('0', 1) - 1, bnA.size() - 1));
  }

  if (mpiA != bnA) {
    std::cout << "openssl: " << std::hex << bnA << std::endl;
    std::cout << "nss:     " << std::hex << mpiA << std::endl;
  }

  assert(mpiA == bnA);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // We require at least size 3 to get two integers from Data.
  if (size <= 3) {
    return 0;
  }
  size_t max_size = 2 * size + 1;

  mp_int a, b, c, r;
  BN_CTX *ctx = BN_CTX_new();
  BN_CTX_start(ctx);
  BIGNUM *A = BN_CTX_get(ctx);
  BIGNUM *B = BN_CTX_get(ctx);
  BIGNUM *C = BN_CTX_get(ctx);
  BIGNUM *R = BN_CTX_get(ctx);
  assert(mp_init(&a) == MP_OKAY);
  assert(mp_init(&b) == MP_OKAY);
  assert(mp_init(&c) == MP_OKAY);
  assert(mp_init(&r) == MP_OKAY);

  // Note that b might overlap a.
  size_t len = (size_t)size / 2;
  assert(mp_read_raw(
             &a, reinterpret_cast<char *>(const_cast<unsigned char *>(data)),
             len) == MP_OKAY);
  assert(mp_read_raw(
             &b,
             reinterpret_cast<char *>(const_cast<unsigned char *>(data)) + len,
             len) == MP_OKAY);
  // Force a positive sign.
  // TODO: add tests for negatives.
  MP_SIGN(&a) = MP_ZPOS;
  MP_SIGN(&b) = MP_ZPOS;

  // Skip the first byte as it's interpreted as sign by NSS.
  assert(BN_bin2bn(data + 1, len - 1, A) != nullptr);
  assert(BN_bin2bn(data + len + 1, len - 1, B) != nullptr);

  check_equal(A, &a, max_size);
  check_equal(B, &b, max_size);

  // Addition
  assert(mp_add(&a, &b, &c) == MP_OKAY);
  (void)BN_add(C, A, B);
  check_equal(C, &c, max_size);

  // Subtraction
  CLEAR_NUMS
  assert(mp_sub(&a, &b, &c) == MP_OKAY);
  (void)BN_sub(C, A, B);
  check_equal(C, &c, max_size);

  // Sqr
  CLEAR_NUMS
  assert(mp_sqr(&a, &c) == MP_OKAY);
  (void)BN_sqr(C, A, ctx);
  check_equal(C, &c, max_size);

  // We can't divide by 0.
  if (mp_cmp_z(&b) != 0) {
    CLEAR_NUMS
    assert(mp_div(&a, &b, &c, &r) == MP_OKAY);
    BN_div(C, R, A, B, ctx);
    check_equal(C, &c, max_size);
    check_equal(R, &r, max_size);

    // Modulo
    CLEAR_NUMS
    assert(mp_mod(&a, &b, &c) == MP_OKAY);
    (void)BN_mod(C, A, B, ctx);
    check_equal(C, &c, max_size);

    // Mod sqr
    CLEAR_NUMS
    assert(mp_sqrmod(&a, &b, &c) == MP_OKAY);
    (void)BN_mod_sqr(C, A, B, ctx);
    check_equal(C, &c, max_size);
  }

  // Mod add
  CLEAR_NUMS
  mp_add(&a, &b, &r);
  (void)BN_add(R, A, B);
  assert(mp_addmod(&a, &b, &r, &c) == MP_OKAY);
  (void)BN_mod_add(C, A, B, R, ctx);
  check_equal(C, &c, max_size);

  // Mod sub
  CLEAR_NUMS
  mp_add(&a, &b, &r);
  (void)BN_add(R, A, B);
  assert(mp_submod(&a, &b, &r, &c) == MP_OKAY);
  (void)BN_mod_sub(C, A, B, R, ctx);
  check_equal(C, &c, max_size);

  // Mod mul
  CLEAR_NUMS
  mp_add(&a, &b, &r);
  (void)BN_add(R, A, B);
  assert(mp_mulmod(&a, &b, &r, &c) == MP_OKAY);
  (void)BN_mod_mul(C, A, B, R, ctx);
  check_equal(C, &c, max_size);

  // Mod exp
  // NOTE: This must be the last test as we change b!
  CLEAR_NUMS
  mp_add(&a, &b, &r);
  mp_add_d(&r, 1, &r);  // NSS doesn't allow 0 as modulus here.
  size_t num = MP_USED(&b) * MP_DIGIT_BIT;
  mp_div_2d(&b, num, &b, nullptr);  // make the exponent smaller, larger
                                    // exponents need too much memory
  MP_USED(&b) = 1;
  (void)BN_add(R, A, B);
  BN_add_word(R, 1);
  BN_rshift(B, B, num);
  check_equal(B, &b, max_size);
  assert(mp_exptmod(&a, &b, &r, &c) == MP_OKAY);
  (void)BN_mod_exp(C, A, B, R, ctx);
  check_equal(C, &c, max_size);

  mp_clear(&a);
  mp_clear(&b);
  mp_clear(&c);
  mp_clear(&r);

  BN_CTX_end(ctx);
  BN_CTX_free(ctx);

  return 0;
}
