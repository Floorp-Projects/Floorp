/*
 *
 * Copyright(c) 2013-2017, Cisco Systems, Inc.
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

#include "auth.h"
#include "alloc.h"
#include "err.h" /* for srtp_debug */
#include "auth_test_cases.h"

#define NSS_PKCS11_2_0_COMPAT 1

#include <nss.h>
#include <pk11pub.h>

#define SHA1_DIGEST_SIZE 20

/* the debug module for authentiation */

srtp_debug_module_t srtp_mod_hmac = {
    0,               /* debugging is off by default */
    "hmac sha-1 nss" /* printable name for module   */
};

typedef struct {
    NSSInitContext *nss;
    PK11SymKey *key;
    PK11Context *ctx;
} srtp_hmac_nss_ctx_t;

static srtp_err_status_t srtp_hmac_alloc(srtp_auth_t **a,
                                         int key_len,
                                         int out_len)
{
    extern const srtp_auth_type_t srtp_hmac;
    srtp_hmac_nss_ctx_t *hmac;
    NSSInitContext *nss;

    debug_print(srtp_mod_hmac, "allocating auth func with key length %d",
                key_len);
    debug_print(srtp_mod_hmac, "                          tag length %d",
                out_len);

    /* check output length - should be less than 20 bytes */
    if (out_len > SHA1_DIGEST_SIZE) {
        return srtp_err_status_bad_param;
    }

    /* Initialize NSS equiv of NSS_NoDB_Init(NULL) */
    nss = NSS_InitContext("", "", "", "", NULL,
                          NSS_INIT_READONLY | NSS_INIT_NOCERTDB |
                              NSS_INIT_NOMODDB | NSS_INIT_FORCEOPEN |
                              NSS_INIT_OPTIMIZESPACE);
    if (!nss) {
        return srtp_err_status_auth_fail;
    }

    *a = (srtp_auth_t *)srtp_crypto_alloc(sizeof(srtp_auth_t));
    if (*a == NULL) {
        NSS_ShutdownContext(nss);
        return srtp_err_status_alloc_fail;
    }

    hmac =
        (srtp_hmac_nss_ctx_t *)srtp_crypto_alloc(sizeof(srtp_hmac_nss_ctx_t));
    if (hmac == NULL) {
        NSS_ShutdownContext(nss);
        srtp_crypto_free(*a);
        *a = NULL;
        return srtp_err_status_alloc_fail;
    }

    hmac->nss = nss;
    hmac->key = NULL;
    hmac->ctx = NULL;

    /* set pointers */
    (*a)->state = hmac;
    (*a)->type = &srtp_hmac;
    (*a)->out_len = out_len;
    (*a)->key_len = key_len;
    (*a)->prefix_len = 0;

    return srtp_err_status_ok;
}

static srtp_err_status_t srtp_hmac_dealloc(srtp_auth_t *a)
{
    srtp_hmac_nss_ctx_t *hmac;

    hmac = (srtp_hmac_nss_ctx_t *)a->state;
    if (hmac) {
        /* free any PK11 values that have been created */
        if (hmac->key) {
            PK11_FreeSymKey(hmac->key);
            hmac->key = NULL;
        }

        if (hmac->ctx) {
            PK11_DestroyContext(hmac->ctx, PR_TRUE);
            hmac->ctx = NULL;
        }

        if (hmac->nss) {
            NSS_ShutdownContext(hmac->nss);
            hmac->nss = NULL;
        }

        /* zeroize everything */
        octet_string_set_to_zero(hmac, sizeof(srtp_hmac_nss_ctx_t));
        srtp_crypto_free(hmac);
    }

    /* free memory */
    srtp_crypto_free(a);

    return srtp_err_status_ok;
}

static srtp_err_status_t srtp_hmac_start(void *statev)
{
    srtp_hmac_nss_ctx_t *hmac;
    hmac = (srtp_hmac_nss_ctx_t *)statev;

    if (PK11_DigestBegin(hmac->ctx) != SECSuccess) {
        return srtp_err_status_auth_fail;
    }
    return srtp_err_status_ok;
}

