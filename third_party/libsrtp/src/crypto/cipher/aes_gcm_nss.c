/*
 * aes_gcm_nss.c
 *
 * AES Galois Counter Mode
 *
 * Richard L. Barnes
 * Cisco Systems, Inc.
 *
 */

/*
 *
 * Copyright (c) 2013-2017, Cisco Systems, Inc.
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

#include "aes_gcm.h"
#include "alloc.h"
#include "err.h" /* for srtp_debug */
#include "crypto_types.h"
#include "cipher_types.h"
#include "cipher_test_cases.h"
#include <secerr.h>
#include <nspr.h>

srtp_debug_module_t srtp_mod_aes_gcm = {
    0,            /* debugging is off by default */
    "aes gcm nss" /* printable module name       */
};

/*
 * For now we only support 8 and 16 octet tags.  The spec allows for
 * optional 12 byte tag, which may be supported in the future.
 */
#define GCM_IV_LEN 12
#define GCM_AUTH_TAG_LEN 16
#define GCM_AUTH_TAG_LEN_8 8

/*
 * This function allocates a new instance of this crypto engine.
 * The key_len parameter should be one of 28 or 44 for
 * AES-128-GCM or AES-256-GCM respectively.  Note that the
 * key length includes the 14 byte salt value that is used when
 * initializing the KDF.
 */
static srtp_err_status_t srtp_aes_gcm_nss_alloc(srtp_cipher_t **c,
                                                int key_len,
                                                int tlen)
{
    srtp_aes_gcm_ctx_t *gcm;
    NSSInitContext *nss;

    debug_print(srtp_mod_aes_gcm, "allocating cipher with key length %d",
                key_len);
    debug_print(srtp_mod_aes_gcm, "allocating cipher with tag length %d", tlen);

    /*
     * Verify the key_len is valid for one of: AES-128/256
     */
    if (key_len != SRTP_AES_GCM_128_KEY_LEN_WSALT &&
        key_len != SRTP_AES_GCM_256_KEY_LEN_WSALT) {
        return (srtp_err_status_bad_param);
    }

    if (tlen != GCM_AUTH_TAG_LEN && tlen != GCM_AUTH_TAG_LEN_8) {
        return (srtp_err_status_bad_param);
    }

    /* Initialize NSS equiv of NSS_NoDB_Init(NULL) */
    nss = NSS_InitContext("", "", "", "", NULL,
                          NSS_INIT_READONLY | NSS_INIT_NOCERTDB |
                              NSS_INIT_NOMODDB | NSS_INIT_FORCEOPEN |
                              NSS_INIT_OPTIMIZESPACE);
    if (!nss) {
        return (srtp_err_status_cipher_fail);
    }

    /* allocate memory a cipher of type aes_gcm */
    *c = (srtp_cipher_t *)srtp_crypto_alloc(sizeof(srtp_cipher_t));
    if (*c == NULL) {
        NSS_ShutdownContext(nss);
        return (srtp_err_status_alloc_fail);
    }

    gcm = (srtp_aes_gcm_ctx_t *)srtp_crypto_alloc(sizeof(srtp_aes_gcm_ctx_t));
    if (gcm == NULL) {
        NSS_ShutdownContext(nss);
        srtp_crypto_free(*c);
        *c = NULL;
        return (srtp_err_status_alloc_fail);
    }

    gcm->nss = nss;

    /* set pointers */
    (*c)->state = gcm;

    /* setup cipher attributes */
    switch (key_len) {
    case SRTP_AES_GCM_128_KEY_LEN_WSALT:
        (*c)->type = &srtp_aes_gcm_128;
        (*c)->algorithm = SRTP_AES_GCM_128;
        gcm->key_size = SRTP_AES_128_KEY_LEN;
        gcm->tag_size = tlen;
        gcm->params.ulTagBits = 8 * tlen;
        break;
    case SRTP_AES_GCM_256_KEY_LEN_WSALT:
        (*c)->type = &srtp_aes_gcm_256;
        (*c)->algorithm = SRTP_AES_GCM_256;
        gcm->key_size = SRTP_AES_256_KEY_LEN;
        gcm->tag_size = tlen;
        gcm->params.ulTagBits = 8 * tlen;
        break;
    default:
        /* this should never hit, but to be sure... */
        return (srtp_err_status_bad_param);
    }

    /* set key size and tag size*/
    (*c)->key_len = key_len;

    return (srtp_err_status_ok);
}

/*
 * This function deallocates a GCM session
 */
