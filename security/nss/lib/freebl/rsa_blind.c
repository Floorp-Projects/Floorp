/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of RSA Blind Signatures.
 * (https://datatracker.ietf.org/doc/draft-irtf-cfrg-rsa-blind-signatures/)
 */
#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "secerr.h"
#include "blapi.h"
#include "mpi.h"
#include "secitem.h"
#include "prerr.h"
#include "blapii.h"
#include "secmpi.h"
#include "mpi-priv.h"
#include "pqg.h"

/*#define RSA_DEBUG*/

#define MP_DIGIT_BYTE (MP_DIGIT_BIT / PR_BITS_PER_BYTE)

#ifdef RSA_DEBUG

void
rsaBlind_Print(PRUint8* m, size_t t)
{
    for (int i = 0; i < t; i++) {
        if (i % 16 == 0)
            printf("\n");
        printf("%02x ", m[i]);
    }
    printf("\n \n");
}

void
mp_print_buf(mp_int* mp)
{
    for (int i = MP_USED(mp) - 1; i >= 0; i--) {
        if (i % 2 == 1)
            printf("\n");
        printf("%016lx  ", (long unsigned int)MP_DIGIT(mp, i));
    }
    printf("\n \n");
}
#endif

/*
 * 4.1. Prepare
 * There are two types of preparation functions:
 * an identity preparation function, and a randomized preparation function.
 * The identity preparation function returns the input message without transformation,
 * i.e., msg = PrepareIdentity(msg).
 * The randomized preparation function augments the input message with fresh randomness.
 *
 * Inputs:
 * - msg, message to be signed, a byte string
 *
 * Outputs:
 * - input_msg, a byte string that is 32 bytes longer than msg

 * Steps:
 *  1. msgPrefix = random(32)
 *  2. input_msg = concat(msgPrefix, msg)
 *  3. output input_msg
 */

SECStatus
RSABlinding_Prepare(PRUint8* preparedMessage, size_t preparedMessageLen, const PRUint8* msg,
                    size_t msgLen, PRBool isDeterministic)
{
    if (!preparedMessage || !msg) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* The identity preparation function: */
    if (isDeterministic) {
        if (preparedMessageLen < msgLen) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
        }
        PORT_Memcpy(preparedMessage, msg, msgLen);
    }
    /* The randomized preparation function: */
    else {
        /* 1. msgPrefix = random(32)*/
        PRUint8 lenRandom = 32;
        if (msgLen > UINT32_MAX - lenRandom) {
            PORT_SetError(SEC_ERROR_INPUT_LEN);
            return SECFailure;
        }
        if (preparedMessageLen < msgLen + lenRandom) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
        }

        RNG_GenerateGlobalRandomBytes(preparedMessage, lenRandom);
        /* 2. input_msg = concat(msgPrefix, msg)*/
        PORT_Memcpy(preparedMessage + lenRandom, msg, msgLen);
    }

    return SECSuccess;
}

/* RSA Blind Signatures
 * Blind(pkS, msg)
 * Parameters:
 * - kLen, the length in bytes of the RSA modulus n
 * - Hash, the hash function used to hash the message
 * - MGF, the mask generation function
 * - sLen, the length in bytes of the salt
 *
 * Inputs:
 * - pkS, server public key (n, e)
 * - msg, message to be signed, a byte string
 *
 * Outputs:
 * - blinded_msg, a byte string of length kLen
 * - inv, an integer used to unblind the signature in Finalize
 */

