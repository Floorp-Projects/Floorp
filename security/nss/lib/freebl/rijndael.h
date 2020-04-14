/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _RIJNDAEL_H_
#define _RIJNDAEL_H_ 1

#include "blapii.h"
#include <stdint.h>

#if defined(NSS_X86_OR_X64)
/* GCC <= 4.8 doesn't support including emmintrin.h without enabling SSE2 */
#if !defined(__clang__) && defined(__GNUC__) && defined(__GNUC_MINOR__) && \
    (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ <= 8))
#pragma GCC push_options
#pragma GCC target("sse2")
#undef NSS_DISABLE_SSE2
#define NSS_DISABLE_SSE2 1
#endif /* GCC <= 4.8 */

#include <emmintrin.h> /* __m128i */

#ifdef NSS_DISABLE_SSE2
#undef NSS_DISABLE_SSE2
#pragma GCC pop_options
#endif /* NSS_DISABLE_SSE2 */
#endif

/* RIJNDAEL_NUM_ROUNDS
 *
 * Number of rounds per execution
 * Nk - number of key bytes
 * Nb - blocksize (in bytes)
 */
#define RIJNDAEL_NUM_ROUNDS(Nk, Nb) \
    (PR_MAX(Nk, Nb) + 6)

/*
 * This magic number is (Nb_max * (Nr_max + 1))
 * where Nb_max is the maximum block size in 32-bit words,
 *       Nr_max is the maximum number of rounds, which is Nb_max + 6
 */
#define RIJNDAEL_MAX_EXP_KEY_SIZE (4 * 15)

/* AESContextStr
 *
 * Values which maintain the state for Rijndael encryption/decryption.
 *
 * keySchedule - 128-bit registers for the key-schedule
 * iv          - initialization vector for CBC mode
 * Nb          - the number of bytes in a block, specified by user
 * Nr          - the number of rounds, specified by a table
 * expandedKey - the round keys in 4-byte words, the length is Nr * Nb
 * worker      - the encryption/decryption function to use with worker_cx
 * destroy     - if not NULL, the destroy function to use with worker_cx
 * worker_cx   - the context for worker and destroy
 * isBlock     - is the mode of operation a block cipher or a stream cipher?
 */
struct AESContextStr {
    /* NOTE: Offsets to members in this struct are hardcoded in assembly.
     * Don't change the struct without updating intel-aes.s and intel-gcm.s. */
    union {
#if defined(NSS_X86_OR_X64)
        __m128i keySchedule[15];
#endif
        PRUint32 expandedKey[RIJNDAEL_MAX_EXP_KEY_SIZE];
    } k;
    unsigned int Nb;
    unsigned int Nr;
    freeblCipherFunc worker;
    unsigned char iv[AES_BLOCK_SIZE];
    freeblAeadFunc worker_aead;
    freeblDestroyFunc destroy;
    void *worker_cx;
    PRBool isBlock;
    int mode;
    void *mem; /* Start of the allocated memory to free. */
};

#endif /* _RIJNDAEL_H_ */
