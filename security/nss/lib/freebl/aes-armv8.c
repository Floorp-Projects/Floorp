/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secerr.h"
#include "rijndael.h"

#if (defined(__clang__) ||                            \
     (defined(__GNUC__) && defined(__GNUC_MINOR__) && \
      (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 8))))

#ifndef __ARM_FEATURE_CRYPTO
#error "Compiler option is invalid"
#endif

#include <arm_neon.h>

SECStatus
arm_aes_encrypt_ecb_128(AESContext *cx, unsigned char *output,
                        unsigned int *outputLen,
                        unsigned int maxOutputLen,
                        const unsigned char *input,
                        unsigned int inputLen,
                        unsigned int blocksize)
{
#if !defined(HAVE_UNALIGNED_ACCESS)
    pre_align unsigned char buf[16] post_align;
#endif
    uint8x16_t key1, key2, key3, key4, key5, key6, key7, key8, key9, key10;
    uint8x16_t key11;
    const PRUint8 *key = (const PRUint8 *)cx->expandedKey;

    if (!inputLen) {
        return SECSuccess;
    }

    key1 = vld1q_u8(key);
    key2 = vld1q_u8(key + 16);
    key3 = vld1q_u8(key + 32);
    key4 = vld1q_u8(key + 48);
    key5 = vld1q_u8(key + 64);
    key6 = vld1q_u8(key + 80);
    key7 = vld1q_u8(key + 96);
    key8 = vld1q_u8(key + 112);
    key9 = vld1q_u8(key + 128);
    key10 = vld1q_u8(key + 144);
    key11 = vld1q_u8(key + 160);

    while (inputLen > 0) {
        uint8x16_t state;
#if defined(HAVE_UNALIGNED_ACCESS)
        state = vld1q_u8(input);
#else
        if ((uintptr_t)input & 0x7) {
            memcpy(buf, input, 16);
            state = vld1q_u8(__builtin_assume_aligned(buf, 16));
        } else {
            state = vld1q_u8(__builtin_assume_aligned(input, 8));
        }
#endif
        input += 16;
        inputLen -= 16;

        /* Rounds */
        state = vaeseq_u8(state, key1);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key2);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key3);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key4);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key5);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key6);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key7);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key8);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key9);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key10);
        /* AddRoundKey */
        state = veorq_u8(state, key11);

#if defined(HAVE_UNALIGNED_ACCESS)
        vst1q_u8(output, state);
#else
        if ((uintptr_t)output & 0x7) {
            vst1q_u8(__builtin_assume_aligned(buf, 16), state);
            memcpy(output, buf, 16);
        } else {
            vst1q_u8(__builtin_assume_aligned(output, 8), state);
        }
#endif
        output += 16;
    }

    return SECSuccess;
}

SECStatus
arm_aes_decrypt_ecb_128(AESContext *cx, unsigned char *output,
                        unsigned int *outputLen,
                        unsigned int maxOutputLen,
                        const unsigned char *input,
                        unsigned int inputLen,
                        unsigned int blocksize)
{
#if !defined(HAVE_UNALIGNED_ACCESS)
    pre_align unsigned char buf[16] post_align;
#endif
    uint8x16_t key1, key2, key3, key4, key5, key6, key7, key8, key9, key10;
    uint8x16_t key11;
    const PRUint8 *key = (const PRUint8 *)cx->expandedKey;

    if (inputLen == 0) {
        return SECSuccess;
    }

    key1 = vld1q_u8(key);
    key2 = vld1q_u8(key + 16);
    key3 = vld1q_u8(key + 32);
    key4 = vld1q_u8(key + 48);
    key5 = vld1q_u8(key + 64);
    key6 = vld1q_u8(key + 80);
    key7 = vld1q_u8(key + 96);
    key8 = vld1q_u8(key + 112);
    key9 = vld1q_u8(key + 128);
    key10 = vld1q_u8(key + 144);
    key11 = vld1q_u8(key + 160);

    while (inputLen > 0) {
        uint8x16_t state;
#if defined(HAVE_UNALIGNED_ACCESS)
        state = vld1q_u8(input);
#else
        if ((uintptr_t)input & 0x7) {
            memcpy(buf, input, 16);
            state = vld1q_u8(__builtin_assume_aligned(buf, 16));
        } else {
            state = vld1q_u8(__builtin_assume_aligned(input, 8));
        }
#endif
        input += 16;
        inputLen -= 16;

        /* Rounds */
        state = vaesdq_u8(state, key11);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key10);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key9);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key8);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key7);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key6);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key5);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key4);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key3);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key2);
        /* AddRoundKey */
        state = veorq_u8(state, key1);

