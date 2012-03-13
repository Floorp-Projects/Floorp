/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "cpr_types.h"
#include "cpr_rand.h"
#include "sdp.h"
#include "fsm.h"
#include "gsm_sdp.h"
#include "util_string.h"
#include "lsm.h"
#include "sip_interface_regmgr.h"
#include "plat_api.h"

static const char *gsmsdp_crypto_suite_name[SDP_SRTP_MAX_NUM_CRYPTO_SUITES] =
{
    "SDP_SRTP_UNKNOWN_CRYPTO_SUITE",
    "SDP_SRTP_AES_CM_128_HMAC_SHA1_32",
    "SDP_SRTP_AES_CM_128_HMAC_SHA1_80",
    "SDP_SRTP_F8_128_HMAC_SHA1_80"
};

/*
 * Cached random number pool. The size of the pool is for 10 key sets
 * ahead. The number of cached keys are arbitrary picked to be large
 * enough to sustain the number of basic calls.
 */
#define RAND_POOL_SIZE ((VCM_SRTP_MAX_KEY_SIZE+VCM_SRTP_MAX_SALT_SIZE)*10)
#define RAND_REQ_LIMIT 256      /* limit random number request */
static unsigned char rand_pool[RAND_POOL_SIZE]; /* random number pool */
static int rand_pool_bytes = 0; /* current numbers in rand pool */

/*
 * Default key life time that phone supports,
 * should be "2^48" but CCM does not support the value. Leave the
 * life time to blank.
 */
#define GSMSDP_DEFALT_KEY_LIFETIME NULL

/* Default algorithmID */
#define GSMSDP_DEFAULT_ALGORITHM_ID VCM_AES_128_COUNTER

/*
 *  Function: gsmsdp_cache_crypto_keys
 *
 *  Parameters:
 *      N/A
 *
 *  Description:
 *      The function fills the crypto graphically random number pool
 *  so that when SRTP key (and salt) is needed it is first obtained from
 *  cached pool during the call for a better response to the real time
 *  signalling event.
 *
 *  Returns:
 *      N/A
 */
void
gsmsdp_cache_crypto_keys (void)
{
    int number_to_fill;
    int accumulate_bytes;
    int bytes;

    /*
     * Only attempt to fill the cache when the pool needs to be
     * filled and the phone is idle.
     */
    if ((rand_pool_bytes == RAND_POOL_SIZE) || !lsm_is_phone_idle()) {
        return;
    }

    number_to_fill = RAND_POOL_SIZE - rand_pool_bytes;
    accumulate_bytes = 0;

    while (accumulate_bytes < number_to_fill) {
        bytes = number_to_fill - accumulate_bytes;
        /*
         * Limit the request the number bytes for each request to
         * crypto graphic random number generator to prevent
         * error. There is a maximum limit to per request.
         */
        if (bytes > RAND_REQ_LIMIT) {
            bytes = RAND_REQ_LIMIT;
        }

        if (platGenerateCryptoRand(&rand_pool[accumulate_bytes], &bytes)) {
            accumulate_bytes += bytes;
        } else {
            /*
             * Failed to get the crypto random number, uses the
             * cpr_rand(), to keep the call going.
             */
            rand_pool[accumulate_bytes] = (uint8_t) (cpr_rand() & 0xff);
            accumulate_bytes++;
        }
    }
    rand_pool_bytes = RAND_POOL_SIZE;
}

/*
 *  Function: gsmsdp_get_rand_from_cached_pool
 *
 *  Parameters:
 *      dst_buf   - pointer to the unsigned char for the buffer to
 *                  store random number obtained from the cached pool.
 *      req_bytes - number of random number requested.
 *
 *  Description:
 *      The function gets the random number from the cache pool.
 *
 *  Returns:
 *      The number of random number obtained from the pool.
 */
static int
gsmsdp_get_rand_from_cached_pool (unsigned char *dst_buf, int req_bytes)
{
    int bytes;

    if (rand_pool_bytes == 0) {
        /* the pool is empty */
        return 0;
    }

    if (rand_pool_bytes >= req_bytes) {
        /* There are enough bytes from the pool */
        bytes = req_bytes;
    } else {
        /*
         * pool does not have enough random bytes, give whatever
         * remains in the pool
         */
        bytes = rand_pool_bytes;
    }

    memcpy(dst_buf, &rand_pool[RAND_POOL_SIZE - rand_pool_bytes], bytes);
    rand_pool_bytes -= bytes;

    /* Returns the actual number of bytes gotten from the pool */
    return (bytes);
}

/*
 *  Function: gsmsdp_generate_key
 *
 *  Description:
 *      The function generates key and salt for SRTP.
 *
 *  Parameters:
 *      algorithmID - algorithm ID
 *      key         - pointer to vcm_crypto_key_t to store key output.
 *
 *  Returns:
 *      None
 */
static void
gsmsdp_generate_key (uint32_t algorithmID, vcm_crypto_key_t * key)
{
    int     accumulate_len, len, total_bytes, bytes_from_cache;
    uint8_t random[sizeof(key->key) + sizeof(key->salt)];
    uint8_t key_len, salt_len;

    if (algorithmID == VCM_AES_128_COUNTER) {
        key_len  = VCM_AES_128_COUNTER_KEY_SIZE;
        salt_len = VCM_AES_128_COUNTER_SALT_SIZE;
    } else {
        /* Unsupported algorithm */
        key_len  = VCM_SRTP_MAX_KEY_SIZE;
        salt_len = VCM_SRTP_MAX_SALT_SIZE;
    }

    /*
     * Request random bytes one time for both key and salt to avoid
     * the overhead of platGenerateCryptoRand().
     *
     * The size of random byes should be based on the algorithmID
     * used but since at this time only AES128 is used.
     */
    accumulate_len = 0;
    total_bytes    = key_len + salt_len;

    while (accumulate_len < total_bytes) {
        len = total_bytes - accumulate_len;

        /* Attempt to get random number from cached pool first */
        bytes_from_cache =
            gsmsdp_get_rand_from_cached_pool(&random[accumulate_len], len);
        if (bytes_from_cache) {
            /* There are some random number from the cache */
            accumulate_len += bytes_from_cache;
        } else {
            /*
             * The cached random number in the pool is empty.
             *
             * Attempt to get all of the random bytes in one request but
             * if the number of random bytes is not enough then requesting
             * until all bytes are accumulated.
             */
            if (len > RAND_REQ_LIMIT) {
                len = RAND_REQ_LIMIT;
            }
            if (platGenerateCryptoRand(&random[accumulate_len], &len)) {
                accumulate_len += len;
            } else {
                /*
                 * Failed to get the crypto random number, uses the
                 * cpr_rand(), to keep the call going.
                 */
                random[accumulate_len] = (uint8_t) (cpr_rand() & 0xff);
                accumulate_len++;
            }
        }
    }

    /*
     * Filled in key and salt from the random bytes generated.
     */
    key->key_len = key_len;
    memcpy(&key->key[0], &random[0], key->key_len);

    key->salt_len = salt_len;
    memcpy(&key->salt[0], &random[key->key_len], key->salt_len);
}

/*
 *  Function: gsmsdp_is_supported_crypto_suite
 *
 *  Description:
 *      The function checks whether the given crypto suite from SDP
 *      is supported or not.
 *
 *  Parameters:
 *      crypto_suite - sdp_srtp_crypto_suite_t type to be checked
 *
 *  Returns:
 *      TRUE  - the crypto suite is supported.
 *      FALSE - the crypto suite is not supported.
 */
static boolean
gsmsdp_is_supported_crypto_suite (sdp_srtp_crypto_suite_t crypto_suite)
{
    switch (crypto_suite) {
    case SDP_SRTP_AES_CM_128_HMAC_SHA1_32:
        /* These are the supported crypto suites */
        return (TRUE);
    default:
        return (FALSE);
    }
}

/*
 *  Function: gsmsdp_is_valid_keysize
 *
 *  Description:
 *      The function checks whether the given key size is valid.
 *
 *  Parameters:
 *      crypto_suite - sdp_srtp_crypto_suite_t type
 *      key_size     - key size to check
 *
 *  Returns:
 *      TRUE  - the key size is valid
 *      FALSE - the key size is not valid
 */
