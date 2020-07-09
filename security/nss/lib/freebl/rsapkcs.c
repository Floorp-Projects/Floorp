/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * RSA PKCS#1 v2.1 (RFC 3447) operations
 */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "secerr.h"

#include "blapi.h"
#include "secitem.h"
#include "blapii.h"

#define RSA_BLOCK_MIN_PAD_LEN 8
#define RSA_BLOCK_FIRST_OCTET 0x00
#define RSA_BLOCK_PRIVATE_PAD_OCTET 0xff
#define RSA_BLOCK_AFTER_PAD_OCTET 0x00

/*
 * RSA block types
 *
 * The values of RSA_BlockPrivate and RSA_BlockPublic are fixed.
 * The value of RSA_BlockRaw isn't fixed by definition, but we are keeping
 * the value that NSS has been using in the past.
 */
typedef enum {
    RSA_BlockPrivate = 1, /* pad for a private-key operation */
    RSA_BlockPublic = 2,  /* pad for a public-key operation */
    RSA_BlockRaw = 4      /* simply justify the block appropriately */
} RSA_BlockType;

/* Needed for RSA-PSS functions */
static const unsigned char eightZeros[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

/* Constant time comparison of a single byte.
 * Returns 1 iff a == b, otherwise returns 0.
 * Note: For ranges of bytes, use constantTimeCompare.
 */
static unsigned char
constantTimeEQ8(unsigned char a, unsigned char b)
{
    unsigned char c = ~((a - b) | (b - a));
    c >>= 7;
    return c;
}

/* Constant time comparison of a range of bytes.
 * Returns 1 iff len bytes of a are identical to len bytes of b, otherwise
 * returns 0.
 */
static unsigned char
constantTimeCompare(const unsigned char *a,
                    const unsigned char *b,
                    unsigned int len)
{
    unsigned char tmp = 0;
    unsigned int i;
    for (i = 0; i < len; ++i, ++a, ++b)
        tmp |= *a ^ *b;
    return constantTimeEQ8(0x00, tmp);
}

/* Constant time conditional.
 * Returns a if c is 1, or b if c is 0. The result is undefined if c is
 * not 0 or 1.
 */
static unsigned int
constantTimeCondition(unsigned int c,
                      unsigned int a,
                      unsigned int b)
{
    return (~(c - 1) & a) | ((c - 1) & b);
}

static unsigned int
rsa_modulusLen(SECItem *modulus)
{
    unsigned char byteZero = modulus->data[0];
    unsigned int modLen = modulus->len - !byteZero;
    return modLen;
}

static unsigned int
rsa_modulusBits(SECItem *modulus)
{
    unsigned char byteZero = modulus->data[0];
    unsigned int numBits = (modulus->len - 1) * 8;

    if (byteZero == 0) {
        numBits -= 8;
        byteZero = modulus->data[1];
    }

    while (byteZero > 0) {
        numBits++;
        byteZero >>= 1;
    }

    return numBits;
}

/*
 * Format one block of data for public/private key encryption using
 * the rules defined in PKCS #1.
 */
static unsigned char *
rsa_FormatOneBlock(unsigned modulusLen,
                   RSA_BlockType blockType,
                   SECItem *data)
{
    unsigned char *block;
    unsigned char *bp;
    unsigned int padLen;
    unsigned int i, j;
    SECStatus rv;

    block = (unsigned char *)PORT_Alloc(modulusLen);
    if (block == NULL)
        return NULL;

    bp = block;

    /*
     * All RSA blocks start with two octets:
     *  0x00 || BlockType
     */
    *bp++ = RSA_BLOCK_FIRST_OCTET;
    *bp++ = (unsigned char)blockType;

    switch (blockType) {

        /*
         * Blocks intended for private-key operation.
         */
        case RSA_BlockPrivate: /* preferred method */
            /*
             * 0x00 || BT || Pad || 0x00 || ActualData
             *   1      1   padLen    1      data->len
             * padLen must be at least RSA_BLOCK_MIN_PAD_LEN (8) bytes.
             * Pad is either all 0x00 or all 0xff bytes, depending on blockType.
             */
            padLen = modulusLen - data->len - 3;
            PORT_Assert(padLen >= RSA_BLOCK_MIN_PAD_LEN);
            if (padLen < RSA_BLOCK_MIN_PAD_LEN) {
                PORT_Free(block);
                return NULL;
            }
            PORT_Memset(bp, RSA_BLOCK_PRIVATE_PAD_OCTET, padLen);
            bp += padLen;
            *bp++ = RSA_BLOCK_AFTER_PAD_OCTET;
            PORT_Memcpy(bp, data->data, data->len);
            break;

        /*
         * Blocks intended for public-key operation.
         */
        case RSA_BlockPublic:
            /*
             * 0x00 || BT || Pad || 0x00 || ActualData
             *   1      1   padLen    1      data->len
             * Pad is 8 or more non-zero random bytes.
             *
             * Build the block left to right.
             * Fill the entire block from Pad to the end with random bytes.
             * Use the bytes after Pad as a supply of extra random bytes from
             * which to find replacements for the zero bytes in Pad.
             * If we need more than that, refill the bytes after Pad with
             * new random bytes as necessary.
             */

            padLen = modulusLen - (data->len + 3);
            PORT_Assert(padLen >= RSA_BLOCK_MIN_PAD_LEN);
            if (padLen < RSA_BLOCK_MIN_PAD_LEN) {
                PORT_Free(block);
                return NULL;
            }
            j = modulusLen - 2;
            rv = RNG_GenerateGlobalRandomBytes(bp, j);
            if (rv == SECSuccess) {
                for (i = 0; i < padLen;) {
                    unsigned char repl;
                    /* Pad with non-zero random data. */
                    if (bp[i] != RSA_BLOCK_AFTER_PAD_OCTET) {
                        ++i;
                        continue;
                    }
                    if (j <= padLen) {
                        rv = RNG_GenerateGlobalRandomBytes(bp + padLen,
                                                           modulusLen - (2 + padLen));
                        if (rv != SECSuccess)
                            break;
                        j = modulusLen - 2;
                    }
                    do {
                        repl = bp[--j];
                    } while (repl == RSA_BLOCK_AFTER_PAD_OCTET && j > padLen);
                    if (repl != RSA_BLOCK_AFTER_PAD_OCTET) {
                        bp[i++] = repl;
                    }
                }
            }
            if (rv != SECSuccess) {
                PORT_Free(block);
                PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
                return NULL;
            }
            bp += padLen;
            *bp++ = RSA_BLOCK_AFTER_PAD_OCTET;
            PORT_Memcpy(bp, data->data, data->len);
            break;

        default:
            PORT_Assert(0);
            PORT_Free(block);
            return NULL;
    }

    return block;
}

static SECStatus
rsa_FormatBlock(SECItem *result,
                unsigned modulusLen,
                RSA_BlockType blockType,
                SECItem *data)
{
    switch (blockType) {
        case RSA_BlockPrivate:
        case RSA_BlockPublic:
            /*
             * 0x00 || BT || Pad || 0x00 || ActualData
             *
             * The "3" below is the first octet + the second octet + the 0x00
             * octet that always comes just before the ActualData.
             */
            if (data->len > (modulusLen - (3 + RSA_BLOCK_MIN_PAD_LEN))) {
                return SECFailure;
            }
            result->data = rsa_FormatOneBlock(modulusLen, blockType, data);
            if (result->data == NULL) {
                result->len = 0;
                return SECFailure;
            }
            result->len = modulusLen;

            break;

        case RSA_BlockRaw:
            /*
             * Pad || ActualData
             * Pad is zeros. The application is responsible for recovering
             * the actual data.
             */
            if (data->len > modulusLen) {
                return SECFailure;
            }
            result->data = (unsigned char *)PORT_ZAlloc(modulusLen);
            result->len = modulusLen;
            PORT_Memcpy(result->data + (modulusLen - data->len),
                        data->data, data->len);
            break;

        default:
            PORT_Assert(0);
            result->data = NULL;
            result->len = 0;
            return SECFailure;
    }

    return SECSuccess;
}

/*
 * Mask generation function MGF1 as defined in PKCS #1 v2.1 / RFC 3447.
 */
static SECStatus
MGF1(HASH_HashType hashAlg,
     unsigned char *mask,
     unsigned int maskLen,
     const unsigned char *mgfSeed,
     unsigned int mgfSeedLen)
{
    unsigned int digestLen;
    PRUint32 counter;
    PRUint32 rounds;
    unsigned char *tempHash;
    unsigned char *temp;
    const SECHashObject *hash;
    void *hashContext;
    unsigned char C[4];
    SECStatus rv = SECSuccess;

    hash = HASH_GetRawHashObject(hashAlg);
    if (hash == NULL) {
        return SECFailure;
    }

    hashContext = (*hash->create)();
    rounds = (maskLen + hash->length - 1) / hash->length;
    for (counter = 0; counter < rounds; counter++) {
        C[0] = (unsigned char)((counter >> 24) & 0xff);
        C[1] = (unsigned char)((counter >> 16) & 0xff);
        C[2] = (unsigned char)((counter >> 8) & 0xff);
        C[3] = (unsigned char)(counter & 0xff);

        /* This could be optimized when the clone functions in
         * rawhash.c are implemented. */
        (*hash->begin)(hashContext);
        (*hash->update)(hashContext, mgfSeed, mgfSeedLen);
        (*hash->update)(hashContext, C, sizeof C);

        tempHash = mask + counter * hash->length;
        if (counter != (rounds - 1)) {
            (*hash->end)(hashContext, tempHash, &digestLen, hash->length);
        } else { /* we're in the last round and need to cut the hash */
            temp = (unsigned char *)PORT_Alloc(hash->length);
            if (!temp) {
                rv = SECFailure;
                goto done;
            }
            (*hash->end)(hashContext, temp, &digestLen, hash->length);
            PORT_Memcpy(tempHash, temp, maskLen - counter * hash->length);
            PORT_Free(temp);
        }
    }

done:
    (*hash->destroy)(hashContext, PR_TRUE);
    return rv;
}

/* XXX Doesn't set error code */
SECStatus
RSA_SignRaw(RSAPrivateKey *key,
            unsigned char *output,
            unsigned int *outputLen,
            unsigned int maxOutputLen,
            const unsigned char *data,
            unsigned int dataLen)
{
    SECStatus rv = SECSuccess;
    unsigned int modulusLen = rsa_modulusLen(&key->modulus);
    SECItem formatted;
    SECItem unformatted;

    if (maxOutputLen < modulusLen)
        return SECFailure;

    unformatted.len = dataLen;
    unformatted.data = (unsigned char *)data;
    formatted.data = NULL;
    rv = rsa_FormatBlock(&formatted, modulusLen, RSA_BlockRaw, &unformatted);
    if (rv != SECSuccess)
        goto done;

    rv = RSA_PrivateKeyOpDoubleChecked(key, output, formatted.data);
    *outputLen = modulusLen;

done:
    if (formatted.data != NULL)
        PORT_ZFree(formatted.data, modulusLen);
    return rv;
}

/* XXX Doesn't set error code */
SECStatus
RSA_CheckSignRaw(RSAPublicKey *key,
                 const unsigned char *sig,
                 unsigned int sigLen,
                 const unsigned char *hash,
                 unsigned int hashLen)
{
    SECStatus rv;
    unsigned int modulusLen = rsa_modulusLen(&key->modulus);
    unsigned char *buffer;

    if (sigLen != modulusLen)
        goto failure;
    if (hashLen > modulusLen)
        goto failure;

    buffer = (unsigned char *)PORT_Alloc(modulusLen + 1);
    if (!buffer)
        goto failure;

    rv = RSA_PublicKeyOp(key, buffer, sig);
    if (rv != SECSuccess)
        goto loser;

    /*
     * make sure we get the same results
     */
    /* XXX(rsleevi): Constant time */
    /* NOTE: should we verify the leading zeros? */
    if (PORT_Memcmp(buffer + (modulusLen - hashLen), hash, hashLen) != 0)
        goto loser;

    PORT_Free(buffer);
    return SECSuccess;

loser:
    PORT_Free(buffer);
failure:
    return SECFailure;
}

/* XXX Doesn't set error code */
SECStatus
RSA_CheckSignRecoverRaw(RSAPublicKey *key,
                        unsigned char *data,
                        unsigned int *dataLen,
                        unsigned int maxDataLen,
                        const unsigned char *sig,
                        unsigned int sigLen)
{
    SECStatus rv;
    unsigned int modulusLen = rsa_modulusLen(&key->modulus);

    if (sigLen != modulusLen)
        goto failure;
    if (maxDataLen < modulusLen)
        goto failure;

    rv = RSA_PublicKeyOp(key, data, sig);
    if (rv != SECSuccess)
        goto failure;

    *dataLen = modulusLen;
    return SECSuccess;

failure:
    return SECFailure;
}

/* XXX Doesn't set error code */
SECStatus
RSA_EncryptRaw(RSAPublicKey *key,
               unsigned char *output,
               unsigned int *outputLen,
               unsigned int maxOutputLen,
               const unsigned char *input,
               unsigned int inputLen)
{
    SECStatus rv;
    unsigned int modulusLen = rsa_modulusLen(&key->modulus);
    SECItem formatted;
    SECItem unformatted;

    formatted.data = NULL;
    if (maxOutputLen < modulusLen)
        goto failure;

    unformatted.len = inputLen;
    unformatted.data = (unsigned char *)input;
    formatted.data = NULL;
    rv = rsa_FormatBlock(&formatted, modulusLen, RSA_BlockRaw, &unformatted);
    if (rv != SECSuccess)
        goto failure;

    rv = RSA_PublicKeyOp(key, output, formatted.data);
    if (rv != SECSuccess)
        goto failure;

    PORT_ZFree(formatted.data, modulusLen);
    *outputLen = modulusLen;
    return SECSuccess;

failure:
    if (formatted.data != NULL)
        PORT_ZFree(formatted.data, modulusLen);
    return SECFailure;
}

/* XXX Doesn't set error code */
SECStatus
RSA_DecryptRaw(RSAPrivateKey *key,
               unsigned char *output,
               unsigned int *outputLen,
               unsigned int maxOutputLen,
               const unsigned char *input,
               unsigned int inputLen)
{
    SECStatus rv;
    unsigned int modulusLen = rsa_modulusLen(&key->modulus);

    if (modulusLen > maxOutputLen)
        goto failure;
    if (inputLen != modulusLen)
        goto failure;

    rv = RSA_PrivateKeyOp(key, output, input);
    if (rv != SECSuccess)
        goto failure;

    *outputLen = modulusLen;
    return SECSuccess;

failure:
    return SECFailure;
}

/*
 * Decodes an EME-OAEP encoded block, validating the encoding in constant
 * time.
 * Described in RFC 3447, section 7.1.2.
 * input contains the encoded block, after decryption.
 * label is the optional value L that was associated with the message.
 * On success, the original message and message length will be stored in
 * output and outputLen.
 */
static SECStatus
eme_oaep_decode(unsigned char *output,
                unsigned int *outputLen,
                unsigned int maxOutputLen,
                const unsigned char *input,
                unsigned int inputLen,
                HASH_HashType hashAlg,
                HASH_HashType maskHashAlg,
                const unsigned char *label,
                unsigned int labelLen)
{
    const SECHashObject *hash;
    void *hashContext;
    SECStatus rv = SECFailure;
    unsigned char labelHash[HASH_LENGTH_MAX];
    unsigned int i;
    unsigned int maskLen;
    unsigned int paddingOffset;
    unsigned char *mask = NULL;
    unsigned char *tmpOutput = NULL;
    unsigned char isGood;
    unsigned char foundPaddingEnd;

    hash = HASH_GetRawHashObject(hashAlg);

    /* 1.c */
    if (inputLen < (hash->length * 2) + 2) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }

    /* Step 3.a - Generate lHash */
    hashContext = (*hash->create)();
    if (hashContext == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }
    (*hash->begin)(hashContext);
    if (labelLen > 0)
        (*hash->update)(hashContext, label, labelLen);
    (*hash->end)(hashContext, labelHash, &i, sizeof(labelHash));
    (*hash->destroy)(hashContext, PR_TRUE);

    tmpOutput = (unsigned char *)PORT_Alloc(inputLen);
    if (tmpOutput == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto done;
    }

    maskLen = inputLen - hash->length - 1;
    mask = (unsigned char *)PORT_Alloc(maskLen);
    if (mask == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto done;
    }

    PORT_Memcpy(tmpOutput, input, inputLen);

    /* 3.c - Generate seedMask */
    MGF1(maskHashAlg, mask, hash->length, &tmpOutput[1 + hash->length],
         inputLen - hash->length - 1);
    /* 3.d - Unmask seed */
    for (i = 0; i < hash->length; ++i)
        tmpOutput[1 + i] ^= mask[i];

    /* 3.e - Generate dbMask */
    MGF1(maskHashAlg, mask, maskLen, &tmpOutput[1], hash->length);
    /* 3.f - Unmask DB */
    for (i = 0; i < maskLen; ++i)
        tmpOutput[1 + hash->length + i] ^= mask[i];

    /* 3.g - Compare Y, lHash, and PS in constant time
     * Warning: This code is timing dependent and must not disclose which of
     * these were invalid.
     */
    paddingOffset = 0;
    isGood = 1;
    foundPaddingEnd = 0;

    /* Compare Y */
    isGood &= constantTimeEQ8(0x00, tmpOutput[0]);

    /* Compare lHash and lHash' */
    isGood &= constantTimeCompare(&labelHash[0],
                                  &tmpOutput[1 + hash->length],
                                  hash->length);

    /* Compare that the padding is zero or more zero octets, followed by a
     * 0x01 octet */
    for (i = 1 + (hash->length * 2); i < inputLen; ++i) {
        unsigned char isZero = constantTimeEQ8(0x00, tmpOutput[i]);
        unsigned char isOne = constantTimeEQ8(0x01, tmpOutput[i]);
        /* non-constant time equivalent:
         * if (tmpOutput[i] == 0x01 && !foundPaddingEnd)
         *     paddingOffset = i;
         */
        paddingOffset = constantTimeCondition(isOne & ~foundPaddingEnd, i,
                                              paddingOffset);
        /* non-constant time equivalent:
         * if (tmpOutput[i] == 0x01)
         *    foundPaddingEnd = true;
         *
         * Note: This may yield false positives, as it will be set whenever
         * a 0x01 byte is encountered. If there was bad padding (eg:
         * 0x03 0x02 0x01), foundPaddingEnd will still be set to true, and
         * paddingOffset will still be set to 2.
         */
        foundPaddingEnd = constantTimeCondition(isOne, 1, foundPaddingEnd);
        /* non-constant time equivalent:
         * if (tmpOutput[i] != 0x00 && tmpOutput[i] != 0x01 &&
         *     !foundPaddingEnd) {
         *    isGood = false;
         * }
         *
         * Note: This may yield false positives, as a message (and padding)
         * that is entirely zeros will result in isGood still being true. Thus
         * it's necessary to check foundPaddingEnd is positive below.
         */
        isGood = constantTimeCondition(~foundPaddingEnd & ~isZero, 0, isGood);
    }

    /* While both isGood and foundPaddingEnd may have false positives, they
     * cannot BOTH have false positives. If both are not true, then an invalid
     * message was received. Note, this comparison must still be done in constant
     * time so as not to leak either condition.
     */
    if (!(isGood & foundPaddingEnd)) {
        PORT_SetError(SEC_ERROR_BAD_DATA);
        goto done;
    }

    /* End timing dependent code */

    ++paddingOffset; /* Skip the 0x01 following the end of PS */

    *outputLen = inputLen - paddingOffset;
    if (*outputLen > maxOutputLen) {
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        goto done;
    }

    if (*outputLen)
        PORT_Memcpy(output, &tmpOutput[paddingOffset], *outputLen);
    rv = SECSuccess;

done:
    if (mask)
        PORT_ZFree(mask, maskLen);
    if (tmpOutput)
        PORT_ZFree(tmpOutput, inputLen);
    return rv;
}