#if defined(HAVE_UNALIGNED_ACCESS)
        vst1q_u8(output, state);
#else
        if ((uintptr_t)output & 0x7) {
            vst1q_u8(__builtin_assume_aligned(buf, 16), state);
            memcpy(output, buf, 16);
        } else {
            vst1q_u8(__builtin_assume_aligned(output, 8), state);
        }
#endif
        output += 16;
    }

    return SECSuccess;
}

SECStatus
arm_aes_encrypt_cbc_128(AESContext *cx, unsigned char *output,
                        unsigned int *outputLen,
                        unsigned int maxOutputLen,
                        const unsigned char *input,
                        unsigned int inputLen,
                        unsigned int blocksize)
{
#if !defined(HAVE_UNALIGNED_ACCESS)
    pre_align unsigned char buf[16] post_align;
#endif
    uint8x16_t key1, key2, key3, key4, key5, key6, key7, key8, key9, key10;
    uint8x16_t key11;
    uint8x16_t iv;
    const PRUint8 *key = (const PRUint8 *)cx->expandedKey;

    if (!inputLen) {
        return SECSuccess;
    }

    /* iv */
    iv = vld1q_u8(cx->iv);

    /* expanedKey */
    key1 = vld1q_u8(key);
    key2 = vld1q_u8(key + 16);
    key3 = vld1q_u8(key + 32);
    key4 = vld1q_u8(key + 48);
    key5 = vld1q_u8(key + 64);
    key6 = vld1q_u8(key + 80);
    key7 = vld1q_u8(key + 96);
    key8 = vld1q_u8(key + 112);
    key9 = vld1q_u8(key + 128);
    key10 = vld1q_u8(key + 144);
    key11 = vld1q_u8(key + 160);

    while (inputLen > 0) {
        uint8x16_t state;
#if defined(HAVE_UNALIGNED_ACCESS)
        state = vld1q_u8(input);
#else
        if ((uintptr_t)input & 0x7) {
            memcpy(buf, input, 16);
            state = vld1q_u8(__builtin_assume_aligned(buf, 16));
        } else {
            state = vld1q_u8(__builtin_assume_aligned(input, 8));
        }
#endif
        input += 16;
        inputLen -= 16;

        state = veorq_u8(state, iv);

        /* Rounds */
        state = vaeseq_u8(state, key1);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key2);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key3);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key4);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key5);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key6);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key7);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key8);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key9);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key10);
        /* AddRoundKey */
        state = veorq_u8(state, key11);

#if defined(HAVE_UNALIGNED_ACCESS)
        vst1q_u8(output, state);
#else
        if ((uintptr_t)output & 0x7) {
            vst1q_u8(__builtin_assume_aligned(buf, 16), state);
            memcpy(output, buf, 16);
        } else {
            vst1q_u8(__builtin_assume_aligned(output, 8), state);
        }
#endif
        output += 16;
        iv = state;
    }
    vst1q_u8(cx->iv, iv);

    return SECSuccess;
}