static boolean
gsmsdp_is_valid_key_size (sdp_srtp_crypto_suite_t crypto_suite,
                          unsigned char key_size)
{
    /*
     * Make sure the size fits the maximum key size currently supported.
     * This to make sure that when a longer key size is added but
     * the maximum key size is not updated.
     */
    if (key_size > VCM_SRTP_MAX_KEY_SIZE) {
        return (FALSE);
    }

    /* Check key size against crypto suite */
    switch (crypto_suite) {
    case SDP_SRTP_AES_CM_128_HMAC_SHA1_32:
        if (key_size == VCM_AES_128_COUNTER_KEY_SIZE) {
            return (TRUE);
        }
        break;

    default:
        break;
    }
    return (FALSE);
}

/*
 *  Function: gsmsdp_is_valid_salt_size
 *
 *  Description:
 *      The function checks whether the given salt size is valid.
 *
 *  Parameters:
 *      crypto_suite - sdp_srtp_crypto_suite_t type
 *      salt_size    - salt size to check
 *
 *  Returns:
 *      TRUE  - the salt size is valid
 *      FALSE - the salt size is not valid
 */
static boolean
gsmsdp_is_valid_salt_size (sdp_srtp_crypto_suite_t crypto_suite,
                           unsigned char salt_size)
{
    /*
     * Make sure the size fits the maximum salt size currently supported.
     * This to make sure that when a longer salt size is added but
     * the maximum salt size is not updated.
     */
    if (salt_size > VCM_SRTP_MAX_SALT_SIZE) {
        return (FALSE);
    }

    /* Check salt size against crypter suite */
    switch (crypto_suite) {
    case SDP_SRTP_AES_CM_128_HMAC_SHA1_32:
        if (salt_size == VCM_AES_128_COUNTER_SALT_SIZE) {
            return (TRUE);
        }
        break;

    default:
        break;
    }
    return (FALSE);
}

/*
 *  Function: gsmdsp_cmp_key
 *
 *  Description:
 *      The function compares 2 keys.
 *
 *  Parameters:
 *      key1 - pointer to vcm_crypto_key_t
 *      key2 - pointer to vcm_crypto_key_t
 *
 *  Returns:
 *      TRUE  - the 2 keys are different
 *      FALSE - the 2 keys are the same
 */
static boolean
gsmdsp_cmp_key (vcm_crypto_key_t *key1, vcm_crypto_key_t *key2)
{
    if ((key1 == NULL) && (key2 != NULL)) {
        /* No key 1 but has key 2 -> different */
        return (TRUE);
    }
    if ((key1 != NULL) && (key2 == NULL)) {
        /* Has key 1 but no key 2 -> different */
        return (TRUE);
    }
    if ((key1 == NULL) && (key2 == NULL)) {
        /* No key 1 and no key 2 -> same */
        return (FALSE);
    }
    /*
     * At this point pointer to key1 and key2 should not be NULL. Compare
     * the key content
     */
    if ((key1->key_len != key2->key_len) || (key1->salt_len != key2->salt_len)) {
        /* Key length or salt length are not the same */
        return (TRUE);
    }
    if (key1->key_len != 0) {
        if (memcmp(key1->key, key2->key, key1->key_len)) {
            return (TRUE);
        }
    }
    if (key1->salt_len != 0) {
        if (memcmp(key1->salt, key2->salt, key1->salt_len)) {
            return (TRUE);
        }
    }
    /* Both keys are the same */
    return (FALSE);
}

/*
 *  Function: gsmsdp_is_supported_session_parm
 *
 *  Description:
 *      The function checks to see whether the session parameters given
 *      is supported or not.
 *
 *  Parameters:
 *      session_parms - pointer to string of session parameters
 *
 *  Returns:
 *      TRUE  - the session parameters are supported
 *      FALSE - the session parameters are not supported
 */
static boolean
gsmsdp_is_supported_session_parm (const char *session_parms)
{
    int         len, wsh;
    const char *parm_ptr;

    if (session_parms == NULL) {
        /* No session parameters, this is acceptable */
        return (TRUE);
    }
    /*
     * Only WSH is allowed even though the phone does not support it.
     * The session param string can only have "WSH=nn" or (size of 6).
     */
    len = strlen(session_parms);
    if (strcmp(session_parms, "WSH=") && (len == 6)) {
        parm_ptr = &session_parms[sizeof("WSH=") - 1]; /* point the wsh value */
        wsh = atoi(parm_ptr);
        /* minimum value of WSH is 64 */
        if (wsh >= 64) {
            return (TRUE);
        }
    }
    /* Other parameters are not supported */
    return (FALSE);
}

/*
 *  Function: gsmsdp_crypto_suite_to_algorithmID
 *
 *  Description:
 *      The function converts the given crypto suite from SDP into
 *      internal enumeration suitable for using with SRTP lib.
 *
 *  Parameters:
 *      crypto_suite - sdp_srtp_crypto_suite_t type to converted into
 *                     internal algorithm ID
 *
 *  Returns:
 *      value VCM algorithmID
 */
static vcm_crypto_algorithmID
gsmsdp_crypto_suite_to_algorithmID (sdp_srtp_crypto_suite_t crypto_suite)
{
    /* Convert the supported crypto suite into the algorithm ID */
    switch (crypto_suite) {
    case SDP_SRTP_AES_CM_128_HMAC_SHA1_32:
        return (VCM_AES_128_COUNTER);
    default:
        return (VCM_INVLID_ALGORITM_ID);
    }
}

/*
 *  Function: gsmsdp_algorithmID_to_crypto_suite
 *
 *  Description:
 *      The function converts the given crypto suite from SDP into
 *      internal enumeration suitable for using with SRTP lib.
 *
 *  Parameters:
 *      algorithmID - algorithm ID to be converted into crypto suite
 *
 *  Returns:
 *      crypto suite that is corresponding to the internal algorithmID
 */
static sdp_srtp_crypto_suite_t
gsmsdp_algorithmID_to_crypto_suite (vcm_crypto_algorithmID algorithmID)
{
    /* Convert the supported crypto suite into the crypto suite */
    switch (algorithmID) {
    case VCM_AES_128_COUNTER:
        return (SDP_SRTP_AES_CM_128_HMAC_SHA1_32);
    default:
        return (SDP_SRTP_UNKNOWN_CRYPTO_SUITE);
    }
}


/*
 *  Function: gsmsdp_crypto_suite_string
 *
 *  Description:
 *      The function converts crypto suite to string name.
 *
 *  Parameters:
 *      crypto_suite - sdp_srtp_crypto_suite_t
 *
 *  Returns:
 *      string constant of the corresponding crypto suite
 */
static const char *
gsmsdp_crypto_suite_string (sdp_srtp_crypto_suite_t crypto_suite)
{
    if (crypto_suite >= SDP_SRTP_MAX_NUM_CRYPTO_SUITES) {
        return (gsmsdp_crypto_suite_name[SDP_SRTP_UNKNOWN_CRYPTO_SUITE]);
    }
    return (gsmsdp_crypto_suite_name[crypto_suite]);
}

/*
 *  Function: gsmsdp_get_key_from_sdp
 *
 *  Description:
 *      The function checks the key from the SDP.
 *
 *  Parameters:
 *      dcb_p    - pointer to the DCB.
 *      sdp_p    - pointer to remote's SDP (void)
 *      level    - the media level of the SDP of the media line.
 *      inst_num - instance number of the crypto attribute.
 *      key_st   - pointer to fsmdef_crypto_key_t for the key to be returned.
 *                 If pointer is NULL, the function acts as key validation.
 *
 *  Returns:
 *      TRUE  - The key is valid.
 *      FALSE - The key is not valid.
 */