/*
 * Generate an EME-OAEP encoded block for encryption
 * Described in RFC 3447, section 7.1.1
 * We use input instead of M for the message to be encrypted
 * label is the optional value L to be associated with the message.
 */
static SECStatus
eme_oaep_encode(unsigned char *em,
                unsigned int emLen,
                const unsigned char *input,
                unsigned int inputLen,
                HASH_HashType hashAlg,
                HASH_HashType maskHashAlg,
                const unsigned char *label,
                unsigned int labelLen,
                const unsigned char *seed,
                unsigned int seedLen)
{
    const SECHashObject *hash;
    void *hashContext;
    SECStatus rv;
    unsigned char *mask;
    unsigned int reservedLen;
    unsigned int dbMaskLen;
    unsigned int i;

    hash = HASH_GetRawHashObject(hashAlg);
    PORT_Assert(seed == NULL || seedLen == hash->length);

    /* Step 1.b */
    reservedLen = (2 * hash->length) + 2;
    if (emLen < reservedLen || inputLen > (emLen - reservedLen)) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }

    /*
     * From RFC 3447, Section 7.1
     *                      +----------+---------+-------+
     *                 DB = |  lHash   |    PS   |   M   |
     *                      +----------+---------+-------+
     *                                     |
     *           +----------+              V
     *           |   seed   |--> MGF ---> xor
     *           +----------+              |
     *                 |                   |
     *        +--+     V                   |
     *        |00|    xor <----- MGF <-----|
     *        +--+     |                   |
     *          |      |                   |
     *          V      V                   V
     *        +--+----------+----------------------------+
     *  EM =  |00|maskedSeed|          maskedDB          |
     *        +--+----------+----------------------------+
     *
     * We use mask to hold the result of the MGF functions, and all other
     * values are generated in their final resting place.
     */
    *em = 0x00;

    /* Step 2.a - Generate lHash */
    hashContext = (*hash->create)();
    if (hashContext == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }
    (*hash->begin)(hashContext);
    if (labelLen > 0)
        (*hash->update)(hashContext, label, labelLen);
    (*hash->end)(hashContext, &em[1 + hash->length], &i, hash->length);
    (*hash->destroy)(hashContext, PR_TRUE);

    /* Step 2.b - Generate PS */
    if (emLen - reservedLen - inputLen > 0) {
        PORT_Memset(em + 1 + (hash->length * 2), 0x00,
                    emLen - reservedLen - inputLen);
    }

    /* Step 2.c. - Generate DB
     * DB = lHash || PS || 0x01 || M
     * Note that PS and lHash have already been placed into em at their
     * appropriate offsets. This just copies M into place
     */
    em[emLen - inputLen - 1] = 0x01;
    if (inputLen)
        PORT_Memcpy(em + emLen - inputLen, input, inputLen);

    if (seed == NULL) {
        /* Step 2.d - Generate seed */
        rv = RNG_GenerateGlobalRandomBytes(em + 1, hash->length);
        if (rv != SECSuccess) {
            return rv;
        }
    } else {
        /* For Known Answer Tests, copy the supplied seed. */
        PORT_Memcpy(em + 1, seed, seedLen);
    }

    /* Step 2.e - Generate dbMask*/
    dbMaskLen = emLen - hash->length - 1;
    mask = (unsigned char *)PORT_Alloc(dbMaskLen);
    if (mask == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }
    MGF1(maskHashAlg, mask, dbMaskLen, em + 1, hash->length);
    /* Step 2.f - Compute maskedDB*/
    for (i = 0; i < dbMaskLen; ++i)
        em[1 + hash->length + i] ^= mask[i];

    /* Step 2.g - Generate seedMask */
    MGF1(maskHashAlg, mask, hash->length, &em[1 + hash->length], dbMaskLen);
    /* Step 2.h - Compute maskedSeed */
    for (i = 0; i < hash->length; ++i)
        em[1 + i] ^= mask[i];

    PORT_ZFree(mask, dbMaskLen);
    return SECSuccess;
}

