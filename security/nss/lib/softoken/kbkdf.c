#include "pkcs11i.h"
#include "blapi.h"
#include "secerr.h"
#include "softoken.h"

/* Overview:
 *
 * This file contains implementations of the three KDFs from NIST SP800-108
 * "Recommendation for Key Derivation Using Pseudorandom Functions":
 *
 *  1. KDF in Counter Mode (section 5.1)
 *  2. KDF in Feedback Mode (section 5.2)
 *  3. KDF in Double-Pipeline Iteration Mode (section 5.3)
 *
 * These KDFs are a form of negotiable building blocks for KDFs: protocol
 * designers can choose various fields, their endianness, and the underlying
 * PRF. These constructs are generic enough to handle creation of arbitrary,
 * (but known ahead of time) length outputs.
 *
 * The families of PRFs described here are used, among other places, in
 * Kerberos and GlobalPlatform's Secure Channel Protocol 03. The PKCS#11 v3.0
 * design for this KDF facilitates a wide range of uses.
 *
 * Implementation Details:
 *
 * We reuse the new sftk_MACCtx for handling the underlying MACing; with a few
 * safe restrictions, we can reuse whatever it gives us to use as a PRF.
 *
 * We implement the core of the KDF in the *Raw(...) version of the function
 * call. The PKCS#11 key handling happens in the non-Raw version. This means
 * we need a single large allocation upfront (large enough to store the entire
 * key stream), but means we can share key parsing logic and enable the
 * creation of data objects.
 */

/* [ section: #define's ] */

#define VALID_CK_BOOL(x) ((x) == CK_TRUE || (x) == CK_FALSE)
#define IS_COUNTER(_mech) ((_mech) == CKM_SP800_108_COUNTER_KDF || (_mech) == CKM_NSS_SP800_108_COUNTER_KDF_DERIVE_DATA)
#define DOES_DERIVE_DATA(_mech) ((_mech) == CKM_NSS_SP800_108_COUNTER_KDF_DERIVE_DATA || (_mech) == CKM_NSS_SP800_108_FEEDBACK_KDF_DERIVE_DATA || (_mech) == CKM_NSS_SP800_108_DOUBLE_PIPELINE_KDF_DERIVE_DATA)

/* [ section: parameter validation ] */

static CK_RV
kbkdf_LoadParameters(CK_MECHANISM_TYPE mech, CK_MECHANISM_PTR pMechanism, CK_SP800_108_KDF_PARAMS_PTR kdf_params, CK_BYTE_PTR *initial_value, CK_ULONG_PTR initial_value_length)
{
    /* This function loads the parameters for the given mechanism into the
     * specified kdf_params, splitting off the IV if present. In PKCS#11 v3.0,
     * CK_SP800_108_FEEDBACK_KDF_PARAMS and CK_SP800_108_KDF_PARAMS have
     * different ordering of internal parameters, which means that it isn't
     * easy to reuse feedback parameters in the same functions as non-feedback
     * parameters. Rather than duplicating the logic, split out the only
     * Feedback-specific data (the IV) into a separate argument and repack it
     * into the passed kdf_params struct instead. */
    PR_ASSERT(pMechanism != NULL && kdf_params != NULL && initial_value != NULL && initial_value_length != NULL);

    CK_SP800_108_KDF_PARAMS_PTR in_params;
    CK_SP800_108_FEEDBACK_KDF_PARAMS_PTR feedback_params;

    if (mech == CKM_SP800_108_FEEDBACK_KDF || mech == CKM_NSS_SP800_108_FEEDBACK_KDF_DERIVE_DATA) {
        if (pMechanism->ulParameterLen != sizeof(CK_SP800_108_FEEDBACK_KDF_PARAMS)) {
            return CKR_MECHANISM_PARAM_INVALID;
        }

        feedback_params = (CK_SP800_108_FEEDBACK_KDF_PARAMS *)pMechanism->pParameter;

        if (feedback_params->pIV == NULL && feedback_params->ulIVLen > 0) {
            return CKR_MECHANISM_PARAM_INVALID;
        }

        kdf_params->prfType = feedback_params->prfType;
        kdf_params->ulNumberOfDataParams = feedback_params->ulNumberOfDataParams;
        kdf_params->pDataParams = feedback_params->pDataParams;
        kdf_params->ulAdditionalDerivedKeys = feedback_params->ulAdditionalDerivedKeys;
        kdf_params->pAdditionalDerivedKeys = feedback_params->pAdditionalDerivedKeys;

        *initial_value = feedback_params->pIV;
        *initial_value_length = feedback_params->ulIVLen;
    } else {
        if (pMechanism->ulParameterLen != sizeof(CK_SP800_108_KDF_PARAMS)) {
            return CKR_MECHANISM_PARAM_INVALID;
        }

        in_params = (CK_SP800_108_KDF_PARAMS *)pMechanism->pParameter;

        (*kdf_params) = *in_params;
    }

    return CKR_OK;
}

static CK_RV
kbkdf_ValidateParameter(CK_MECHANISM_TYPE mech, const CK_PRF_DATA_PARAM *data)
{
    /* This function validates that the passed data parameter (data) conforms
     * to PKCS#11 v3.0's expectations for KDF parameters. This depends both on
     * the type of this parameter (data->type) and on the KDF mechanism (mech)
     * as certain parameters are context dependent (like Iteration Variable).
     */

    /* If the parameter is missing a value when one is expected, then this
     * parameter is invalid. */
    if ((data->pValue == NULL) != (data->ulValueLen == 0)) {
        return CKR_MECHANISM_PARAM_INVALID;
    }

    switch (data->type) {
        case CK_SP800_108_ITERATION_VARIABLE:
        case CK_SP800_108_OPTIONAL_COUNTER: {
            if (data->type == CK_SP800_108_ITERATION_VARIABLE && !IS_COUNTER(mech)) {
                /* In Feedback and Double Pipeline KDFs, PKCS#11 v3.0 connotes the
                 * iteration variable as the chaining value from the previous PRF
                 * invocation. In contrast, counter mode treats this variable as a
                 * COUNTER_FORMAT descriptor. Thus we can skip validation of
                 * iteration variable parameters outside of counter mode. However,
                 * PKCS#11 v3.0 technically mandates that pValue is NULL, so we
                 * still have to validate that. */

                if (data->pValue != NULL) {
                    return CKR_MECHANISM_PARAM_INVALID;
                }

                return CKR_OK;
            }

            /* In counter mode, data->pValue should be a pointer to an instance of
             * CK_SP800_108_COUNTER_FORMAT; validate its length. */
            if (data->ulValueLen != sizeof(CK_SP800_108_COUNTER_FORMAT)) {
                return CKR_MECHANISM_PARAM_INVALID;
            }

            CK_SP800_108_COUNTER_FORMAT_PTR param = (CK_SP800_108_COUNTER_FORMAT_PTR)data->pValue;

            /* Validate the endian parameter. */
            if (!VALID_CK_BOOL(param->bLittleEndian)) {
                return CKR_MECHANISM_PARAM_INVALID;
            }

            /* Due to restrictions by our underlying hashes, we restrict bit
             * widths to actually be byte widths by ensuring they're a multiple
             * of eight. */
            if ((param->ulWidthInBits % 8) != 0) {
                return CKR_MECHANISM_PARAM_INVALID;
            }

            /* Note that section 5.1 denotes the maximum length of the counter
             * to be 32. */
            if (param->ulWidthInBits > 32) {
                return CKR_MECHANISM_PARAM_INVALID;
            }
            break;
        }
        case CK_SP800_108_DKM_LENGTH: {
            /* data->pValue should be a pointer to an instance of
             * CK_SP800_108_DKM_LENGTH_FORMAT; validate its length. */
            if (data->ulValueLen != sizeof(CK_SP800_108_DKM_LENGTH_FORMAT)) {
                return CKR_MECHANISM_PARAM_INVALID;
            }

            CK_SP800_108_DKM_LENGTH_FORMAT_PTR param = (CK_SP800_108_DKM_LENGTH_FORMAT_PTR)data->pValue;

            /* Validate the method parameter. */
            if (param->dkmLengthMethod != CK_SP800_108_DKM_LENGTH_SUM_OF_KEYS &&
                param->dkmLengthMethod != CK_SP800_108_DKM_LENGTH_SUM_OF_SEGMENTS) {
                return CKR_MECHANISM_PARAM_INVALID;
            }

            /* Validate the endian parameter. */
            if (!VALID_CK_BOOL(param->bLittleEndian)) {
                return CKR_MECHANISM_PARAM_INVALID;
            }

            /* Validate the maximum width: we restrict it to being a byte width
             * instead of a bit width due to restrictions by the underlying
             * PRFs. */
            if ((param->ulWidthInBits % 8) != 0) {
                return CKR_MECHANISM_PARAM_INVALID;
            }

            /* Ensure that the width doesn't overflow a 64-bit int. This
             * restriction is arbitrary but since the counters can't exceed
             * 32-bits (and most PRFs output at most 1024 bits), you're unlikely
             * to need all 64-bits of length indicator. */
            if (param->ulWidthInBits > 64) {
                return CKR_MECHANISM_PARAM_INVALID;
            }
            break;
        }
        case CK_SP800_108_BYTE_ARRAY:
            /* There is no additional data to validate for byte arrays; we can
             * only assume the byte array is of the specified size. */
            break;
        default:
            /* Unexpected parameter type. */
            return CKR_MECHANISM_PARAM_INVALID;
    }

    return CKR_OK;
}