SECStatus
arm_aes_decrypt_cbc_128(AESContext *cx, unsigned char *output,
                        unsigned int *outputLen,
                        unsigned int maxOutputLen,
                        const unsigned char *input,
                        unsigned int inputLen,
                        unsigned int blocksize)
{
#if !defined(HAVE_UNALIGNED_ACCESS)
    pre_align unsigned char buf[16] post_align;
#endif
    uint8x16_t iv;
    uint8x16_t key1, key2, key3, key4, key5, key6, key7, key8, key9, key10;
    uint8x16_t key11;
    const PRUint8 *key = (const PRUint8 *)cx->expandedKey;

    if (!inputLen) {
        return SECSuccess;
    }

    /* iv */
    iv = vld1q_u8(cx->iv);

    /* expanedKey */
    key1 = vld1q_u8(key);
    key2 = vld1q_u8(key + 16);
    key3 = vld1q_u8(key + 32);
    key4 = vld1q_u8(key + 48);
    key5 = vld1q_u8(key + 64);
    key6 = vld1q_u8(key + 80);
    key7 = vld1q_u8(key + 96);
    key8 = vld1q_u8(key + 112);
    key9 = vld1q_u8(key + 128);
    key10 = vld1q_u8(key + 144);
    key11 = vld1q_u8(key + 160);

    while (inputLen > 0) {
        uint8x16_t state, old_state;
#if defined(HAVE_UNALIGNED_ACCESS)
        state = vld1q_u8(input);
#else
        if ((uintptr_t)input & 0x7) {
            memcpy(buf, input, 16);
            state = vld1q_u8(__builtin_assume_aligned(buf, 16));
        } else {
            state = vld1q_u8(__builtin_assume_aligned(input, 8));
        }
#endif
        old_state = state;
        input += 16;
        inputLen -= 16;

        /* Rounds */
        state = vaesdq_u8(state, key11);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key10);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key9);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key8);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key7);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key6);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key5);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key4);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key3);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key2);
        /* AddRoundKey */
        state = veorq_u8(state, key1);

        state = veorq_u8(state, iv);

#if defined(HAVE_UNALIGNED_ACCESS)
        vst1q_u8(output, state);
#else
        if ((uintptr_t)output & 0x7) {
            vst1q_u8(__builtin_assume_aligned(buf, 16), state);
            memcpy(output, buf, 16);
        } else {
            vst1q_u8(__builtin_assume_aligned(output, 8), state);
        }
#endif
        output += 16;

        iv = old_state;
    }
    vst1q_u8(cx->iv, iv);

    return SECSuccess;
}

SECStatus
arm_aes_encrypt_ecb_192(AESContext *cx, unsigned char *output,
                        unsigned int *outputLen,
                        unsigned int maxOutputLen,
                        const unsigned char *input,
                        unsigned int inputLen,
                        unsigned int blocksize)
{
#if !defined(HAVE_UNALIGNED_ACCESS)
    pre_align unsigned char buf[16] post_align;
#endif
    uint8x16_t key1, key2, key3, key4, key5, key6, key7, key8, key9, key10;
    uint8x16_t key11, key12, key13;
    PRUint8 *key = (PRUint8 *)cx->expandedKey;

    if (!inputLen) {
        return SECSuccess;
    }

    key1 = vld1q_u8(key);
    key2 = vld1q_u8(key + 16);
    key3 = vld1q_u8(key + 32);
    key4 = vld1q_u8(key + 48);
    key5 = vld1q_u8(key + 64);
    key6 = vld1q_u8(key + 80);
    key7 = vld1q_u8(key + 96);
    key8 = vld1q_u8(key + 112);
    key9 = vld1q_u8(key + 128);
    key10 = vld1q_u8(key + 144);
    key11 = vld1q_u8(key + 160);
    key12 = vld1q_u8(key + 176);
    key13 = vld1q_u8(key + 192);

    while (inputLen > 0) {
        uint8x16_t state;
#if defined(HAVE_UNALIGNED_ACCESS)
        state = vld1q_u8(input);
#else
        if ((uintptr_t)input & 0x7) {
            memcpy(buf, input, 16);
            state = vld1q_u8(__builtin_assume_aligned(buf, 16));
        } else {
            state = vld1q_u8(__builtin_assume_aligned(input, 8));
        }
#endif
        input += 16;
        inputLen -= 16;

        /* Rounds */
        state = vaeseq_u8(state, key1);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key2);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key3);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key4);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key5);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key6);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key7);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key8);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key9);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key10);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key11);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key12);
        /* AddRoundKey */
        state = veorq_u8(state, key13);