SECStatus
RSA_EncryptOAEP(RSAPublicKey *key,
                HASH_HashType hashAlg,
                HASH_HashType maskHashAlg,
                const unsigned char *label,
                unsigned int labelLen,
                const unsigned char *seed,
                unsigned int seedLen,
                unsigned char *output,
                unsigned int *outputLen,
                unsigned int maxOutputLen,
                const unsigned char *input,
                unsigned int inputLen)
{
    SECStatus rv = SECFailure;
    unsigned int modulusLen = rsa_modulusLen(&key->modulus);
    unsigned char *oaepEncoded = NULL;

    if (maxOutputLen < modulusLen) {
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        return SECFailure;
    }

    if ((hashAlg == HASH_AlgNULL) || (maskHashAlg == HASH_AlgNULL)) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        return SECFailure;
    }

    if ((labelLen == 0 && label != NULL) ||
        (labelLen > 0 && label == NULL)) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        return SECFailure;
    }

    oaepEncoded = (unsigned char *)PORT_Alloc(modulusLen);
    if (oaepEncoded == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }
    rv = eme_oaep_encode(oaepEncoded, modulusLen, input, inputLen,
                         hashAlg, maskHashAlg, label, labelLen, seed, seedLen);
    if (rv != SECSuccess)
        goto done;

    rv = RSA_PublicKeyOp(key, output, oaepEncoded);
    if (rv != SECSuccess)
        goto done;
    *outputLen = modulusLen;

