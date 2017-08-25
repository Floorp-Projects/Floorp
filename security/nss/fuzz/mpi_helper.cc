/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Helper functions for MPI fuzzing targets. */

#include "mpi_helper.h"
#include <cstdlib>
#include <random>

char *to_char(const uint8_t *x) {
  return reinterpret_cast<char *>(const_cast<unsigned char *>(x));
}

void print_bn(std::string label, BIGNUM *x) {
  char *xc = BN_bn2hex(x);
  std::cout << label << ": " << std::hex << xc << std::endl;
  OPENSSL_free(xc);
}

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

// Parse data into two numbers for MPI and OpenSSL Bignum.
void parse_input(const uint8_t *data, size_t size, BIGNUM *A, BIGNUM *B,
                 mp_int *a, mp_int *b) {
  // Note that b might overlap a.
  size_t len = (size_t)size / 2;
  assert(mp_read_raw(a, to_char(data), len) == MP_OKAY);
  assert(mp_read_raw(b, to_char(data) + len, len) == MP_OKAY);
  // Force a positive sign.
  // TODO: add tests for negatives.
  MP_SIGN(a) = MP_ZPOS;
  MP_SIGN(b) = MP_ZPOS;

  // Skip the first byte as it's interpreted as sign by NSS.
  assert(BN_bin2bn(data + 1, len - 1, A) != nullptr);
  assert(BN_bin2bn(data + len + 1, len - 1, B) != nullptr);

  check_equal(A, a, 2 * size + 1);
  check_equal(B, b, 2 * size + 1);
}

// Parse data into a number for MPI and OpenSSL Bignum.
void parse_input(const uint8_t *data, size_t size, BIGNUM *A, mp_int *a) {
  assert(mp_read_raw(a, to_char(data), size) == MP_OKAY);

  // Force a positive sign.
  // TODO: add tests for negatives.
  MP_SIGN(a) = MP_ZPOS;

  // Skip the first byte as it's interpreted as sign by NSS.
  assert(BN_bin2bn(data + 1, size - 1, A) != nullptr);

  check_equal(A, a, 4 * size + 1);
}

// Take a chunk in the middle of data and use it as modulus.
std::tuple<BIGNUM *, mp_int> get_modulus(const uint8_t *data, size_t size,
                                         BN_CTX *ctx) {
  BIGNUM *r1 = BN_CTX_get(ctx);
  mp_int r2;
  assert(mp_init(&r2) == MP_OKAY);

  size_t len = static_cast<size_t>(size / 4);
  if (len != 0) {
    assert(mp_read_raw(&r2, to_char(data + len), len) == MP_OKAY);
    MP_SIGN(&r2) = MP_ZPOS;

    assert(BN_bin2bn(data + len + 1, len - 1, r1) != nullptr);
    check_equal(r1, &r2, 2 * len + 1);
  }

  // If we happen to get 0 for the modulus, take a random number.
  if (mp_cmp_z(&r2) == 0 || len == 0) {
    mp_zero(&r2);
    BN_zero(r1);
    std::mt19937 rng(data[0]);
    std::uniform_int_distribution<mp_digit> dist(1, MP_DIGIT_MAX);
    mp_digit x = dist(rng);
    mp_add_d(&r2, x, &r2);
    BN_add_word(r1, x);
  }

  return std::make_tuple(r1, r2);
}
