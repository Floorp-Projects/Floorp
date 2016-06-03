/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "blapi.h"
#include "ec.h"
#include "ecl-curve.h"
#include "nss.h"
#include "secutil.h"
#include "pkcs11.h"
#include "nspr.h"
#include <stdio.h>

#define __PASTE(x, y) x##y

/*
 * Get the NSS specific PKCS #11 function names.
 */
#undef CK_PKCS11_FUNCTION_INFO
#undef CK_NEED_ARG_LIST

#define CK_EXTERN extern
#define CK_PKCS11_FUNCTION_INFO(func) \
    CK_RV __PASTE(NS, func)
#define CK_NEED_ARG_LIST 1

#include "pkcs11f.h"

/* mapping between ECCurveName enum and pointers to ECCurveParams */
static SECOidTag ecCurve_oid_map[] = {
    SEC_OID_UNKNOWN,                /* ECCurve_noName */
    SEC_OID_ANSIX962_EC_PRIME192V1, /* ECCurve_NIST_P192 */
    SEC_OID_SECG_EC_SECP224R1,      /* ECCurve_NIST_P224 */
    SEC_OID_ANSIX962_EC_PRIME256V1, /* ECCurve_NIST_P256 */
    SEC_OID_SECG_EC_SECP384R1,      /* ECCurve_NIST_P384 */
    SEC_OID_SECG_EC_SECP521R1,      /* ECCurve_NIST_P521 */
    SEC_OID_SECG_EC_SECT163K1,      /* ECCurve_NIST_K163 */
    SEC_OID_SECG_EC_SECT163R1,      /* ECCurve_NIST_B163 */
    SEC_OID_SECG_EC_SECT233K1,      /* ECCurve_NIST_K233 */
    SEC_OID_SECG_EC_SECT233R1,      /* ECCurve_NIST_B233 */
    SEC_OID_SECG_EC_SECT283K1,      /* ECCurve_NIST_K283 */
    SEC_OID_SECG_EC_SECT283R1,      /* ECCurve_NIST_B283 */
    SEC_OID_SECG_EC_SECT409K1,      /* ECCurve_NIST_K409 */
    SEC_OID_SECG_EC_SECT409R1,      /* ECCurve_NIST_B409 */
    SEC_OID_SECG_EC_SECT571K1,      /* ECCurve_NIST_K571 */
    SEC_OID_SECG_EC_SECT571R1,      /* ECCurve_NIST_B571 */
    SEC_OID_ANSIX962_EC_PRIME192V2,
    SEC_OID_ANSIX962_EC_PRIME192V3,
    SEC_OID_ANSIX962_EC_PRIME239V1,
    SEC_OID_ANSIX962_EC_PRIME239V2,
    SEC_OID_ANSIX962_EC_PRIME239V3,
    SEC_OID_ANSIX962_EC_C2PNB163V1,
    SEC_OID_ANSIX962_EC_C2PNB163V2,
    SEC_OID_ANSIX962_EC_C2PNB163V3,
    SEC_OID_ANSIX962_EC_C2PNB176V1,
    SEC_OID_ANSIX962_EC_C2TNB191V1,
    SEC_OID_ANSIX962_EC_C2TNB191V2,
    SEC_OID_ANSIX962_EC_C2TNB191V3,
    SEC_OID_ANSIX962_EC_C2PNB208W1,
    SEC_OID_ANSIX962_EC_C2TNB239V1,
    SEC_OID_ANSIX962_EC_C2TNB239V2,
    SEC_OID_ANSIX962_EC_C2TNB239V3,
    SEC_OID_ANSIX962_EC_C2PNB272W1,
    SEC_OID_ANSIX962_EC_C2PNB304W1,
    SEC_OID_ANSIX962_EC_C2TNB359V1,
    SEC_OID_ANSIX962_EC_C2PNB368W1,
    SEC_OID_ANSIX962_EC_C2TNB431R1,
    SEC_OID_SECG_EC_SECP112R1,
    SEC_OID_SECG_EC_SECP112R2,
    SEC_OID_SECG_EC_SECP128R1,
    SEC_OID_SECG_EC_SECP128R2,
    SEC_OID_SECG_EC_SECP160K1,
    SEC_OID_SECG_EC_SECP160R1,
    SEC_OID_SECG_EC_SECP160R2,
    SEC_OID_SECG_EC_SECP192K1,
    SEC_OID_SECG_EC_SECP224K1,
    SEC_OID_SECG_EC_SECP256K1,
    SEC_OID_SECG_EC_SECT113R1,
    SEC_OID_SECG_EC_SECT113R2,
    SEC_OID_SECG_EC_SECT131R1,
    SEC_OID_SECG_EC_SECT131R2,
    SEC_OID_SECG_EC_SECT163R1,
    SEC_OID_SECG_EC_SECT193R1,
    SEC_OID_SECG_EC_SECT193R2,
    SEC_OID_SECG_EC_SECT239K1,
    SEC_OID_UNKNOWN, /* ECCurve_WTLS_1 */
    SEC_OID_UNKNOWN, /* ECCurve_WTLS_8 */
    SEC_OID_UNKNOWN, /* ECCurve_WTLS_9 */
    SEC_OID_UNKNOWN /* ECCurve_pastLastCurve */
};