done:
    PORT_Free(oaepEncoded);
    return rv;
}

SECStatus
RSA_DecryptOAEP(RSAPrivateKey *key,
                HASH_HashType hashAlg,
                HASH_HashType maskHashAlg,
                const unsigned char *label,
                unsigned int labelLen,
                unsigned char *output,
                unsigned int *outputLen,
                unsigned int maxOutputLen,
                const unsigned char *input,
                unsigned int inputLen)
{
    SECStatus rv = SECFailure;
    unsigned int modulusLen = rsa_modulusLen(&key->modulus);
    unsigned char *oaepEncoded = NULL;

    if ((hashAlg == HASH_AlgNULL) || (maskHashAlg == HASH_AlgNULL)) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        return SECFailure;
    }

    if (inputLen != modulusLen) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }

    if ((labelLen == 0 && label != NULL) ||
        (labelLen > 0 && label == NULL)) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        return SECFailure;
    }

    oaepEncoded = (unsigned char *)PORT_Alloc(modulusLen);
    if (oaepEncoded == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }

    rv = RSA_PrivateKeyOpDoubleChecked(key, oaepEncoded, input);
    if (rv != SECSuccess) {
        goto done;
    }
    rv = eme_oaep_decode(output, outputLen, maxOutputLen, oaepEncoded,
                         modulusLen, hashAlg, maskHashAlg, label,
                         labelLen);