/* The length of the random buffer is n. */
SECStatus
RSABlinding_Blind(HASH_HashType hashAlg, PRUint8* blindedMsg, size_t blindedMsgLen,
                  PRUint8* inv, size_t invLen, const PRUint8* msg, size_t msgLen,
                  const PRUint8* salt, size_t saltLen,
                  RSAPublicKey* pkS, const PRUint8* randomBuf, size_t randomBufLen)
{
    if (!blindedMsgLen || !inv || !msg || !pkS) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    mp_err err = MP_OKAY;
    const size_t modulus_len = pkS->modulus.len;

    if (blindedMsgLen != modulus_len || invLen != modulus_len) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (randomBufLen != 0 && randomBufLen != modulus_len) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if ((PRUint64)pkS->modulus.len * PR_BITS_PER_BYTE - 1 > UINT32_MAX) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }

    PRUint8* encoded_msg = PORT_ZAlloc(modulus_len);
    PRUint8* rBuf = PORT_ZAlloc(modulus_len);
    PRUint8* xBuf = PORT_ZAlloc(modulus_len);

    mp_int m, n, r, mask, invR, rsavp1, blindSign;
    MP_DIGITS(&m) = 0;
    MP_DIGITS(&invR) = 0;
    MP_DIGITS(&rsavp1) = 0;
    MP_DIGITS(&blindSign) = 0;
    MP_DIGITS(&n) = 0;
    MP_DIGITS(&r) = 0;
    MP_DIGITS(&mask) = 0;

    CHECK_MPI_OK(mp_init(&m));
    CHECK_MPI_OK(mp_init(&invR));
    CHECK_MPI_OK(mp_init(&rsavp1));
    CHECK_MPI_OK(mp_init(&blindSign));
    CHECK_MPI_OK(mp_init(&r));
    CHECK_MPI_OK(mp_init(&n));
    CHECK_MPI_OK(mp_init(&mask));

    CHECK_MPI_OK(mp_read_unsigned_octets(&n, pkS->modulus.data, pkS->modulus.len));
    SECStatus rv = SECFailure;
    size_t bit_len_n = pkS->modulus.len * PR_BITS_PER_BYTE - 1;

    if (!randomBuf || randomBufLen == 0) {
        CHECK_MPI_OK(mp_2expt(&mask, bit_len_n + 1));
        CHECK_MPI_OK(mp_sub_d(&mask, 1, &mask));
        do {
            CHECK_MPI_OK(mpp_random_secure(&r));
            for (size_t i = 0; i < mask.alloc; i++) {
                r.dp[i] = mask.dp[i] & r.dp[i];
            }
        } while (mp_cmp(&r, &n) != MP_LT);
        CHECK_MPI_OK(mp_init_copy(&r, &mask));
    } else {
        CHECK_MPI_OK(mp_read_unsigned_octets(&r, randomBuf, pkS->modulus.len));
    }

    /* 1. encoded_msg = EMSA-PSS-ENCODE(msg, bit_len(n)). */
    PRUint8 msgHash[HASH_LENGTH_MAX] = { 0 };
    rv = PQG_HashBuf(hashAlg, msgHash, msg, msgLen);
    if (rv != SECSuccess) {
        goto cleanup;
    }

    rv = RSA_EMSAEncodePSS(encoded_msg, pkS->modulus.len, bit_len_n, msgHash, hashAlg, hashAlg, salt, saltLen);

    /* 2. If EMSA-PSS-ENCODE raises an error, raise the error and stop. */
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_FAILED_TO_ENCODE_DATA);
        goto cleanup;
    }

#ifdef RSA_DEBUG
    printf("encoded_msg: \n");
    rsaBlind_Print(encoded_msg, modulus_len);
#endif

    /* 3. m = bytes_to_int(encoded_msg). */
    CHECK_MPI_OK(mp_read_unsigned_octets(&m, encoded_msg, pkS->modulus.len));

    /* 4. c = mp_is_coprime(m, n).
     ** 5. If c is false, raise an "invalid input" error and stop.    
     ** 7. inv = inverse_mod(r, n)
     */
    err = mp_invmod(&r, &n, &invR);
    if (err == MP_UNDEF) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto cleanup;
    } else if (err) {
        goto cleanup;
    }

#ifdef RSA_DEBUG
    printf("inverse r: \n");
    mp_print_buf(&invR);
#endif

    /* 9. x = RSAVP1(pkS, r)*/
    CHECK_MPI_OK(mp_to_fixlen_octets(&r, rBuf, pkS->modulus.len));
    rv = RSA_PublicKeyOp(pkS, xBuf, rBuf);
    if (rv != SECSuccess) {
        goto cleanup;
    }
    CHECK_MPI_OK(mp_read_unsigned_octets(&rsavp1, xBuf, pkS->modulus.len));