static CK_RV
kbkdf_ValidateDerived(CK_DERIVED_KEY_PTR key)
{
    CK_KEY_TYPE keyType = CKK_GENERIC_SECRET;
    PRUint64 keySize = 0;

    /* The pointer to the key handle shouldn't be NULL. If it is, we can't
     * do anything else, so exit early. Every other failure case sets the
     * key->phKey = CK_INVALID_HANDLE, so we can't use `goto failure` here. */
    if (key->phKey == NULL) {
        return CKR_MECHANISM_PARAM_INVALID;
    }

    /* Validate that we have no attributes if and only if pTemplate is NULL.
     * Otherwise, there's an inconsistency somewhere. */
    if ((key->ulAttributeCount == 0) != (key->pTemplate == NULL)) {
        goto failure;
    }

    for (size_t offset = 0; offset < key->ulAttributeCount; offset++) {
        CK_ATTRIBUTE_PTR template = key->pTemplate + offset;

        /* We only look for the CKA_VALUE_LEN and CKA_KEY_TYPE attributes.
         * Everything else we assume we can set on the key if it is passed
         * here. However, if we can't inquire as to a length (and barring
         * that, if we have a key type without a standard length), we're
         * definitely stuck. This mirrors the logic at the top of
         * NSC_DeriveKey(...). */
        if (template->type == CKA_KEY_TYPE) {
            if (template->ulValueLen != sizeof(CK_KEY_TYPE)) {
                goto failure;
            }

            keyType = *(CK_KEY_TYPE *)template->pValue;
        } else if (template->type == CKA_VALUE_LEN) {
            if (template->ulValueLen != sizeof(CK_ULONG)) {
                goto failure;
            }

            keySize = *(CK_ULONG *)template->pValue;
        }
    }

    if (keySize == 0) {
        /* When we lack a keySize, see if we can infer it from the type of the
         * passed key. */
        keySize = sftk_MapKeySize(keyType);
    }

    /* The main piece of information we validate is that we have a length for
     * this key. */
    if (keySize == 0 || keySize >= (1ull << 32ull)) {
        goto failure;
    }

    return CKR_OK;

failure:
    /* PKCS#11 v3.0: If the failure was caused by the content of a specific
     * key's template (ie the template defined by the content of pTemplate),
     * the corresponding phKey value will be set to CK_INVALID_HANDLE to
     * identify the offending template. */
    *(key->phKey) = CK_INVALID_HANDLE;
    return CKR_MECHANISM_PARAM_INVALID;
}

static CK_RV
kbkdf_ValidateParameters(CK_MECHANISM_TYPE mech, const CK_SP800_108_KDF_PARAMS *params, CK_ULONG keySize)
{
    CK_RV ret = CKR_MECHANISM_PARAM_INVALID;
    int param_type_count[5] = { 0, 0, 0, 0, 0 };
    size_t offset = 0;

    /* Start with checking the prfType as a mechanism against a list of
     * PRFs allowed by PKCS#11 v3.0. */
    if (!(/* The following types aren't defined in NSS yet. */
          /* params->prfType != CKM_3DES_CMAC && */
          params->prfType == CKM_AES_CMAC || /* allow */
          /* We allow any HMAC except MD2 and MD5. */
          params->prfType != CKM_MD2_HMAC ||                        /* disallow */
          params->prfType != CKM_MD5_HMAC ||                        /* disallow */
          sftk_HMACMechanismToHash(params->prfType) != HASH_AlgNULL /* Valid HMAC <-> HASH isn't NULL */
          )) {
        return CKR_MECHANISM_PARAM_INVALID;
    }

    /* We can't have a null pDataParams pointer: we always need at least one
     * parameter to succeed. */
    if (params->pDataParams == NULL) {
        return CKR_HOST_MEMORY;
    }

    /* Validate each KDF parameter. */
    for (offset = 0; offset < params->ulNumberOfDataParams; offset++) {
        /* Validate this parameter has acceptable values. */
        ret = kbkdf_ValidateParameter(mech, params->pDataParams + offset);
        if (ret != CKR_OK) {
            return CKR_MECHANISM_PARAM_INVALID;
        }

        /* Count that we have a parameter of this type. The above logic
         * in ValidateParameter MUST validate that type is within the
         * appropriate range. */
        PR_ASSERT(params->pDataParams[offset].type < sizeof(param_type_count) / sizeof(param_type_count[0]));
        param_type_count[params->pDataParams[offset].type] += 1;
    }

    if (IS_COUNTER(mech)) {
        /* We have to have at least one iteration variable parameter. */
        if (param_type_count[CK_SP800_108_ITERATION_VARIABLE] == 0) {
            return CKR_MECHANISM_PARAM_INVALID;
        }

        /* We can't have any optional counters parameters -- these belong in
         * iteration variable parameters instead. */
        if (param_type_count[CK_SP800_108_OPTIONAL_COUNTER] != 0) {
            return CKR_MECHANISM_PARAM_INVALID;
        }
    }

    /* Validate basic assumptions about derived keys:
     *      NULL <-> ulAdditionalDerivedKeys > 0
     */
    if ((params->ulAdditionalDerivedKeys == 0) != (params->pAdditionalDerivedKeys == NULL)) {
        return CKR_MECHANISM_PARAM_INVALID;
    }

    /* Validate each derived key. */
    for (offset = 0; offset < params->ulAdditionalDerivedKeys; offset++) {
        ret = kbkdf_ValidateDerived(params->pAdditionalDerivedKeys + offset);
        if (ret != CKR_OK) {
            return CKR_MECHANISM_PARAM_INVALID;
        }
    }

    /* Validate the length of our primary key. */
    if (keySize == 0 || ((PRUint64)keySize) >= (1ull << 32ull)) {
        return CKR_KEY_SIZE_RANGE;
    }

    return CKR_OK;
}

/* [ section: parameter helpers ] */

static CK_VOID_PTR
kbkdf_FindParameter(const CK_SP800_108_KDF_PARAMS *params, CK_PRF_DATA_TYPE type)
{
    for (size_t offset = 0; offset < params->ulNumberOfDataParams; offset++) {
        if (params->pDataParams[offset].type == type) {
            return params->pDataParams[offset].pValue;
        }
    }

    return NULL;
}

size_t
kbkdf_IncrementBuffer(size_t cur_offset, size_t consumed, size_t prf_length)
{
    return cur_offset + PR_ROUNDUP(consumed, prf_length);
}

CK_ULONG
kbkdf_GetDerivedKeySize(CK_DERIVED_KEY_PTR derived_key)
{
    /* Precondition: kbkdf_ValidateDerived(...) returns CKR_OK for this key,
     * which implies that keySize is defined. */

    CK_KEY_TYPE keyType = CKK_GENERIC_SECRET;
    CK_ULONG keySize = 0;

    for (size_t offset = 0; offset < derived_key->ulAttributeCount; offset++) {
        CK_ATTRIBUTE_PTR template = derived_key->pTemplate + offset;

        /* Find the two attributes we care about. */
        if (template->type == CKA_KEY_TYPE) {
            keyType = *(CK_KEY_TYPE *)template->pValue;
        } else if (template->type == CKA_VALUE_LEN) {
            keySize = *(CK_ULONG *)template->pValue;
        }
    }

    /* Prefer keySize, if we have it. */
    if (keySize > 0) {
        return keySize;
    }

    /* Else, fall back to this mapping. We know kbkdf_ValidateDerived(...)
     * passed, so this should return non-zero. */
    return sftk_MapKeySize(keyType);
}