static boolean
gsmsdp_get_key_from_sdp (fsmdef_dcb_t *dcb_p, void *sdp_p, uint16_t level,
                         uint16_t inst_num, vcm_crypto_key_t *key_st)
{
    const char    *fname = "gsmsdp_get_key_from_sdp";
    unsigned char  key_size;
    unsigned char  salt_size;
    const char    *key, *salt;
    sdp_srtp_crypto_suite_t crypto_suite;

    /* Get the crypto suite */
    crypto_suite = sdp_attr_get_sdescriptions_crypto_suite(sdp_p,
                                                           level, 0, inst_num);

    /* Get key */
    key_size = sdp_attr_get_sdescriptions_key_size(sdp_p, level, 0, inst_num);
    if (!gsmsdp_is_valid_key_size(crypto_suite, key_size)) {
        GSM_DEBUG_ERROR(GSM_L_C_F_PREFIX
                  "SDP has invalid key size %d at media level %d\n",
                  dcb_p->line, dcb_p->call_id, fname, key_size, level);
        return (FALSE);
    }

    key = sdp_attr_get_sdescriptions_key(sdp_p, level, 0, inst_num);
    if (key == NULL) {
        GSM_DEBUG_ERROR(GSM_L_C_F_PREFIX
                  "SDP has no key at media level %d\n",
                  dcb_p->line, dcb_p->call_id, fname, level);
        return (FALSE);
    }

    /* Get salt */
    salt_size = sdp_attr_get_sdescriptions_salt_size(sdp_p, level, 0, inst_num);
    if (!gsmsdp_is_valid_salt_size(crypto_suite, salt_size)) {
        GSM_DEBUG_ERROR(GSM_L_C_F_PREFIX
                  "SDP has invalid salt size %d at media level %d\n",
                  dcb_p->line, dcb_p->call_id, fname, salt_size, level);
        return (FALSE);
    }
    salt = sdp_attr_get_sdescriptions_salt(sdp_p, level, 0, inst_num);
    if (salt == NULL) {
        GSM_DEBUG_ERROR(GSM_L_C_F_PREFIX
                  "SDP has no salt at media level %d\n",
                  dcb_p->line, dcb_p->call_id, fname, level);
        return (FALSE);
    }

    /*
     * Key and salt have been sanity check including their sizes,
     * copy key and salt if the caller requests for it.
     */
    if (key_st != NULL) {
        /* The caller needs to copy of the key */
        key_st->key_len = key_size;
        memcpy(key_st->key, key, key_size);
        key_st->salt_len = salt_size;
        memcpy(key_st->salt, salt, salt_size);
    }
    return (TRUE);
}

/*
 *  Function: gsmsdp_local_offer_srtp
 *
 *  Description:
 *      The function checks whether the local (phone) SRTP has been offered.
 *
 *  Parameters:
 *      media - pointer to the fsmdef_media_t for the media entry.
 *
 *  Returns:
 *      TRUE  - local has offered SRTP
 *      FALSE - local has not offered SRTP
 */
static boolean
gsmsdp_local_offer_srtp (fsmdef_media_t *media)
{
    /*
     * Use the local tag as an indication whether we have offered,
     * SRTP or not.
     */
    if (media->local_crypto.tag == SDP_INVALID_VALUE) {
        return (FALSE);
    }
    return (TRUE);
}

/*
 *  Function: gsmsdp_clear_local_offer_srtp
 *
 *  Description:
 *      The function clears the flag to mark that the local SRTP has
 *  been offered.
 *
 *  Parameters:
 *      media - pointer to the fsmdef_media_t for the media entry.
 *
 *  Returns:
 *      none
 */
static void
gsmsdp_clear_local_offer_srtp (fsmdef_media_t *media)
{
    /*
     * Use the local tag as an indication whether we have offered,
     * SRTP or not.
     */
    media->local_crypto.tag = SDP_INVALID_VALUE;
}

/*
 *  Function: gsmsdp_check_common_crypto_param
 *
 *  Description:
 *      The function checks common crypto parameters that can be shared
 *  by selecting an offer and checking the answer SDP.
 *
 *  Parameters:
 *      dcb_p     - pointer to the DCB whose local SDP is to be updated
 *      cc_sdp_p  - pointer to cc_sdp_t structure
 *      level     - the media level of the SDP of the media line
 *      inst      - crypto attribute instance number of the
 *                  answer crypto line
 *      offer     - boolean indicates offer if it is set to TRUE
 *
 *  Returns:
 *      TRUE  - when crypto parameters are valid and acceptable
 *      FALSE - when crypto parameters are not valid or not acceptable
 */
static boolean
gsmsdp_check_common_crypto_param (fsmdef_dcb_t *dcb_p, void *sdp_p,
                                  uint16_t level, uint16_t inst, boolean offer)
{
    const char *fname = "gsmsdp_check_common_crypto_param";
    const char *dir_str; /* direction string */
    const char *session_parms;
    const char *mki_value = NULL;
    uint16_t    mki_length = 0;

    if (offer) {
        dir_str = "Offer";      /* the caller is working on an offer SDP */
    } else {
        dir_str = "Answer";     /* the caller is working on an answer SDP */
    }

    /* Validate the key */
    if (!gsmsdp_get_key_from_sdp(dcb_p, sdp_p, level, inst, NULL)) {
        GSM_DEBUG_ERROR(GSM_L_C_F_PREFIX
                  "%s SDP has invalid key at media level %d\n",
                  dcb_p->line, dcb_p->call_id, fname, dir_str, level);
        return (FALSE);
    }

    /* Check MKI, we do not support it */
    if (sdp_attr_get_sdescriptions_mki(sdp_p, level, 0, inst,
                                       &mki_value, &mki_length)
        != SDP_SUCCESS) {
        /* something is wrong with decoding MKI field, do not use it */
        GSM_DEBUG_ERROR(GSM_L_C_F_PREFIX
                  "Fail to obtain MKI from %s SDP at media level %d\n",
                  dcb_p->line, dcb_p->call_id, fname, dir_str, level);
        return (FALSE);
    }
    if (mki_length) {
        /* this crypto line has MKI specified, we do not support it */
        GSM_DEBUG_ERROR(GSM_L_C_F_PREFIX
                  "%s SDP has MKI %d (not supported) at media level %d\n",
                  dcb_p->line, dcb_p->call_id, fname, dir_str, mki_length,
                  level);
        return (FALSE);
    }

    /* Check session parameters */
    session_parms = sdp_attr_get_sdescriptions_session_params(sdp_p,
                                                              level, 0, inst);
    if (!gsmsdp_is_supported_session_parm(session_parms)) {
        /* some unsupported session parameters are found */
        GSM_DEBUG_ERROR(GSM_L_C_F_PREFIX
                  "%s SDP has unsupported session param at media level %d\n",
                  dcb_p->line, dcb_p->call_id, fname, dir_str, level);
        return (FALSE);
    }

    /* This is good and acceptable one */
    return (TRUE);
}

/*
 *  Function: gsmsdp_select_offer_crypto
 *
 *  Description:
 *      Select remote the crypto attributes from the remote offered SDP.
 *
 *  Parameters:
 *      dcb_p       - pointer to the DCB
 *      sdp_p       - pointer to remote's SDP (void)
 *      level       - the media level of the SDP of the media line
 *      crypto_inst - pointer to crypto attribute instance number of the
 *                    selected crypto line
 *
 *  Returns:
 *      TRUE  - when matching crypto parameters are found.
 *      FALSE - when matching crypto parameters are not found.
 */
static boolean
gsmsdp_select_offer_crypto (fsmdef_dcb_t *dcb_p, void *sdp_p, uint16_t level,
                            uint16_t *crypto_inst)
{
    const char  *fname = "gsmsdp_select_offer_crypto";
    uint16_t     num_attrs = 0; /* number of attributes */
    uint16_t     attr;
    int32_t      tag;
    sdp_attr_e   attr_type;
    sdp_result_e rc;
    sdp_srtp_crypto_suite_t crypto_suite;

    /* Find the number of attributes at this level of media line */
    rc = sdp_get_total_attrs(sdp_p, level, 0, &num_attrs);
    if (rc != SDP_SUCCESS) {
        GSM_DEBUG_ERROR(GSM_L_C_F_PREFIX
                  "Failed finding attributes for media level %d\n",
                  dcb_p->line, dcb_p->call_id, fname, level);
        return (FALSE);
    }

    /*
     * Search all crypto attributes to find a valid one that phone
     * can support.
     *
     * The search starts from the first attributes to the higher. The
     * attributes are listed from the most preferred attribute to the
     * least in the SDP.
     */
    for (attr = 1; attr <= num_attrs; attr++) {
        rc = sdp_get_attr_type(sdp_p, level, 0, attr, &attr_type, crypto_inst);
        if ((rc != SDP_SUCCESS) || (attr_type != SDP_ATTR_SDESCRIPTIONS)) {
            /* Can't not get the attribute or it is not sdescription */
            continue;
        }
        /*
         * Found a crypto attribute, try to match the supported crypto suite
         */
        crypto_suite = sdp_attr_get_sdescriptions_crypto_suite(sdp_p,
                                                               level, 0,
                                                               *crypto_inst);
        if (!gsmsdp_is_supported_crypto_suite(crypto_suite)) {
            /* this one is we can not support, look further */
            continue;
        }
        /* get crypto tag */
        tag = sdp_attr_get_sdescriptions_tag(sdp_p, level, 0, *crypto_inst);
        if (tag == SDP_INVALID_VALUE) {
            /* no tag associated with this crypto attribute */
            continue;
        }

        /* Check common crypto parameters */
        if (!gsmsdp_check_common_crypto_param(dcb_p, sdp_p, level,
                                              *crypto_inst, TRUE)) {
            /* Some thing is wrong with the common crypto parameters */
            continue;
        }

        /* Found a good crypto attribute to use */
        return (TRUE);
    }

    GSM_DEBUG_ERROR(GSM_L_C_F_PREFIX
              "Failed finding supported crypto attribute for media level %d\n",
              dcb_p->line, dcb_p->call_id, fname, level);
    return (FALSE);
}

