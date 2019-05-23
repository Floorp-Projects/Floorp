/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Anti-replay measures for TLS 1.3.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nss.h"      /* for NSS_RegisterShutdown */
#include "nssilock.h" /* for PZMonitor */
#include "pk11pub.h"
#include "prinit.h" /* for PR_CallOnce */
#include "prmon.h"
#include "prtime.h"
#include "secerr.h"
#include "ssl.h"
#include "sslbloom.h"
#include "sslimpl.h"
#include "tls13hkdf.h"

static struct {
    /* Used to ensure that we only initialize the cleanup function once. */
    PRCallOnceType init;
    /* Used to serialize access to the filters. */
    PZMonitor *lock;
    /* The filters, use of which alternates. */
    sslBloomFilter filters[2];
    /* Which of the two filters is active (0 or 1). */
    PRUint8 current;
    /* The time that we will next update. */
    PRTime nextUpdate;
    /* The width of the window; i.e., the period of updates. */
    PRTime window;
    /* This key ensures that the bloom filter index is unpredictable. */
    PK11SymKey *key;
} ssl_anti_replay;

/* Clear the current state and free any resources we allocated. The signature
 * here is odd to allow this to be called during shutdown. */
static SECStatus
tls13_AntiReplayReset(void *appData, void *nssData)
{
    if (ssl_anti_replay.key) {
        PK11_FreeSymKey(ssl_anti_replay.key);
        ssl_anti_replay.key = NULL;
    }
    if (ssl_anti_replay.lock) {
        PZ_DestroyMonitor(ssl_anti_replay.lock);
        ssl_anti_replay.lock = NULL;
    }
    sslBloom_Destroy(&ssl_anti_replay.filters[0]);
    sslBloom_Destroy(&ssl_anti_replay.filters[1]);
    return SECSuccess;
}