typedef SECStatus (*op_func)(void *, void *, void *);
typedef SECStatus (*pk11_op_func)(CK_SESSION_HANDLE, void *, void *, void *);

typedef struct ThreadDataStr {
    op_func op;
    void *p1;
    void *p2;
    void *p3;
    int iters;
    PRLock *lock;
    int count;
    SECStatus status;
    int isSign;
} ThreadData;

void
PKCS11Thread(void *data)
{
    ThreadData *threadData = (ThreadData *)data;
    pk11_op_func op = (pk11_op_func)threadData->op;
    int iters = threadData->iters;
    unsigned char sigData[256];
    SECItem sig;
    CK_SESSION_HANDLE session;
    CK_RV crv;

    threadData->status = SECSuccess;
    threadData->count = 0;

    /* get our thread's session */
    PR_Lock(threadData->lock);
    crv = NSC_OpenSession(1, CKF_SERIAL_SESSION, NULL, 0, &session);
    PR_Unlock(threadData->lock);
    if (crv != CKR_OK) {
        return;
    }

    if (threadData->isSign) {
        sig.data = sigData;
        sig.len = sizeof(sigData);
        threadData->p2 = (void *)&sig;
    }

    while (iters--) {
        threadData->status = (*op)(session, threadData->p1,
                                   threadData->p2, threadData->p3);
        if (threadData->status != SECSuccess) {
            break;
        }
        threadData->count++;
    }
    return;
}

void
genericThread(void *data)
{
    ThreadData *threadData = (ThreadData *)data;
    int iters = threadData->iters;
    unsigned char sigData[256];
    SECItem sig;

    threadData->status = SECSuccess;
    threadData->count = 0;

    if (threadData->isSign) {
        sig.data = sigData;
        sig.len = sizeof(sigData);
        threadData->p2 = (void *)&sig;
    }

    while (iters--) {
        threadData->status = (*threadData->op)(threadData->p1,
                                               threadData->p2, threadData->p3);
        if (threadData->status != SECSuccess) {
            break;
        }
        threadData->count++;
    }
    return;
}