/*
 *  Function: gsmsdp_check_answer_crypto_param
 *
 *  Description:
 *      The function processes the answer crypto attributes.
 *
 *  Parameters:
 *      dcb_p       - pointer to the DCB whose local SDP is to be updated
 *      cc_sdp_p    - pointer to cc_sdp_t structure
 *      media       - pointer to the media for the SDP of the media line
 *      crypto_inst - pointer to crypto attribute instance number of the
 *                    answer crypto line.
 *
 *  Returns:
 *      TRUE  - when matching crypto parameters are found
 *      FALSE - when matching crypto parameters are not found
 */
static boolean
gsmsdp_check_answer_crypto_param (fsmdef_dcb_t *dcb_p, cc_sdp_t * cc_sdp_p,
                                  fsmdef_media_t *media, uint16_t *crypto_inst)
{
    const char     *fname = "gsmsdp_check_answer_crypto_param";
    uint16_t        num_attrs = 0; /* number of attributes */
    uint16_t        attr;
    uint16_t        num_crypto_attr = 0;
    int32_t         dest_crypto_tag, offered_tag;
    sdp_attr_e      attr_type;
    sdp_result_e    rc;
    sdp_srtp_crypto_suite_t crypto_suite;
    vcm_crypto_algorithmID algorithmID;
    void           *dest_sdp = cc_sdp_p->dest_sdp;
    uint16_t        temp_inst, inst = 0;
    uint16_t        level;

    level        = media->level;
    /* Clear the crypto instance */
    *crypto_inst = 0;

    /* Find the number of attributes at this level of media line */
    if (sdp_get_total_attrs(dest_sdp, level, 0, &num_attrs) != SDP_SUCCESS) {
        GSM_DEBUG(DEB_L_C_F_PREFIX
                  "Failed finding attributes for media level %d\n",
                  DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname), level);
        return (FALSE);
    }

    /*
     * Check to make sure that there is only crypto attribute in the
     * answer SDP.
     */
    for (attr = 1, num_crypto_attr = 0; attr <= num_attrs; attr++) {
        rc = sdp_get_attr_type(dest_sdp, level, 0, attr, &attr_type,
                               &temp_inst);
        if ((rc == SDP_SUCCESS) && (attr_type == SDP_ATTR_SDESCRIPTIONS)) {
            num_crypto_attr++;
            inst = temp_inst;
        }
    }
    if (num_crypto_attr != 1) {
        /* The remote answers with zero or more than one crypto attribute */
        GSM_DEBUG_ERROR(GSM_L_C_F_PREFIX
                  "Answer SDP contains invalid number of"
                  " crypto attributes %d for media level %d\n",
                  dcb_p->line, dcb_p->call_id, fname, 
				  num_crypto_attr, level);
        return (FALSE);
    }

    /*
     * Check to make sure that the tag in the answer SDP is one of the
     * offered SDP.
     * Note: At this time of implementation there is only one crypto
     *       line sent out. When more than one cypher suites are supported,
     *       then the answer tag should be used to find the corresponding
     *       local crypto parameter.
     */
    dest_crypto_tag = sdp_attr_get_sdescriptions_tag(dest_sdp, level, 0, inst);

    /*
     * Selecting tag, if we have made an offer and have not received an answer
     * before this one then tag to match is from the offered tag otherwise
     * use the negotiated tag to match. The later case can occur as the
     * following:
     * phone  ---  INVITE+SDP  ---->  CCM
     *       <---  100         -----
     *       <---  183+SDP     -----
     *       <---  200+SDP     -----
     * The first 183+SDP will conclude the offer/answer. The 200+SDP
     * is also an answer which also contains negotiated tag.
     */
    if (gsmsdp_local_offer_srtp(media)) {
        /*
         * We have made an offered and has not received an answer before
         * this one, use the tag from the local crypto.
         */
        offered_tag = media->local_crypto.tag;
        algorithmID = media->local_crypto.algorithmID;
    } else {
        /* offer/answer has complete, use negotiated crypto */
        offered_tag = media->negotiated_crypto.tag;
        algorithmID = media->negotiated_crypto.algorithmID;
    }
    if (dest_crypto_tag != offered_tag) {
        GSM_DEBUG_ERROR(GSM_L_C_F_PREFIX
                  "Answer SDP contains wrong tag %d vs %d"
                  " for the media level %d\n",
                  dcb_p->line, dcb_p->call_id, fname, 
				  dest_crypto_tag, offered_tag, level);
        return (FALSE);
    }

    /* Check make sure that crypto suite is the same one that is offered */
    crypto_suite = sdp_attr_get_sdescriptions_crypto_suite(dest_sdp, level,
                                                           0, inst);
    if (gsmsdp_crypto_suite_to_algorithmID(crypto_suite) != algorithmID) {
        GSM_DEBUG_ERROR(GSM_L_C_F_PREFIX
                  "Answer SDP mismatch crypto suite %s at media level %d\n",
                  dcb_p->line, dcb_p->call_id, fname,
                  gsmsdp_crypto_suite_string(crypto_suite), level);
        return (FALSE);
    }

    /* Check common crypto parameters */
    if (!gsmsdp_check_common_crypto_param(dcb_p, dest_sdp, level,
                                          inst, FALSE)) {
        /* Some thing is wrong with the common crypto parameters */
        return (FALSE);
    }

    *crypto_inst = inst;
    return (TRUE);
}

/*
 *
 *  Function: gsmsdp_negotiate_offer_crypto
 *
 *  Description:
 *      The function handles crypto parameters negotiation for
 *      an offer SDP.
 *
 *  Parameters:
 *     dcb_p       - pointer to the DCB whose local SDP is to be updated
 *     cc_sdp_p    - pointer to cc_sdp_t structure
 *     media       - pointer to fsmdef_media_t where the media transport is.
 *     crypto_inst - pointer to crypto attribute instance number of the
 *                   selected crypto line
 *
 *  Returns:
 *      transport - sdp_transport_e for RTP or SRTP or invalid
 */
static sdp_transport_e
gsmsdp_negotiate_offer_crypto (fsmdef_dcb_t *dcb_p, cc_sdp_t *cc_sdp_p,
                               fsmdef_media_t *media, uint16_t *crypto_inst)
{
    sdp_transport_e remote_transport;
    sdp_transport_e negotiated_transport = SDP_TRANSPORT_INVALID;
    void           *sdp_p = cc_sdp_p->dest_sdp;
    uint16_t       level;

    level = media->level;
    *crypto_inst     = 0;
    remote_transport = sdp_get_media_transport(sdp_p, level);

    /* negotiate media transport */
    switch (remote_transport) {
    case SDP_TRANSPORT_RTPAVP:
        /* Remote offers RTP for media transport, negotiated transport to RTP */
        negotiated_transport = SDP_TRANSPORT_RTPAVP;
        break;

    case SDP_TRANSPORT_RTPSAVP:
        /* Remote offer SRTP for media transport */
        if ((sip_regmgr_get_sec_level(dcb_p->line) == ENCRYPTED) &&
            FSM_CHK_FLAGS(media->flags, FSM_MEDIA_F_SUPPORT_SECURITY)) {
            /* The signalling with this line is encrypted, try to use SRTP */
            if (gsmsdp_select_offer_crypto(dcb_p, sdp_p, level, crypto_inst)) {
                /* Found a suitable crypto line from the remote offer */
                negotiated_transport = SDP_TRANSPORT_RTPSAVP;
            }
        }

        if (negotiated_transport == SDP_TRANSPORT_INVALID) {
            /*
             * Unable to find a suitable crypto or the signaling is
             * not secure. Fall back to RTP if fallback is enabled.
             */
            if (sip_regmgr_srtp_fallback_enabled(dcb_p->line)) {
                negotiated_transport = SDP_TRANSPORT_RTPAVP;
            }
        }
        break;

    default:
        /* Unknown */
        break;
    }
    return (negotiated_transport);
}