#ifdef RSA_DEBUG
    printf("x (RSAVP1): \n");
    mp_print_buf(&rsavp1);
#endif

    /* 10. z = m * x mod n*/
    CHECK_MPI_OK(mp_mulmod(&m, &rsavp1, &n, &blindSign));

#ifdef RSA_DEBUG
    printf("blindSign: \n");
    mp_print_buf(&blindSign);
#endif

    CHECK_MPI_OK(mp_to_fixlen_octets(&blindSign, blindedMsg, blindedMsgLen));
    CHECK_MPI_OK(mp_to_fixlen_octets(&invR, inv, invLen));

cleanup:
    mp_clear(&m);
    mp_clear(&n);
    mp_clear(&r);
    mp_clear(&invR);
    mp_clear(&rsavp1);
    mp_clear(&blindSign);
    mp_clear(&mask);

    PORT_Free(encoded_msg);
    PORT_Free(rBuf);
    PORT_Free(xBuf);

    if (err) {
        MP_TO_SEC_ERROR(err);
        return SECFailure;
    }

    return rv;
}

/* 4.3. BlindSign
 * BlindSign(skS, blinded_msg)
 *
 * Parameters:
 *  - kLen, the length in bytes of the RSA modulus n
 *
 * Inputs:
 *  - skS, server private key
 *  - blinded_msg, encoded and blinded message to be signed, a byte string
 */