#if defined(HAVE_UNALIGNED_ACCESS)
        vst1q_u8(output, state);
#else
        if ((uintptr_t)output & 0x7) {
            vst1q_u8(__builtin_assume_aligned(buf, 16), state);
            memcpy(output, buf, 16);
        } else {
            vst1q_u8(__builtin_assume_aligned(output, 8), state);
        }
#endif
        output += 16;
    }

    return SECSuccess;
}

SECStatus
arm_aes_decrypt_ecb_192(AESContext *cx, unsigned char *output,
                        unsigned int *outputLen,
                        unsigned int maxOutputLen,
                        const unsigned char *input,
                        unsigned int inputLen,
                        unsigned int blocksize)
{
#if !defined(HAVE_UNALIGNED_ACCESS)
    pre_align unsigned char buf[16] post_align;
#endif
    uint8x16_t key1, key2, key3, key4, key5, key6, key7, key8, key9, key10;
    uint8x16_t key11, key12, key13;
    const PRUint8 *key = (const PRUint8 *)cx->expandedKey;

    if (!inputLen) {
        return SECSuccess;
    }

    key1 = vld1q_u8(key);
    key2 = vld1q_u8(key + 16);
    key3 = vld1q_u8(key + 32);
    key4 = vld1q_u8(key + 48);
    key5 = vld1q_u8(key + 64);
    key6 = vld1q_u8(key + 80);
    key7 = vld1q_u8(key + 96);
    key8 = vld1q_u8(key + 112);
    key9 = vld1q_u8(key + 128);
    key10 = vld1q_u8(key + 144);
    key11 = vld1q_u8(key + 160);
    key12 = vld1q_u8(key + 176);
    key13 = vld1q_u8(key + 192);

    while (inputLen > 0) {
        uint8x16_t state;
#if defined(HAVE_UNALIGNED_ACCESS)
        state = vld1q_u8(input);
#else
        if ((uintptr_t)input & 0x7) {
            memcpy(buf, input, 16);
            state = vld1q_u8(__builtin_assume_aligned(buf, 16));
        } else {
            state = vld1q_u8(__builtin_assume_aligned(input, 8));
        }
#endif
        input += 16;
        inputLen -= 16;

        /* Rounds */
        state = vaesdq_u8(state, key13);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key12);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key11);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key10);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key9);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key8);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key7);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key6);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key5);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key4);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key3);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key2);
        /* AddRoundKey */
        state = veorq_u8(state, key1);

#if defined(HAVE_UNALIGNED_ACCESS)
        vst1q_u8(output, state);
#else
        if ((uintptr_t)output & 0x7) {
            vst1q_u8(__builtin_assume_aligned(buf, 16), state);
            memcpy(output, buf, 16);
        } else {
            vst1q_u8(__builtin_assume_aligned(output, 8), state);
        }
#endif
        output += 16;
    }

    return SECSuccess;
}