static CK_RV
kbkdf_CalculateLength(const CK_SP800_108_KDF_PARAMS *params, sftk_MACCtx *ctx, CK_ULONG ret_key_size, PRUint64 *output_bitlen, size_t *buffer_length)
{
    /* Two cases: either we have additional derived keys or we don't. In the
     * case that we don't, the length of the derivation is the size of the
     * single derived key, and that is the length of the PRF buffer. Otherwise,
     * we need to use the proper CK_SP800_108_DKM_LENGTH_METHOD to calculate
     * the length of the output (in bits), with a separate value for the size
     * of the PRF data buffer. This means that, under PKCS#11 with additional
     * derived keys, we lie to the KDF about the _actual_ length of the PRF
     * output.
     *
     * Note that *output_bitlen is the L parameter in NIST SP800-108 and is in
     * bits. However, *buffer_length is in bytes.
     */

    if (params->ulAdditionalDerivedKeys == 0) {
        /* When we have no additional derived keys, we get the keySize from
         * the value passed to one of our KBKDF_* methods. */
        *output_bitlen = ret_key_size;
        *buffer_length = ret_key_size;
    } else {
        /* Offset in the additional derived keys array. */
        size_t offset = 0;

        /* Size of the derived key. */
        CK_ULONG derived_size = 0;

        /* In the below, we place the sum of the keys into *output_bitlen
         * and the size of the buffer (with padding mandated by PKCS#11 v3.0)
         * into *buffer_length. If the method is the segment sum, then we
         * replace *output_bitlen with *buffer_length at the end. This ensures
         * we always get a output buffer large enough to handle all derived
         * keys, and *output_bitlen reflects the correct L value. */

        /* Count the initial derived key. */
        *output_bitlen = ret_key_size;
        *buffer_length = kbkdf_IncrementBuffer(0, ret_key_size, ctx->mac_size);

        /* Handle n - 1 keys. The last key is special. */
        for (; offset < params->ulAdditionalDerivedKeys - 1; offset++) {
            derived_size = kbkdf_GetDerivedKeySize(params->pAdditionalDerivedKeys + offset);

            *output_bitlen += derived_size;
            *buffer_length = kbkdf_IncrementBuffer(*buffer_length, derived_size, ctx->mac_size);
        }

        /* Handle the last key. */
        derived_size = kbkdf_GetDerivedKeySize(params->pAdditionalDerivedKeys + offset);

        *output_bitlen += derived_size;
        *buffer_length = kbkdf_IncrementBuffer(*buffer_length, derived_size, ctx->mac_size);

        /* Pointer to the DKM method parameter. Note that this implicit cast
         * is safe since we've assumed we've been validated by
         * kbkdf_ValidateParameters(...). When kdm_param is NULL, we don't
         * use the output_bitlen parameter. */
        CK_SP800_108_DKM_LENGTH_FORMAT_PTR dkm_param = kbkdf_FindParameter(params, CK_SP800_108_DKM_LENGTH);
        if (dkm_param != NULL) {
            if (dkm_param->dkmLengthMethod == CK_SP800_108_DKM_LENGTH_SUM_OF_SEGMENTS) {
                *output_bitlen = *buffer_length;
            }
        }
    }

    /* Note that keySize is the size in bytes and ctx->mac_size is also
     * the size in bytes. However, output_bitlen needs to be in bits, so
     * multiply by 8 here. */
    *output_bitlen *= 8;

    return CKR_OK;
}

static CK_RV
kbkdf_CalculateIterations(CK_MECHANISM_TYPE mech, const CK_SP800_108_KDF_PARAMS *params, sftk_MACCtx *ctx, size_t buffer_length, PRUint32 *num_iterations)
{
    CK_SP800_108_COUNTER_FORMAT_PTR param_ptr = NULL;
    PRUint64 iteration_count;
    PRUint64 r = 32;

    /* We need to know how many full iterations are required. This is done
     * by rounding up the division of the PRF length into buffer_length.
     * However, we're not guaranteed that the last output is a full PRF
     * invocation, so handle that here. */
    iteration_count = buffer_length + (ctx->mac_size - 1);
    iteration_count = iteration_count / ctx->mac_size;

    /* NIST SP800-108, section 5.1, process step #2:
     *
     *      if n > 2^r - 1, then indicate an error and stop.
     *
     * In non-counter mode KDFs, r is set at 32, leaving behavior
     * under-defined when the optional counter is included but fewer than
     * 32 bits. This implementation assumes r is 32, but if the counter
     * parameter is included, validates it against that. In counter-mode
     * KDFs, this is in the ITERATION_VARIABLE parameter; in feedback- or
     * pipeline-mode KDFs, this is in the COUNTER parameter.
     *
     * This is consistent with the supplied sample CAVP tests; none reuses the
     * same counter value. In some configurations, this could result in
     * duplicated KDF output. We seek to avoid that from happening.
     */
    if (IS_COUNTER(mech)) {
        param_ptr = kbkdf_FindParameter(params, CK_SP800_108_ITERATION_VARIABLE);

        /* Validated by kbkdf_ValidateParameters(...) above. */
        PR_ASSERT(param_ptr != NULL);

        r = ((CK_SP800_108_COUNTER_FORMAT_PTR)param_ptr)->ulWidthInBits;
    } else {
        param_ptr = kbkdf_FindParameter(params, CK_SP800_108_COUNTER);

        /* Not guaranteed to exist, hence the default value of r=32. */
        if (param_ptr != NULL) {
            r = ((CK_SP800_108_COUNTER_FORMAT_PTR)param_ptr)->ulWidthInBits;
        }
    }

    if (iteration_count >= (1ull << r) || r > 32) {
        return CKR_MECHANISM_PARAM_INVALID;
    }

    *num_iterations = (PRUint32)iteration_count;

    return CKR_OK;
}

static CK_RV
kbkdf_AddParameters(CK_MECHANISM_TYPE mech, sftk_MACCtx *ctx, const CK_SP800_108_KDF_PARAMS *params, PRUint32 counter, PRUint64 length, const unsigned char *chaining_prf, size_t chaining_prf_len, CK_PRF_DATA_TYPE exclude)
{
    size_t offset = 0;
    CK_RV ret = CKR_OK;

    for (offset = 0; offset < params->ulNumberOfDataParams; offset++) {
        CK_PRF_DATA_PARAM_PTR param = params->pDataParams + offset;

        if (param->type == exclude) {
            /* Necessary for Double Pipeline mode: when constructing the IV,
             * we skip the  optional counter. */
            continue;
        }

        switch (param->type) {
            case CK_SP800_108_ITERATION_VARIABLE: {
                /* When present in COUNTER mode, this signifies adding the counter
                 * variable to the PRF. Otherwise, it signifies the chaining
                 * value for other KDF modes. */
                if (IS_COUNTER(mech)) {
                    CK_SP800_108_COUNTER_FORMAT_PTR counter_format = (CK_SP800_108_COUNTER_FORMAT_PTR)param->pValue;
                    CK_BYTE buffer[sizeof(PRUint64)];
                    CK_ULONG num_bytes;
                    sftk_EncodeInteger(counter, counter_format->ulWidthInBits, counter_format->bLittleEndian, buffer, &num_bytes);
                    ret = sftk_MAC_Update(ctx, buffer, num_bytes);
                } else {
                    ret = sftk_MAC_Update(ctx, chaining_prf, chaining_prf_len);
                }
                break;
            }
            case CK_SP800_108_COUNTER: {
                /* Only present in the case when not using COUNTER mode. */
                PR_ASSERT(!IS_COUNTER(mech));

                /* We should've already validated that this parameter is of
                 * type COUNTER_FORMAT. */
                CK_SP800_108_COUNTER_FORMAT_PTR counter_format = (CK_SP800_108_COUNTER_FORMAT_PTR)param->pValue;
                CK_BYTE buffer[sizeof(PRUint64)];
                CK_ULONG num_bytes;
                sftk_EncodeInteger(counter, counter_format->ulWidthInBits, counter_format->bLittleEndian, buffer, &num_bytes);
                ret = sftk_MAC_Update(ctx, buffer, num_bytes);
                break;
            }
            case CK_SP800_108_BYTE_ARRAY:
                ret = sftk_MAC_Update(ctx, (CK_BYTE_PTR)param->pValue, param->ulValueLen);
                break;
            case CK_SP800_108_DKM_LENGTH: {
                /* We've already done the hard work of calculating the length in
                 * the kbkdf_CalculateIterations function; we merely need to add
                 * the length to the desired point in the input stream. */
                CK_SP800_108_DKM_LENGTH_FORMAT_PTR length_format = (CK_SP800_108_DKM_LENGTH_FORMAT_PTR)param->pValue;
                CK_BYTE buffer[sizeof(PRUint64)];
                CK_ULONG num_bytes;
                sftk_EncodeInteger(length, length_format->ulWidthInBits, length_format->bLittleEndian, buffer, &num_bytes);
                ret = sftk_MAC_Update(ctx, buffer, num_bytes);
                break;
            }
            default:
                /* This should've been caught by kbkdf_ValidateParameters(...). */
                PR_ASSERT(PR_FALSE);
                return CKR_MECHANISM_PARAM_INVALID;
        }

        if (ret != CKR_OK) {
            return ret;
        }
    }

    return CKR_OK;
}

