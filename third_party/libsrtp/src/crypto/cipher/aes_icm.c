/*
 * aes_icm.c
 *
 * AES Integer Counter Mode
 *
 * David A. McGrew
 * Cisco Systems, Inc.
 */

/*
 *
 * Copyright (c) 2001-2017 Cisco Systems, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define ALIGN_32 0

#include "aes_icm.h"
#include "alloc.h"
#include "cipher_types.h"
#include "cipher_test_cases.h"

srtp_debug_module_t srtp_mod_aes_icm = {
    0,        /* debugging is off by default */
    "aes icm" /* printable module name       */
};

/*
 * integer counter mode works as follows:
 *
 * 16 bits
 * <----->
 * +------+------+------+------+------+------+------+------+
 * |           nonce           |    pakcet index    |  ctr |---+
 * +------+------+------+------+------+------+------+------+   |
 *                                                             |
 * +------+------+------+------+------+------+------+------+   v
 * |                      salt                      |000000|->(+)
 * +------+------+------+------+------+------+------+------+   |
 *                                                             |
 *                                                        +---------+
 *							  | encrypt |
 *							  +---------+
 *							       |
 * +------+------+------+------+------+------+------+------+   |
 * |                    keystream block                    |<--+
 * +------+------+------+------+------+------+------+------+
 *
 * All fields are big-endian
 *
 * ctr is the block counter, which increments from zero for
 * each packet (16 bits wide)
 *
 * packet index is distinct for each packet (48 bits wide)
 *
 * nonce can be distinct across many uses of the same key, or
 * can be a fixed value per key, or can be per-packet randomness
 * (64 bits)
 *
 */

static srtp_err_status_t srtp_aes_icm_alloc(srtp_cipher_t **c,
                                            int key_len,
                                            int tlen)
{
    srtp_aes_icm_ctx_t *icm;
    (void)tlen;

    debug_print(srtp_mod_aes_icm, "allocating cipher with key length %d",
                key_len);

    /*
     * The check for key_len = 30/46 does not apply. Our usage
     * of aes functions with key_len = values other than 30
     * has not broken anything. Don't know what would be the
     * effect of skipping this check for srtp in general.
     */
    if (key_len != SRTP_AES_ICM_128_KEY_LEN_WSALT &&
        key_len != SRTP_AES_ICM_256_KEY_LEN_WSALT) {
        return srtp_err_status_bad_param;
    }

    /* allocate memory a cipher of type aes_icm */
    *c = (srtp_cipher_t *)srtp_crypto_alloc(sizeof(srtp_cipher_t));
    if (*c == NULL) {
        return srtp_err_status_alloc_fail;
    }

    icm = (srtp_aes_icm_ctx_t *)srtp_crypto_alloc(sizeof(srtp_aes_icm_ctx_t));
    if (icm == NULL) {
        srtp_crypto_free(*c);
        *c = NULL;
        return srtp_err_status_alloc_fail;
    }

    /* set pointers */
    (*c)->state = icm;

    switch (key_len) {
    case SRTP_AES_ICM_256_KEY_LEN_WSALT:
        (*c)->algorithm = SRTP_AES_ICM_256;
        (*c)->type = &srtp_aes_icm_256;
        break;
    default:
        (*c)->algorithm = SRTP_AES_ICM_128;
        (*c)->type = &srtp_aes_icm_128;
        break;
    }

    /* set key size        */
    icm->key_size = key_len;
    (*c)->key_len = key_len;

    return srtp_err_status_ok;
}

static srtp_err_status_t srtp_aes_icm_dealloc(srtp_cipher_t *c)
{
    srtp_aes_icm_ctx_t *ctx;

    if (c == NULL) {
        return srtp_err_status_bad_param;
    }

    ctx = (srtp_aes_icm_ctx_t *)c->state;
    if (ctx) {
        /* zeroize the key material */
        octet_string_set_to_zero(ctx, sizeof(srtp_aes_icm_ctx_t));
        srtp_crypto_free(ctx);
    }

    /* free the cipher context */
    srtp_crypto_free(c);

    return srtp_err_status_ok;
}

/*
 * aes_icm_context_init(...) initializes the aes_icm_context
 * using the value in key[].
 *
 * the key is the secret key
 *
 * the salt is unpredictable (but not necessarily secret) data which
 * randomizes the starting point in the keystream
 */

