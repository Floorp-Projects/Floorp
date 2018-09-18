/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <limits.h>
#include <mprio.h>
#include <nss.h>
#include <pk11pub.h>
#include <prinit.h>

#include "debug.h"
#include "rand.h"
#include "util.h"

#define CHUNK_SIZE 8192

static NSSInitContext* prioGlobalContext = NULL;

SECStatus
rand_init(void)
{
  if (prioGlobalContext)
    return SECSuccess;

  prioGlobalContext =
    NSS_InitContext("", "", "", "", NULL,
                    NSS_INIT_READONLY | NSS_INIT_NOCERTDB | NSS_INIT_NOMODDB |
                      NSS_INIT_FORCEOPEN | NSS_INIT_NOROOTINIT);

  return (prioGlobalContext != NULL) ? SECSuccess : SECFailure;
}

static SECStatus
rand_bytes_internal(void* user_data, unsigned char* out, size_t n_bytes)
{
  // No pointer should ever be passed in.
  if (user_data != NULL)
    return SECFailure;
  if (!NSS_IsInitialized()) {
    PRIO_DEBUG("NSS not initialized. Call rand_init() first.");
    return SECFailure;
  }

  SECStatus rv = SECFailure;

  int to_go = n_bytes;
  unsigned char* cp = out;
  while (to_go) {
    int to_gen = MIN(CHUNK_SIZE, to_go);
    if ((rv = PK11_GenerateRandom(cp, to_gen)) != SECSuccess) {
      PRIO_DEBUG("Error calling PK11_GenerateRandom");
      return SECFailure;
    }

    cp += CHUNK_SIZE;
    to_go -= to_gen;
  }

  return rv;
}

SECStatus
rand_bytes(unsigned char* out, size_t n_bytes)
{
  return rand_bytes_internal(NULL, out, n_bytes);
}

SECStatus
rand_int(mp_int* out, const mp_int* max)
{
  return rand_int_rng(out, max, &rand_bytes_internal, NULL);
}

SECStatus
rand_int_rng(mp_int* out, const mp_int* max, RandBytesFunc rng_func,
             void* user_data)
{
  SECStatus rv = SECSuccess;

  // Ensure max value is > 0
  if (mp_cmp_z(max) == 0)
    return SECFailure;

  // Compute max-1, which tells us the largest
  // value we will ever need to generate.
  MP_CHECK(mp_sub_d(max, 1, out));

  const int nbytes = mp_unsigned_octet_size(out);

  // Figure out how many MSBs we need to get in the
  // most-significant byte.
  unsigned char max_bytes[nbytes];
  MP_CHECK(mp_to_fixlen_octets(out, max_bytes, nbytes));
  const unsigned char mask = msb_mask(max_bytes[0]);

  // Buffer to store the pseudo-random bytes
  unsigned char buf[nbytes];

  do {
    // Use  rejection sampling to find a value strictly less than max.
    P_CHECK(rng_func(user_data, buf, nbytes));

    // Mask off high-order bits that we will never need.
    P_CHECK(rng_func(user_data, &buf[0], 1));
    if (mask)
      buf[0] &= mask;

    MP_CHECK(mp_read_unsigned_octets(out, buf, nbytes));
  } while (mp_cmp(out, max) != -1);

  return 0;
}

void
rand_clear(void)
{
  if (prioGlobalContext) {
    NSS_ShutdownContext(prioGlobalContext);
#ifdef DO_PR_CLEANUP
    PR_Cleanup();
#endif
  }

  prioGlobalContext = NULL;
}