/* Time iter repetitions of operation op. */
SECStatus
M_TimeOperation(void (*threadFunc)(void *),
                op_func opfunc, char *op, void *param1, void *param2,
                void *param3, int iters, int numThreads, PRLock *lock,
                CK_SESSION_HANDLE session, int isSign, double *rate)
{
    double dUserTime;
    int i, total;
    PRIntervalTime startTime, totalTime;
    PRThread **threadIDs;
    ThreadData *threadData;
    pk11_op_func pk11_op = (pk11_op_func)opfunc;
    SECStatus rv;

    /* verify operation works before testing performance */
    if (session) {
        rv = (*pk11_op)(session, param1, param2, param3);
    } else {
        rv = (*opfunc)(param1, param2, param3);
    }
    if (rv != SECSuccess) {
        SECU_PrintError("Error:", op);
        return rv;
    }

    /* get Data structures */
    threadIDs = (PRThread **)PORT_Alloc(numThreads * sizeof(PRThread *));
    threadData = (ThreadData *)PORT_Alloc(numThreads * sizeof(ThreadData));

    startTime = PR_Now();
    if (numThreads == 1) {
        for (i = 0; i < iters; i++) {
            if (session) {
                rv = (*pk11_op)(session, param1, param2, param3);
            } else {
                rv = (*opfunc)(param1, param2, param3);
            }
            if (rv != SECSuccess) {
                PORT_Free(threadIDs);
                PORT_Free(threadData);
                SECU_PrintError("Error:", op);
                return rv;
            }
        }
        total = iters;
    } else {
        for (i = 0; i < numThreads; i++) {
            threadData[i].op = opfunc;
            threadData[i].p1 = (void *)param1;
            threadData[i].p2 = (void *)param2;
            threadData[i].p3 = (void *)param3;
            threadData[i].iters = iters;
            threadData[i].lock = lock;
            threadData[i].isSign = isSign;
            threadIDs[i] = PR_CreateThread(PR_USER_THREAD, threadFunc,
                                           (void *)&threadData[i], PR_PRIORITY_NORMAL,
                                           PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);
        }

        total = 0;
        for (i = 0; i < numThreads; i++) {
            PR_JoinThread(threadIDs[i]);
            /* check the status */
            total += threadData[i].count;
        }
    }

    totalTime = PR_Now() - startTime;
    /* SecondsToInterval seems to be broken here ... */
    dUserTime = (double)totalTime / (double)1000000;
    if (dUserTime) {
        printf("    %-15s count:%4d sec: %3.2f op/sec: %6.2f\n",
               op, total, dUserTime, (double)total / dUserTime);
        if (rate) {
            *rate = ((double)total) / dUserTime;
        }
    }
    PORT_Free(threadIDs);
    PORT_Free(threadData);

    return SECSuccess;
}

#define GFP_POPULATE(params, name_v)                         \
    params.name = name_v;                                    \
    if ((params.name < ECCurve_noName) ||                    \
        (params.name > ECCurve_pastLastCurve))               \
        goto cleanup;                                        \
    params.type = ec_params_named;                           \
    params.curveOID.data = NULL;                             \
    params.curveOID.len = 0;                                 \
    params.curve.seed.data = NULL;                           \
    params.curve.seed.len = 0;                               \
    params.DEREncoding.data = NULL;                          \
    params.DEREncoding.len = 0;                              \
    params.arena = NULL;                                     \
    params.fieldID.size = ecCurve_map[name_v]->size;         \
    params.fieldID.type = ec_field_GFp;                      \
    hexString2SECItem(params.arena, &params.fieldID.u.prime, \
                      ecCurve_map[name_v]->irr);             \
    hexString2SECItem(params.arena, &params.curve.a,         \
                      ecCurve_map[name_v]->curvea);          \
    hexString2SECItem(params.arena, &params.curve.b,         \
                      ecCurve_map[name_v]->curveb);          \
    genenc[0] = '0';                                         \
    genenc[1] = '4';                                         \
    genenc[2] = '\0';                                        \
    strcat(genenc, ecCurve_map[name_v]->genx);               \
    strcat(genenc, ecCurve_map[name_v]->geny);               \
    hexString2SECItem(params.arena, &params.base,            \
                      genenc);                               \
    hexString2SECItem(params.arena, &params.order,           \
                      ecCurve_map[name_v]->order);           \
    params.cofactor = ecCurve_map[name_v]->cofactor;