done:
    if (oaepEncoded)
        PORT_ZFree(oaepEncoded, modulusLen);
    return rv;
}

/* XXX Doesn't set error code */
SECStatus
RSA_EncryptBlock(RSAPublicKey *key,
                 unsigned char *output,
                 unsigned int *outputLen,
                 unsigned int maxOutputLen,
                 const unsigned char *input,
                 unsigned int inputLen)
{
    SECStatus rv;
    unsigned int modulusLen = rsa_modulusLen(&key->modulus);
    SECItem formatted;
    SECItem unformatted;

    formatted.data = NULL;
    if (maxOutputLen < modulusLen)
        goto failure;

    unformatted.len = inputLen;
    unformatted.data = (unsigned char *)input;
    formatted.data = NULL;
    rv = rsa_FormatBlock(&formatted, modulusLen, RSA_BlockPublic,
                         &unformatted);
    if (rv != SECSuccess)
        goto failure;

    rv = RSA_PublicKeyOp(key, output, formatted.data);
    if (rv != SECSuccess)
        goto failure;

    PORT_ZFree(formatted.data, modulusLen);
    *outputLen = modulusLen;
    return SECSuccess;

failure:
    if (formatted.data != NULL)
        PORT_ZFree(formatted.data, modulusLen);
    return SECFailure;
}