SECStatus
arm_aes_encrypt_cbc_192(AESContext *cx, unsigned char *output,
                        unsigned int *outputLen,
                        unsigned int maxOutputLen,
                        const unsigned char *input,
                        unsigned int inputLen,
                        unsigned int blocksize)
{
#if !defined(HAVE_UNALIGNED_ACCESS)
    pre_align unsigned char buf[16] post_align;
#endif
    uint8x16_t key1, key2, key3, key4, key5, key6, key7, key8, key9, key10;
    uint8x16_t key11, key12, key13;
    uint8x16_t iv;
    PRUint8 *key = (PRUint8 *)cx->expandedKey;

    if (!inputLen) {
        return SECSuccess;
    }

    /* iv */
    iv = vld1q_u8(cx->iv);

    /* expanedKey */
    key1 = vld1q_u8(key);
    key2 = vld1q_u8(key + 16);
    key3 = vld1q_u8(key + 32);
    key4 = vld1q_u8(key + 48);
    key5 = vld1q_u8(key + 64);
    key6 = vld1q_u8(key + 80);
    key7 = vld1q_u8(key + 96);
    key8 = vld1q_u8(key + 112);
    key9 = vld1q_u8(key + 128);
    key10 = vld1q_u8(key + 144);
    key11 = vld1q_u8(key + 160);
    key12 = vld1q_u8(key + 176);
    key13 = vld1q_u8(key + 192);

    while (inputLen > 0) {
        uint8x16_t state;
#if defined(HAVE_UNALIGNED_ACCESS)
        state = vld1q_u8(input);
#else
        if ((uintptr_t)input & 0x7) {
            memcpy(buf, input, 16);
            state = vld1q_u8(__builtin_assume_aligned(buf, 16));
        } else {
            state = vld1q_u8(__builtin_assume_aligned(input, 8));
        }
#endif
        input += 16;
        inputLen -= 16;

        state = veorq_u8(state, iv);

        /* Rounds */
        state = vaeseq_u8(state, key1);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key2);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key3);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key4);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key5);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key6);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key7);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key8);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key9);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key10);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key11);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key12);
        state = veorq_u8(state, key13);

#if defined(HAVE_UNALIGNED_ACCESS)
        vst1q_u8(output, state);
#else
        if ((uintptr_t)output & 0x7) {
            vst1q_u8(__builtin_assume_aligned(buf, 16), state);
            memcpy(output, buf, 16);
        } else {
            vst1q_u8(__builtin_assume_aligned(output, 8), state);
        }
#endif
        output += 16;
        iv = state;
    }
    vst1q_u8(cx->iv, iv);

    return SECSuccess;
}

SECStatus
arm_aes_decrypt_cbc_192(AESContext *cx, unsigned char *output,
                        unsigned int *outputLen,
                        unsigned int maxOutputLen,
                        const unsigned char *input,
                        unsigned int inputLen,
                        unsigned int blocksize)
{
#if !defined(HAVE_UNALIGNED_ACCESS)
    pre_align unsigned char buf[16] post_align;
#endif
    uint8x16_t iv;
    uint8x16_t key1, key2, key3, key4, key5, key6, key7, key8, key9, key10;
    uint8x16_t key11, key12, key13;
    const PRUint8 *key = (const PRUint8 *)cx->expandedKey;

    if (!inputLen) {
        return SECSuccess;
    }

    /* iv */
    iv = vld1q_u8(cx->iv);

    /* expanedKey */
    key1 = vld1q_u8(key);
    key2 = vld1q_u8(key + 16);
    key3 = vld1q_u8(key + 32);
    key4 = vld1q_u8(key + 48);
    key5 = vld1q_u8(key + 64);
    key6 = vld1q_u8(key + 80);
    key7 = vld1q_u8(key + 96);
    key8 = vld1q_u8(key + 112);
    key9 = vld1q_u8(key + 128);
    key10 = vld1q_u8(key + 144);
    key11 = vld1q_u8(key + 160);
    key12 = vld1q_u8(key + 176);
    key13 = vld1q_u8(key + 192);

    while (inputLen > 0) {
        uint8x16_t state, old_state;
#if defined(HAVE_UNALIGNED_ACCESS)
        state = vld1q_u8(input);
#else
        if ((uintptr_t)input & 0x7) {
            memcpy(buf, input, 16);
            state = vld1q_u8(__builtin_assume_aligned(buf, 16));
        } else {
            state = vld1q_u8(__builtin_assume_aligned(input, 8));
        }
#endif
        old_state = state;
        input += 16;
        inputLen -= 16;

        /* Rounds */
        state = vaesdq_u8(state, key13);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key12);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key11);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key10);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key9);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key8);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key7);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key6);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key5);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key4);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key3);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key2);
        /* AddRoundKey */
        state = veorq_u8(state, key1);

        state = veorq_u8(state, iv);