/*
 *
 *  Function: gsmsdp_negotiate_answer_crypto
 *
 *  Description:
 *      The function handles crypto parameters negotiation for
 *      an answer SDP.
 *
 *  Parameters:
 *     dcb_p       - pointer to the DCB whose local SDP is to be updated
 *     cc_sdp_p    - pointer to cc_sdp_t structure
 *     media       - pointer to fsmdef_media_t where the media transport is.
 *     crypto_inst - pointer to crypto attribute instance number of the
 *                   answer crypto line
 *
 *  Returns:
 *      transport - sdp_transport_e for RTP or SRTP or invalid
 */
static sdp_transport_e
gsmsdp_negotiate_answer_crypto (fsmdef_dcb_t *dcb_p, cc_sdp_t *cc_sdp_p,
                                fsmdef_media_t *media, uint16_t *crypto_inst)
{
    const char     *fname = "gsmsdp_check_answer_crypto";
    sdp_transport_e remote_transport, local_transport;
    sdp_transport_e negotiated_transport = SDP_TRANSPORT_INVALID;
    uint16_t        level;

    level            = media->level;
    *crypto_inst     = 0;
    remote_transport = sdp_get_media_transport(cc_sdp_p->dest_sdp, level);
    local_transport  = sdp_get_media_transport(cc_sdp_p->src_sdp, level);
    GSM_DEBUG(GSM_F_PREFIX "remote transport %d\n", fname, remote_transport);
    GSM_DEBUG(GSM_F_PREFIX "local transport %d\n", fname, local_transport);

    /* negotiate media transport */
    switch (remote_transport) {
    case SDP_TRANSPORT_RTPAVP:
        /* Remote answer with RTP */
        if (local_transport == SDP_TRANSPORT_RTPSAVP) {
            if (sip_regmgr_srtp_fallback_enabled(dcb_p->line)) {
                /* Fall back allows, falls back to RTP */
                negotiated_transport = SDP_TRANSPORT_RTPAVP;
            }
        } else {
            /* local and remote are using RTP */
            negotiated_transport = SDP_TRANSPORT_RTPAVP;
        }
        break;

    case SDP_TRANSPORT_RTPSAVP:
        GSM_DEBUG(GSM_F_PREFIX "remote SAVP case\n", fname);
        /* Remote answer with SRTP */
        if (local_transport == SDP_TRANSPORT_RTPSAVP) {
        GSM_DEBUG(GSM_F_PREFIX "local SAVP case\n", fname);
            /* Remote and local media transport are using SRTP */
            if (gsmsdp_check_answer_crypto_param(dcb_p, cc_sdp_p, media,
                                                 crypto_inst)) {
                /* Remote's answer crypto parameters are ok */
                negotiated_transport = SDP_TRANSPORT_RTPSAVP;
                GSM_DEBUG(GSM_F_PREFIX "crypto params verified\n", fname);
            }
        } else {
            /* we offered RTP but remote comes back with SRTP, fail */
        }
        break;

    default:
        /* Unknown */
        break;
    }
    GSM_DEBUG(GSM_F_PREFIX "negotiated transport %d\n", fname, negotiated_transport);
    return (negotiated_transport);
}

/*
 *
 *  Function: gsmsdp_negotiate_media_transport
 *
 *  Description:
 *      The function is a wrapper for negotiation RTP or SRTP for offer
 *      or answer SDP. The negotiated crypto attributes instance number
 *      will be returned via pointer crypto_inst.
 *
 *  Parameters:
 *     dcb_p       - pointer to the DCB whose local SDP is to be updated.
 *     cc_sdp_p    - pointer to cc_sdp_t structure that contains remote SDP.
 *     offer       - boolean indicates the remote SDP is an offer SDP.
 *     media       - pointer to fsmdef_media_t where the media transport is.
 *     crypto_inst - pointer to crypto attribute instance number of the
 *                   selected crypto line.
 *
 *  Returns:
 *      transport - sdp_transport_e for RTP or SRTP or invalid
 */
sdp_transport_e
gsmsdp_negotiate_media_transport (fsmdef_dcb_t *dcb_p, cc_sdp_t *cc_sdp_p,
                                  boolean offer, fsmdef_media_t *media,
                                  uint16_t *crypto_inst)
{
    sdp_transport_e transport;

    /* negotiate media transport based on offer or answer from the remote */
    if (offer) {
        transport = gsmsdp_negotiate_offer_crypto(dcb_p, cc_sdp_p, media,
                                                  crypto_inst);
    } else {
        transport = gsmsdp_negotiate_answer_crypto(dcb_p, cc_sdp_p, media,
                                                   crypto_inst);
    }
    return (transport);
}

/*
 *
 *  Function: gsmsdp_add_single_crypto_attr
 *
 *  Description:
 *      The function adds a single crypto attributes to the SDP.
 *
 *  Parameters:
 *     cc_sdp_p     - pointer to SDP
 *     level        - the media level of the SDP where the media transport is
 *     crypto_suite - crypto suite
 *     key          - pointer to SRTP key (fsmdef_crypto_key_t)
 *     lifetime     - pointer to string const for the life time
 *
 *  Returns:
 *      sdp_result_e
 */
static sdp_result_e
gsmsdp_add_single_crypto_attr (void *sdp_p, uint16_t level, int32_t tag,
                               sdp_srtp_crypto_suite_t crypto_suite,
                               vcm_crypto_key_t * key, char *lifetime)
{
    sdp_result_e rc;
    uint16       inst_num;

    if (key == NULL) {
        return (SDP_INVALID_PARAMETER);
    }

    /*
     * Add crypto attributes tag to the  SDP, the tag is using
     * from the remote tag which should have been negotiated.
     */
    rc = sdp_add_new_attr(sdp_p, level, 0, SDP_ATTR_SDESCRIPTIONS, &inst_num);
    if (rc != SDP_SUCCESS) {
        return (rc);
    }

    rc = sdp_attr_set_sdescriptions_tag(sdp_p, level, 0, inst_num, tag);
    if (rc != SDP_SUCCESS) {
        return (rc);
    }

    /* Add crypto suite */
    rc = sdp_attr_set_sdescriptions_crypto_suite(sdp_p, level, 0, inst_num,
                                                 crypto_suite);
    if (rc != SDP_SUCCESS) {
        return (rc);
    }

    /* Add local key */
    rc = sdp_attr_set_sdescriptions_key(sdp_p, level, 0, inst_num,
                                        (char *) key->key);
    if (rc != SDP_SUCCESS) {
        return (rc);
    }

    rc = sdp_attr_set_sdescriptions_key_size(sdp_p, level, 0, inst_num,
                                             key->key_len);
    if (rc != SDP_SUCCESS) {
        return (rc);
    }

    /* Add salt */
    rc = sdp_attr_set_sdescriptions_salt(sdp_p, level, 0, inst_num,
                                         (char *) key->salt);
    if (rc != SDP_SUCCESS) {
        return (rc);
    }

    rc = sdp_attr_set_sdescriptions_salt_size(sdp_p, level, 0, inst_num,
                                              key->salt_len);
    if (rc != SDP_SUCCESS) {
        return (rc);
    }

    if (lifetime != NULL) {
        rc = sdp_attr_set_sdescriptions_lifetime(sdp_p, level, 0, inst_num,
                                                 lifetime);
    }
    return (rc);
}