static srtp_err_status_t srtp_hmac_init(void *statev,
                                        const uint8_t *key,
                                        int key_len)
{
    srtp_hmac_nss_ctx_t *hmac;
    hmac = (srtp_hmac_nss_ctx_t *)statev;
    PK11SymKey *sym_key;
    PK11Context *ctx;

    if (hmac->ctx) {
        PK11_DestroyContext(hmac->ctx, PR_TRUE);
        hmac->ctx = NULL;
    }

    if (hmac->key) {
        PK11_FreeSymKey(hmac->key);
        hmac->key = NULL;
    }

    PK11SlotInfo *slot = PK11_GetBestSlot(CKM_SHA_1_HMAC, NULL);
    if (!slot) {
        return srtp_err_status_bad_param;
    }

    SECItem key_item = { siBuffer, (unsigned char *)key, key_len };
    sym_key = PK11_ImportSymKey(slot, CKM_SHA_1_HMAC, PK11_OriginUnwrap,
                                CKA_SIGN, &key_item, NULL);
    PK11_FreeSlot(slot);

    if (!sym_key) {
        return srtp_err_status_auth_fail;
    }

    SECItem param_item = { siBuffer, NULL, 0 };
    ctx = PK11_CreateContextBySymKey(CKM_SHA_1_HMAC, CKA_SIGN, sym_key,
                                     &param_item);
    if (!ctx) {
        PK11_FreeSymKey(sym_key);
        return srtp_err_status_auth_fail;
    }

    hmac->key = sym_key;
    hmac->ctx = ctx;

    return srtp_err_status_ok;
}

static srtp_err_status_t srtp_hmac_update(void *statev,
                                          const uint8_t *message,
                                          int msg_octets)
{
    srtp_hmac_nss_ctx_t *hmac;
    hmac = (srtp_hmac_nss_ctx_t *)statev;

    debug_print(srtp_mod_hmac, "input: %s",
                srtp_octet_string_hex_string(message, msg_octets));

    if (PK11_DigestOp(hmac->ctx, message, msg_octets) != SECSuccess) {
        return srtp_err_status_auth_fail;
    }

    return srtp_err_status_ok;
}

static srtp_err_status_t srtp_hmac_compute(void *statev,
                                           const uint8_t *message,
                                           int msg_octets,
                                           int tag_len,
                                           uint8_t *result)
{
    srtp_hmac_nss_ctx_t *hmac;
    hmac = (srtp_hmac_nss_ctx_t *)statev;
    uint8_t hash_value[SHA1_DIGEST_SIZE];
    int i;
    unsigned int len;

    debug_print(srtp_mod_hmac, "input: %s",
                srtp_octet_string_hex_string(message, msg_octets));

    /* check tag length, return error if we can't provide the value expected */
    if (tag_len > SHA1_DIGEST_SIZE) {
        return srtp_err_status_bad_param;
    }

    if (PK11_DigestOp(hmac->ctx, message, msg_octets) != SECSuccess) {
        return srtp_err_status_auth_fail;
    }

    if (PK11_DigestFinal(hmac->ctx, hash_value, &len, SHA1_DIGEST_SIZE) !=
        SECSuccess) {
        return srtp_err_status_auth_fail;
    }

    if (tag_len < 0 || len < (unsigned int)tag_len)
        return srtp_err_status_auth_fail;

    /* copy hash_value to *result */
    for (i = 0; i < tag_len; i++) {
        result[i] = hash_value[i];
    }

    debug_print(srtp_mod_hmac, "output: %s",
                srtp_octet_string_hex_string(hash_value, tag_len));

    return srtp_err_status_ok;
}

static const char srtp_hmac_description[] =
    "hmac sha-1 authentication function";

/*
 * srtp_auth_type_t hmac is the hmac metaobject
 */

const srtp_auth_type_t srtp_hmac = {
    srtp_hmac_alloc,        /* */
    srtp_hmac_dealloc,      /* */
    srtp_hmac_init,         /* */
    srtp_hmac_compute,      /* */
    srtp_hmac_update,       /* */
    srtp_hmac_start,        /* */
    srtp_hmac_description,  /* */
    &srtp_hmac_test_case_0, /* */
    SRTP_HMAC_SHA1          /* */
};