/* XXX Doesn't set error code */
SECStatus
RSA_DecryptBlock(RSAPrivateKey *key,
                 unsigned char *output,
                 unsigned int *outputLen,
                 unsigned int maxOutputLen,
                 const unsigned char *input,
                 unsigned int inputLen)
{
    PRInt8 rv;
    unsigned int modulusLen = rsa_modulusLen(&key->modulus);
    unsigned int i;
    unsigned char *buffer = NULL;
    unsigned int outLen = 0;
    unsigned int copyOutLen = modulusLen - 11;

    if (inputLen != modulusLen || modulusLen < 10) {
        return SECFailure;
    }

    if (copyOutLen > maxOutputLen) {
        copyOutLen = maxOutputLen;
    }

    // Allocate enough space to decrypt + copyOutLen to allow copying outLen later.
    buffer = PORT_ZAlloc(modulusLen + 1 + copyOutLen);
    if (!buffer) {
        return SECFailure;
    }

    // rv is 0 if everything is going well and 1 if an error occurs.
    rv = RSA_PrivateKeyOp(key, buffer, input) != SECSuccess;
    rv |= (buffer[0] != RSA_BLOCK_FIRST_OCTET) |
          (buffer[1] != (unsigned char)RSA_BlockPublic);

    // There have to be at least 8 bytes of padding.
    for (i = 2; i < 10; i++) {
        rv |= buffer[i] == RSA_BLOCK_AFTER_PAD_OCTET;
    }

    for (i = 10; i < modulusLen; i++) {
        unsigned int newLen = modulusLen - i - 1;
        unsigned int c = (buffer[i] == RSA_BLOCK_AFTER_PAD_OCTET) & (outLen == 0);
        outLen = constantTimeCondition(c, newLen, outLen);
    }
    rv |= outLen == 0;
    rv |= outLen > maxOutputLen;

    // Note that output is set even if SECFailure is returned.
    PORT_Memcpy(output, buffer + modulusLen - outLen, copyOutLen);
    *outputLen = constantTimeCondition(outLen > maxOutputLen, maxOutputLen,
                                       outLen);

    PORT_Free(buffer);

    for (i = 1; i < sizeof(rv) * 8; i <<= 1) {
        rv |= rv << i;
    }
    return (SECStatus)rv;
}

/*
 * Encode a RSA-PSS signature.
 * Described in RFC 3447, section 9.1.1.
 * We use mHash instead of M as input.
 * emBits from the RFC is just modBits - 1, see section 8.1.1.
 * We only support MGF1 as the MGF.
 */
static SECStatus
emsa_pss_encode(unsigned char *em,
                unsigned int emLen,
                unsigned int emBits,
                const unsigned char *mHash,
                HASH_HashType hashAlg,
                HASH_HashType maskHashAlg,
                const unsigned char *salt,
                unsigned int saltLen)
{
    const SECHashObject *hash;
    void *hash_context;
    unsigned char *dbMask;
    unsigned int dbMaskLen;
    unsigned int i;
    SECStatus rv;

    hash = HASH_GetRawHashObject(hashAlg);
    dbMaskLen = emLen - hash->length - 1;

    /* Step 3 */
    if (emLen < hash->length + saltLen + 2) {
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        return SECFailure;
    }

    /* Step 4 */
    if (salt == NULL) {
        rv = RNG_GenerateGlobalRandomBytes(&em[dbMaskLen - saltLen], saltLen);
        if (rv != SECSuccess) {
            return rv;
        }
    } else {
        PORT_Memcpy(&em[dbMaskLen - saltLen], salt, saltLen);
    }

    /* Step 5 + 6 */
    /* Compute H and store it at its final location &em[dbMaskLen]. */
    hash_context = (*hash->create)();
    if (hash_context == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }
    (*hash->begin)(hash_context);
    (*hash->update)(hash_context, eightZeros, 8);
    (*hash->update)(hash_context, mHash, hash->length);
    (*hash->update)(hash_context, &em[dbMaskLen - saltLen], saltLen);
    (*hash->end)(hash_context, &em[dbMaskLen], &i, hash->length);
    (*hash->destroy)(hash_context, PR_TRUE);

    /* Step 7 + 8 */
    PORT_Memset(em, 0, dbMaskLen - saltLen - 1);
    em[dbMaskLen - saltLen - 1] = 0x01;

    /* Step 9 */
    dbMask = (unsigned char *)PORT_Alloc(dbMaskLen);
    if (dbMask == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }
    MGF1(maskHashAlg, dbMask, dbMaskLen, &em[dbMaskLen], hash->length);

    /* Step 10 */
    for (i = 0; i < dbMaskLen; i++)
        em[i] ^= dbMask[i];
    PORT_Free(dbMask);

    /* Step 11 */
    em[0] &= 0xff >> (8 * emLen - emBits);

    /* Step 12 */
    em[emLen - 1] = 0xbc;

    return SECSuccess;
}