#if defined(HAVE_UNALIGNED_ACCESS)
        vst1q_u8(output, state);
#else
        if ((uintptr_t)output & 0x7) {
            vst1q_u8(__builtin_assume_aligned(buf, 16), state);
            memcpy(output, buf, 16);
        } else {
            vst1q_u8(__builtin_assume_aligned(output, 8), state);
        }
#endif
        output += 16;

        iv = old_state;
    }
    vst1q_u8(cx->iv, iv);

    return SECSuccess;
}

SECStatus
arm_aes_encrypt_ecb_256(AESContext *cx, unsigned char *output,
                        unsigned int *outputLen,
                        unsigned int maxOutputLen,
                        const unsigned char *input,
                        unsigned int inputLen,
                        unsigned int blocksize)
{
#if !defined(HAVE_UNALIGNED_ACCESS)
    pre_align unsigned char buf[16] post_align;
#endif
    uint8x16_t key1, key2, key3, key4, key5, key6, key7, key8, key9, key10;
    uint8x16_t key11, key12, key13, key14, key15;
    PRUint8 *key = (PRUint8 *)cx->expandedKey;

    if (inputLen == 0) {
        return SECSuccess;
    }

    key1 = vld1q_u8(key);
    key2 = vld1q_u8(key + 16);
    key3 = vld1q_u8(key + 32);
    key4 = vld1q_u8(key + 48);
    key5 = vld1q_u8(key + 64);
    key6 = vld1q_u8(key + 80);
    key7 = vld1q_u8(key + 96);
    key8 = vld1q_u8(key + 112);
    key9 = vld1q_u8(key + 128);
    key10 = vld1q_u8(key + 144);
    key11 = vld1q_u8(key + 160);
    key12 = vld1q_u8(key + 176);
    key13 = vld1q_u8(key + 192);
    key14 = vld1q_u8(key + 208);
    key15 = vld1q_u8(key + 224);

    while (inputLen > 0) {
        uint8x16_t state;
#if defined(HAVE_UNALIGNED_ACCESS)
        state = vld1q_u8(input);
#else
        if ((uintptr_t)input & 0x7) {
            memcpy(buf, input, 16);
            state = vld1q_u8(__builtin_assume_aligned(buf, 16));
        } else {
            state = vld1q_u8(__builtin_assume_aligned(input, 8));
        }
#endif
        input += 16;
        inputLen -= 16;

        /* Rounds */
        state = vaeseq_u8(state, key1);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key2);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key3);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key4);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key5);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key6);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key7);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key8);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key9);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key10);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key11);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key12);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key13);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key14);
        /* AddRoundKey */
        state = veorq_u8(state, key15);

#if defined(HAVE_UNALIGNED_ACCESS)
        vst1q_u8(output, state);
#else
        if ((uintptr_t)output & 0x7) {
            vst1q_u8(__builtin_assume_aligned(buf, 16), state);
            memcpy(output, buf, 16);
        } else {
            vst1q_u8(__builtin_assume_aligned(output, 8), state);
        }
#endif
        output += 16;
    }
    return SECSuccess;
}

