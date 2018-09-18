/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __UTIL_H__
#define __UTIL_H__

#include <mpi.h>
#include <mprio.h>

// Minimum of two values
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// Check a Prio error code and return failure if the call fails.
#define P_CHECK(s)                                                             \
  do {                                                                         \
    if ((rv = (s)) != SECSuccess)                                              \
      return rv;                                                               \
  } while (0);

// Check an allocation that should not return NULL. If the allocation returns
// NULL, set the return value and jump to the cleanup label to free memory.
#define P_CHECKA(s)                                                            \
  do {                                                                         \
    if ((s) == NULL) {                                                         \
      rv = SECFailure;                                                         \
      goto cleanup;                                                            \
    }                                                                          \
  } while (0);

// Check a Prio library call that should return SECSuccess. If it doesn't,
// jump to the cleanup label.
#define P_CHECKC(s)                                                            \
  do {                                                                         \
    if ((rv = (s)) != SECSuccess) {                                            \
      goto cleanup;                                                            \
    }                                                                          \
  } while (0);

// Check a boolean that should be true. If it not,
// jump to the cleanup label.
#define P_CHECKCB(s)                                                           \
  do {                                                                         \
    if (!(s)) {                                                                \
      rv = SECFailure;                                                         \
      goto cleanup;                                                            \
    }                                                                          \
  } while (0);

// Check an MPI library call and return failure if it fails.
#define MP_CHECK(s)                                                            \
  do {                                                                         \
    if ((s) != MP_OKAY)                                                        \
      return SECFailure;                                                       \
  } while (0);

// Check a msgpack object unpacked correctly
#define UP_CHECK(s)                                                            \
  do {                                                                         \
    int r = (s);                                                               \
    if (r != MSGPACK_UNPACK_SUCCESS && r != MSGPACK_UNPACK_EXTRA_BYTES)        \
      return SECFailure;                                                       \
  } while (0);

// Check an MPI library call. If it fails, set the return code and jump
// to the cleanup label.
#define MP_CHECKC(s)                                                           \
  do {                                                                         \
    if ((s) != MP_OKAY) {                                                      \
      rv = SECFailure;                                                         \
      goto cleanup;                                                            \
    }                                                                          \
  } while (0);

static inline int
next_power_of_two(int val)
{
  int i = val;
  int out = 0;
  for (; i > 0; i >>= 1) {
    out++;
  }

  int pow = 1 << out;
  return (pow > 1 && pow / 2 == val) ? val : pow;
}

/*
 * Return a mask that masks out all of the zero bits
 */
static inline unsigned char
msb_mask(unsigned char val)
{
  unsigned char mask;
  for (mask = 0x00; (val & mask) != val; mask = (mask << 1) + 1)
    ;
  return mask;
}

/*
 * Specify that a parameter should be unused.
 */
#define UNUSED(x) (void)(x)

#endif /* __UTIL_H__ */