CK_RV
kbkdf_SaveKey(SFTKObject *key, unsigned char *key_buffer, unsigned int key_len)
{
    return sftk_forceAttribute(key, CKA_VALUE, key_buffer, key_len);
}

CK_RV
kbkdf_CreateKey(CK_MECHANISM_TYPE kdf_mech, CK_SESSION_HANDLE hSession, CK_DERIVED_KEY_PTR derived_key, SFTKObject **ret_key)
{
    /* Largely duplicated from NSC_DeriveKey(...) */
    CK_RV ret = CKR_HOST_MEMORY;
    SFTKObject *key = NULL;
    SFTKSlot *slot = sftk_SlotFromSessionHandle(hSession);
    size_t offset = 0;

    /* Slot should be non-NULL because NSC_DeriveKey(...) has already
     * performed a sftk_SlotFromSessionHandle(...) call on this session
     * handle. However, Coverity incorrectly flagged this (see 1607955). */
    PR_ASSERT(slot != NULL);
    PR_ASSERT(ret_key != NULL);
    PR_ASSERT(derived_key != NULL);
    PR_ASSERT(derived_key->phKey != NULL);

    if (slot == NULL) {
        return CKR_SESSION_HANDLE_INVALID;
    }

    /* Create the new key object for this additional derived key. */
    key = sftk_NewObject(slot);
    if (key == NULL) {
        return CKR_HOST_MEMORY;
    }

    /* Setup the key from the provided template. */
    for (offset = 0; offset < derived_key->ulAttributeCount; offset++) {
        ret = sftk_AddAttributeType(key, sftk_attr_expand(derived_key->pTemplate + offset));
        if (ret != CKR_OK) {
            sftk_FreeObject(key);
            return ret;
        }
    }

    /* When using the CKM_SP800_* series of mechanisms, the result must be a
     * secret key, so its contents can be adequately protected in FIPS mode.
     * However, when using the special CKM_NSS_SP800_*_DERIVE_DATA series, the
     * contents need not be protected, so we set CKO_DATA on these "keys". */
    CK_OBJECT_CLASS classType = CKO_SECRET_KEY;
    if (DOES_DERIVE_DATA(kdf_mech)) {
        classType = CKO_DATA;
    }

    ret = sftk_forceAttribute(key, CKA_CLASS, &classType, sizeof(classType));
    if (ret != CKR_OK) {
        sftk_FreeObject(key);
        return ret;
    }

    *ret_key = key;
    return CKR_OK;
}

CK_RV
kbkdf_FinalizeKey(CK_SESSION_HANDLE hSession, CK_DERIVED_KEY_PTR derived_key, SFTKObject *key)
{
    /* Largely duplicated from NSC_DeriveKey(...) */
    CK_RV ret = CKR_HOST_MEMORY;
    SFTKSession *session = NULL;

    PR_ASSERT(derived_key != NULL && key != NULL);

    SFTKSessionObject *sessionForKey = sftk_narrowToSessionObject(key);
    PR_ASSERT(sessionForKey != NULL);
    sessionForKey->wasDerived = PR_TRUE;

    session = sftk_SessionFromHandle(hSession);

    /* Session should be non-NULL because NSC_DeriveKey(...) has already
     * performed a sftk_SessionFromHandle(...) call on this session handle. */
    PR_ASSERT(session != NULL);

    ret = sftk_handleObject(key, session);
    if (ret != CKR_OK) {
        goto done;
    }

    *(derived_key->phKey) = key->handle;

done:
    /* Guaranteed that key != NULL */
    sftk_FreeObject(key);

    /* Doesn't do anything. */
    if (session) {
        sftk_FreeSession(session);
    }

    return ret;
}

CK_RV
kbkdf_SaveKeys(CK_MECHANISM_TYPE mech, CK_SESSION_HANDLE hSession, CK_SP800_108_KDF_PARAMS_PTR params, unsigned char *output_buffer, size_t buffer_len, size_t prf_length, SFTKObject *ret_key, CK_ULONG ret_key_size)
{
    CK_RV ret;
    size_t key_offset = 0;
    size_t buffer_offset = 0;

    PR_ASSERT(output_buffer != NULL && buffer_len > 0 && ret_key != NULL);

    /* First place key material into the main key. */
    ret = kbkdf_SaveKey(ret_key, output_buffer + buffer_offset, ret_key_size);
    if (ret != CKR_OK) {
        return ret;
    }

    /* Then increment the offset based on PKCS#11 additional key guidelines:
     * no two keys may share the key stream from the same PRF invocation. */
    buffer_offset = kbkdf_IncrementBuffer(buffer_offset, ret_key_size, prf_length);

    if (params->ulAdditionalDerivedKeys > 0) {
        /* Note that the following code is technically incorrect: PKCS#11 v3.0
         * says that _no_ key should be set in the event of failure to derive
         * _any_ key. */
        for (key_offset = 0; key_offset < params->ulAdditionalDerivedKeys; key_offset++) {
            CK_DERIVED_KEY_PTR derived_key = params->pAdditionalDerivedKeys + key_offset;
            SFTKObject *key_obj = NULL;
            size_t key_size = kbkdf_GetDerivedKeySize(derived_key);

            /* Create a new internal key object for this derived key. */
            ret = kbkdf_CreateKey(mech, hSession, derived_key, &key_obj);
            if (ret != CKR_OK) {
                *(derived_key->phKey) = CK_INVALID_HANDLE;
                return ret;
            }

            /* Save the underlying key bytes to the key object. */
            ret = kbkdf_SaveKey(key_obj, output_buffer + buffer_offset, key_size);
            if (ret != CKR_OK) {
                /* When kbkdf_CreateKey(...) exits with an error, it will free
                 * the constructed key object. kbkdf_FinalizeKey(...) also
                 * always frees the key object. In the unlikely event that
                 * kbkdf_SaveKey(...) _does_ fail, we thus need to free it
                 * manually. */
                sftk_FreeObject(key_obj);
                *(derived_key->phKey) = CK_INVALID_HANDLE;
                return ret;
            }

            /* Handle the increment. */
            buffer_offset = kbkdf_IncrementBuffer(buffer_offset, key_size, prf_length);

            /* Finalize this key. */
            ret = kbkdf_FinalizeKey(hSession, derived_key, key_obj);
            if (ret != CKR_OK) {
                *(derived_key->phKey) = CK_INVALID_HANDLE;
                return ret;
            }
        }
    }

    return CKR_OK;
}

/* [ section: KDFs ] */

static CK_RV
kbkdf_CounterRaw(const CK_SP800_108_KDF_PARAMS *params, sftk_MACCtx *ctx, unsigned char *ret_buffer, size_t buffer_length, PRUint64 output_bitlen)
{
    CK_RV ret = CKR_OK;

    /* Counter variable for this KDF instance. */
    PRUint32 counter;

    /* Number of iterations required of this PRF necessary to reach the
     * desired output length. */
    PRUint32 num_iterations;

    /* Offset in ret_buffer that we're at. */
    size_t buffer_offset = 0;

    /* Size of this block, in bytes. Defaults to ctx->mac_size except on
     * the last iteration where it could be a partial block. */
    size_t block_size = ctx->mac_size;

    /* Calculate the number of iterations required based on the size of the
     * output buffer. */
    ret = kbkdf_CalculateIterations(CKM_SP800_108_COUNTER_KDF, params, ctx, buffer_length, &num_iterations);
    if (ret != CKR_OK) {
        return ret;
    }

    /*
     * 5.1 - [ KDF in Counter Mode ]
     *
     * Fixed values:
     *      1. h - the length of the PRF in bits (ctx->mac_size)
     *      2. r - the length of the binary representation of the counter i
     *          (params[k: params[k].type == CK_SP800_108_ITERATION_VARIABLE:].data->ulWidthInBits)
     * Input:
     *      1. K_I - the key for the PRF (base_key)
     *      2. label - a binary data field, usually before the separator. Optional.
     *      3. context - a binary data field, usually after the separator. Optional.
     *      4. L - length of the output in bits (output_bitlen)
     *
     * Process:
     *      1. n := ceil(L / h) (num_iterations)
     *      2. if n > 2^r - 1, then indicate an error and stop
     *      3. result(0) = NULL
     *      4. for i = 1 to n, do
     *          a. K(i) = PRF(K_I, [i]_2 || Label || 0x00 || Context || [L]_2)
     *          b. result(i) := result(i - 1) || K(i).
     *      5. return K_O := the leftmost L bits of result(n).
     */
    for (counter = 1; counter <= num_iterations; counter++) {
        if (counter == num_iterations) {
            block_size = buffer_length - buffer_offset;

            /* Assumption: if we've validated our arguments correctly, this
             * should always be true. */
            PR_ASSERT(block_size <= ctx->mac_size);
        }

        /* Add all parameters required by this instance of the KDF to the
         * input stream of the underlying PRF. */
        ret = kbkdf_AddParameters(CKM_SP800_108_COUNTER_KDF, ctx, params, counter, output_bitlen, NULL, 0 /* chaining_prf output */, 0 /* exclude */);
        if (ret != CKR_OK) {
            return ret;
        }

        /* Finalize this iteration of the PRF. */
        ret = sftk_MAC_Finish(ctx, ret_buffer + buffer_offset, NULL, block_size);
        if (ret != CKR_OK) {
            return ret;
        }

        /* Increment our position in the key material. */
        buffer_offset += block_size;

        if (counter < num_iterations) {
            /* Reset the underlying PRF for the next iteration. Only do this
             * when we have a next iteration since it isn't necessary to do
             * either before the first iteration (MAC is already initialized)
             * or after the last iteration (we won't be called again). */
            ret = sftk_MAC_Reset(ctx);
            if (ret != CKR_OK) {
                return ret;
            }
        }
    }

    return CKR_OK;
}

