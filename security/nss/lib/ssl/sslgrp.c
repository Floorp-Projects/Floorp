/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file contains prototypes for the public SSL functions.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nss.h"
#include "pk11func.h"
#include "ssl.h"
#include "sslimpl.h"

struct {
    sslEphemeralKeyPair *keyPair;
    PRCallOnceType once;
} gECDHEKeyPairs[SSL_NAMED_GROUP_COUNT];

/* Function to clear out the ECDHE keys. */
static SECStatus
ssl_CleanupECDHEKeys(void *appData, void *nssData)
{
    unsigned int i;

    for (i = 0; i < SSL_NAMED_GROUP_COUNT; i++) {
        if (gECDHEKeyPairs[i].keyPair) {
            ssl_FreeEphemeralKeyPair(gECDHEKeyPairs[i].keyPair);
        }
    }
    memset(gECDHEKeyPairs, 0, sizeof(gECDHEKeyPairs));
    return SECSuccess;
}

/* Only run the cleanup once. */
static PRCallOnceType cleanupECDHEKeysOnce;
static PRStatus
ssl_SetupCleanupECDHEKeysOnce(void)
{
    SECStatus rv = NSS_RegisterShutdown(ssl_CleanupECDHEKeys, NULL);
    return (rv != SECSuccess) ? PR_FAILURE : PR_SUCCESS;
}

/* This creates a key pair for each of the supported EC groups.  If that works,
 * we assume that the token supports that group.  Since this is relatively
 * expensive, this is only done for the first socket that is used.  That means
 * that if tokens are added or removed, then this will not pick up any changes.
 */
static PRStatus
ssl_CreateStaticECDHEKeyPair(void *arg)
{
    const sslNamedGroupDef *group = (const sslNamedGroupDef *)arg;
    unsigned int i = group - ssl_named_groups;
    SECStatus rv;

    PORT_Assert(group->keaType == ssl_kea_ecdh);
    PORT_Assert(i < SSL_NAMED_GROUP_COUNT);
    rv = ssl_CreateECDHEphemeralKeyPair(group, &gECDHEKeyPairs[i].keyPair);
    if (rv != SECSuccess) {
        gECDHEKeyPairs[i].keyPair = NULL;
        SSL_TRC(5, ("%d: SSL[-]: disabling group %d",
                    SSL_GETPID(), group->name));
    }

    return PR_SUCCESS;
}

void
ssl_FilterSupportedGroups(sslSocket *ss)
{
    unsigned int i;
    PRStatus prv;

    prv = PR_CallOnce(&cleanupECDHEKeysOnce, ssl_SetupCleanupECDHEKeysOnce);
    PORT_Assert(prv == PR_SUCCESS);
    if (prv != PR_SUCCESS) {
        return;
    }

    for (i = 0; i < SSL_NAMED_GROUP_COUNT; ++i) {
        PRUint32 policy;
        SECStatus srv;
        unsigned int index;
        const sslNamedGroupDef *group = ss->namedGroupPreferences[i];
        if (!group) {
            continue;
        }

        srv = NSS_GetAlgorithmPolicy(group->oidTag, &policy);
        if (srv == SECSuccess && !(policy & NSS_USE_ALG_IN_SSL_KX)) {
            ss->namedGroupPreferences[i] = NULL;
            continue;
        }

        if (group->assumeSupported) {
            continue;
        }

        /* For EC groups, we have to test that a key pair can be created. This
         * is gross, and expensive, so only do it once. */
        index = group - ssl_named_groups;
        PORT_Assert(index < SSL_NAMED_GROUP_COUNT);

        prv = PR_CallOnceWithArg(&gECDHEKeyPairs[index].once,
                                 ssl_CreateStaticECDHEKeyPair,
                                 (void *)group);
        PORT_Assert(prv == PR_SUCCESS);
        if (prv != PR_SUCCESS) {
            continue;
        }

        if (!gECDHEKeyPairs[index].keyPair) {
            ss->namedGroupPreferences[i] = NULL;
        }
    }
}

/*
 * Creates the static "ephemeral" public and private ECDH keys used by server in
 * ECDHE_RSA and ECDHE_ECDSA handshakes when we reuse the same key.
 */
SECStatus
ssl_CreateStaticECDHEKey(sslSocket *ss, const sslNamedGroupDef *ecGroup)
{
    sslEphemeralKeyPair *keyPair;
    /* We index gECDHEKeyPairs by the named group.  Pointer arithmetic! */
    unsigned int index = ecGroup - ssl_named_groups;
    PRStatus prv;

    prv = PR_CallOnceWithArg(&gECDHEKeyPairs[index].once,
                             ssl_CreateStaticECDHEKeyPair,
                             (void *)ecGroup);
    PORT_Assert(prv == PR_SUCCESS);
    if (prv != PR_SUCCESS) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    keyPair = gECDHEKeyPairs[index].keyPair;
    if (!keyPair) {
        /* Attempting to use a key pair for an unsupported group. */
        PORT_Assert(keyPair);
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    keyPair = ssl_CopyEphemeralKeyPair(keyPair);
    if (!keyPair)
        return SECFailure;

    PORT_Assert(PR_CLIST_IS_EMPTY(&ss->ephemeralKeyPairs));
    PR_APPEND_LINK(&keyPair->link, &ss->ephemeralKeyPairs);
    return SECSuccess;
}
