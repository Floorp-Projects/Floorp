/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "blapi.h"
#include "blapit.h"
#include "chacha20.h"
#include "nssilock.h"
#include "seccomon.h"
#include "secerr.h"

static unsigned long globalNumCalls = 0;

SECStatus
prng_ResetForFuzzing(PZLock *rng_lock)
{
    /* Check for a valid RNG lock. */
    PORT_Assert(rng_lock != NULL);
    if (rng_lock == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* --- LOCKED --- */
    PZ_Lock(rng_lock);
    globalNumCalls = 0;
    PZ_Unlock(rng_lock);
    /* --- UNLOCKED --- */

    return SECSuccess;
}

SECStatus
prng_GenerateDeterministicRandomBytes(PZLock *rng_lock, void *dest, size_t len)
{
    static const uint8_t key[32];
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
    ChaCha20XOR(dest, dest, len, key, nonce, 0);
    ChaCha20Poly1305_DestroyContext(cx, PR_TRUE);

    PZ_Unlock(rng_lock);
    /* --- UNLOCKED --- */
    return SECSuccess;
}