/*
 * Verify a RSA-PSS signature.
 * Described in RFC 3447, section 9.1.2.
 * We use mHash instead of M as input.
 * emBits from the RFC is just modBits - 1, see section 8.1.2.
 * We only support MGF1 as the MGF.
 */
static SECStatus
emsa_pss_verify(const unsigned char *mHash,
                const unsigned char *em,
                unsigned int emLen,
                unsigned int emBits,
                HASH_HashType hashAlg,
                HASH_HashType maskHashAlg,
                unsigned int saltLen)
{
    const SECHashObject *hash;
    void *hash_context;
    unsigned char *db;
    unsigned char *H_; /* H' from the RFC */
    unsigned int i;
    unsigned int dbMaskLen;
    unsigned int zeroBits;
    SECStatus rv;

    hash = HASH_GetRawHashObject(hashAlg);
    dbMaskLen = emLen - hash->length - 1;

    /* Step 3 + 4 */
    if ((emLen < (hash->length + saltLen + 2)) ||
        (em[emLen - 1] != 0xbc)) {
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
        return SECFailure;
    }

    /* Step 6 */
    zeroBits = 8 * emLen - emBits;
    if (em[0] >> (8 - zeroBits)) {
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
        return SECFailure;
    }

    /* Step 7 */
    db = (unsigned char *)PORT_Alloc(dbMaskLen);
    if (db == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }
    /* &em[dbMaskLen] points to H, used as mgfSeed */
    MGF1(maskHashAlg, db, dbMaskLen, &em[dbMaskLen], hash->length);

    /* Step 8 */
    for (i = 0; i < dbMaskLen; i++) {
        db[i] ^= em[i];
    }

    /* Step 9 */
    db[0] &= 0xff >> zeroBits;

    /* Step 10 */
    for (i = 0; i < (dbMaskLen - saltLen - 1); i++) {
        if (db[i] != 0) {
            PORT_Free(db);
            PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
            return SECFailure;
        }
    }
    if (db[dbMaskLen - saltLen - 1] != 0x01) {
        PORT_Free(db);
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
        return SECFailure;
    }

    /* Step 12 + 13 */
    H_ = (unsigned char *)PORT_Alloc(hash->length);
    if (H_ == NULL) {
        PORT_Free(db);
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }
    hash_context = (*hash->create)();
    if (hash_context == NULL) {
        PORT_Free(db);
        PORT_Free(H_);
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }
    (*hash->begin)(hash_context);
    (*hash->update)(hash_context, eightZeros, 8);
    (*hash->update)(hash_context, mHash, hash->length);
    (*hash->update)(hash_context, &db[dbMaskLen - saltLen], saltLen);
    (*hash->end)(hash_context, H_, &i, hash->length);
    (*hash->destroy)(hash_context, PR_TRUE);

    PORT_Free(db);

    /* Step 14 */
    if (PORT_Memcmp(H_, &em[dbMaskLen], hash->length) != 0) {
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
        rv = SECFailure;
    } else {
        rv = SECSuccess;
    }

    PORT_Free(H_);
    return rv;
}

SECStatus
RSA_SignPSS(RSAPrivateKey *key,
            HASH_HashType hashAlg,
            HASH_HashType maskHashAlg,
            const unsigned char *salt,
            unsigned int saltLength,
            unsigned char *output,
            unsigned int *outputLen,
            unsigned int maxOutputLen,
            const unsigned char *input,
            unsigned int inputLen)
{
    SECStatus rv = SECSuccess;
    unsigned int modulusLen = rsa_modulusLen(&key->modulus);
    unsigned int modulusBits = rsa_modulusBits(&key->modulus);
    unsigned int emLen = modulusLen;
    unsigned char *pssEncoded, *em;

    if (maxOutputLen < modulusLen) {
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        return SECFailure;
    }

    if ((hashAlg == HASH_AlgNULL) || (maskHashAlg == HASH_AlgNULL)) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        return SECFailure;
    }

    pssEncoded = em = (unsigned char *)PORT_Alloc(modulusLen);
    if (pssEncoded == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }

    /* len(em) == ceil((modulusBits - 1) / 8). */
    if (modulusBits % 8 == 1) {
        em[0] = 0;
        emLen--;
        em++;
    }
    rv = emsa_pss_encode(em, emLen, modulusBits - 1, input, hashAlg,
                         maskHashAlg, salt, saltLength);
    if (rv != SECSuccess)
        goto done;

    // This sets error codes upon failure.
    rv = RSA_PrivateKeyOpDoubleChecked(key, output, pssEncoded);
    *outputLen = modulusLen;

done:
    PORT_Free(pssEncoded);
    return rv;
}