SECStatus
arm_aes_decrypt_ecb_256(AESContext *cx, unsigned char *output,
                        unsigned int *outputLen,
                        unsigned int maxOutputLen,
                        const unsigned char *input,
                        unsigned int inputLen,
                        unsigned int blocksize)
{
#if !defined(HAVE_UNALIGNED_ACCESS)
    pre_align unsigned char buf[16] post_align;
#endif
    uint8x16_t key1, key2, key3, key4, key5, key6, key7, key8, key9, key10;
    uint8x16_t key11, key12, key13, key14, key15;
    const PRUint8 *key = (const PRUint8 *)cx->expandedKey;

    if (!inputLen) {
        return SECSuccess;
    }

    key1 = vld1q_u8(key);
    key2 = vld1q_u8(key + 16);
    key3 = vld1q_u8(key + 32);
    key4 = vld1q_u8(key + 48);
    key5 = vld1q_u8(key + 64);
    key6 = vld1q_u8(key + 80);
    key7 = vld1q_u8(key + 96);
    key8 = vld1q_u8(key + 112);
    key9 = vld1q_u8(key + 128);
    key10 = vld1q_u8(key + 144);
    key11 = vld1q_u8(key + 160);
    key12 = vld1q_u8(key + 176);
    key13 = vld1q_u8(key + 192);
    key14 = vld1q_u8(key + 208);
    key15 = vld1q_u8(key + 224);

    while (inputLen > 0) {
        uint8x16_t state;
#if defined(HAVE_UNALIGNED_ACCESS)
        state = vld1q_u8(input);
#else
        if ((uintptr_t)input & 0x7) {
            memcpy(buf, input, 16);
            state = vld1q_u8(__builtin_assume_aligned(buf, 16));
        } else {
            state = vld1q_u8(__builtin_assume_aligned(input, 8));
        }
#endif
        input += 16;
        inputLen -= 16;

        /* Rounds */
        state = vaesdq_u8(state, key15);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key14);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key13);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key12);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key11);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key10);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key9);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key8);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key7);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key6);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key5);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key4);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key3);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key2);
        /* AddRoundKey */
        state = veorq_u8(state, key1);

#if defined(HAVE_UNALIGNED_ACCESS)
        vst1q_u8(output, state);
#else
        if ((uintptr_t)output & 0x7) {
            vst1q_u8(__builtin_assume_aligned(buf, 16), state);
            memcpy(output, buf, 16);
        } else {
            vst1q_u8(__builtin_assume_aligned(output, 8), state);
        }
#endif
        output += 16;
    }

    return SECSuccess;
}

SECStatus
arm_aes_encrypt_cbc_256(AESContext *cx, unsigned char *output,
                        unsigned int *outputLen,
                        unsigned int maxOutputLen,
                        const unsigned char *input,
                        unsigned int inputLen,
                        unsigned int blocksize)
{
#if !defined(HAVE_UNALIGNED_ACCESS)
    pre_align unsigned char buf[16] post_align;
#endif
    uint8x16_t key1, key2, key3, key4, key5, key6, key7, key8, key9, key10;
    uint8x16_t key11, key12, key13, key14, key15;
    uint8x16_t iv;
    const PRUint8 *key = (const PRUint8 *)cx->expandedKey;

    if (!inputLen) {
        return SECSuccess;
    }

    /* iv */
    iv = vld1q_u8(cx->iv);

    /* expanedKey */
    key1 = vld1q_u8(key);
    key2 = vld1q_u8(key + 16);
    key3 = vld1q_u8(key + 32);
    key4 = vld1q_u8(key + 48);
    key5 = vld1q_u8(key + 64);
    key6 = vld1q_u8(key + 80);
    key7 = vld1q_u8(key + 96);
    key8 = vld1q_u8(key + 112);
    key9 = vld1q_u8(key + 128);
    key10 = vld1q_u8(key + 144);
    key11 = vld1q_u8(key + 160);
    key12 = vld1q_u8(key + 176);
    key13 = vld1q_u8(key + 192);
    key14 = vld1q_u8(key + 208);
    key15 = vld1q_u8(key + 224);

    while (inputLen > 0) {
        uint8x16_t state;
#if defined(HAVE_UNALIGNED_ACCESS)
        state = vld1q_u8(input);
#else
        if ((uintptr_t)input & 0x7) {
            memcpy(buf, input, 16);
            state = vld1q_u8(__builtin_assume_aligned(buf, 16));
        } else {
            state = vld1q_u8(__builtin_assume_aligned(input, 8));
        }
#endif
        input += 16;
        inputLen -= 16;

        state = veorq_u8(state, iv);

        /* Rounds */
        state = vaeseq_u8(state, key1);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key2);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key3);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key4);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key5);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key6);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key7);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key8);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key9);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key10);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key11);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key12);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key13);
        state = vaesmcq_u8(state);
        state = vaeseq_u8(state, key14);
        /* AddRoundKey */
        state = veorq_u8(state, key15);