static PRStatus
tls13_AntiReplayInit(void)
{
    SECStatus rv = NSS_RegisterShutdown(tls13_AntiReplayReset, NULL);
    if (rv != SECSuccess) {
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}

static SECStatus
tls13_AntiReplayKeyGen()
{
    PRUint8 buf[32];
    SECItem keyItem = { siBuffer, buf, sizeof(buf) };
    PK11SlotInfo *slot;
    SECStatus rv;

    slot = PK11_GetInternalSlot();
    if (!slot) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    rv = PK11_GenerateRandomOnSlot(slot, buf, sizeof(buf));
    if (rv != SECSuccess) {
        goto loser;
    }

    ssl_anti_replay.key = PK11_ImportSymKey(slot, CKM_NSS_HKDF_SHA256,
                                            PK11_OriginUnwrap, CKA_DERIVE,
                                            &keyItem, NULL);
    if (!ssl_anti_replay.key) {
        goto loser;
    }

    PK11_FreeSlot(slot);
    return SECSuccess;

loser:
    PK11_FreeSlot(slot);
    return SECFailure;
}

/* Set a limit on the combination of number of hashes and bits in each hash. */
#define SSL_MAX_BLOOM_FILTER_SIZE 64

/*
 * The structures created by this function can be called concurrently on
 * multiple threads if the server is multi-threaded.  A monitor is used to
 * ensure that only one thread can access the structures that change over time,
 * but no such guarantee is provided for configuration data.
 *
 * Functions that read from static configuration data depend on there being a
 * memory barrier between the setup and use of this function.
 */
SECStatus
SSLExp_InitAntiReplay(PRTime now, PRTime window, unsigned int k, unsigned int bits)
{
    SECStatus rv;

    if (k == 0 || bits == 0) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    if ((k * (bits + 7) / 8) > SSL_MAX_BLOOM_FILTER_SIZE) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (PR_SUCCESS != PR_CallOnce(&ssl_anti_replay.init,
                                  tls13_AntiReplayInit)) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    (void)tls13_AntiReplayReset(NULL, NULL);

    ssl_anti_replay.lock = PZ_NewMonitor(nssILockSSL);
    if (!ssl_anti_replay.lock) {
        goto loser; /* Code already set. */
    }

    rv = tls13_AntiReplayKeyGen();
    if (rv != SECSuccess) {
        goto loser; /* Code already set. */
    }

    rv = sslBloom_Init(&ssl_anti_replay.filters[0], k, bits);
    if (rv != SECSuccess) {
        goto loser; /* Code already set. */
    }
    rv = sslBloom_Init(&ssl_anti_replay.filters[1], k, bits);
    if (rv != SECSuccess) {
        goto loser; /* Code already set. */
    }
    /* When starting out, ensure that 0-RTT is not accepted until the window is
     * updated.  A ClientHello might have been accepted prior to a restart. */
    sslBloom_Fill(&ssl_anti_replay.filters[1]);

    ssl_anti_replay.current = 0;
    ssl_anti_replay.nextUpdate = now + window;
    ssl_anti_replay.window = window;
    return SECSuccess;

loser:
    (void)tls13_AntiReplayReset(NULL, NULL);
    return SECFailure;
}

static void
tls13_AntiReplayUpdate(PRTime now)
{
    PR_ASSERT_CURRENT_THREAD_IN_MONITOR(ssl_anti_replay.lock);
    if (now >= ssl_anti_replay.nextUpdate) {
        ssl_anti_replay.current ^= 1;
        ssl_anti_replay.nextUpdate = now + ssl_anti_replay.window;
        sslBloom_Zero(ssl_anti_replay.filters + ssl_anti_replay.current);
    }
}

PRBool
tls13_InWindow(const sslSocket *ss, const sslSessionID *sid)
{
    PRInt32 timeDelta;

    /* Calculate the difference between the client's view of the age of the
     * ticket (in |ss->xtnData.ticketAge|) and the server's view, which we now
     * calculate.  The result should be close to zero.  timeDelta is signed to
     * make the comparisons below easier. */
    timeDelta = ss->xtnData.ticketAge -
                ((ssl_Time(ss) - sid->creationTime) / PR_USEC_PER_MSEC);

    /* Only allow the time delta to be at most half of our window.  This is
     * symmetrical, though it doesn't need to be; this assumes that clock errors
     * on server and client will tend to cancel each other out.
     *
     * There are two anti-replay filters that roll over each window.  In the
     * worst case, immediately after a rollover of the filters, we only have a
     * single window worth of recorded 0-RTT attempts.  Thus, the period in
     * which we can accept 0-RTT is at most one window wide.  This uses PR_ABS()
     * and half the window so that the first attempt can be up to half a window
     * early and then replays will be caught until the attempts are half a
     * window late.
     *
     * For example, a 0-RTT attempt arrives early, but near the end of window 1.
     * The attempt is then recorded in window 1.  Rollover to window 2 could
     * occur immediately afterwards.  Window 1 is still checked for new 0-RTT
     * attempts for the remainder of window 2.  Therefore, attempts to replay
     * are detected because the value is recorded in window 1.  When rollover
     * occurs again, window 1 is erased and window 3 instated.  If we allowed an
     * attempt to be late by more than half a window, then this check would not
     * prevent the same 0-RTT attempt from being accepted during window 1 and
     * later window 3.
     */
    return PR_ABS(timeDelta) < (ssl_anti_replay.window / (PR_USEC_PER_MSEC * 2));
}

/* Checks for a duplicate in the two filters we have.  Performs maintenance on
 * the filters as a side-effect. This only detects a probable replay, it's
 * possible that this will return true when the 0-RTT attempt is not genuinely a
 * replay.  In that case, we reject 0-RTT unnecessarily, but that's OK because
 * no client expects 0-RTT to work every time. */
PRBool
tls13_IsReplay(const sslSocket *ss, const sslSessionID *sid)
{
    PRBool replay;
    unsigned int size;
    PRUint8 index;
    SECStatus rv;
    static const char *label = "tls13 anti-replay";
    PRUint8 buf[SSL_MAX_BLOOM_FILTER_SIZE];

    /* If SSL_SetupAntiReplay hasn't been called, then treat all attempts at
     * 0-RTT as a replay. */
    if (!ssl_anti_replay.init.initialized) {
        return PR_TRUE;
    }

    if (!tls13_InWindow(ss, sid)) {
        return PR_TRUE;
    }

    size = ssl_anti_replay.filters[0].k *
           (ssl_anti_replay.filters[0].bits + 7) / 8;
    PORT_Assert(size <= SSL_MAX_BLOOM_FILTER_SIZE);
    rv = tls13_HkdfExpandLabelRaw(ssl_anti_replay.key, ssl_hash_sha256,
                                  ss->xtnData.pskBinder.data,
                                  ss->xtnData.pskBinder.len,
                                  label, strlen(label),
                                  buf, size);
    if (rv != SECSuccess) {
        return PR_TRUE;
    }

    PZ_EnterMonitor(ssl_anti_replay.lock);
    tls13_AntiReplayUpdate(ssl_Time(ss));

    index = ssl_anti_replay.current;
    replay = sslBloom_Add(&ssl_anti_replay.filters[index], buf);
    if (!replay) {
        replay = sslBloom_Check(&ssl_anti_replay.filters[index ^ 1], buf);
    }

    PZ_ExitMonitor(ssl_anti_replay.lock);
    return replay;
}
