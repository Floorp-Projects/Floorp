/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Helper functions for MPI fuzzing targets. */

#ifndef mpi_helper_h__
#define mpi_helper_h__

#include <iostream>
#include <string>
#include <tuple>
#include <vector>

#include "hasht.h"
#include "mpi.h"

#include <openssl/bn.h>

void check_equal(BIGNUM *b, mp_int *m, size_t max_size);
void parse_input(const uint8_t *data, size_t size, BIGNUM *A, BIGNUM *B,
                 mp_int *a, mp_int *b);
void parse_input(const uint8_t *data, size_t size, BIGNUM *A, mp_int *a);
std::tuple<BIGNUM *, mp_int> get_modulus(const uint8_t *data, size_t size,
                                         BN_CTX *ctx);
void print_bn(std::string label, BIGNUM *x);

// Initialise MPI and BN variables
// XXX: Also silence unused variable warnings for R.
#define INIT_FOUR_NUMBERS                \
  mp_int a, b, c, r;                     \
  mp_int *m1 = nullptr;                  \
  BN_CTX *ctx = BN_CTX_new();            \
  BN_CTX_start(ctx);                     \
  BIGNUM *A = BN_CTX_get(ctx);           \
  BIGNUM *B = BN_CTX_get(ctx);           \
  BIGNUM *C = BN_CTX_get(ctx);           \
  BIGNUM *R = BN_CTX_get(ctx);           \
  assert(mp_init(&a) == MP_OKAY);        \
  assert(mp_init(&b) == MP_OKAY);        \
  assert(mp_init(&c) == MP_OKAY);        \
  assert(mp_init(&r) == MP_OKAY);        \
  size_t max_size = 2 * size + 1;        \
  parse_input(data, size, A, B, &a, &b); \
  do {                                   \
    (void)(R);                           \
  } while (0);

// Initialise MPI and BN variables
// XXX: Also silence unused variable warnings for B.
#define INIT_THREE_NUMBERS        \
  mp_int a, b, c;                 \
  BN_CTX *ctx = BN_CTX_new();     \
  BN_CTX_start(ctx);              \
  BIGNUM *A = BN_CTX_get(ctx);    \
  BIGNUM *B = BN_CTX_get(ctx);    \
  BIGNUM *C = BN_CTX_get(ctx);    \
  assert(mp_init(&a) == MP_OKAY); \
  assert(mp_init(&b) == MP_OKAY); \
  assert(mp_init(&c) == MP_OKAY); \
  size_t max_size = 4 * size + 1; \
  parse_input(data, size, A, &a); \
  do {                            \
    (void)(B);                    \
  } while (0);

#define CLEANUP_AND_RETURN \
  mp_clear(&a);            \
  mp_clear(&b);            \
  mp_clear(&c);            \
  mp_clear(&r);            \
  if (m1) {                \
    mp_clear(m1);          \
  }                        \
  BN_CTX_end(ctx);         \
  BN_CTX_free(ctx);        \
  return 0;

#define CLEANUP_AND_RETURN_THREE \
  mp_clear(&a);                  \
  mp_clear(&b);                  \
  mp_clear(&c);                  \
  BN_CTX_end(ctx);               \
  BN_CTX_free(ctx);              \
  return 0;

#endif  // mpi_helper_h__