static srtp_err_status_t srtp_aes_gcm_nss_dealloc(srtp_cipher_t *c)
{
    srtp_aes_gcm_ctx_t *ctx;

    ctx = (srtp_aes_gcm_ctx_t *)c->state;
    if (ctx) {
        /* release NSS resources */
        if (ctx->key) {
            PK11_FreeSymKey(ctx->key);
        }

        if (ctx->nss) {
            NSS_ShutdownContext(ctx->nss);
            ctx->nss = NULL;
        }

        /* zeroize the key material */
        octet_string_set_to_zero(ctx, sizeof(srtp_aes_gcm_ctx_t));
        srtp_crypto_free(ctx);
    }

    /* free memory */
    srtp_crypto_free(c);

    return (srtp_err_status_ok);
}

/*
 * aes_gcm_nss_context_init(...) initializes the aes_gcm_context
 * using the value in key[].
 *
 * the key is the secret key
 */
static srtp_err_status_t srtp_aes_gcm_nss_context_init(void *cv,
                                                       const uint8_t *key)
{
    srtp_aes_gcm_ctx_t *c = (srtp_aes_gcm_ctx_t *)cv;

    c->dir = srtp_direction_any;

    debug_print(srtp_mod_aes_gcm, "key:  %s",
                srtp_octet_string_hex_string(key, c->key_size));

    if (c->key) {
        PK11_FreeSymKey(c->key);
        c->key = NULL;
    }

    PK11SlotInfo *slot = PK11_GetBestSlot(CKM_AES_GCM, NULL);
    if (!slot) {
        return (srtp_err_status_cipher_fail);
    }

    SECItem key_item = { siBuffer, (unsigned char *)key, c->key_size };
    c->key = PK11_ImportSymKey(slot, CKM_AES_GCM, PK11_OriginUnwrap,
                               CKA_ENCRYPT, &key_item, NULL);
    PK11_FreeSlot(slot);

    if (!c->key) {
        return (srtp_err_status_cipher_fail);
    }

    return (srtp_err_status_ok);
}

/*
 * aes_gcm_nss_set_iv(c, iv) sets the counter value to the exor of iv with
 * the offset
 */
static srtp_err_status_t srtp_aes_gcm_nss_set_iv(
    void *cv,
    uint8_t *iv,
    srtp_cipher_direction_t direction)
{
    srtp_aes_gcm_ctx_t *c = (srtp_aes_gcm_ctx_t *)cv;

    if (direction != srtp_direction_encrypt &&
        direction != srtp_direction_decrypt) {
        return (srtp_err_status_bad_param);
    }
    c->dir = direction;

    debug_print(srtp_mod_aes_gcm, "setting iv: %s",
                srtp_octet_string_hex_string(iv, GCM_IV_LEN));

    memcpy(c->iv, iv, GCM_IV_LEN);

    return (srtp_err_status_ok);
}

/*
 * This function processes the AAD
 *
 * Parameters:
 *	c	Crypto context
 *	aad	Additional data to process for AEAD cipher suites
 *	aad_len	length of aad buffer
 */
static srtp_err_status_t srtp_aes_gcm_nss_set_aad(void *cv,
                                                  const uint8_t *aad,
                                                  uint32_t aad_len)
{
    srtp_aes_gcm_ctx_t *c = (srtp_aes_gcm_ctx_t *)cv;

    debug_print(srtp_mod_aes_gcm, "setting AAD: %s",
                srtp_octet_string_hex_string(aad, aad_len));

    if (aad_len + c->aad_size > MAX_AD_SIZE) {
        return srtp_err_status_bad_param;
    }

    memcpy(c->aad + c->aad_size, aad, aad_len);
    c->aad_size += aad_len;

    return (srtp_err_status_ok);
}

static srtp_err_status_t srtp_aes_gcm_nss_do_crypto(void *cv,
                                                    int encrypt,
                                                    unsigned char *buf,
                                                    unsigned int *enc_len)
{
    srtp_aes_gcm_ctx_t *c = (srtp_aes_gcm_ctx_t *)cv;

    c->params.pIv = c->iv;
    c->params.ulIvLen = GCM_IV_LEN;
    c->params.pAAD = c->aad;
    c->params.ulAADLen = c->aad_size;

    // Reset AAD
    c->aad_size = 0;

    int rv;
    SECItem param = { siBuffer, (unsigned char *)&c->params,
                      sizeof(CK_GCM_PARAMS) };
    if (encrypt) {
        rv = PK11_Encrypt(c->key, CKM_AES_GCM, &param, buf, enc_len,
                          *enc_len + 16, buf, *enc_len);
    } else {
        rv = PK11_Decrypt(c->key, CKM_AES_GCM, &param, buf, enc_len, *enc_len,
                          buf, *enc_len);
    }

    srtp_err_status_t status = (srtp_err_status_ok);
    if (rv != SECSuccess) {
        status = (srtp_err_status_cipher_fail);
    }

    return status;
}