SECStatus
RSABlinding_BlindSign(PRUint8* blindSig, size_t blindSigLen,
                      const PRUint8* blindedMsg, size_t blindedMsgLen, RSAPrivateKey* skS, RSAPublicKey* pkS)
{
    SECStatus rv = SECSuccess;
    if (!blindSig || !blindedMsg || !skS) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if ((blindSigLen != skS->modulus.len) || (skS->modulus.len != pkS->modulus.len) || (blindedMsgLen != skS->modulus.len)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    PRUint8* sBuf = (PRUint8*)PORT_Alloc(skS->modulus.len);
    PRUint8* mPrimeBuf = (PRUint8*)PORT_Alloc(pkS->modulus.len);

    mp_err err = MP_OKAY;
    mp_int z, mPrime;
    MP_DIGITS(&z) = 0;
    MP_DIGITS(&mPrime) = 0;

    CHECK_MPI_OK(mp_init(&z));
    CHECK_MPI_OK(mp_init(&mPrime));

    CHECK_MPI_OK(mp_read_unsigned_octets(&z, blindedMsg, skS->modulus.len));

    /* 2. s = rsasp1(skS, z). */
    rv = RSA_PrivateKeyOp(skS, sBuf, blindedMsg);
    if (rv != SECSuccess) {
        goto cleanup;
    }

#ifdef RSA_DEBUG
    printf("Blinded Signature: \n");
    mp_print_buf(&s);
#endif

    /* 3. mPrime = rsavp1(pkS, s). */
    rv = RSA_PublicKeyOp(pkS, mPrimeBuf, sBuf);
    if (rv != SECSuccess) {
        goto cleanup;
    }

    CHECK_MPI_OK(mp_read_unsigned_octets(&mPrime, mPrimeBuf, skS->modulus.len));

#ifdef RSA_DEBUG
    printf("mPrime: \n");
    mp_print_buf(&mPrime);
#endif

    /* 4. If m != m', raise "signing failure" and stop. */
    PRBool isBlindedMsgCorrect = mp_cmp(&mPrime, &z) == 0;

    /* 5. blind_sig = int_to_bytes(s, kLen). */
    if (isBlindedMsgCorrect) {
        PORT_Memcpy(blindSig, sBuf, skS->modulus.len);
    }

cleanup:
    mp_clear(&z);
    mp_clear(&mPrime);

    PORT_Free(sBuf);
    PORT_Free(mPrimeBuf);

    if (err) {
        MP_TO_SEC_ERROR(err);
        return SECFailure;
    }
    if (rv != SECSuccess) {
        return SECFailure;
    }

    return SECSuccess;
}

/*
 * 4.4. Finalize.
 * Finalize validates the server's response, unblinds the message to produce a signature,
 * verifies it for correctness, and outputs the signature upon success.
 *
 *  Parameters:
 * - kLen, the length in bytes of the RSA modulus n
 * - Hash, the hash function used to hash the message
 * - MGF, the mask generation function
 * - sLen, the length in bytes of the salt
 *
 * Inputs:
 * - pkS, server public key (n, e)
 * - msg, message to be signed, a byte string
 * - blind_sig, signed and blinded element, a byte string of
 *   length kLen
 * - inv, inverse of the blind, an integer
 *
 * Outputs:
 * - sig, a byte string of length kLen
 *
 * Blinded Signature Len should be the same as modulus len. 
 */

SECStatus
RSABlinding_Finalize(HASH_HashType hashAlg, PRUint8* signature, const PRUint8* msg, PRUint32 msgLen,
                     const PRUint8* blindSig, size_t blindSigLen,
                     const PRUint8* inv, size_t invLen, RSAPublicKey* pkS, size_t saltLen)
{
    if (!signature || !msg || !blindSig || !inv || !pkS || msgLen == 0) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (blindSigLen != pkS->modulus.len || invLen != pkS->modulus.len) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    mp_err err = MP_OKAY;
    SECStatus rv = SECFailure;

    mp_int inv_mp, blindSig_mp, n_mp, sig_mp;
    MP_DIGITS(&inv_mp) = 0;
    MP_DIGITS(&blindSig_mp) = 0;
    MP_DIGITS(&n_mp) = 0;
    MP_DIGITS(&sig_mp) = 0;

    CHECK_MPI_OK(mp_init(&n_mp));
    CHECK_MPI_OK(mp_init(&inv_mp));
    CHECK_MPI_OK(mp_init(&blindSig_mp));
    CHECK_MPI_OK(mp_init(&sig_mp));

    CHECK_MPI_OK(mp_read_unsigned_octets(&n_mp, pkS->modulus.data, pkS->modulus.len));
    CHECK_MPI_OK(mp_read_unsigned_octets(&blindSig_mp, blindSig, pkS->modulus.len));
    CHECK_MPI_OK(mp_read_unsigned_octets(&inv_mp, inv, pkS->modulus.len));

    /* 3. s = z * inv mod n. */
    CHECK_MPI_OK(mp_mulmod(&blindSig_mp, &inv_mp, &n_mp, &sig_mp));

#ifdef RSA_DEBUG
    printf("Computed Signature : \n");
    mp_print_buf(&sig_mp);
#endif

    CHECK_MPI_OK(mp_to_fixlen_octets(&sig_mp, signature, pkS->modulus.len));

    PRUint8 mHash[HASH_LENGTH_MAX] = { 0 };
    rv = PQG_HashBuf(hashAlg, mHash, msg, msgLen);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_BAD_DATA);
        goto cleanup;
    }

    /* 5. result = RSASSA-PSS-VERIFY(pkS, msg, sig) with Hash, MGF, and sLen as defined in the parameters. */
    rv = RSA_CheckSignPSS(pkS, hashAlg, hashAlg, saltLen, signature, sig_mp.used * MP_DIGIT_BYTE, mHash, 0);

    /* If result = "valid signature", output sig, else raise "invalid signature" and stop. */
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
    }

#ifdef RSA_DEBUG
    if (rv == SECFailure) {
        printf("%s\n", "RSA CheckSignPSS has failed. ");
    } else {
        printf("%s\n", "RSA CheckSignPSS has succeeded. ");
    }
#endif

cleanup:
    mp_clear(&inv_mp);
    mp_clear(&blindSig_mp);
    mp_clear(&n_mp);
    mp_clear(&sig_mp);
    if (err) {
        MP_TO_SEC_ERROR(err);
        return SECFailure;
    }

    return rv;
}
