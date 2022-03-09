/*
 * hmac_mbedtls.c
 *
 * Implementation of hmac srtp_auth_type_t that leverages Mbedtls
 *
 * YongCheng Yang
 */
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
#include <mbedtls/md.h>

#define SHA1_DIGEST_SIZE 20

/* the debug module for authentiation */

srtp_debug_module_t srtp_mod_hmac = {
    0,                   /* debugging is off by default */
    "hmac sha-1 mbedtls" /* printable name for module   */
};

static srtp_err_status_t srtp_hmac_mbedtls_alloc(srtp_auth_t **a,
                                                 int key_len,
                                                 int out_len)
{
    extern const srtp_auth_type_t srtp_hmac;

    debug_print(srtp_mod_hmac, "allocating auth func with key length %d",
                key_len);
    debug_print(srtp_mod_hmac, "                          tag length %d",
                out_len);

    /* check output length - should be less than 20 bytes */
    if (key_len > SHA1_DIGEST_SIZE) {
        return srtp_err_status_bad_param;
    }
    /* check output length - should be less than 20 bytes */
    if (out_len > SHA1_DIGEST_SIZE) {
        return srtp_err_status_bad_param;
    }

    *a = (srtp_auth_t *)srtp_crypto_alloc(sizeof(srtp_auth_t));
    if (*a == NULL) {
        return srtp_err_status_alloc_fail;
    }
    // allocate the buffer of mbedtls context.
    (*a)->state = srtp_crypto_alloc(sizeof(mbedtls_md_context_t));
    if ((*a)->state == NULL) {
        srtp_crypto_free(*a);
        *a = NULL;
        return srtp_err_status_alloc_fail;
    }
    mbedtls_md_init((mbedtls_md_context_t *)(*a)->state);

    /* set pointers */
    (*a)->type = &srtp_hmac;
    (*a)->out_len = out_len;
    (*a)->key_len = key_len;
    (*a)->prefix_len = 0;

    return srtp_err_status_ok;
}

static srtp_err_status_t srtp_hmac_mbedtls_dealloc(srtp_auth_t *a)
{
    mbedtls_md_context_t *hmac_ctx;
    hmac_ctx = (mbedtls_md_context_t *)a->state;
    mbedtls_md_free(hmac_ctx);
    srtp_crypto_free(hmac_ctx);
    /* zeroize entire state*/
    octet_string_set_to_zero(a, sizeof(srtp_auth_t));

    /* free memory */
    srtp_crypto_free(a);

    return srtp_err_status_ok;
}

static srtp_err_status_t srtp_hmac_mbedtls_start(void *statev)
{
    mbedtls_md_context_t *state = (mbedtls_md_context_t *)statev;
    if (mbedtls_md_hmac_reset(state) != 0)
        return srtp_err_status_auth_fail;

    return srtp_err_status_ok;
}

static srtp_err_status_t srtp_hmac_mbedtls_init(void *statev,
                                                const uint8_t *key,
                                                int key_len)
{
    mbedtls_md_context_t *state = (mbedtls_md_context_t *)statev;
    const mbedtls_md_info_t *info = NULL;

    info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    if (info == NULL)
        return srtp_err_status_auth_fail;

    if (mbedtls_md_setup(state, info, 1) != 0)
        return srtp_err_status_auth_fail;

    debug_print(srtp_mod_hmac, "mbedtls setup, name: %s",
                mbedtls_md_get_name(info));
    debug_print(srtp_mod_hmac, "mbedtls setup, size: %d",
                mbedtls_md_get_size(info));

    if (mbedtls_md_hmac_starts(state, key, key_len) != 0)
        return srtp_err_status_auth_fail;

    return srtp_err_status_ok;
}

static srtp_err_status_t srtp_hmac_mbedtls_update(void *statev,
                                                  const uint8_t *message,
                                                  int msg_octets)
{
    mbedtls_md_context_t *state = (mbedtls_md_context_t *)statev;

    debug_print(srtp_mod_hmac, "input: %s",
                srtp_octet_string_hex_string(message, msg_octets));

    if (mbedtls_md_hmac_update(state, message, msg_octets) != 0)
        return srtp_err_status_auth_fail;

    return srtp_err_status_ok;
}

static srtp_err_status_t srtp_hmac_mbedtls_compute(void *statev,
                                                   const uint8_t *message,
                                                   int msg_octets,
                                                   int tag_len,
                                                   uint8_t *result)
{
    mbedtls_md_context_t *state = (mbedtls_md_context_t *)statev;
    uint8_t hash_value[SHA1_DIGEST_SIZE];
    int i;

    /* check tag length, return error if we can't provide the value expected */
    if (tag_len > SHA1_DIGEST_SIZE) {
        return srtp_err_status_bad_param;
    }

    /* hash message, copy output into H */
    if (mbedtls_md_hmac_update(statev, message, msg_octets) != 0)
        return srtp_err_status_auth_fail;

    if (mbedtls_md_hmac_finish(state, hash_value) != 0)
        return srtp_err_status_auth_fail;

    /* copy hash_value to *result */
    for (i = 0; i < tag_len; i++) {
        result[i] = hash_value[i];
    }

    debug_print(srtp_mod_hmac, "output: %s",
                srtp_octet_string_hex_string(hash_value, tag_len));

    return srtp_err_status_ok;
}

/* end test case 0 */

static const char srtp_hmac_mbedtls_description[] =
    "hmac sha-1 authentication function using mbedtls";

/*
 * srtp_auth_type_t hmac is the hmac metaobject
 */

const srtp_auth_type_t srtp_hmac = {
    srtp_hmac_mbedtls_alloc,       /* */
    srtp_hmac_mbedtls_dealloc,     /* */
    srtp_hmac_mbedtls_init,        /* */
    srtp_hmac_mbedtls_compute,     /* */
    srtp_hmac_mbedtls_update,      /* */
    srtp_hmac_mbedtls_start,       /* */
    srtp_hmac_mbedtls_description, /* */
    &srtp_hmac_test_case_0,        /* */
    SRTP_HMAC_SHA1                 /* */
};