/*
 *
 *  Function: gsmsdp_add_all_crypto_lines
 *
 *  Parameters:
 *
 *      dcb_p  - pointer to the DCB whose local SDP is to be updated.
 *      sdp_p  - pointer to SDP.
 *      media  - pointer to fsmdef_media_t where the media transport is.
 *
 *  Description:
 *      The function adds all crypto lines to the SDP.
 *
 *  Returns:
 *      N/A.
 */
static void
gsmsdp_add_all_crypto_lines (fsmdef_dcb_t *dcb_p, void *sdp_p, 
                             fsmdef_media_t *media)
{
    const char *fname = "gsmsdp_add_all_crypto_lines";
    sdp_srtp_crypto_suite_t crypto_suite;

    /*
     * Add all crypto lines, currently there is only one crypto
     * line to add.
     */
    media->local_crypto.tag = 1;
    media->local_crypto.algorithmID = GSMSDP_DEFAULT_ALGORITHM_ID;

    /* Generate key */
    gsmsdp_generate_key(media->local_crypto.algorithmID,
                        &media->local_crypto.key);

    /* Get the crypto suite based on the algorithm ID */
    crypto_suite =
        gsmsdp_algorithmID_to_crypto_suite(media->local_crypto.algorithmID);
    if (gsmsdp_add_single_crypto_attr(sdp_p, media->level, 
            media->local_crypto.tag, crypto_suite, &media->local_crypto.key, 
            GSMSDP_DEFALT_KEY_LIFETIME) != SDP_SUCCESS) {
        GSM_DEBUG_ERROR(GSM_L_C_F_PREFIX
                  "Failed to add crypto attributes\n",
                  dcb_p->line, dcb_p->call_id, fname);
    }
}

/*
 *  Function: gsmsdp_init_crypto_context
 *
 *  Description:
 *      Initializes crypto context.
 *
 *  Parameters:
 *      media - pointer to the fsmdef_media_t for the media entry.
 *
 *  Returns:
 *      none
 */
static void
gsmsdp_init_crypto_context (fsmdef_media_t *media)
{
    /* initialize local crypto parameters */
    media->local_crypto.tag = SDP_INVALID_VALUE;
    media->local_crypto.algorithmID = VCM_NO_ENCRYPTION;
    media->local_crypto.key.key_len  = 0;
    media->local_crypto.key.salt_len = 0;

    /* Initialized negotiated crypto parameter */
    media->negotiated_crypto.tag = SDP_INVALID_VALUE;
    media->negotiated_crypto.algorithmID = VCM_NO_ENCRYPTION;
    media->negotiated_crypto.tx_key.key_len  = 0;
    media->negotiated_crypto.tx_key.salt_len = 0;
    media->negotiated_crypto.rx_key.key_len  = 0;
    media->negotiated_crypto.rx_key.salt_len = 0;

    media->negotiated_crypto.flags = 0;
}

/*
 *  Function: gsmsdp_init_sdp_media_transport
 *
 *  Description:
 *      The prepares or initializes crypto context for a fresh negotiation.
 *
 *  Parameters:
 *      dcb_p    - pointer to the fsmdef_dcb_t
 *      sdp      - pointer to SDP (void)
 *      media    - pointer to the fsmdef_media_t for the media entry.
 *
 *  Returns:
 *      N/A
 */
void
gsmsdp_init_sdp_media_transport (fsmdef_dcb_t *dcb_p, void *sdp_p,
                                 fsmdef_media_t *media)
{
    /* Initialize crypto context */
    gsmsdp_init_crypto_context(media);

    if ((sip_regmgr_get_sec_level(dcb_p->line) != ENCRYPTED) ||
        (!FSM_CHK_FLAGS(media->flags, FSM_MEDIA_F_SUPPORT_SECURITY))) {
        /*
         * The signaling is not encrypted or this media can not support
         * security. 
         */
        media->transport = SDP_TRANSPORT_RTPAVP;
        return; 
    }

    media->transport = SDP_TRANSPORT_RTPSAVP;
}

/*
 *  Function: gsmsdp_reset_sdp_media_transport
 *
 *  Description:
 *      The function resets media transport during mid call such as resume.
 *      The media transport may change from the current negotiated media.
 *
 *  Parameters:
 *      dcb_p    - pointer to the fsmdef_dcb_t
 *      sdp      - pointer to SDP (void)
 *      media    - pointer to fsmdef_media_t where the media transport is.
 *      hold     - indicates call is being hold
 *
 *  Returns:
 *      N/A.
 */
void
gsmsdp_reset_sdp_media_transport (fsmdef_dcb_t *dcb_p, void *sdp_p,
                                  fsmdef_media_t *media, boolean hold)
{
    if (hold) {
        /* We are sending out hold, uses the same transport as negotiated */
    } else {
        /* Resuming a call */
        if ((sip_regmgr_get_cc_mode(dcb_p->line) == REG_MODE_CCM)) {
            /*
             * In CCM mode, try to resume with full SRTP offer again
             * if the current signalling is encrypted. In CCM mode,
             * CCM will assist in crypto negotiation and fall back to
             * RTP if necessary. This is useful when resuming a different
             * end point and the new end point support SRTP even when the
             * current media is not SRTP. For an example, when holding
             * a call that was transferred to a new end point which
             * it may be able to support SRTP.
             */
            gsmsdp_init_sdp_media_transport(dcb_p,
                                            dcb_p->sdp ? dcb_p->sdp->src_sdp : NULL,
                                            media);
        } else {
            /*
             * In non CCM mode, we do not know whether the new end
             * point supports SRTP fall back and no assistance from
             * CCM in negotiating of the crypto parameter, play safe by
             * resuming with the same media transport.
             */
        }
    }
}

/*
 *  Function: gsmsdp_set_media_transport_for_option
 *
 *  Parameters:
 *      sdp   - pointer to SDP (void).
 *      level - the media level of the SDP where the media transport is.
 *
 *  Description:
 *      The function sets media transport for OPTION message.
 *
 *  Returns:
 *      N/A.
 */
void
gsmsdp_set_media_transport_for_option (void *sdp_p, uint16_t level)
{
    const char      *fname = "gsmsdp_set_media_transport_for_option";
    uint32_t         algorithmID;
    vcm_crypto_key_t key;
    sdp_srtp_crypto_suite_t crypto_suite;


    /* Use line 1 to determine whether to have RTP or SRTP */
    if (sip_regmgr_get_sec_level(1) != ENCRYPTED) {
        /* line one is none secure, use RTP */
        (void) sdp_set_media_transport(sdp_p, level, SDP_TRANSPORT_RTPAVP);
        return;
    }

    /* Advertise SRTP with default attributes */
    (void) sdp_set_media_transport(sdp_p, level, SDP_TRANSPORT_RTPSAVP);

    /* Generate dummy key */
    crypto_suite = SDP_SRTP_AES_CM_128_HMAC_SHA1_32;
    algorithmID = gsmsdp_crypto_suite_to_algorithmID(crypto_suite);
    gsmsdp_generate_key(algorithmID, &key);

    /* Add crypto attributes */
    if (gsmsdp_add_single_crypto_attr(sdp_p, level, 1, crypto_suite, &key,
            GSMSDP_DEFALT_KEY_LIFETIME) != SDP_SUCCESS) {
        GSM_DEBUG_ERROR(GSM_F_PREFIX
                  "Failed to add crypto attributes\n",
                  fname);
    }
}

/*
 *  Function: gsmsdp_update_local_sdp_media_transport
 *
 *  Description:
 *      The function updates the crypto attributes to the local SDP.
 *
 *  Parameters:
 *      dcb_p - pointer to the fsmdef_dcb_t
 *      sdp   - pointer to SDP (void)
 *      media - pointer to fsmdef_media_t where the media transport is.
 *      sdp_transport_e - current media transport
 *      all   - indicates that the update for new offer SDP
 *
 *  Returns:
 *      none
 */