SECStatus
RSA_CheckSignPSS(RSAPublicKey *key,
                 HASH_HashType hashAlg,
                 HASH_HashType maskHashAlg,
                 unsigned int saltLength,
                 const unsigned char *sig,
                 unsigned int sigLen,
                 const unsigned char *hash,
                 unsigned int hashLen)
{
    SECStatus rv;
    unsigned int modulusLen = rsa_modulusLen(&key->modulus);
    unsigned int modulusBits = rsa_modulusBits(&key->modulus);
    unsigned int emLen = modulusLen;
    unsigned char *buffer, *em;

    if (sigLen != modulusLen) {
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
        return SECFailure;
    }

    if ((hashAlg == HASH_AlgNULL) || (maskHashAlg == HASH_AlgNULL)) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        return SECFailure;
    }

    buffer = em = (unsigned char *)PORT_Alloc(modulusLen);
    if (!buffer) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }

    rv = RSA_PublicKeyOp(key, buffer, sig);
    if (rv != SECSuccess) {
        PORT_Free(buffer);
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
        return SECFailure;
    }

    /* len(em) == ceil((modulusBits - 1) / 8). */
    if (modulusBits % 8 == 1) {
        emLen--;
        em++;
    }
    rv = emsa_pss_verify(hash, em, emLen, modulusBits - 1, hashAlg,
                         maskHashAlg, saltLength);

    PORT_Free(buffer);
    return rv;
}

SECStatus
RSA_Sign(RSAPrivateKey *key,
         unsigned char *output,
         unsigned int *outputLen,
         unsigned int maxOutputLen,
         const unsigned char *input,
         unsigned int inputLen)
{
    SECStatus rv = SECFailure;
    unsigned int modulusLen = rsa_modulusLen(&key->modulus);
    SECItem formatted = { siBuffer, NULL, 0 };
    SECItem unformatted = { siBuffer, (unsigned char *)input, inputLen };

    if (maxOutputLen < modulusLen) {
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        goto done;
    }

    rv = rsa_FormatBlock(&formatted, modulusLen, RSA_BlockPrivate,
                         &unformatted);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        goto done;
    }

    // This sets error codes upon failure.
    rv = RSA_PrivateKeyOpDoubleChecked(key, output, formatted.data);
    *outputLen = modulusLen;

done:
    if (formatted.data != NULL) {
        PORT_ZFree(formatted.data, modulusLen);
    }
    return rv;
}

SECStatus
RSA_CheckSign(RSAPublicKey *key,
              const unsigned char *sig,
              unsigned int sigLen,
              const unsigned char *data,
              unsigned int dataLen)
{
    SECStatus rv = SECFailure;
    unsigned int modulusLen = rsa_modulusLen(&key->modulus);
    unsigned int i;
    unsigned char *buffer = NULL;

    if (sigLen != modulusLen) {
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
        goto done;
    }

    /*
     * 0x00 || BT || Pad || 0x00 || ActualData
     *
     * The "3" below is the first octet + the second octet + the 0x00
     * octet that always comes just before the ActualData.
     */
    if (dataLen > modulusLen - (3 + RSA_BLOCK_MIN_PAD_LEN)) {
        PORT_SetError(SEC_ERROR_BAD_DATA);
        goto done;
    }

    buffer = (unsigned char *)PORT_Alloc(modulusLen + 1);
    if (!buffer) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto done;
    }

    if (RSA_PublicKeyOp(key, buffer, sig) != SECSuccess) {
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
        goto done;
    }

    /*
     * check the padding that was used
     */
    if (buffer[0] != RSA_BLOCK_FIRST_OCTET ||
        buffer[1] != (unsigned char)RSA_BlockPrivate) {
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
        goto done;
    }
    for (i = 2; i < modulusLen - dataLen - 1; i++) {
        if (buffer[i] != RSA_BLOCK_PRIVATE_PAD_OCTET) {
            PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
            goto done;
        }
    }
    if (buffer[i] != RSA_BLOCK_AFTER_PAD_OCTET) {
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
        goto done;
    }

    /*
     * make sure we get the same results
     */
    if (PORT_Memcmp(buffer + modulusLen - dataLen, data, dataLen) == 0) {
        rv = SECSuccess;
    }

done:
    if (buffer) {
        PORT_Free(buffer);
    }
    return rv;
}

SECStatus
RSA_CheckSignRecover(RSAPublicKey *key,
                     unsigned char *output,
                     unsigned int *outputLen,
                     unsigned int maxOutputLen,
                     const unsigned char *sig,
                     unsigned int sigLen)
{
    SECStatus rv = SECFailure;
    unsigned int modulusLen = rsa_modulusLen(&key->modulus);
    unsigned int i;
    unsigned char *buffer = NULL;
    unsigned int padLen;

    if (sigLen != modulusLen) {
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
        goto done;
    }

    buffer = (unsigned char *)PORT_Alloc(modulusLen + 1);
    if (!buffer) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto done;
    }

    if (RSA_PublicKeyOp(key, buffer, sig) != SECSuccess) {
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
        goto done;
    }

    *outputLen = 0;

    /*
     * check the padding that was used
     */
    if (buffer[0] != RSA_BLOCK_FIRST_OCTET ||
        buffer[1] != (unsigned char)RSA_BlockPrivate) {
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
        goto done;
    }
    for (i = 2; i < modulusLen; i++) {
        if (buffer[i] == RSA_BLOCK_AFTER_PAD_OCTET) {
            *outputLen = modulusLen - i - 1;
            break;
        }
        if (buffer[i] != RSA_BLOCK_PRIVATE_PAD_OCTET) {
            PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
            goto done;
        }
    }
    padLen = i - 2;
    if (padLen < RSA_BLOCK_MIN_PAD_LEN) {
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
        goto done;
    }
    if (*outputLen == 0) {
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
        goto done;
    }
    if (*outputLen > maxOutputLen) {
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        goto done;
    }

    PORT_Memcpy(output, buffer + modulusLen - *outputLen, *outputLen);
    rv = SECSuccess;

done:
    if (buffer) {
        PORT_Free(buffer);
    }
    return rv;
}
