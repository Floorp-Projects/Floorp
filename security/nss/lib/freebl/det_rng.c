/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "blapi.h"
#include "blapit.h"
#include "Hacl_Chacha20.h"
#include "nssilock.h"
#include "seccomon.h"
#include "secerr.h"
#include "prinit.h"

#define GLOBAL_BYTES_SIZE 100
static PRUint8 globalBytes[GLOBAL_BYTES_SIZE];
static unsigned long globalNumCalls = 0;
static PZLock *rng_lock = NULL;
static PRCallOnceType coRNGInit;
static const PRCallOnceType pristineCallOnce;

static PRStatus
rng_init(void)
{
    rng_lock = PZ_NewLock(nssILockOther);
    if (!rng_lock) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return PR_FAILURE;
    }
    /* --- LOCKED --- */
    PZ_Lock(rng_lock);
    memset(globalBytes, 0, GLOBAL_BYTES_SIZE);
    PZ_Unlock(rng_lock);
    /* --- UNLOCKED --- */

    return PR_SUCCESS;
}

SECStatus
RNG_RNGInit(void)
{
    /* Allow only one call to initialize the context */
    if (PR_CallOnce(&coRNGInit, rng_init) != PR_SUCCESS) {
        return SECFailure;
    }

    return SECSuccess;
}

/* Take min(size, GLOBAL_BYTES_SIZE) bytes from data and use as seed and reset
 * the rng state. */
SECStatus
RNG_RandomUpdate(const void *data, size_t bytes)
{
    /* Check for a valid RNG lock. */
    PORT_Assert(rng_lock != NULL);
    if (rng_lock == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* --- LOCKED --- */
    PZ_Lock(rng_lock);
    memset(globalBytes, 0, GLOBAL_BYTES_SIZE);
    globalNumCalls = 0;
    if (data) {
        memcpy(globalBytes, (PRUint8 *)data, PR_MIN(bytes, GLOBAL_BYTES_SIZE));
    }
    PZ_Unlock(rng_lock);
    /* --- UNLOCKED --- */

    return SECSuccess;
}

SECStatus
RNG_GenerateGlobalRandomBytes(void *dest, size_t len)
{
    static const uint8_t key[32] = { 0 };
    uint8_t nonce[12] = { 0 };

    /* Check for a valid RNG lock. */
    PORT_Assert(rng_lock != NULL);
    if (rng_lock == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* --- LOCKED --- */
    PZ_Lock(rng_lock);

    memcpy(nonce, &globalNumCalls, sizeof(globalNumCalls));
    globalNumCalls++;

    ChaCha20Poly1305Context *cx =
        ChaCha20Poly1305_CreateContext(key, sizeof(key), 16);
    if (!cx) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        PZ_Unlock(rng_lock);
        return SECFailure;
    }

    memset(dest, 0, len);
    memcpy(dest, globalBytes, PR_MIN(len, GLOBAL_BYTES_SIZE));
    Hacl_Chacha20_chacha20_encrypt(len, (uint8_t *)dest, (uint8_t *)dest,
                                   (uint8_t *)key, nonce, 0);
    ChaCha20Poly1305_DestroyContext(cx, PR_TRUE);

    PZ_Unlock(rng_lock);
    /* --- UNLOCKED --- */

    return SECSuccess;
}

void
RNG_RNGShutdown(void)
{
    if (rng_lock) {
        PZ_DestroyLock(rng_lock);
        rng_lock = NULL;
    }
    coRNGInit = pristineCallOnce;
}

/* Test functions are not implemented! */
SECStatus
PRNGTEST_Instantiate(const PRUint8 *entropy, unsigned int entropy_len,
                     const PRUint8 *nonce, unsigned int nonce_len,
                     const PRUint8 *personal_string, unsigned int ps_len)
{
    return SECFailure;
}

SECStatus
PRNGTEST_Reseed(const PRUint8 *entropy, unsigned int entropy_len,
                const PRUint8 *additional, unsigned int additional_len)
{
    return SECFailure;
}

SECStatus
PRNGTEST_Generate(PRUint8 *bytes, unsigned int bytes_len,
                  const PRUint8 *additional, unsigned int additional_len)
{
    return SECFailure;
}

SECStatus
PRNGTEST_Uninstantiate()
{
    return SECFailure;
}

SECStatus
PRNGTEST_RunHealthTests()
{
    return SECFailure;
}

SECStatus
PRNGTEST_Instantiate_Kat(const PRUint8 *entropy, unsigned int entropy_len,
                         const PRUint8 *nonce, unsigned int nonce_len,
                         const PRUint8 *personal_string, unsigned int ps_len)
{
    return SECFailure;
}