#if defined(HAVE_UNALIGNED_ACCESS)
        vst1q_u8(output, state);
#else
        if ((uintptr_t)output & 0x7) {
            vst1q_u8(__builtin_assume_aligned(buf, 16), state);
            memcpy(output, buf, 16);
        } else {
            vst1q_u8(__builtin_assume_aligned(output, 8), state);
        }
#endif
        output += 16;
        iv = state;
    }
    vst1q_u8(cx->iv, iv);

    return SECSuccess;
}

SECStatus
arm_aes_decrypt_cbc_256(AESContext *cx, unsigned char *output,
                        unsigned int *outputLen,
                        unsigned int maxOutputLen,
                        const unsigned char *input,
                        unsigned int inputLen,
                        unsigned int blocksize)
{
#if !defined(HAVE_UNALIGNED_ACCESS)
    pre_align unsigned char buf[16] post_align;
#endif
    uint8x16_t iv;
    uint8x16_t key1, key2, key3, key4, key5, key6, key7, key8, key9, key10;
    uint8x16_t key11, key12, key13, key14, key15;
    const PRUint8 *key = (const PRUint8 *)cx->expandedKey;

    if (!inputLen) {
        return SECSuccess;
    }

    /* iv */
    iv = vld1q_u8(cx->iv);

    /* expanedKey */
    key1 = vld1q_u8(key);
    key2 = vld1q_u8(key + 16);
    key3 = vld1q_u8(key + 32);
    key4 = vld1q_u8(key + 48);
    key5 = vld1q_u8(key + 64);
    key6 = vld1q_u8(key + 80);
    key7 = vld1q_u8(key + 96);
    key8 = vld1q_u8(key + 112);
    key9 = vld1q_u8(key + 128);
    key10 = vld1q_u8(key + 144);
    key11 = vld1q_u8(key + 160);
    key12 = vld1q_u8(key + 176);
    key13 = vld1q_u8(key + 192);
    key14 = vld1q_u8(key + 208);
    key15 = vld1q_u8(key + 224);

    while (inputLen > 0) {
        uint8x16_t state, old_state;
#if defined(HAVE_UNALIGNED_ACCESS)
        state = vld1q_u8(input);
#else
        if ((uintptr_t)input & 0x7) {
            memcpy(buf, input, 16);
            state = vld1q_u8(__builtin_assume_aligned(buf, 16));
        } else {
            state = vld1q_u8(__builtin_assume_aligned(input, 8));
        }
#endif
        old_state = state;
        input += 16;
        inputLen -= 16;

        /* Rounds */
        state = vaesdq_u8(state, key15);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key14);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key13);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key12);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key11);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key10);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key9);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key8);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key7);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key6);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key5);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key4);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key3);
        state = vaesimcq_u8(state);
        state = vaesdq_u8(state, key2);
        /* AddRoundKey */
        state = veorq_u8(state, key1);

        state = veorq_u8(state, iv);

#if defined(HAVE_UNALIGNED_ACCESS)
        vst1q_u8(output, state);
#else
        if ((uintptr_t)output & 0x7) {
            vst1q_u8(__builtin_assume_aligned(buf, 16), state);
            memcpy(output, buf, 16);
        } else {
            vst1q_u8(__builtin_assume_aligned(output, 8), state);
        }
#endif
        output += 16;

        iv = old_state;
    }
    vst1q_u8(cx->iv, iv);

    return SECSuccess;
}

#endif