/*
 * This function encrypts a buffer using AES GCM mode
 *
 * XXX(rlb@ipv.sx): We're required to break off and cache the tag
 * here, because the get_tag() method is separate and the tests expect
 * encrypt() not to change the size of the plaintext.  It might be
 * good to update the calling API so that this is cleaner.
 *
 * Parameters:
 *	c	Crypto context
 *	buf	data to encrypt
 *	enc_len	length of encrypt buffer
 */
static srtp_err_status_t srtp_aes_gcm_nss_encrypt(void *cv,
                                                  unsigned char *buf,
                                                  unsigned int *enc_len)
{
    srtp_aes_gcm_ctx_t *c = (srtp_aes_gcm_ctx_t *)cv;

    // When we get a non-NULL buffer, we know that the caller is
    // prepared to also take the tag.  When we get a NULL buffer,
    // even though there's no data, we need to give NSS a buffer
    // where it can write the tag.  We can't just use c->tag because
    // memcpy has undefined behavior on overlapping ranges.
    unsigned char tagbuf[16];
    unsigned char *non_null_buf = buf;
    if (!non_null_buf && (*enc_len == 0)) {
        non_null_buf = tagbuf;
    } else if (!non_null_buf) {
        return srtp_err_status_bad_param;
    }

    srtp_err_status_t status =
        srtp_aes_gcm_nss_do_crypto(cv, 1, non_null_buf, enc_len);
    if (status != srtp_err_status_ok) {
        return status;
    }

    memcpy(c->tag, non_null_buf + (*enc_len - c->tag_size), c->tag_size);
    *enc_len -= c->tag_size;
    return srtp_err_status_ok;
}

/*
 * This function calculates and returns the GCM tag for a given context.
 * This should be called after encrypting the data.  The *len value
 * is increased by the tag size.  The caller must ensure that *buf has
 * enough room to accept the appended tag.
 *
 * Parameters:
 *	c	Crypto context
 *	buf	data to encrypt
 *	len	length of encrypt buffer
 */
static srtp_err_status_t srtp_aes_gcm_nss_get_tag(void *cv,
                                                  uint8_t *buf,
                                                  uint32_t *len)
{
    srtp_aes_gcm_ctx_t *c = (srtp_aes_gcm_ctx_t *)cv;
    *len = c->tag_size;
    memcpy(buf, c->tag, c->tag_size);
    return (srtp_err_status_ok);
}

/*
 * This function decrypts a buffer using AES GCM mode
 *
 * Parameters:
 *	c	Crypto context
 *	buf	data to encrypt
 *	enc_len	length of encrypt buffer
 */
static srtp_err_status_t srtp_aes_gcm_nss_decrypt(void *cv,
                                                  unsigned char *buf,
                                                  unsigned int *enc_len)
{
    srtp_err_status_t status = srtp_aes_gcm_nss_do_crypto(cv, 0, buf, enc_len);
    if (status != srtp_err_status_ok) {
        int err = PR_GetError();
        if (err == SEC_ERROR_BAD_DATA) {
            status = srtp_err_status_auth_fail;
        }
    }

    return status;
}

/*
 * Name of this crypto engine
 */
static const char srtp_aes_gcm_128_nss_description[] = "AES-128 GCM using NSS";
static const char srtp_aes_gcm_256_nss_description[] = "AES-256 GCM using NSS";

/*
 * This is the vector function table for this crypto engine.
 */
/* clang-format off */
const srtp_cipher_type_t srtp_aes_gcm_128 = {
    srtp_aes_gcm_nss_alloc,
    srtp_aes_gcm_nss_dealloc,
    srtp_aes_gcm_nss_context_init,
    srtp_aes_gcm_nss_set_aad,
    srtp_aes_gcm_nss_encrypt,
    srtp_aes_gcm_nss_decrypt,
    srtp_aes_gcm_nss_set_iv,
    srtp_aes_gcm_nss_get_tag,
    srtp_aes_gcm_128_nss_description,
    &srtp_aes_gcm_128_test_case_0,
    SRTP_AES_GCM_128
};
/* clang-format on */

/*
 * This is the vector function table for this crypto engine.
 */
/* clang-format off */
const srtp_cipher_type_t srtp_aes_gcm_256 = {
    srtp_aes_gcm_nss_alloc,
    srtp_aes_gcm_nss_dealloc,
    srtp_aes_gcm_nss_context_init,
    srtp_aes_gcm_nss_set_aad,
    srtp_aes_gcm_nss_encrypt,
    srtp_aes_gcm_nss_decrypt,
    srtp_aes_gcm_nss_set_iv,
    srtp_aes_gcm_nss_get_tag,
    srtp_aes_gcm_256_nss_description,
    &srtp_aes_gcm_256_test_case_0,
    SRTP_AES_GCM_256
};
/* clang-format on */