/* Test curve using specific field arithmetic. */
#define ECTEST_NAMED_GFP(name_c, name_v)                               \
    if (usefreebl) {                                                   \
        printf("Testing %s using freebl implementation...\n", name_c); \
        rv = ectest_curve_freebl(name_v, iterations, numThreads);      \
        if (rv != SECSuccess)                                          \
            goto cleanup;                                              \
        printf("... okay.\n");                                         \
    }                                                                  \
    if (usepkcs11) {                                                   \
        printf("Testing %s using pkcs11 implementation...\n", name_c); \
        rv = ectest_curve_pkcs11(name_v, iterations, numThreads);      \
        if (rv != SECSuccess)                                          \
            goto cleanup;                                              \
        printf("... okay.\n");                                         \
    }

/*
 * Initializes a SECItem from a hexadecimal string
 *
 * Warning: This function ignores leading 00's, so any leading 00's
 * in the hexadecimal string must be optional.
 */
static SECItem *
hexString2SECItem(PLArenaPool *arena, SECItem *item, const char *str)
{
    int i = 0;
    int byteval = 0;
    int tmp = PORT_Strlen(str);

    if ((tmp % 2) != 0) {
        return NULL;
    }

    /* skip leading 00's unless the hex string is "00" */
    while ((tmp > 2) && (str[0] == '0') && (str[1] == '0')) {
        str += 2;
        tmp -= 2;
    }

    item->data = (unsigned char *)PORT_Alloc(tmp / 2);
    if (item->data == NULL)
        return NULL;
    item->len = tmp / 2;

    while (str[i]) {
        if ((str[i] >= '0') && (str[i] <= '9'))
            tmp = str[i] - '0';
        else if ((str[i] >= 'a') && (str[i] <= 'f'))
            tmp = str[i] - 'a' + 10;
        else if ((str[i] >= 'A') && (str[i] <= 'F'))
            tmp = str[i] - 'A' + 10;
        else
            return NULL;

        byteval = byteval * 16 + tmp;
        if ((i % 2) != 0) {
            item->data[i / 2] = byteval;
            byteval = 0;
        }
        i++;
    }

    return item;
}

#define PK11_SETATTRS(x, id, v, l) \
    (x)->type = (id);              \
    (x)->pValue = (v);             \
    (x)->ulValueLen = (l);

SECStatus
PKCS11_Derive(CK_SESSION_HANDLE session, CK_OBJECT_HANDLE *hKey,
              CK_MECHANISM *pMech, int *dummy)
{
    CK_RV crv;
    CK_OBJECT_HANDLE newKey;
    CK_BBOOL cktrue = CK_TRUE;
    CK_OBJECT_CLASS keyClass = CKO_SECRET_KEY;
    CK_KEY_TYPE keyType = CKK_GENERIC_SECRET;
    CK_ATTRIBUTE keyTemplate[3];
    CK_ATTRIBUTE *attrs = keyTemplate;

    PK11_SETATTRS(attrs, CKA_CLASS, &keyClass, sizeof(keyClass));
    attrs++;
    PK11_SETATTRS(attrs, CKA_KEY_TYPE, &keyType, sizeof(keyType));
    attrs++;
    PK11_SETATTRS(attrs, CKA_DERIVE, &cktrue, 1);
    attrs++;

    crv = NSC_DeriveKey(session, pMech, *hKey, keyTemplate, 3, &newKey);
    if (crv != CKR_OK) {
        printf("Derive Failed CK_RV=0x%x\n", (int)crv);
        return SECFailure;
    }
    return SECSuccess;
}

