/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ENCRYPT_H__
#define __ENCRYPT_H__

#include <limits.h>
#include <mprio.h>

/*******
 * These functions attempt to implement CCA-secure public-key encryption using
 * the NSS library. We use hashed-ElGamal encryption with Curve25519 as the
 * underlying group and AES128-GCM as the bulk encryption mode of operation.
 *
 * I make no guarantees that I am using NSS correctly or that this encryption
 * scheme is actually CCA secure. As far as I can tell, NSS does not provide
 * any public-key hybrid encryption scheme out of the box, so I had to cook my
 * own. If you want to be really safe, you should use the NaCl Box routines
 * to implement these functions.
 */

/*
 * Messages encrypted using this library must be smaller than MAX_ENCRYPT_LEN.
 * Enforcing this length limit helps avoid integer overflow.
 */
#define MAX_ENCRYPT_LEN (INT_MAX >> 3)

/*
 * Write the number of bytes needed to store a ciphertext that encrypts a
 * plaintext message of length `inputLen` and authenticated data of length
 * `adLen` into the variable pointed to by `outputLen`. If `inputLen`
 * is too large (larger than `MAX_ENCRYPT_LEN`), this function returns
 * an error.
 */
SECStatus PublicKey_encryptSize(unsigned int inputLen, unsigned int* outputLen);

/*
 * Generate a new keypair for public-key encryption.
 */
SECStatus Keypair_new(PrivateKey* pvtkey, PublicKey* pubkey);

/*
 * Encrypt an arbitrary bitstring to the specified public key. The buffer
 * `output` should be large enough to store the ciphertext. Use the
 * `PublicKey_encryptSize()` function above to figure out how large of a buffer
 * you need.
 *
 * The value `inputLen` must be smaller than `MAX_ENCRYPT_LEN`.
 */
SECStatus PublicKey_encrypt(PublicKey pubkey, unsigned char* output,
                            unsigned int* outputLen, unsigned int maxOutputLen,
                            const unsigned char* input, unsigned int inputLen);

/*
 * Decrypt an arbitrary bitstring using the specified private key.  The output
 * buffer should be at least 16 bytes larger than the plaintext you expect. If
 * `outputLen` >= `inputLen`, you should be safe.
 */
SECStatus PrivateKey_decrypt(PrivateKey privkey, unsigned char* output,
                             unsigned int* outputLen, unsigned int maxOutputLen,
                             const unsigned char* input, unsigned int inputLen);

#endif /* __ENCRYPT_H__ */