static CK_RV
kbkdf_FeedbackRaw(const CK_SP800_108_KDF_PARAMS *params, const unsigned char *initial_value, CK_ULONG initial_value_length, sftk_MACCtx *ctx, unsigned char *ret_buffer, size_t buffer_length, PRUint64 output_bitlen)
{
    CK_RV ret = CKR_OK;

    /* Counter variable for this KDF instance. */
    PRUint32 counter;

    /* Number of iterations required of this PRF necessary to reach the
     * desired output length. */
    PRUint32 num_iterations;

    /* Offset in ret_buffer that we're at. */
    size_t buffer_offset = 0;

    /* Size of this block, in bytes. Defaults to ctx->mac_size except on
     * the last iteration where it could be a partial block. */
    size_t block_size = ctx->mac_size;

    /* The last PRF invocation and/or the initial value; used for feedback
     * chaining in this KDF. Note that we have to make it large enough to
     * fit the output of the PRF, but we can delay its actual creation until
     * the first PRF invocation. Until then, point to the IV value. */
    unsigned char *chaining_value = (unsigned char *)initial_value;

    /* Size of the chaining value discussed above. Defaults to the size of
     * the IV value. */
    size_t chaining_length = initial_value_length;

    /* Calculate the number of iterations required based on the size of the
     * output buffer. */
    ret = kbkdf_CalculateIterations(CKM_SP800_108_FEEDBACK_KDF, params, ctx, buffer_length, &num_iterations);
    if (ret != CKR_OK) {
        goto finish;
    }

    /*
     * 5.2 - [ KDF in Feedback Mode ]
     *
     * Fixed values:
     *      1. h - the length of the PRF in bits (ctx->mac_size)
     *      2. r - the length of the binary representation of the counter i
     *          (params[k: params[k].type == CK_SP800_108_OPTIONAL_COUNTER:].data->ulWidthInBits)
     *          Note that it is only specified when the optional counter is requested.
     * Input:
     *      1. K_I - the key for the PRF (base_key)
     *      2. label - a binary data field, usually before the separator. Optional.
     *      3. context - a binary data field, usually after the separator. Optional.
     *      4. IV - a binary data field, initial PRF value. (params->pIV)
     *      5. L - length of the output in bits (output_bitlen)
     *
     * Process:
     *      1. n := ceil(L / h) (num_iterations)
     *      2. if n > 2^32 - 1, then indicate an error and stop
     *      3. result(0) = NULL, K(0) := IV (chaining_value)
     *      4. for i = 1 to n, do
     *          a. K(i) = PRF(K_I, K(i-1) {|| [i]_2} || Label || 0x00 || Context || [L]_2)
     *          b. result(i) := result(i - 1) || K(i).
     *      5. return K_O := the leftmost L bits of result(n).
     */
    for (counter = 1; counter <= num_iterations; counter++) {
        if (counter == num_iterations) {
            block_size = buffer_length - buffer_offset;

            /* Assumption: if we've validated our arguments correctly, this
             * should always be true. */
            PR_ASSERT(block_size <= ctx->mac_size);
        }

        /* Add all parameters required by this instance of the KDF to the
         * input stream of the underlying PRF. */
        ret = kbkdf_AddParameters(CKM_SP800_108_FEEDBACK_KDF, ctx, params, counter, output_bitlen, chaining_value, chaining_length, 0 /* exclude */);
        if (ret != CKR_OK) {
            goto finish;
        }

        if (counter == 1) {
            /* On the first iteration, chaining_value points to the IV from
             * the caller and chaining_length is the length of that IV. We
             * now need to allocate a buffer of suitable length to store the
             * MAC output. */
            chaining_value = PORT_ZNewArray(unsigned char, ctx->mac_size);
            chaining_length = ctx->mac_size;

            if (chaining_value == NULL) {
                ret = CKR_HOST_MEMORY;
                goto finish;
            }
        }

        /* Finalize this iteration of the PRF. Unlike other KDF forms, we
         * first save this to the chaining value so that we can reuse it
         * in the next iteration before copying the necessary length to
         * the output buffer. */
        ret = sftk_MAC_Finish(ctx, chaining_value, NULL, chaining_length);
        if (ret != CKR_OK) {
            goto finish;
        }

        /* Save as much of the chaining value as we need for output. */
        PORT_Memcpy(ret_buffer + buffer_offset, chaining_value, block_size);

        /* Increment our position in the key material. */
        buffer_offset += block_size;

        if (counter < num_iterations) {
            /* Reset the underlying PRF for the next iteration. Only do this
             * when we have a next iteration since it isn't necessary to do
             * either before the first iteration (MAC is already initialized)
             * or after the last iteration (we won't be called again). */
            ret = sftk_MAC_Reset(ctx);
            if (ret != CKR_OK) {
                goto finish;
            }
        }
    }

finish:
    if (chaining_value != initial_value && chaining_value != NULL) {
        PORT_ZFree(chaining_value, chaining_length);
    }

    return ret;
}