SECStatus
PKCS11_Sign(CK_SESSION_HANDLE session, CK_OBJECT_HANDLE *hKey,
            SECItem *sig, SECItem *digest)
{
    CK_RV crv;
    CK_MECHANISM mech;
    CK_ULONG sigLen = sig->len;

    mech.mechanism = CKM_ECDSA;
    mech.pParameter = NULL;
    mech.ulParameterLen = 0;

    crv = NSC_SignInit(session, &mech, *hKey);
    if (crv != CKR_OK) {
        printf("Sign Failed CK_RV=0x%x\n", (int)crv);
        return SECFailure;
    }
    crv = NSC_Sign(session, digest->data, digest->len, sig->data, &sigLen);
    if (crv != CKR_OK) {
        printf("Sign Failed CK_RV=0x%x\n", (int)crv);
        return SECFailure;
    }
    sig->len = (unsigned int)sigLen;
    return SECSuccess;
}

SECStatus
PKCS11_Verify(CK_SESSION_HANDLE session, CK_OBJECT_HANDLE *hKey,
              SECItem *sig, SECItem *digest)
{
    CK_RV crv;
    CK_MECHANISM mech;

    mech.mechanism = CKM_ECDSA;
    mech.pParameter = NULL;
    mech.ulParameterLen = 0;

    crv = NSC_VerifyInit(session, &mech, *hKey);
    if (crv != CKR_OK) {
        printf("Verify Failed CK_RV=0x%x\n", (int)crv);
        return SECFailure;
    }
    crv = NSC_Verify(session, digest->data, digest->len, sig->data, sig->len);
    if (crv != CKR_OK) {
        printf("Verify Failed CK_RV=0x%x\n", (int)crv);
        return SECFailure;
    }
    return SECSuccess;
}

static SECStatus
ecName2params(ECCurveName curve, SECKEYECParams *params)
{
    SECOidData *oidData = NULL;

    if ((curve < ECCurve_noName) || (curve > ECCurve_pastLastCurve) ||
        ((oidData = SECOID_FindOIDByTag(ecCurve_oid_map[curve])) == NULL)) {
        PORT_SetError(SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE);
        return SECFailure;
    }

    SECITEM_AllocItem(NULL, params, (2 + oidData->oid.len));
    /*
     * params->data needs to contain the ASN encoding of an object ID (OID)
     * representing the named curve. The actual OID is in
     * oidData->oid.data so we simply prepend 0x06 and OID length
     */
    params->data[0] = SEC_ASN1_OBJECT_ID;
    params->data[1] = oidData->oid.len;
    memcpy(params->data + 2, oidData->oid.data, oidData->oid.len);

    return SECSuccess;
}

/* Performs basic tests of elliptic curve cryptography over prime fields.
 * If tests fail, then it prints an error message, aborts, and returns an
 * error code. Otherwise, returns 0. */
