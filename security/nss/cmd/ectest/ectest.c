/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "blapi.h"
#include "ec.h"
#include "ecl-curve.h"
#include "nss.h"
#include "secutil.h"
#include "secitem.h"
#include "nspr.h"
#include <stdio.h>

typedef struct {
    ECCurveName curve;
    char *privhex;
    char *our_pubhex;
    char *their_pubhex;
    char *common_key;
    char *name;
} ECDH_KAT;

#include "testvecs.h"

/*
 * Initializes a SECItem from a hexadecimal string
 *
 */
static SECItem *
hexString2SECItem(PLArenaPool *arena, SECItem *item, const char *str)
{
    int i = 0;
    int byteval = 0;
    int tmp = PORT_Strlen(str);

    PORT_Assert(arena);
    PORT_Assert(item);

    if ((tmp % 2) != 0) {
        return NULL;
    }

    item = SECITEM_AllocItem(arena, item, tmp / 2);
    if (item == NULL) {
        return NULL;
    }

    while (str[i]) {
        if ((str[i] >= '0') && (str[i] <= '9')) {
            tmp = str[i] - '0';
        } else if ((str[i] >= 'a') && (str[i] <= 'f')) {
            tmp = str[i] - 'a' + 10;
        } else if ((str[i] >= 'A') && (str[i] <= 'F')) {
            tmp = str[i] - 'A' + 10;
        } else {
            /* item is in arena and gets freed by the caller */
            return NULL;
        }

        byteval = byteval * 16 + tmp;
        if ((i % 2) != 0) {
            item->data[i / 2] = byteval;
            byteval = 0;
        }
        i++;
    }

    return item;
}

SECStatus
ectest_ecdh_kat(ECDH_KAT *kat)
{
    ECCurveName curve = kat->curve;
    ECParams ecParams = { 0 };
    ECPrivateKey *ecPriv = NULL;
    SECItem theirKey = { siBuffer, NULL, 0 };
    SECStatus rv = SECFailure;
    PLArenaPool *arena;
    SECItem seed = { siBuffer, NULL, 0 };
    SECItem answer = { siBuffer, NULL, 0 };
    SECItem answer2 = { siBuffer, NULL, 0 };
    SECItem derived = { siBuffer, NULL, 0 };
    char genenc[3 + 2 * 2 * MAX_ECKEY_LEN];
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena) {
        return SECFailure;
    }

    if ((curve < ECCurve_noName) || (curve > ECCurve_pastLastCurve)) {
        PORT_FreeArena(arena, PR_FALSE);
        return SECFailure;
    }

    ecParams.name = curve;
    ecParams.type = ec_params_named;
    ecParams.curveOID.data = NULL;
    ecParams.curveOID.len = 0;
    ecParams.curve.seed.data = NULL;
    ecParams.curve.seed.len = 0;
    ecParams.DEREncoding.data = NULL;
    ecParams.DEREncoding.len = 0;

    ecParams.fieldID.size = ecCurve_map[curve]->size;
    ecParams.fieldID.type = ec_field_GFp;
    hexString2SECItem(arena, &ecParams.fieldID.u.prime, ecCurve_map[curve]->irr);
    hexString2SECItem(arena, &ecParams.curve.a, ecCurve_map[curve]->curvea);
    hexString2SECItem(arena, &ecParams.curve.b, ecCurve_map[curve]->curveb);
    genenc[0] = '0';
    genenc[1] = '4';
    genenc[2] = '\0';
    strcat(genenc, ecCurve_map[curve]->genx);
    strcat(genenc, ecCurve_map[curve]->geny);
    hexString2SECItem(arena, &ecParams.base, genenc);
    hexString2SECItem(arena, &ecParams.order, ecCurve_map[curve]->order);
    ecParams.cofactor = ecCurve_map[curve]->cofactor;

    hexString2SECItem(arena, &answer, kat->our_pubhex);
    hexString2SECItem(arena, &seed, kat->privhex);
    rv = EC_NewKeyFromSeed(&ecParams, &ecPriv, seed.data, seed.len);
    if (rv != SECSuccess) {
        rv = SECFailure;
        goto cleanup;
    }
    if (SECITEM_CompareItem(&answer, &ecPriv->publicValue) != SECEqual) {
        rv = SECFailure;
        goto cleanup;
    }

    hexString2SECItem(arena, &theirKey, kat->their_pubhex);
    hexString2SECItem(arena, &answer2, kat->common_key);
    rv = ECDH_Derive(&theirKey, &ecParams, &ecPriv->privateValue, PR_TRUE, &derived);
    if (rv != SECSuccess) {
        rv = SECFailure;
        goto cleanup;
    }

    if (SECITEM_CompareItem(&answer2, &derived) != SECEqual) {
        rv = SECFailure;
        goto cleanup;
    }
cleanup:
    PORT_FreeArena(arena, PR_FALSE);
    PORT_FreeArena(ecPriv->ecParams.arena, PR_FALSE);
    SECITEM_FreeItem(&derived, PR_FALSE);
    return rv;
}

/* Performs tests of elliptic curve cryptography over prime fields If
 * tests fail, then it prints an error message, aborts, and returns an
 * error code. Otherwise, returns 0. */
int
main(int argv, char **argc)
{
    SECStatus rv = SECSuccess;
    int numkats = 0;
    int i = 0;
    rv = NSS_NoDB_Init(NULL);
    if (rv != SECSuccess) {
        SECU_PrintError("Error:", "NSS_NoDB_Init");
        goto cleanup;
    }

    while (ecdh_testvecs[numkats].curve != ECCurve_pastLastCurve) {
        numkats++;
    }
    printf("1..%d\n", numkats);
    for (i = 0; ecdh_testvecs[i].curve != ECCurve_pastLastCurve; i++) {
        rv = ectest_ecdh_kat(&ecdh_testvecs[i]);
        if (rv != SECSuccess) {
            printf("not okay %d - %s\n", i + 1, ecdh_testvecs[i].name);
        } else {
            printf("okay %d - %s\n", i + 1, ecdh_testvecs[i].name);
        }
    }

cleanup:
    rv |= NSS_Shutdown();

    if (rv != SECSuccess) {
        printf("Error: exiting with error value\n");
    }
    return rv;
}