static CK_RV
kbkdf_PipelineRaw(const CK_SP800_108_KDF_PARAMS *params, sftk_MACCtx *ctx, unsigned char *ret_buffer, size_t buffer_length, PRUint64 output_bitlen)
{
    CK_RV ret = CKR_OK;

    /* Counter variable for this KDF instance. */
    PRUint32 counter;

    /* Number of iterations required of this PRF necessary to reach the
     * desired output length. */
    PRUint32 num_iterations;

    /* Offset in ret_buffer that we're at. */
    size_t buffer_offset = 0;

    /* Size of this block, in bytes. Defaults to ctx->mac_size except on
     * the last iteration where it could be a partial block. */
    size_t block_size = ctx->mac_size;

    /* The last PRF invocation. This is used for the first of the double
     * PRF invocations this KDF is named after. This defaults to NULL,
     * signifying that we have to calculate the initial value from params;
     * when non-NULL, we directly add only this value to the PRF. */
    unsigned char *chaining_value = NULL;

    /* Size of the chaining value discussed above. Defaults to 0. */
    size_t chaining_length = 0;

    /* Calculate the number of iterations required based on the size of the
     * output buffer. */
    ret = kbkdf_CalculateIterations(CKM_SP800_108_DOUBLE_PIPELINE_KDF, params, ctx, buffer_length, &num_iterations);
    if (ret != CKR_OK) {
        goto finish;
    }

    /*
     * 5.3 - [ KDF in Double-Pipeline Iteration Mode ]
     *
     * Fixed values:
     *      1. h - the length of the PRF in bits (ctx->mac_size)
     *      2. r - the length of the binary representation of the counter i
     *          (params[k: params[k].type == CK_SP800_108_OPTIONAL_COUNTER:].data->ulWidthInBits)
     *          Note that it is only specified when the optional counter is requested.
     * Input:
     *      1. K_I - the key for the PRF (base_key)
     *      2. label - a binary data field, usually before the separator. Optional.
     *      3. context - a binary data field, usually after the separator. Optional.
     *      4. L - length of the output in bits (output_bitlen)
     *
     * Process:
     *      1. n := ceil(L / h) (num_iterations)
     *      2. if n > 2^32 - 1, then indicate an error and stop
     *      3. result(0) = NULL
     *      4. A(0) := IV := Label || 0x00 || Context || [L]_2
     *      5. for i = 1 to n, do
     *          a. A(i) := PRF(K_I, A(i-1))
     *          b. K(i) := PRF(K_I, A(i) {|| [i]_2} || Label || 0x00 || Context || [L]_2
     *          c. result(i) := result(i-1) || K(i)
     *      6. return K_O := the leftmost L bits of result(n).
     */
    for (counter = 1; counter <= num_iterations; counter++) {
        if (counter == num_iterations) {
            block_size = buffer_length - buffer_offset;

            /* Assumption: if we've validated our arguments correctly, this
             * should always be true. */
            PR_ASSERT(block_size <= ctx->mac_size);
        }

        /* ===== First pipeline: construct A(i) ===== */
        if (counter == 1) {
            /* On the first iteration, we have no chaining value so specify
             * NULL for the pointer and 0 for the length, and exclude the
             * optional counter if it exists. This is what NIST specifies as
             * the IV for the KDF. */
            ret = kbkdf_AddParameters(CKM_SP800_108_DOUBLE_PIPELINE_KDF, ctx, params, counter, output_bitlen, NULL, 0, CK_SP800_108_OPTIONAL_COUNTER);
            if (ret != CKR_OK) {
                goto finish;
            }

            /* Allocate the chaining value so we can save the PRF output. */
            chaining_value = PORT_ZNewArray(unsigned char, ctx->mac_size);
            chaining_length = ctx->mac_size;
            if (chaining_value == NULL) {
                ret = CKR_HOST_MEMORY;
                goto finish;
            }
        } else {
            /* On all other iterations, the next stage of the first pipeline
             * comes directly from this stage. */
            ret = sftk_MAC_Update(ctx, chaining_value, chaining_length);
            if (ret != CKR_OK) {
                goto finish;
            }
        }

        /* Save the PRF output to chaining_value for use in the second
         * pipeline. */
        ret = sftk_MAC_Finish(ctx, chaining_value, NULL, chaining_length);
        if (ret != CKR_OK) {
            goto finish;
        }

        /* Reset the PRF so we can reuse it for the second pipeline. */
        ret = sftk_MAC_Reset(ctx);
        if (ret != CKR_OK) {
            goto finish;
        }

        /* ===== Second pipeline: construct K(i) ===== */

        /* Add all parameters required by this instance of the KDF to the
         * input stream of the underlying PRF. Note that this includes the
         * chaining value we calculated from the previous pipeline stage. */
        ret = kbkdf_AddParameters(CKM_SP800_108_FEEDBACK_KDF, ctx, params, counter, output_bitlen, chaining_value, chaining_length, 0 /* exclude */);
        if (ret != CKR_OK) {
            goto finish;
        }

        /* Finalize this iteration of the PRF directly to the output buffer.
         * Unlike Feedback mode, this pipeline doesn't influence the previous
         * stage. */
        ret = sftk_MAC_Finish(ctx, ret_buffer + buffer_offset, NULL, block_size);
        if (ret != CKR_OK) {
            goto finish;
        }

        /* Increment our position in the key material. */
        buffer_offset += block_size;

        if (counter < num_iterations) {
            /* Reset the underlying PRF for the next iteration. Only do this
             * when we have a next iteration since it isn't necessary to do
             * either before the first iteration (MAC is already initialized)
             * or after the last iteration (we won't be called again). */
            ret = sftk_MAC_Reset(ctx);
            if (ret != CKR_OK) {
                goto finish;
            }
        }
    }

finish:
    PORT_ZFree(chaining_value, chaining_length);

    return ret;
}

static CK_RV
kbkdf_RawDispatch(CK_MECHANISM_TYPE mech,
                  const CK_SP800_108_KDF_PARAMS *kdf_params,
                  const CK_BYTE *initial_value,
                  CK_ULONG initial_value_length,
                  SFTKObject *prf_key, const unsigned char *prf_key_bytes,
                  unsigned int prf_key_length, unsigned char **out_key_bytes,
                  size_t *out_key_length, unsigned int *mac_size,
                  CK_ULONG ret_key_size)
{
    CK_RV ret;
    /* Context for our underlying PRF function.
     *
     * Zeroing context required unconditional call of sftk_MAC_Destroy.
     */
    sftk_MACCtx ctx = { 0 };

    /* We need one buffers large enough to fit the entire KDF key stream for
     * all iterations of the PRF. This needs only include to the end of the
     * last key, so it isn't an even multiple of the PRF output size. */
    unsigned char *output_buffer = NULL;

    /* Size of the above buffer, in bytes. Note that this is technically
     * separate from the below output_bitlen variable due to the presence
     * of additional derived keys. See commentary in kbkdf_CalculateLength.
     */
    size_t buffer_length = 0;

    /* While NIST specifies a maximum length (in bits) for the counter, they
     * don't for the maximum length. It is unlikely, but theoretically
     * possible for output of the PRF to exceed 32 bits while keeping the
     * counter under 2^32. Thus, use a 64-bit variable for the maximum
     * output length.
     *
     * It is unlikely any caller will request this much data in practice.
     * 2^32 invocations of the PRF (for a 512-bit PRF) would be 256GB of
     * data in the KDF key stream alone. The bigger limit is the number of
     * and size of keys (again, 2^32); this could easily exceed 256GB when
     * counting the backing softoken key, the key data, template data, and
     * the input parameters to this KDF.
     *
     * This is the L parameter in NIST SP800-108.
     */
    PRUint64 output_bitlen = 0;

    /* First validate our passed input parameters against PKCS#11 v3.0
     * and NIST SP800-108 requirements. */
    ret = kbkdf_ValidateParameters(mech, kdf_params, ret_key_size);
    if (ret != CKR_OK) {
        goto finish;
    }

    /* Initialize the underlying PRF state. */
    if (prf_key) {
        ret = sftk_MAC_Init(&ctx, kdf_params->prfType, prf_key);
    } else {
        ret = sftk_MAC_InitRaw(&ctx, kdf_params->prfType, prf_key_bytes,
                               prf_key_length, PR_TRUE);
    }
    if (ret != CKR_OK) {
        goto finish;
    }

    /* Compute the size of our output buffer based on passed parameters and
     * the output size of the underlying PRF. */
    ret = kbkdf_CalculateLength(kdf_params, &ctx, ret_key_size, &output_bitlen, &buffer_length);
    if (ret != CKR_OK) {
        goto finish;
    }

    /* Allocate memory for the PRF output */
    output_buffer = PORT_ZNewArray(unsigned char, buffer_length);
    if (output_buffer == NULL) {
        ret = CKR_HOST_MEMORY;
        goto finish;
    }

    /* Call into the underlying KDF */
    switch (mech) {
        case CKM_NSS_SP800_108_COUNTER_KDF_DERIVE_DATA: /* fall through */
        case CKM_SP800_108_COUNTER_KDF:
            ret = kbkdf_CounterRaw(kdf_params, &ctx, output_buffer, buffer_length, output_bitlen);
            break;
        case CKM_NSS_SP800_108_FEEDBACK_KDF_DERIVE_DATA: /* fall through */
        case CKM_SP800_108_FEEDBACK_KDF:
            ret = kbkdf_FeedbackRaw(kdf_params, initial_value, initial_value_length, &ctx, output_buffer, buffer_length, output_bitlen);
            break;
        case CKM_NSS_SP800_108_DOUBLE_PIPELINE_KDF_DERIVE_DATA: /* fall through */
        case CKM_SP800_108_DOUBLE_PIPELINE_KDF:
            ret = kbkdf_PipelineRaw(kdf_params, &ctx, output_buffer, buffer_length, output_bitlen);
            break;
        default:
            /* Shouldn't happen unless NIST introduces a new KBKDF type. */
            PR_ASSERT(PR_FALSE);
            ret = CKR_FUNCTION_FAILED;
    }

    /* Validate the above KDF succeeded. */
    if (ret != CKR_OK) {
        goto finish;
    }

    *out_key_bytes = output_buffer;
    *out_key_length = buffer_length;
    *mac_size = ctx.mac_size;

    output_buffer = NULL; /* returning the buffer, don't zero and free it */

finish:
    PORT_ZFree(output_buffer, buffer_length);

    /* Free the PRF. This should handle clearing all sensitive information. */
    sftk_MAC_Destroy(&ctx, PR_FALSE);
    return ret;
}

/* [ section: PKCS#11 entry ] */