SECStatus
ectest_curve_pkcs11(ECCurveName curve, int iterations, int numThreads)
{
    CK_OBJECT_HANDLE ecPriv;
    CK_OBJECT_HANDLE ecPub;
    CK_SESSION_HANDLE session;
    SECItem sig;
    SECItem digest;
    SECKEYECParams ecParams;
    CK_MECHANISM mech;
    CK_ECDH1_DERIVE_PARAMS ecdh_params;
    unsigned char sigData[256];
    unsigned char digestData[20];
    unsigned char pubKeyData[256];
    PRLock *lock = NULL;
    double signRate, deriveRate;
    CK_ATTRIBUTE template;
    SECStatus rv;
    CK_RV crv;

    ecParams.data = NULL;
    ecParams.len = 0;
    rv = ecName2params(curve, &ecParams);
    if (rv != SECSuccess) {
        goto cleanup;
    }

    crv = NSC_OpenSession(1, CKF_SERIAL_SESSION, NULL, 0, &session);
    if (crv != CKR_OK) {
        printf("OpenSession Failed CK_RV=0x%x\n", (int)crv);
        return SECFailure;
    }

    PORT_Memset(digestData, 0xa5, sizeof(digestData));
    digest.data = digestData;
    digest.len = sizeof(digestData);
    sig.data = sigData;
    sig.len = sizeof(sigData);

    template.type = CKA_EC_PARAMS;
    template.pValue = ecParams.data;
    template.ulValueLen = ecParams.len;
    mech.mechanism = CKM_EC_KEY_PAIR_GEN;
    mech.pParameter = NULL;
    mech.ulParameterLen = 0;
    crv = NSC_GenerateKeyPair(session, &mech,
                              &template, 1, NULL, 0, &ecPub, &ecPriv);
    if (crv != CKR_OK) {
        printf("GenerateKeyPair Failed CK_RV=0x%x\n", (int)crv);
        return SECFailure;
    }

    template.type = CKA_EC_POINT;
    template.pValue = pubKeyData;
    template.ulValueLen = sizeof(pubKeyData);
    crv = NSC_GetAttributeValue(session, ecPub, &template, 1);
    if (crv != CKR_OK) {
        printf("GenerateKeyPair Failed CK_RV=0x%x\n", (int)crv);
        return SECFailure;
    }

    ecdh_params.kdf = CKD_NULL;
    ecdh_params.ulSharedDataLen = 0;
    ecdh_params.pSharedData = NULL;
    ecdh_params.ulPublicDataLen = template.ulValueLen;
    ecdh_params.pPublicData = template.pValue;

    mech.mechanism = CKM_ECDH1_DERIVE;
    mech.pParameter = (void *)&ecdh_params;
    mech.ulParameterLen = sizeof(ecdh_params);

    lock = PR_NewLock();

    rv = M_TimeOperation(PKCS11Thread, (op_func)PKCS11_Derive, "ECDH_Derive",
                         &ecPriv, &mech, NULL, iterations, numThreads,
                         lock, session, 0, &deriveRate);
    if (rv != SECSuccess) {
        goto cleanup;
    }
    rv = M_TimeOperation(PKCS11Thread, (op_func)PKCS11_Sign, "ECDSA_Sign",
                         (void *)&ecPriv, &sig, &digest, iterations, numThreads,
                         lock, session, 1, &signRate);
    if (rv != SECSuccess) {
        goto cleanup;
    }
    printf("        ECDHE max rate = %.2f\n", (deriveRate + signRate) / 4.0);
    /* get a signature */
    rv = PKCS11_Sign(session, &ecPriv, &sig, &digest);
    if (rv != SECSuccess) {
        goto cleanup;
    }
    rv = M_TimeOperation(PKCS11Thread, (op_func)PKCS11_Verify, "ECDSA_Verify",
                         (void *)&ecPub, &sig, &digest, iterations, numThreads,
                         lock, session, 0, NULL);
    if (rv != SECSuccess) {
        goto cleanup;
    }

cleanup:
    if (lock) {
        PR_DestroyLock(lock);
    }
    return rv;
}

SECStatus
ECDH_DeriveWrap(ECPrivateKey *priv, ECPublicKey *pub, int *dummy)
{
    SECItem secret;
    unsigned char secretData[256];
    SECStatus rv;

    secret.data = secretData;
    secret.len = sizeof(secretData);

    rv = ECDH_Derive(&pub->publicValue, &pub->ecParams,
                     &priv->privateValue, 0, &secret);
#ifdef notdef
    if (rv == SECSuccess) {
        PORT_Free(secret.data);
    }
#endif
    return rv;
}

/* Performs basic tests of elliptic curve cryptography over prime fields.
 * If tests fail, then it prints an error message, aborts, and returns an
 * error code. Otherwise, returns 0. */
