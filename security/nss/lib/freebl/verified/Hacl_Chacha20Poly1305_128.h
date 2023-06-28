/* MIT License
 *
 * Copyright (c) 2016-2022 INRIA, CMU and Microsoft Corporation
 * Copyright (c) 2022-2023 HACL* Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __Hacl_Chacha20Poly1305_128_H
#define __Hacl_Chacha20Poly1305_128_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <string.h>
#include "krml/internal/types.h"
#include "krml/lowstar_endianness.h"
#include "krml/internal/target.h"

#include "Hacl_Poly1305_128.h"
#include "Hacl_Chacha20_Vec128.h"

/**
Encrypt a message `m` with key `k`.

The arguments `k`, `n`, `aadlen`, and `aad` are same in encryption/decryption.
Note: Encryption and decryption can be executed in-place, i.e., `m` and `cipher` can point to the same memory.

@param k Pointer to 32 bytes of memory where the AEAD key is read from.
@param n Pointer to 12 bytes of memory where the AEAD nonce is read from.
@param aadlen Length of the associated data.
@param aad Pointer to `aadlen` bytes of memory where the associated data is read from.

@param mlen Length of the message.
@param m Pointer to `mlen` bytes of memory where the message is read from.
@param cipher Pointer to `mlen` bytes of memory where the ciphertext is written to.
@param mac Pointer to 16 bytes of memory where the mac is written to.
*/
void
Hacl_Chacha20Poly1305_128_aead_encrypt(
    uint8_t *k,
    uint8_t *n,
    uint32_t aadlen,
    uint8_t *aad,
    uint32_t mlen,
    uint8_t *m,
    uint8_t *cipher,
    uint8_t *mac);

/**
Decrypt a ciphertext `cipher` with key `k`.

The arguments `k`, `n`, `aadlen`, and `aad` are same in encryption/decryption.
Note: Encryption and decryption can be executed in-place, i.e., `m` and `cipher` can point to the same memory.

If decryption succeeds, the resulting plaintext is stored in `m` and the function returns the success code 0.
If decryption fails, the array `m` remains unchanged and the function returns the error code 1.

@param k Pointer to 32 bytes of memory where the AEAD key is read from.
@param n Pointer to 12 bytes of memory where the AEAD nonce is read from.
@param aadlen Length of the associated data.
@param aad Pointer to `aadlen` bytes of memory where the associated data is read from.

@param mlen Length of the ciphertext.
@param m Pointer to `mlen` bytes of memory where the message is written to.
@param cipher Pointer to `mlen` bytes of memory where the ciphertext is read from.
@param mac Pointer to 16 bytes of memory where the mac is read from.

@returns 0 on succeess; 1 on failure.
*/
uint32_t
Hacl_Chacha20Poly1305_128_aead_decrypt(
    uint8_t *k,
    uint8_t *n,
    uint32_t aadlen,
    uint8_t *aad,
    uint32_t mlen,
    uint8_t *m,
    uint8_t *cipher,
    uint8_t *mac);

#if defined(__cplusplus)
}
#endif

#define __Hacl_Chacha20Poly1305_128_H_DEFINED
#endif