CK_RV
kbkdf_Dispatch(CK_MECHANISM_TYPE mech, CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism, SFTKObject *prf_key, SFTKObject *ret_key, CK_ULONG ret_key_size)
{
    /* This handles boilerplate common to all KBKDF types. Instead of placing
     * this in pkcs11c.c, place it here to reduce clutter. */

    CK_RV ret;

    /* Assumptions about our calling environment. */
    PR_ASSERT(pMechanism != NULL && prf_key != NULL && ret_key != NULL);

    /* Validate that the caller passed parameters. */
    if (pMechanism->pParameter == NULL) {
        return CKR_MECHANISM_PARAM_INVALID;
    }

    /* Create a common set of parameters to use for all KDF types. This
     * separates out the KDF parameters from the Feedback-specific IV,
     * allowing us to use a common type for all calls. */
    CK_SP800_108_KDF_PARAMS kdf_params = { 0 };
    CK_BYTE_PTR initial_value = NULL;
    CK_ULONG initial_value_length = 0;
    unsigned char *output_buffer = NULL;
    size_t buffer_length = 0;
    unsigned int mac_size = 0;

    /* Split Feedback-specific IV from remaining KDF parameters. */
    ret = kbkdf_LoadParameters(mech, pMechanism, &kdf_params, &initial_value, &initial_value_length);
    if (ret != CKR_OK) {
        goto finish;
    }
    /* let rawDispatch handle the rest. We split this out so we could
     * handle the POST test without accessing pkcs #11 objects. */
    ret = kbkdf_RawDispatch(mech, &kdf_params, initial_value,
                            initial_value_length, prf_key, NULL, 0,
                            &output_buffer, &buffer_length, &mac_size,
                            ret_key_size);
    if (ret != CKR_OK) {
        goto finish;
    }

    /* Write the output of the PRF into the appropriate keys. */
    ret = kbkdf_SaveKeys(mech, hSession, &kdf_params, output_buffer, buffer_length, mac_size, ret_key, ret_key_size);
    if (ret != CKR_OK) {
        goto finish;
    }

finish:
    PORT_ZFree(output_buffer, buffer_length);

    return ret;
}

struct sftk_SP800_Test_struct {
    CK_MECHANISM_TYPE mech;
    CK_SP800_108_KDF_PARAMS kdf_params;
    unsigned int expected_mac_size;
    unsigned int ret_key_length;
    const unsigned char expected_key_bytes[64];
};

static const CK_SP800_108_COUNTER_FORMAT counter_32 = { 0, 32 };
static const CK_PRF_DATA_PARAM counter_32_data = { CK_SP800_108_ITERATION_VARIABLE, (CK_VOID_PTR)&counter_32, sizeof(counter_32) };

#ifdef NSS_FULL_POST
static const CK_SP800_108_COUNTER_FORMAT counter_16 = { 0, 16 };
static const CK_PRF_DATA_PARAM counter_16_data = { CK_SP800_108_ITERATION_VARIABLE, (CK_VOID_PTR)&counter_16, sizeof(counter_16) };
static const CK_PRF_DATA_PARAM counter_null_data = { CK_SP800_108_ITERATION_VARIABLE, NULL, 0 };
#endif