SECStatus
ectest_curve_freebl(ECCurveName curve, int iterations, int numThreads)
{
    ECParams ecParams;
    ECPrivateKey *ecPriv = NULL;
    ECPublicKey ecPub;
    SECItem sig;
    SECItem digest;
    unsigned char sigData[256];
    unsigned char digestData[20];
    double signRate, deriveRate;
    char genenc[3 + 2 * 2 * MAX_ECKEY_LEN];
    SECStatus rv = SECFailure;

    GFP_POPULATE(ecParams, curve);

    PORT_Memset(digestData, 0xa5, sizeof(digestData));
    digest.data = digestData;
    digest.len = sizeof(digestData);
    sig.data = sigData;
    sig.len = sizeof(sigData);

    rv = EC_NewKey(&ecParams, &ecPriv);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    ecPub.ecParams = ecParams;
    ecPub.publicValue = ecPriv->publicValue;

    rv = M_TimeOperation(genericThread, (op_func)ECDH_DeriveWrap, "ECDH_Derive",
                         ecPriv, &ecPub, NULL, iterations, numThreads, 0, 0, 0, &deriveRate);
    if (rv != SECSuccess) {
        goto cleanup;
    }
    rv = M_TimeOperation(genericThread, (op_func)ECDSA_SignDigest, "ECDSA_Sign",
                         ecPriv, &sig, &digest, iterations, numThreads, 0, 0, 1, &signRate);
    if (rv != SECSuccess)
        goto cleanup;
    printf("        ECDHE max rate = %.2f\n", (deriveRate + signRate) / 4.0);
    rv = ECDSA_SignDigest(ecPriv, &sig, &digest);
    if (rv != SECSuccess) {
        goto cleanup;
    }
    rv = M_TimeOperation(genericThread, (op_func)ECDSA_VerifyDigest, "ECDSA_Verify",
                         &ecPub, &sig, &digest, iterations, numThreads, 0, 0, 0, NULL);
    if (rv != SECSuccess) {
        goto cleanup;
    }

cleanup:
    return rv;
}

/* Prints help information. */
void
printUsage(char *prog)
{
    printf("Usage: %s [-i iterations] [-t threads ] [-ans] [-fp] [-Al]\n"
           "-a: ansi\n-n: nist\n-s: secp\n-f: usefreebl\n-p: usepkcs11\n-A: all\n",
           prog);
}

/* Performs tests of elliptic curve cryptography over prime fields If
 * tests fail, then it prints an error message, aborts, and returns an
 * error code. Otherwise, returns 0. */
