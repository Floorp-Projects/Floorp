/*
 * sha1.h
 *
 * interface to the Secure Hash Algorithm v.1 (SHA-1), specified in
 * FIPS 180-1
 *
 * David A. McGrew
 * Cisco Systems, Inc.
 */

/*
 *	
 * Copyright (c) 2001-2006, Cisco Systems, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 * 
 *   Neither the name of the Cisco Systems, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef SHA1_H
#define SHA1_H

#include "err.h"
#include "datatypes.h"

typedef struct {
  uint32_t H[5];             /* state vector                    */
  uint32_t M[16];            /* message buffer                  */
  int octets_in_buffer;      /* octets of message in buffer     */
  uint32_t num_bits_in_msg;  /* total number of bits in message */
} sha1_ctx_t;

/*
 * sha1(&ctx, msg, len, output) hashes the len octets starting at msg
 * into the SHA1 context, then writes the result to the 20 octets at
 * output
 * 
 */

void
sha1(const uint8_t *message,  int octets_in_msg, uint32_t output[5]);

/*
 * sha1_init(&ctx) initializes the SHA1 context ctx
 * 
 * sha1_update(&ctx, msg, len) hashes the len octets starting at msg
 * into the SHA1 context
 * 
 * sha1_final(&ctx, output) performs the final processing of the SHA1
 * context and writes the result to the 20 octets at output
 *
 */

void
sha1_init(sha1_ctx_t *ctx);

void
sha1_update(sha1_ctx_t *ctx, const uint8_t *M, int octets_in_msg);

void
sha1_final(sha1_ctx_t *ctx, uint32_t output[5]);

/*
 * The sha1_core function is INTERNAL to SHA-1, but it is declared
 * here because it is also used by the cipher SEAL 3.0 in its key
 * setup algorithm.  
 */

/*
 *  sha1_core(M, H) computes the core sha1 compression function, where M is
 *  the next part of the message and H is the intermediate state {H0,
 *  H1, ...}
 *
 *  this function does not do any of the padding required in the
 *  complete sha1 function
 */

void
sha1_core(const uint32_t M[16], uint32_t hash_value[5]);
     
#endif /* SHA1_H */
