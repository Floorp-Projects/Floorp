/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "blapi.h"
#include "nss.h"
#include "secutil.h"
#include "secitem.h"
#include "nspr.h"
#include "pk11pub.h"
#include <stdio.h>

void
printBuf(const SECItem *item)
{
    int i;
    if (!item || !item->len) {
        printf("(null)\n");
        return;
    }

    for (i = 0; i < item->len; i++) {
        printf("%02x", item->data[i]);
    }
    printf("\n");
}

void
PrintKey(PK11SymKey *symKey)
{
    char *name = PK11_GetSymKeyNickname(symKey);
    int len = PK11_GetKeyLength(symKey);
    int strength = PK11_GetKeyStrength(symKey, NULL);
    SECItem *value = NULL;
    CK_KEY_TYPE type = PK11_GetSymKeyType(symKey);
    (void)PK11_ExtractKeyValue(symKey);

    value = PK11_GetKeyData(symKey);
    printf("%s %3d   %4d   %s  ", name ? name : "no-name", len, strength,
           type == CKK_GENERIC_SECRET ? "generic" : "ERROR! UNKNOWN KEY TYPE");
    printBuf(value);

    PORT_Free(name);
}

SECStatus
ectest_curve_pkcs11(SECOidTag oid)
{
    SECKEYECParams pk_11_ecParams = { siBuffer, NULL, 0 };
    SECKEYPublicKey *pubKey = NULL;
    SECKEYPrivateKey *privKey = NULL;
    SECOidData *oidData = NULL;
    CK_MECHANISM_TYPE target = CKM_TLS12_MASTER_KEY_DERIVE_DH;
    PK11SymKey *symKey = NULL;
    SECStatus rv = SECFailure;

    oidData = SECOID_FindOIDByTag(oid);
    if (oidData == NULL) {
        printf(" >>> SECOID_FindOIDByTag failed.\n");
        goto cleanup;
    }
    PORT_Assert(oidData->oid.len < 256);
    SECITEM_AllocItem(NULL, &pk_11_ecParams, (2 + oidData->oid.len));
    pk_11_ecParams.data[0] = SEC_ASN1_OBJECT_ID; /* we have to prepend 0x06 */
    pk_11_ecParams.data[1] = oidData->oid.len;
    memcpy(pk_11_ecParams.data + 2, oidData->oid.data, oidData->oid.len);

    privKey = SECKEY_CreateECPrivateKey(&pk_11_ecParams, &pubKey, NULL);
    if (!privKey || !pubKey) {
        printf(" >>> SECKEY_CreateECPrivateKey failed.\n");
        goto cleanup;
    }

    symKey = PK11_PubDeriveWithKDF(privKey, pubKey, PR_FALSE, NULL, NULL,
                                   CKM_ECDH1_DERIVE, target, CKA_DERIVE, 0,
                                   CKD_NULL, NULL, NULL);
    if (!symKey) {
        printf(" >>> PK11_PubDeriveWithKDF failed.\n");
        goto cleanup;
    }
    PrintKey(symKey);
    rv = SECSuccess;

cleanup:
    if (privKey) {
        SECKEY_DestroyPrivateKey(privKey);
    }
    if (pubKey) {
        SECKEY_DestroyPublicKey(pubKey);
    }
    if (symKey) {
        PK11_FreeSymKey(symKey);
    }
    SECITEM_FreeItem(&pk_11_ecParams, PR_FALSE);

    return rv;
}

void
printUsage(char *prog)
{
    printf("Usage: %s [-fp] [-nd]\n"
           "\t-n: NIST curves\n"
           "\t-d: non-NIST curves\n"
           "You have to specify at at least one of n or d.\n"
           "By default no tests are executed.\n",
           prog);
}

/* Performs tests of elliptic curve cryptography over prime fields If
 * tests fail, then it prints an error message, aborts, and returns an
 * error code. Otherwise, returns 0. */
int
main(int argv, char **argc)
{
    SECStatus rv = SECSuccess;
    int i = 0;
    int nist = 0;
    int nonnist = 0;
    SECOidTag nistOids[3] = { SEC_OID_SECG_EC_SECP256R1,
                              SEC_OID_SECG_EC_SECP384R1,
                              SEC_OID_SECG_EC_SECP521R1 };

    for (i = 1; i < argv; i++) {
        if (PL_strcasecmp(argc[i], "-n") == 0) {
            nist = 1;
        } else if (PL_strcasecmp(argc[i], "-d") == 0) {
            nonnist = 1;
        } else {
            printUsage(argc[0]);
            return 1;
        }
    }
    if (!nist && !nonnist) {
        printUsage(argc[0]);
        return 1;
    }

    rv = NSS_NoDB_Init(NULL);
    if (rv != SECSuccess) {
        SECU_PrintError("Error:", "NSS_NoDB_Init");
        goto cleanup;
    }

    if (nonnist) {
        if (ectest_curve_pkcs11(SEC_OID_CURVE25519) != SECSuccess) {
            printf("not okay (OID %d) - PK11 test\n", SEC_OID_CURVE25519);
            rv = SECFailure;
        } else {
            printf("okay (OID %d) - PK11 test\n", SEC_OID_CURVE25519);
        }
    }
    if (nist) {
        for (i = 0; i < 3; ++i) {
            if (ectest_curve_pkcs11(nistOids[i]) != SECSuccess) {
                printf("not okay (OID %d) - PK11 test\n", nistOids[i]);
                rv = SECFailure;
            } else {
                printf("okay (OID %d) - PK11 test\n", nistOids[i]);
            }
        }
    }

cleanup:
    rv |= NSS_Shutdown();

    if (rv != SECSuccess) {
        printf("Error: exiting with error value\n");
    }
    return rv;
}