static srtp_err_status_t srtp_aes_icm_context_init(void *cv, const uint8_t *key)
{
    srtp_aes_icm_ctx_t *c = (srtp_aes_icm_ctx_t *)cv;
    srtp_err_status_t status;
    int base_key_len, copy_len;

    if (c->key_size == SRTP_AES_ICM_128_KEY_LEN_WSALT ||
        c->key_size == SRTP_AES_ICM_256_KEY_LEN_WSALT) {
        base_key_len = c->key_size - SRTP_SALT_LEN;
    } else {
        return srtp_err_status_bad_param;
    }

    /*
     * set counter and initial values to 'offset' value, being careful not to
     * go past the end of the key buffer
     */
    v128_set_to_zero(&c->counter);
    v128_set_to_zero(&c->offset);

    copy_len = c->key_size - base_key_len;
    /* force last two octets of the offset to be left zero (for srtp
     * compatibility) */
    if (copy_len > SRTP_SALT_LEN) {
        copy_len = SRTP_SALT_LEN;
    }

    memcpy(&c->counter, key + base_key_len, copy_len);
    memcpy(&c->offset, key + base_key_len, copy_len);

    debug_print(srtp_mod_aes_icm, "key:  %s",
                srtp_octet_string_hex_string(key, base_key_len));
    debug_print(srtp_mod_aes_icm, "offset: %s", v128_hex_string(&c->offset));

    /* expand key */
    status =
        srtp_aes_expand_encryption_key(key, base_key_len, &c->expanded_key);
    if (status) {
        v128_set_to_zero(&c->counter);
        v128_set_to_zero(&c->offset);
        return status;
    }

    /* indicate that the keystream_buffer is empty */
    c->bytes_in_buffer = 0;

    return srtp_err_status_ok;
}

/*
 * aes_icm_set_iv(c, iv) sets the counter value to the exor of iv with
 * the offset
 */

static srtp_err_status_t srtp_aes_icm_set_iv(void *cv,
                                             uint8_t *iv,
                                             srtp_cipher_direction_t direction)
{
    srtp_aes_icm_ctx_t *c = (srtp_aes_icm_ctx_t *)cv;
    v128_t nonce;
    (void)direction;

    /* set nonce (for alignment) */
    v128_copy_octet_string(&nonce, iv);

    debug_print(srtp_mod_aes_icm, "setting iv: %s", v128_hex_string(&nonce));

    v128_xor(&c->counter, &c->offset, &nonce);

    debug_print(srtp_mod_aes_icm, "set_counter: %s",
                v128_hex_string(&c->counter));

    /* indicate that the keystream_buffer is empty */
    c->bytes_in_buffer = 0;

    return srtp_err_status_ok;
}

/*
 * aes_icm_advance(...) refills the keystream_buffer and
 * advances the block index of the sicm_context forward by one
 *
 * this is an internal, hopefully inlined function
 */
static void srtp_aes_icm_advance(srtp_aes_icm_ctx_t *c)
{
    /* fill buffer with new keystream */
    v128_copy(&c->keystream_buffer, &c->counter);
    srtp_aes_encrypt(&c->keystream_buffer, &c->expanded_key);
    c->bytes_in_buffer = sizeof(v128_t);

    debug_print(srtp_mod_aes_icm, "counter:    %s",
                v128_hex_string(&c->counter));
    debug_print(srtp_mod_aes_icm, "ciphertext: %s",
                v128_hex_string(&c->keystream_buffer));

    /* clock counter forward */
    if (!++(c->counter.v8[15])) {
        ++(c->counter.v8[14]);
    }
}

/*
 * icm_encrypt deals with the following cases:
 *
 * bytes_to_encr < bytes_in_buffer
 *  - add keystream into data
 *
 * bytes_to_encr > bytes_in_buffer
 *  - add keystream into data until keystream_buffer is depleted
 *  - loop over blocks, filling keystream_buffer and then
 *    adding keystream into data
 *  - fill buffer then add in remaining (< 16) bytes of keystream
 */