int
main(int argv, char **argc)
{
    int ansi = 0;
    int nist = 0;
    int secp = 0;
    int usefreebl = 0;
    int usepkcs11 = 0;
    int i;
    SECStatus rv = SECSuccess;
    int iterations = 100;
    int numThreads = 1;

    const CK_C_INITIALIZE_ARGS pk11args = {
        NULL, NULL, NULL, NULL, CKF_LIBRARY_CANT_CREATE_OS_THREADS,
        (void *)"flags=readOnly,noCertDB,noModDB", NULL
    };

    /* read command-line arguments */
    for (i = 1; i < argv; i++) {
        if (PL_strcasecmp(argc[i], "-i") == 0) {
            i++;
            iterations = atoi(argc[i]);
        } else if (PL_strcasecmp(argc[i], "-t") == 0) {
            i++;
            numThreads = atoi(argc[i]);
        } else if (PL_strcasecmp(argc[i], "-A") == 0) {
            ansi = nist = secp = 1;
            usepkcs11 = usefreebl = 1;
        } else if (PL_strcasecmp(argc[i], "-a") == 0) {
            ansi = 1;
        } else if (PL_strcasecmp(argc[i], "-n") == 0) {
            nist = 1;
        } else if (PL_strcasecmp(argc[i], "-s") == 0) {
            secp = 1;
        } else if (PL_strcasecmp(argc[i], "-p") == 0) {
            usepkcs11 = 1;
        } else if (PL_strcasecmp(argc[i], "-f") == 0) {
            usefreebl = 1;
        } else {
            printUsage(argc[0]);
            return 0;
        }
    }

    if ((ansi | nist | secp) == 0) {
        nist = 1;
    }
    if ((usepkcs11 | usefreebl) == 0) {
        usefreebl = 1;
    }

    rv = NSS_NoDB_Init(NULL);
    if (rv != SECSuccess) {
        SECU_PrintError("Error:", "NSS_NoDB_Init");
        goto cleanup;
    }

    if (usepkcs11) {
        CK_RV crv = NSC_Initialize((CK_VOID_PTR)&pk11args);
        if (crv != CKR_OK) {
            fprintf(stderr, "NSC_Initialize failed crv=0x%x\n", (unsigned int)crv);
            return SECFailure;
        }
    }

    /* specific arithmetic tests */
    if (nist) {
#ifdef NSS_ECC_MORE_THAN_SUITE_B
        ECTEST_NAMED_GFP("SECP-160K1", ECCurve_SECG_PRIME_160K1);
        ECTEST_NAMED_GFP("NIST-P192", ECCurve_NIST_P192);
        ECTEST_NAMED_GFP("NIST-P224", ECCurve_NIST_P224);
#endif
        ECTEST_NAMED_GFP("NIST-P256", ECCurve_NIST_P256);
        ECTEST_NAMED_GFP("NIST-P384", ECCurve_NIST_P384);
        ECTEST_NAMED_GFP("NIST-P521", ECCurve_NIST_P521);
    }
#ifdef NSS_ECC_MORE_THAN_SUITE_B
    if (ansi) {
        ECTEST_NAMED_GFP("ANSI X9.62 PRIME192v1", ECCurve_X9_62_PRIME_192V1);
        ECTEST_NAMED_GFP("ANSI X9.62 PRIME192v2", ECCurve_X9_62_PRIME_192V2);
        ECTEST_NAMED_GFP("ANSI X9.62 PRIME192v3", ECCurve_X9_62_PRIME_192V3);
        ECTEST_NAMED_GFP("ANSI X9.62 PRIME239v1", ECCurve_X9_62_PRIME_239V1);
        ECTEST_NAMED_GFP("ANSI X9.62 PRIME239v2", ECCurve_X9_62_PRIME_239V2);
        ECTEST_NAMED_GFP("ANSI X9.62 PRIME239v3", ECCurve_X9_62_PRIME_239V3);
        ECTEST_NAMED_GFP("ANSI X9.62 PRIME256v1", ECCurve_X9_62_PRIME_256V1);
    }
    if (secp) {
        ECTEST_NAMED_GFP("SECP-112R1", ECCurve_SECG_PRIME_112R1);
        ECTEST_NAMED_GFP("SECP-112R2", ECCurve_SECG_PRIME_112R2);
        ECTEST_NAMED_GFP("SECP-128R1", ECCurve_SECG_PRIME_128R1);
        ECTEST_NAMED_GFP("SECP-128R2", ECCurve_SECG_PRIME_128R2);
        ECTEST_NAMED_GFP("SECP-160K1", ECCurve_SECG_PRIME_160K1);
        ECTEST_NAMED_GFP("SECP-160R1", ECCurve_SECG_PRIME_160R1);
        ECTEST_NAMED_GFP("SECP-160R2", ECCurve_SECG_PRIME_160R2);
        ECTEST_NAMED_GFP("SECP-192K1", ECCurve_SECG_PRIME_192K1);
        ECTEST_NAMED_GFP("SECP-192R1", ECCurve_SECG_PRIME_192R1);
        ECTEST_NAMED_GFP("SECP-224K1", ECCurve_SECG_PRIME_224K1);
        ECTEST_NAMED_GFP("SECP-224R1", ECCurve_SECG_PRIME_224R1);
        ECTEST_NAMED_GFP("SECP-256K1", ECCurve_SECG_PRIME_256K1);
        ECTEST_NAMED_GFP("SECP-256R1", ECCurve_SECG_PRIME_256R1);
        ECTEST_NAMED_GFP("SECP-384R1", ECCurve_SECG_PRIME_384R1);
        ECTEST_NAMED_GFP("SECP-521R1", ECCurve_SECG_PRIME_521R1);
    }
#endif

cleanup:
    if (rv != SECSuccess) {
        printf("Error: exiting with error value\n");
    }
    return rv;
}
