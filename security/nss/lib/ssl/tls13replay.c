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
#include "prmon.h"
#include "prtime.h"
#include "secerr.h"
#include "ssl.h"
#include "sslbloom.h"
#include "sslimpl.h"
#include "tls13hkdf.h"

struct SSLAntiReplayContextStr {
    /* The number of outstanding references to this context. */
    PRInt32 refCount;
    /* Used to serialize access. */
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
};

void
tls13_ReleaseAntiReplayContext(SSLAntiReplayContext *ctx)
{
    if (!ctx) {
        return;
    }
    if (PR_ATOMIC_DECREMENT(&ctx->refCount) >= 1) {
        return;
    }

    if (ctx->lock) {
        PZ_DestroyMonitor(ctx->lock);
        ctx->lock = NULL;
    }
    PK11_FreeSymKey(ctx->key);
    ctx->key = NULL;
    sslBloom_Destroy(&ctx->filters[0]);
    sslBloom_Destroy(&ctx->filters[1]);
    PORT_Free(ctx);
}

/* Clear the current state and free any resources we allocated. The signature
 * here is odd to allow this to be called during shutdown. */
SECStatus
SSLExp_ReleaseAntiReplayContext(SSLAntiReplayContext *ctx)
{
    tls13_ReleaseAntiReplayContext(ctx);
    return SECSuccess;
}

SSLAntiReplayContext *
tls13_RefAntiReplayContext(SSLAntiReplayContext *ctx)
{
    PORT_Assert(ctx);
    PR_ATOMIC_INCREMENT(&ctx->refCount);
    return ctx;
}

static SECStatus
tls13_AntiReplayKeyGen(SSLAntiReplayContext *ctx)
{
    PK11SlotInfo *slot;

    PORT_Assert(ctx);

    slot = PK11_GetBestSlot(CKM_HKDF_DERIVE, NULL);
    if (!slot) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    ctx->key = PK11_KeyGen(slot, CKM_HKDF_KEY_GEN, NULL, 32, NULL);
    if (!ctx->key) {
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
 * The context created by this function can be called concurrently on multiple
 * threads if the server is multi-threaded.  A monitor is used to ensure that
 * only one thread can access the structures that change over time, but no such
 * guarantee is provided for configuration data.
 */
SECStatus
SSLExp_CreateAntiReplayContext(PRTime now, PRTime window, unsigned int k,
                               unsigned int bits, SSLAntiReplayContext **pctx)
{
    SECStatus rv;

    if (window <= 0 || k == 0 || bits == 0 || pctx == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    if ((k * (bits + 7) / 8) > SSL_MAX_BLOOM_FILTER_SIZE) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    SSLAntiReplayContext *ctx = PORT_ZNew(SSLAntiReplayContext);
    if (!ctx) {
        return SECFailure; /* Code already set. */
    }

    ctx->refCount = 1;
    ctx->lock = PZ_NewMonitor(nssILockSSL);
    if (!ctx->lock) {
        goto loser; /* Code already set. */
    }

    rv = tls13_AntiReplayKeyGen(ctx);
    if (rv != SECSuccess) {
        goto loser; /* Code already set. */
    }

    rv = sslBloom_Init(&ctx->filters[0], k, bits);
    if (rv != SECSuccess) {
        goto loser; /* Code already set. */
    }
    rv = sslBloom_Init(&ctx->filters[1], k, bits);
    if (rv != SECSuccess) {
        goto loser; /* Code already set. */
    }
    /* When starting out, ensure that 0-RTT is not accepted until the window is
     * updated.  A ClientHello might have been accepted prior to a restart. */
    sslBloom_Fill(&ctx->filters[1]);

    ctx->current = 0;
    ctx->nextUpdate = now + window;
    ctx->window = window;
    *pctx = ctx;
    return SECSuccess;

loser:
    tls13_ReleaseAntiReplayContext(ctx);
    return SECFailure;
}

SECStatus
SSLExp_SetAntiReplayContext(PRFileDesc *fd, SSLAntiReplayContext *ctx)
{
    sslSocket *ss = ssl_FindSocket(fd);
    if (!ss) {
        return SECFailure; /* Code already set. */
    }
    tls13_ReleaseAntiReplayContext(ss->antiReplay);
    if (ctx != NULL) {
        ss->antiReplay = tls13_RefAntiReplayContext(ctx);
    } else {
        ss->antiReplay = NULL;
    }
    return SECSuccess;
}

static void
tls13_AntiReplayUpdate(SSLAntiReplayContext *ctx, PRTime now)
{
    PR_ASSERT_CURRENT_THREAD_IN_MONITOR(ctx->lock);
    if (now >= ctx->nextUpdate) {
        ctx->current ^= 1;
        ctx->nextUpdate = now + ctx->window;
        sslBloom_Zero(ctx->filters + ctx->current);
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
    PRInt32 allowance = ss->antiReplay->window / (PR_USEC_PER_MSEC * 2);
    SSL_TRC(10, ("%d: TLS13[%d]: replay check time delta=%d, allow=%d",
                 SSL_GETPID(), ss->fd, timeDelta, allowance));
    return PR_ABS(timeDelta) < allowance;
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
    static const char *label = "anti-replay";
    PRUint8 buf[SSL_MAX_BLOOM_FILTER_SIZE];
    SSLAntiReplayContext *ctx = ss->antiReplay;

    /* If SSL_SetAntiReplayContext hasn't been called with a valid context, then
     * treat all attempts at 0-RTT as a replay. */
    if (ctx == NULL) {
        return PR_TRUE;
    }

    if (!tls13_InWindow(ss, sid)) {
        return PR_TRUE;
    }

    size = ctx->filters[0].k * (ctx->filters[0].bits + 7) / 8;
    PORT_Assert(size <= SSL_MAX_BLOOM_FILTER_SIZE);
    rv = tls13_HkdfExpandLabelRaw(ctx->key, ssl_hash_sha256,
                                  ss->xtnData.pskBinder.data,
                                  ss->xtnData.pskBinder.len,
                                  label, strlen(label),
                                  ss->protocolVariant, buf, size);
    if (rv != SECSuccess) {
        return PR_TRUE;
    }

    PZ_EnterMonitor(ctx->lock);
    tls13_AntiReplayUpdate(ctx, ssl_Time(ss));

    index = ctx->current;
    replay = sslBloom_Add(&ctx->filters[index], buf);
    SSL_TRC(10, ("%d: TLS13[%d]: replay check current window: %s",
                 SSL_GETPID(), ss->fd, replay ? "replay" : "ok"));
    if (!replay) {
        replay = sslBloom_Check(&ctx->filters[index ^ 1], buf);
        SSL_TRC(10, ("%d: TLS13[%d]: replay check previous window: %s",
                     SSL_GETPID(), ss->fd, replay ? "replay" : "ok"));
    }

    PZ_ExitMonitor(ctx->lock);
    return replay;
}