static const struct sftk_SP800_Test_struct sftk_SP800_Tests[] = {
#ifdef NSS_FULL_POST
    {
        CKM_SP800_108_COUNTER_KDF,
        { CKM_AES_CMAC, 1, (CK_PRF_DATA_PARAM_PTR)&counter_16_data, 0, NULL },
        16,
        64,
        { 0x7b, 0x1c, 0xe7, 0xf3, 0x14, 0x67, 0x15, 0xdd,
          0xde, 0x0c, 0x09, 0x46, 0x3f, 0x47, 0x7b, 0xa6,
          0xb8, 0xba, 0x40, 0x07, 0x7c, 0xe3, 0x19, 0x53,
          0x26, 0xac, 0x4c, 0x2e, 0x2b, 0x37, 0x41, 0xe4,
          0x1b, 0x01, 0x3f, 0x2f, 0x2d, 0x16, 0x95, 0xee,
          0xeb, 0x7e, 0x72, 0x7d, 0xa4, 0xab, 0x2e, 0x67,
          0x1d, 0xef, 0x6f, 0xa2, 0xc6, 0xee, 0x3c, 0xcf,
          0xef, 0x88, 0xfd, 0x5c, 0x1d, 0x7b, 0xa0, 0x5a },
    },
    {
        CKM_SP800_108_COUNTER_KDF,
        { CKM_SHA384_HMAC, 1, (CK_PRF_DATA_PARAM_PTR)&counter_32_data, 0, NULL },
        48,
        64,
        { 0xe6, 0x62, 0xa4, 0x32, 0x5c, 0xe4, 0xc2, 0x28,
          0x73, 0x8a, 0x5d, 0x94, 0xe7, 0x05, 0xe0, 0x5a,
          0x71, 0x61, 0xb2, 0x3c, 0x51, 0x28, 0x03, 0x1d,
          0xa7, 0xf5, 0x10, 0x83, 0x34, 0xdb, 0x11, 0x73,
          0x92, 0xa6, 0x79, 0x74, 0x81, 0x5d, 0x22, 0x7e,
          0x8d, 0xf2, 0x59, 0x14, 0x56, 0x60, 0xcf, 0xb2,
          0xb3, 0xfd, 0x46, 0xfd, 0x9b, 0x74, 0xfe, 0x4a,
          0x09, 0x30, 0x4a, 0xdf, 0x07, 0x43, 0xfe, 0x85 },
    },
    {
        CKM_SP800_108_COUNTER_KDF,
        { CKM_SHA512_HMAC, 1, (CK_PRF_DATA_PARAM_PTR)&counter_32_data, 0, NULL },
        64,
        64,
        { 0xb0, 0x78, 0x36, 0xe1, 0x15, 0xd6, 0xf0, 0xac,
          0x68, 0x7b, 0x42, 0xd3, 0xb6, 0x82, 0x51, 0xad,
          0x95, 0x0a, 0x69, 0x88, 0x84, 0xc2, 0x2e, 0x07,
          0x34, 0x62, 0x8d, 0x42, 0x72, 0x0f, 0x22, 0xe6,
          0xd5, 0x7f, 0x80, 0x15, 0xe6, 0x84, 0x00, 0x65,
          0xef, 0x64, 0x77, 0x29, 0xd6, 0x3b, 0xc7, 0x9a,
          0x15, 0x6d, 0x36, 0xf3, 0x96, 0xc9, 0x14, 0x3f,
          0x2d, 0x4a, 0x7c, 0xdb, 0xc3, 0x6c, 0x3d, 0x6a },
    },
    {
        CKM_SP800_108_FEEDBACK_KDF,
        { CKM_AES_CMAC, 1, (CK_PRF_DATA_PARAM_PTR)&counter_null_data, 0, NULL },
        16,
        64,
        { 0xc0, 0xa0, 0x23, 0x96, 0x16, 0x4d, 0xd6, 0xbd,
          0x2a, 0x75, 0x8e, 0x72, 0xf5, 0xc3, 0xa0, 0xb8,
          0x78, 0x83, 0x15, 0x21, 0x34, 0xd3, 0xd8, 0x71,
          0xc9, 0xe7, 0x4b, 0x20, 0xb7, 0x65, 0x5b, 0x13,
          0xbc, 0x85, 0x54, 0xe3, 0xb6, 0xee, 0x73, 0xd5,
          0xf2, 0xa0, 0x94, 0x1a, 0x79, 0x66, 0x3b, 0x1e,
          0x67, 0x3e, 0x69, 0xa4, 0x12, 0x40, 0xa9, 0xda,
          0x8d, 0x14, 0xb1, 0xce, 0xf1, 0x4b, 0x79, 0x4e },
    },
    {
        CKM_SP800_108_FEEDBACK_KDF,
        { CKM_SHA256_HMAC, 1, (CK_PRF_DATA_PARAM_PTR)&counter_null_data, 0, NULL },
        32,
        64,
        { 0x99, 0x9b, 0x08, 0x79, 0x14, 0x2e, 0x58, 0x34,
          0xd7, 0x92, 0xa7, 0x7e, 0x7f, 0xc2, 0xf0, 0x34,
          0xa3, 0x4e, 0x33, 0xf0, 0x63, 0x95, 0x2d, 0xad,
          0xbf, 0x3b, 0xcb, 0x6d, 0x4e, 0x07, 0xd9, 0xe9,
          0xbd, 0xbd, 0x77, 0x54, 0xe1, 0xa3, 0x36, 0x26,
          0xcd, 0xb1, 0xf9, 0x2d, 0x80, 0x68, 0xa2, 0x01,
          0x4e, 0xbf, 0x35, 0xec, 0x65, 0xae, 0xfd, 0x71,
          0xa6, 0xd7, 0x62, 0x26, 0x2c, 0x3f, 0x73, 0x63 },
    },
    {
        CKM_SP800_108_FEEDBACK_KDF,
        { CKM_SHA384_HMAC, 1, (CK_PRF_DATA_PARAM_PTR)&counter_null_data, 0, NULL },
        48,
        64,
        { 0xc8, 0x7a, 0xf8, 0xd9, 0x6b, 0x90, 0x82, 0x35,
          0xea, 0xf5, 0x2c, 0x8f, 0xce, 0xaa, 0x3b, 0xa5,
          0x68, 0xd3, 0x7f, 0xae, 0x31, 0x93, 0xe6, 0x69,
          0x0c, 0xd1, 0x74, 0x7f, 0x8f, 0xc2, 0xe2, 0x33,
          0x93, 0x45, 0x23, 0xba, 0xb3, 0x73, 0xc9, 0x2c,
          0xd6, 0xd2, 0x10, 0x16, 0xe9, 0x9f, 0x9e, 0xe8,
          0xc1, 0x0e, 0x29, 0x95, 0x3d, 0x16, 0x68, 0x24,
          0x40, 0x4d, 0x40, 0x21, 0x41, 0xa6, 0xc8, 0xdb },
    },
    {
        CKM_SP800_108_FEEDBACK_KDF,
        { CKM_SHA512_HMAC, 1, (CK_PRF_DATA_PARAM_PTR)&counter_null_data, 0, NULL },
        64,
        64,
        { 0x81, 0x39, 0x12, 0xc2, 0xf9, 0x31, 0x24, 0x7c,
          0x71, 0x12, 0x97, 0x08, 0x82, 0x76, 0x83, 0x55,
          0x8c, 0x82, 0xf3, 0x09, 0xd6, 0x1b, 0x7a, 0xa2,
          0x6e, 0x71, 0x6b, 0xad, 0x46, 0x57, 0x60, 0x89,
          0x38, 0xcf, 0x63, 0xfa, 0xf4, 0x38, 0x27, 0xef,
          0xf0, 0xaf, 0x75, 0x4e, 0xc2, 0xe0, 0x31, 0xdb,
          0x59, 0x7d, 0x19, 0xc9, 0x6d, 0xbb, 0xed, 0x95,
          0xaf, 0x3e, 0xd8, 0x33, 0x76, 0xab, 0xec, 0xfa },
    },
    {
        CKM_SP800_108_DOUBLE_PIPELINE_KDF,
        { CKM_AES_CMAC, 1, (CK_PRF_DATA_PARAM_PTR)&counter_null_data, 0, NULL },
        16,
        64,
        { 0x3e, 0xa8, 0xbf, 0x77, 0x84, 0x90, 0xb0, 0x3a,
          0x89, 0x16, 0x32, 0x01, 0x92, 0xd3, 0x1f, 0x1b,
          0xc1, 0x06, 0xc5, 0x32, 0x62, 0x03, 0x50, 0x16,
          0x3b, 0xb9, 0xa7, 0xdc, 0xb5, 0x68, 0x6a, 0xbb,
          0xbb, 0x7d, 0x63, 0x69, 0x24, 0x6e, 0x09, 0xd6,
          0x6f, 0x80, 0x57, 0x65, 0xc5, 0x62, 0x33, 0x96,
          0x69, 0xe6, 0xab, 0x65, 0x36, 0xd0, 0xe2, 0x5c,
          0xd7, 0xbd, 0xe4, 0x68, 0x13, 0xd6, 0xb1, 0x46 },
    },
    {
        CKM_SP800_108_DOUBLE_PIPELINE_KDF,
        { CKM_SHA256_HMAC, 1, (CK_PRF_DATA_PARAM_PTR)&counter_null_data, 0, NULL },
        32,
        64,
        { 0xeb, 0x28, 0xd9, 0x2c, 0x19, 0x33, 0xb9, 0x2a,
          0xf9, 0xac, 0x85, 0xbd, 0xf4, 0xdb, 0xfa, 0x88,
          0x73, 0xf4, 0x36, 0x08, 0xdb, 0xfe, 0x13, 0xd1,
          0x5a, 0xec, 0x7b, 0x68, 0x13, 0x53, 0xb3, 0xd1,
          0x31, 0xf2, 0x83, 0xae, 0x9f, 0x75, 0x47, 0xb6,
          0x6d, 0x3c, 0x20, 0x16, 0x47, 0x9c, 0x27, 0x66,
          0xec, 0xa9, 0xdf, 0x0c, 0xda, 0x2a, 0xf9, 0xf4,
          0x55, 0x74, 0xde, 0x9d, 0x3f, 0xe3, 0x5e, 0x14 },
    },
    {
        CKM_SP800_108_DOUBLE_PIPELINE_KDF,
        { CKM_SHA384_HMAC, 1, (CK_PRF_DATA_PARAM_PTR)&counter_null_data, 0, NULL },
        48,
        64,
        { 0xa5, 0xca, 0x32, 0x40, 0x00, 0x93, 0xb2, 0xcc,
          0x78, 0x3c, 0xa6, 0xc4, 0xaf, 0xa8, 0xb3, 0xd0,
          0xa4, 0x6b, 0xb5, 0x31, 0x35, 0x87, 0x33, 0xa2,
          0x6a, 0x6b, 0xe1, 0xff, 0xea, 0x1d, 0x6e, 0x9e,
          0x0b, 0xde, 0x8b, 0x92, 0x15, 0xd6, 0x56, 0x2f,
          0xb6, 0x1a, 0xd7, 0xd2, 0x01, 0x3e, 0x28, 0x2e,
          0xfa, 0x84, 0x3c, 0xc0, 0xe8, 0xbe, 0x94, 0xc0,
          0x06, 0xbd, 0xbf, 0x87, 0x1f, 0xb8, 0x64, 0xc2 },
    },
    {
        CKM_SP800_108_DOUBLE_PIPELINE_KDF,
        { CKM_SHA512_HMAC, 1, (CK_PRF_DATA_PARAM_PTR)&counter_null_data, 0, NULL },
        64,
        64,
        { 0x3f, 0xd9, 0x4e, 0x80, 0x58, 0x21, 0xc8, 0xea,
          0x22, 0x17, 0xcf, 0x7d, 0xce, 0xfd, 0xec, 0x03,
          0xb9, 0xe4, 0xa2, 0xf7, 0xc0, 0xf1, 0x68, 0x81,
          0x53, 0x71, 0xb7, 0x42, 0x14, 0x4e, 0x5b, 0x09,
          0x05, 0x31, 0xb9, 0x27, 0x18, 0x2d, 0x23, 0xf8,
          0x9c, 0x3d, 0x4e, 0xd0, 0xdd, 0xf3, 0x1e, 0x4b,
          0xf2, 0xf9, 0x1a, 0x5d, 0x00, 0x66, 0x22, 0x83,
          0xae, 0x3c, 0x53, 0xd2, 0x54, 0x4b, 0x06, 0x4c },
    },
#endif
    {
        CKM_SP800_108_COUNTER_KDF,
        { CKM_SHA256_HMAC, 1, (CK_PRF_DATA_PARAM_PTR)&counter_32_data, 0, NULL },
        32,
        64,
        { 0xfb, 0x2b, 0xb5, 0xde, 0xce, 0x5a, 0x2b, 0xdc,
          0x25, 0x8f, 0x54, 0x17, 0x4b, 0x5a, 0xa7, 0x90,
          0x64, 0x36, 0xeb, 0x43, 0x1f, 0x1d, 0xf9, 0x23,
          0xb2, 0x22, 0x29, 0xa0, 0xfa, 0x2e, 0x21, 0xb6,
          0xb7, 0xfb, 0x27, 0x0a, 0x1c, 0xa6, 0x58, 0x43,
          0xa1, 0x16, 0x44, 0x29, 0x4b, 0x1c, 0xb3, 0x72,
          0xd5, 0x98, 0x9d, 0x27, 0xd5, 0x75, 0x25, 0xbf,
          0x23, 0x61, 0x40, 0x48, 0xbb, 0x0b, 0x49, 0x8e },
    }
};

SECStatus
sftk_fips_SP800_108_PowerUpSelfTests(void)
{
    int i;
    CK_RV crv;

    const unsigned char prf_key[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
        0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
        0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
        0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78
    };
    for (i = 0; i < PR_ARRAY_SIZE(sftk_SP800_Tests); i++) {
        const struct sftk_SP800_Test_struct *test = &sftk_SP800_Tests[i];
        unsigned char *output_buffer;
        size_t buffer_length;
        unsigned int mac_size;

        crv = kbkdf_RawDispatch(test->mech, &test->kdf_params,
                                prf_key, test->expected_mac_size,
                                NULL, prf_key, test->expected_mac_size,
                                &output_buffer, &buffer_length, &mac_size,
                                test->ret_key_length);
        if (crv != CKR_OK) {
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            return SECFailure;
        }
        if ((mac_size != test->expected_mac_size) ||
            (buffer_length != test->ret_key_length) ||
            (output_buffer == NULL) ||
            (PORT_Memcmp(output_buffer, test->expected_key_bytes, buffer_length) != 0)) {
            PORT_ZFree(output_buffer, buffer_length);
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            return SECFailure;
        }
        PORT_ZFree(output_buffer, buffer_length);
    }
    return SECSuccess;
}