void
gsmsdp_update_local_sdp_media_transport (fsmdef_dcb_t *dcb_p, void *sdp_p,
                                         fsmdef_media_t *media,
                                         sdp_transport_e transport, boolean all)
{
    const char *fname = "gsmsdp_update_local_sdp_media_transport";
    sdp_srtp_crypto_suite_t crypto_suite;
    uint16_t level;            

    level = media->level;
    /* Get the current transport before delete the media line */
    if (transport == SDP_TRANSPORT_INVALID) {
        /*
         * The transport is not specified, get it from the negotiated
         * transport. This condition can occur when receive initial
         * offered SDP.
         */
        transport = media->transport;
    }

    /*
     * set media transport in local SDP only first time so that we remember what we offered.
     */
    if (sdp_get_media_transport(sdp_p, level) == SDP_TRANSPORT_INVALID) {
        (void) sdp_set_media_transport(sdp_p, level, transport);
    }

    if (transport != SDP_TRANSPORT_RTPSAVP) {
        /*
         * transport is not SRTP, if this is the fall back to RTP then
         * the existing crypto lines should have been removed by the
         * media transport negotiation and thus no code is added here.
         * The reason for this is not to call to the remove all crypto
         * line all the time i.e. only removed them when related to
         * SRTP media transport.
         */
        return;
    }

    /* Add crypto attributes to local SDP. */
    if (all || (media->negotiated_crypto.tag == SDP_INVALID_VALUE)) {
        /* This is new offer or mid call media change occurs ,
         * or could be the result of sending full offer during mid-call invite
         * without SDP (delayed media invite) as well
         */
        if (media->negotiated_crypto.tag == SDP_INVALID_VALUE) {
            /*
             * This is the first time or fresh offering add all crypto
             * lines.
             */
            gsmsdp_add_all_crypto_lines(dcb_p, sdp_p, media);
            return;
        } else {
            /* Fall through below and use existing crypto parameters */
        }
    }

    /*
     * tag and algorithm ID are from the negotiated crypto parameters.
     */
    crypto_suite =
        gsmsdp_algorithmID_to_crypto_suite(
            media->negotiated_crypto.algorithmID);
    if (gsmsdp_add_single_crypto_attr(sdp_p, level,
                                      media->negotiated_crypto.tag,
                                      crypto_suite,
                                      &media->negotiated_crypto.tx_key,
                                      GSMSDP_DEFALT_KEY_LIFETIME)
            != SDP_SUCCESS) {
        GSM_DEBUG_ERROR(GSM_L_C_F_PREFIX
                  "Failed to add crypto attributes\n",
                  dcb_p->line, dcb_p->call_id, fname);
    }
}

/*
 *  Function: gsmsdp_update_crypto_transmit_key
 *
 *  Description:
 *      The function updates transmit key for SRTP session.
 *
 *  Parameters:
 *      dcb_p         - pointer to the fsmdef_dcb_t
 *      media         - pointer to fsmdef_media_t where the media transport is.
 *      offer         - boolean indicates it is an offer
 *      initial_offer - boolean indicates it is an initial offer
 *      direction     - new offered media direction
 *
 *  Returns:
 *      none
 */
void
gsmsdp_update_crypto_transmit_key (fsmdef_dcb_t *dcb_p,
                                   fsmdef_media_t *media,
                                   boolean offer,
                                   boolean initial_offer,
                                   sdp_direction_e direction)
{
    const char *fname = "gsmsdp_update_crypto_transmit_key";
    boolean     generate_key = FALSE;

    if (media->transport != SDP_TRANSPORT_RTPSAVP) {
        return;
    }

    if (initial_offer || offer) {
        /* This is an offer SDP, see if a new key needs to be generated */
        if (initial_offer) {
            /* An initial offer always needs new key */
            generate_key = TRUE;
        } else if ((util_compare_ip(&(media->previous_sdp.dest_addr), 
                                &(media->dest_addr)) == FALSE) &&
                   media->dest_addr.type != CPR_IP_ADDR_INVALID) {
            //Todo IPv6: IPv6 does not support 0.0.0.0 hold.

            GSM_DEBUG(DEB_L_C_F_PREFIX
                      "Received offer with dest. address changes\n",
                      DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname));
            /*
             * This is just an offer and the destination address changes to
             * is not 0.0.0.0 (hold) address, new key may be needed. There
             * is a scenario where we get a delay media INVITE during the
             * resume call (from the CCM for an example) and we have sent
             * out 200 OK (offer SDP) with crypto parameters. Upon receiving
             * ACK with SDP, the SIP stack in this notify GSM as a
             * FEATURE MEDIA to GSM.  GSM treats this as an offer SDP.
             * This code path is entered with offer flag set where it should
             * be an answer to our 200 OK offer early on. Check  for this
             * condition to see if we have an offer sent prior and if so
             * do not generate new key if we already sent an offer out
             * and has not get and answer SDP. Otherwise we will be generating
             * a new key and do not sent out the key to the remote end which
             * result in wrong local transmit key is being used.
             */
            if (gsmsdp_local_offer_srtp(media)) {
                /*
                 * We have sent out an offer but has not got an answer,
                 * use the already generated key
                 */
                media->negotiated_crypto.tx_key = media->local_crypto.key;
                GSM_DEBUG(DEB_L_C_F_PREFIX
                          "Local offered SDP has been sent, use offered key\n",
                          DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname));
            } else {
                generate_key = TRUE;
            }
        } else if ((media->direction == SDP_DIRECTION_INACTIVE) &&
                   (direction != SDP_DIRECTION_INACTIVE)) {
            if (!gsmsdp_local_offer_srtp(media)) {
                /*
                 * Received an offer that changes the direction from
                 * inactive to some thing other than inactive, then
                 * generate a new key. The phone could be transferred
                 * to a new endpoint without destination address changes.
                 * This is possible if there is a passthrough MTP in
                 * between. The remote end point may be changed
                 * behind MTP but from this phone the destination addr.
                 * stays the same.
                 */
                generate_key = TRUE;
                GSM_DEBUG(DEB_L_C_F_PREFIX
                          "Received direction changes from inactive\n",
                          DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname));
            }
        } else if (media->negotiated_crypto.tx_key.key_len == 0) {
            if (gsmsdp_local_offer_srtp(media)) {
                /*
                 * This an answer to our offer sent similar to 
                 * the address change scenario above (delayed media, we 
                 * sent SDP in 200OK and got SDP in ACK but GSM treats
                 * SDP in ACK case as an offer rather than an answer).
                 * Use the key in the offered SDP.
                 */
                media->negotiated_crypto.tx_key = media->local_crypto.key;
                GSM_DEBUG(DEB_L_C_F_PREFIX
                          "Local offered SDP has been sent, use offered key\n",
                          DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname));
            } else {
                /*
                 * Do not have negotiated transmit key.
                 *
                 * The scenario that this can occur is when the
                 * call transition from RTP to SRTP in mid-call.
                 */
                generate_key = TRUE;
                GSM_DEBUG(DEB_L_C_F_PREFIX
                          "Received offer but no tx key, generate new key\n",
                          DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname));
            }
        } else {
            /* No need to generate new key */
        }
        if (generate_key) {
            /*
             * This is an initial offer or mid-call offer and
             * some conditions changes , generate a new transmit key.
             */
            gsmsdp_generate_key(media->negotiated_crypto.algorithmID,
                                &media->negotiated_crypto.tx_key);
            GSM_DEBUG(DEB_L_C_F_PREFIX
                      "Generate tx key\n",
                      DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname));
            media->negotiated_crypto.flags |= FSMDEF_CRYPTO_TX_CHANGE;
        }
    } else {
        /* This is an answer to our offer, set the tx key to the local key
         *
         * Note that when adding support to offer multiple crypto suite to
         * the remote end, we need to select the corresponding local offer
         * SDP from the matching tag. At this point, we only support one
         * crypto to offer and just get to that entry
         */
        if (gsmdsp_cmp_key(&media->local_crypto.key,
                           &media->negotiated_crypto.tx_key)) {
            media->negotiated_crypto.flags |= FSMDEF_CRYPTO_TX_CHANGE;
            GSM_DEBUG(DEB_L_C_F_PREFIX
                      "tx key changes in answered SDP\n",
                      DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname));
        }
        media->negotiated_crypto.tx_key = media->local_crypto.key; /* tx key */
    }
    /* Complete offer/answer, clear condition that we have made an offered */
    gsmsdp_clear_local_offer_srtp(media);
}