static srtp_err_status_t srtp_aes_icm_encrypt(void *cv,
                                              unsigned char *buf,
                                              unsigned int *enc_len)
{
    srtp_aes_icm_ctx_t *c = (srtp_aes_icm_ctx_t *)cv;
    unsigned int bytes_to_encr = *enc_len;
    unsigned int i;
    uint32_t *b;

    /* check that there's enough segment left*/
    unsigned int bytes_of_new_keystream = bytes_to_encr - c->bytes_in_buffer;
    unsigned int blocks_of_new_keystream = (bytes_of_new_keystream + 15) >> 4;
    if ((blocks_of_new_keystream + htons(c->counter.v16[7])) > 0xffff) {
        return srtp_err_status_terminus;
    }

    debug_print(srtp_mod_aes_icm, "block index: %d", htons(c->counter.v16[7]));
    if (bytes_to_encr <= (unsigned int)c->bytes_in_buffer) {
        /* deal with odd case of small bytes_to_encr */
        for (i = (sizeof(v128_t) - c->bytes_in_buffer);
             i < (sizeof(v128_t) - c->bytes_in_buffer + bytes_to_encr); i++) {
            *buf++ ^= c->keystream_buffer.v8[i];
        }

        c->bytes_in_buffer -= bytes_to_encr;

        /* return now to avoid the main loop */
        return srtp_err_status_ok;

    } else {
        /* encrypt bytes until the remaining data is 16-byte aligned */
        for (i = (sizeof(v128_t) - c->bytes_in_buffer); i < sizeof(v128_t);
             i++) {
            *buf++ ^= c->keystream_buffer.v8[i];
        }

        bytes_to_encr -= c->bytes_in_buffer;
        c->bytes_in_buffer = 0;
    }

    /* now loop over entire 16-byte blocks of keystream */
    for (i = 0; i < (bytes_to_encr / sizeof(v128_t)); i++) {
        /* fill buffer with new keystream */
        srtp_aes_icm_advance(c);

        /*
         * add keystream into the data buffer (this would be a lot faster
         * if we could assume 32-bit alignment!)
         */

#if ALIGN_32
        b = (uint32_t *)buf;
        *b++ ^= c->keystream_buffer.v32[0];
        *b++ ^= c->keystream_buffer.v32[1];
        *b++ ^= c->keystream_buffer.v32[2];
        *b++ ^= c->keystream_buffer.v32[3];
        buf = (uint8_t *)b;
#else
        if ((((uintptr_t)buf) & 0x03) != 0) {
            *buf++ ^= c->keystream_buffer.v8[0];
            *buf++ ^= c->keystream_buffer.v8[1];
            *buf++ ^= c->keystream_buffer.v8[2];
            *buf++ ^= c->keystream_buffer.v8[3];
            *buf++ ^= c->keystream_buffer.v8[4];
            *buf++ ^= c->keystream_buffer.v8[5];
            *buf++ ^= c->keystream_buffer.v8[6];
            *buf++ ^= c->keystream_buffer.v8[7];
            *buf++ ^= c->keystream_buffer.v8[8];
            *buf++ ^= c->keystream_buffer.v8[9];
            *buf++ ^= c->keystream_buffer.v8[10];
            *buf++ ^= c->keystream_buffer.v8[11];
            *buf++ ^= c->keystream_buffer.v8[12];
            *buf++ ^= c->keystream_buffer.v8[13];
            *buf++ ^= c->keystream_buffer.v8[14];
            *buf++ ^= c->keystream_buffer.v8[15];
        } else {
            b = (uint32_t *)buf;
            *b++ ^= c->keystream_buffer.v32[0];
            *b++ ^= c->keystream_buffer.v32[1];
            *b++ ^= c->keystream_buffer.v32[2];
            *b++ ^= c->keystream_buffer.v32[3];
            buf = (uint8_t *)b;
        }
#endif /* #if ALIGN_32 */
    }

    /* if there is a tail end of the data, process it */
    if ((bytes_to_encr & 0xf) != 0) {
        /* fill buffer with new keystream */
        srtp_aes_icm_advance(c);

        for (i = 0; i < (bytes_to_encr & 0xf); i++) {
            *buf++ ^= c->keystream_buffer.v8[i];
        }

        /* reset the keystream buffer size to right value */
        c->bytes_in_buffer = sizeof(v128_t) - i;
    } else {
        /* no tail, so just reset the keystream buffer size to zero */
        c->bytes_in_buffer = 0;
    }

    return srtp_err_status_ok;
}

static const char srtp_aes_icm_128_description[] =
    "AES-128 integer counter mode";
static const char srtp_aes_icm_256_description[] =
    "AES-256 integer counter mode";

/*
 * note: the encrypt function is identical to the decrypt function
 */

const srtp_cipher_type_t srtp_aes_icm_128 = {
    srtp_aes_icm_alloc,            /* */
    srtp_aes_icm_dealloc,          /* */
    srtp_aes_icm_context_init,     /* */
    0,                             /* set_aad */
    srtp_aes_icm_encrypt,          /* */
    srtp_aes_icm_encrypt,          /* */
    srtp_aes_icm_set_iv,           /* */
    0,                             /* get_tag */
    srtp_aes_icm_128_description,  /* */
    &srtp_aes_icm_128_test_case_0, /* */
    SRTP_AES_ICM_128               /* */
};

const srtp_cipher_type_t srtp_aes_icm_256 = {
    srtp_aes_icm_alloc,            /* */
    srtp_aes_icm_dealloc,          /* */
    srtp_aes_icm_context_init,     /* */
    0,                             /* set_aad */
    srtp_aes_icm_encrypt,          /* */
    srtp_aes_icm_encrypt,          /* */
    srtp_aes_icm_set_iv,           /* */
    0,                             /* get_tag */
    srtp_aes_icm_256_description,  /* */
    &srtp_aes_icm_256_test_case_0, /* */
    SRTP_AES_ICM_256               /* */
};
