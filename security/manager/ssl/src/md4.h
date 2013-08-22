/* vim:set ts=2 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef md4_h__
#define md4_h__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * md4sum - computes the MD4 sum over the input buffer per RFC 1320
 * 
 * @param input
 *        buffer containing input data
 * @param inputLen
 *        length of input buffer (number of bytes)
 * @param result
 *        16-byte buffer that will contain the MD4 sum upon return
 *
 * NOTE: MD4 is superceded by MD5.  do not use MD4 unless required by the
 * protocol you are implementing (e.g., NTLM requires MD4).
 *
 * NOTE: this interface is designed for relatively small buffers.  A streaming
 * interface would make more sense if that were a requirement.  Currently, this
 * is good enough for the applications we care about.
 */
void md4sum(const uint8_t *input, uint32_t inputLen, uint8_t *result);

#ifdef __cplusplus
}
#endif

#endif /* md4_h__ */