/*
 *  Function: gsmsdp_update_negotiated_transport
 *
 *  Description:
 *      The function updates the transport and crypto parameters after the
 *      all negotiated parameters has been done.
 *
 *  Parameters:
 *      dcb_p - pointer to the fsmdef_dcb_t
 *      sdp   - pointer to cc_sdp_t structure
 *      level - pointer to fsmdef_media_t where the media transport is.
 *      crypto_inst     - pointer to crypto attribute instance number of the
 *                        selected crypto line
 *      sdp_transport_e - negotiated media transport
 *
 *  Returns:
 *      none
 */
void
gsmsdp_update_negotiated_transport (fsmdef_dcb_t *dcb_p,
                                    cc_sdp_t *cc_sdp_p,
                                    fsmdef_media_t *media,
                                    uint16_t crypto_inst,
                                    sdp_transport_e transport)
{
    const char *fname = "gsmsdp_update_negotiated_transport";
    sdp_srtp_crypto_suite_t crypto_suite;
    void                   *dest_sdp = cc_sdp_p->dest_sdp;
    vcm_crypto_algorithmID  algorithmID;
    vcm_crypto_key_t        key;
    uint16_t                level;

    level = media->level;
    /*
     * Also detect changes of the crypto parameters for Tx and Rx.
     * It is done here to avoid adding last crypto parameters for Tx and
     * Rx into the dcb structure since the parameters include key which
     * are rather long. This minimize the dcb's increasing.
     */
    /* reset changes flags */
    media->negotiated_crypto.flags &= ~(FSMDEF_CRYPTO_TX_CHANGE |
                                        FSMDEF_CRYPTO_RX_CHANGE);
    /*
     * Detect the transport change between RTP to SRTP or ignore
     * the first time transition from invalid transport. This
     * prevents the change bits to be set for first time in RTP
     * connection and causes the RX to be closed unnecessary.
     */
    if ((media->transport != SDP_TRANSPORT_INVALID) &&
        (transport != media->transport)) {
        /* We could fallback to RTP or resume from RTP to SRTP */
        media->negotiated_crypto.flags |= (FSMDEF_CRYPTO_TX_CHANGE |
                                           FSMDEF_CRYPTO_RX_CHANGE);
        GSM_DEBUG(DEB_L_C_F_PREFIX
                  "SDP media transport changed to %d\n",
                  DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname), transport);
    }
    /* Update the negotiated media transport */
    media->transport = transport;

    if (media->transport != SDP_TRANSPORT_RTPSAVP) {
        /*
         * negotiate media transport to RTP, clear local offer SDP
         * condition.
         */
        GSM_DEBUG(DEB_L_C_F_PREFIX
                  "SDP media transport is RTP\n",
                  DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname));
        return;
    }

    GSM_DEBUG(DEB_L_C_F_PREFIX "SDP media transport is SRTP\n",
              DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname));

    /*
     * Get the crypto parameters from the remote's SDP,
     * the parameters should already be validated during the
     * media transport negotiation.  Update the negotiated crypto parameter.
     */
    crypto_suite = sdp_attr_get_sdescriptions_crypto_suite(dest_sdp, level,
                                                           0, crypto_inst);

    /* Save the negotiated algorithm ID */
    algorithmID = gsmsdp_crypto_suite_to_algorithmID(crypto_suite);
    if (algorithmID != media->negotiated_crypto.algorithmID) {
        media->negotiated_crypto.flags |= (FSMDEF_CRYPTO_TX_CHANGE |
                                           FSMDEF_CRYPTO_RX_CHANGE);
        GSM_DEBUG(DEB_L_C_F_PREFIX "SDP algorithm ID change to %d\n",
                  DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname), algorithmID);
    }
    media->negotiated_crypto.algorithmID = algorithmID;

    /* Save the negotiated crypto line tag */
    media->negotiated_crypto.tag = sdp_attr_get_sdescriptions_tag(dest_sdp,
                                                                  level, 0,
                                                                  crypto_inst);

    /* Get the remote's key */
    (void) gsmsdp_get_key_from_sdp(dcb_p, dest_sdp, level, crypto_inst, &key);
    if (gsmdsp_cmp_key(&key, &media->negotiated_crypto.rx_key)) {
        media->negotiated_crypto.flags |= FSMDEF_CRYPTO_RX_CHANGE;
        GSM_DEBUG(DEB_L_C_F_PREFIX "SDP rx key changes\n",
                  DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname));
    }
    media->negotiated_crypto.rx_key = key;
}

/*
 *  Function: gsmsdp_is_crypto_ready
 *
 *  Parameters:
 *      media - pointer to fsmdef_media_t where the media transport is.
 *      rx    - boolean indicates receiver.
 *
 *  Description:
 *      The function returns to the caller whether receiver/transmitter is
 * ready for open.
 *
 *  Returns:
 *      TRUE  - when the receiver is ready to received.
 *      FALSE - when the receive is not ready.
 */
boolean
gsmsdp_is_crypto_ready (fsmdef_media_t *media, boolean rx)
{
    /*
     * If we offered SRTP then we need to wait for the negotiated key before
     * allowing the Rx/Tx to be opened.  If we did not offer (we received
     * an offered SDP instead then we should have key negotiated).
     */
    if (media->transport == SDP_TRANSPORT_RTPAVP) {
        /*
         * The local did not offer SRTP.
         */
        return (TRUE);
    }

    /*
     * Local has offered SRTP, check to see if the key is available.
     */
    if (rx) {
        if (media->negotiated_crypto.rx_key.key_len == 0) {
            /* Have not received remote's crypto parameter yet, can't open Rx */
            return (FALSE);
        }
    } else {
        if (media->negotiated_crypto.tx_key.key_len == 0) {
            /* Have not received remote's crypto parameter yet, can't open Tx */
            return (FALSE);
        }
    }
    /* Have remote's key */
    return (TRUE);
}

/*
 *  Function: gsmsdp_is_media_encrypted
 *
 *  Description:
 *      The function returns to the caller whether the media is
 *      encrypted or not.
 *
 *  Parameters:
 *      dcb_p - pointer to the fsmdef_dcb_t.
 *
 *  Returns:
 *      TRUE  - when the media is encrypted.
 *      FALSE - when the media is not encrypted.
 */
boolean
gsmsdp_is_media_encrypted (fsmdef_dcb_t *dcb_p)
{
    fsmdef_media_t *media;
    uint8_t num_encrypted;

    if (dcb_p == NULL) {
        return(FALSE);
    }
    num_encrypted = 0;
    GSMSDP_FOR_ALL_MEDIA(media, dcb_p) {
        if (!GSMSDP_MEDIA_ENABLED(media)) {
            continue;
        }
        
        if (media->transport == SDP_TRANSPORT_RTPSAVP) {
            num_encrypted++;
        }
    }

    if ((num_encrypted == 0) ||
        (num_encrypted != GSMSDP_MEDIA_COUNT(dcb_p))) {
        /*
         * the call does not have any media that is encrypted or
         * there are some medias that are not encrypted. This is
         * considered as non secure leg.
         */
        return (FALSE);
    }
    return (TRUE);
}

/*
 *  Function: gsmsdp_crypto_params_change
 *
 *  Description:
 *      The function returns to the caller whether the crypto parameters
 *      change from the previous SDP or not.
 *
 *  Parameters:
 *      rcv_only - If TRUE, check for receive port perspective.
 *      media    - pointer to fsmdef_media_t where the media transport is.
 *
 *  Returns:
 *      TRUE  - when crypto parameters changed
 *      FALSE - when crypto parameters did not change
 */
boolean
gsmsdp_crypto_params_change (boolean rcv_only, fsmdef_media_t *media)
{
    if (rcv_only) {
        if (media->negotiated_crypto.flags & FSMDEF_CRYPTO_RX_CHANGE) {
            return (TRUE);
        }
    } else {
        if (media->negotiated_crypto.flags & FSMDEF_CRYPTO_TX_CHANGE) {
            return (TRUE);
        }
    }
    return (FALSE);
}

/**
 * The function resets crypto parameters change status.
 *
 * @param[in] media - pointer to fsmdef_media_t.
 *
 * @return            None.
 *  
 * @pre               (media not_eq NULL)
 */
void
gsmsdp_crypto_reset_params_change (fsmdef_media_t *media)
{
    media->negotiated_crypto.flags &= ~(FSMDEF_CRYPTO_RX_CHANGE | 
                                        FSMDEF_CRYPTO_TX_CHANGE);
}
