/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DEV_H
#include "dev.h"
#endif /* DEV_H */

#ifndef PKIM_H
#include "pkim.h"
#endif /* PKIM_H */

#ifndef PKISTORE_H
#include "pkistore.h"
#endif /* PKISTORE_H */

extern const NSSError NSS_ERROR_NOT_FOUND;
extern const NSSError NSS_ERROR_INVALID_ARGUMENT;

NSS_IMPLEMENT NSSCryptoContext *
nssCryptoContext_Create(
    NSSTrustDomain *td,
    NSSCallback *uhhOpt)
{
    NSSArena *arena;
    NSSCryptoContext *rvCC;
    arena = NSSArena_Create();
    if (!arena) {
        return NULL;
    }
    rvCC = nss_ZNEW(arena, NSSCryptoContext);
    if (!rvCC) {
        return NULL;
    }
    rvCC->td = td;
    rvCC->arena = arena;
    rvCC->certStore = nssCertificateStore_Create(rvCC->arena);
    if (!rvCC->certStore) {
        nssArena_Destroy(arena);
        return NULL;
    }

    return rvCC;
}

NSS_IMPLEMENT PRStatus
NSSCryptoContext_Destroy(NSSCryptoContext *cc)
{
    PRStatus status = PR_SUCCESS;
    PORT_Assert(cc && cc->certStore);
    if (!cc) {
        return PR_FAILURE;
    }
    if (cc->certStore) {
        status = nssCertificateStore_Destroy(cc->certStore);
        if (status == PR_FAILURE) {
            return status;
        }
    } else {
        status = PR_FAILURE;
    }
    nssArena_Destroy(cc->arena);
    return status;
}

NSS_IMPLEMENT PRStatus
NSSCryptoContext_SetDefaultCallback(
    NSSCryptoContext *td,
    NSSCallback *newCallback,
    NSSCallback **oldCallbackOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT NSSCallback *
NSSCryptoContext_GetDefaultCallback(
    NSSCryptoContext *td,
    PRStatus *statusOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSTrustDomain *
NSSCryptoContext_GetTrustDomain(NSSCryptoContext *td)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate *
NSSCryptoContext_FindOrImportCertificate(
    NSSCryptoContext *cc,
    NSSCertificate *c)
{
    NSSCertificate *rvCert = NULL;

    PORT_Assert(cc && cc->certStore);
    if (!cc || !cc->certStore) {
        nss_SetError(NSS_ERROR_INVALID_ARGUMENT);
        return rvCert;
    }
    rvCert = nssCertificateStore_FindOrAdd(cc->certStore, c);
    if (rvCert == c && c->object.cryptoContext != cc) {
        PORT_Assert(!c->object.cryptoContext);
        c->object.cryptoContext = cc;
    }
    if (rvCert) {
        /* an NSSCertificate cannot be part of two crypto contexts
        ** simultaneously.  If this assertion fails, then there is
        ** a serious Stan design flaw.
        */
        PORT_Assert(cc == c->object.cryptoContext);
    }
    return rvCert;
}

NSS_IMPLEMENT NSSCertificate *
NSSCryptoContext_ImportPKIXCertificate(
    NSSCryptoContext *cc,
    struct NSSPKIXCertificateStr *pc)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate *
NSSCryptoContext_ImportEncodedCertificate(
    NSSCryptoContext *cc,
    NSSBER *ber)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT PRStatus
NSSCryptoContext_ImportEncodedPKIXCertificateChain(
    NSSCryptoContext *cc,
    NSSBER *ber)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
nssCryptoContext_ImportTrust(
    NSSCryptoContext *cc,
    NSSTrust *trust)
{
    PRStatus nssrv;
    PORT_Assert(cc && cc->certStore);
    if (!cc || !cc->certStore) {
        return PR_FAILURE;
    }
    nssrv = nssCertificateStore_AddTrust(cc->certStore, trust);
#if 0
    if (nssrv == PR_SUCCESS) {
    trust->object.cryptoContext = cc;
    }
#endif
    return nssrv;
}

NSS_IMPLEMENT PRStatus
nssCryptoContext_ImportSMIMEProfile(
    NSSCryptoContext *cc,
    nssSMIMEProfile *profile)
{
    PRStatus nssrv;
    PORT_Assert(cc && cc->certStore);
    if (!cc || !cc->certStore) {
        return PR_FAILURE;
    }
    nssrv = nssCertificateStore_AddSMIMEProfile(cc->certStore, profile);
#if 0
    if (nssrv == PR_SUCCESS) {
    profile->object.cryptoContext = cc;
    }
#endif
    return nssrv;
}

NSS_IMPLEMENT NSSCertificate *
NSSCryptoContext_FindBestCertificateByNickname(
    NSSCryptoContext *cc,
    const NSSUTF8 *name,
    NSSTime *timeOpt, /* NULL for "now" */
    NSSUsage *usage,
    NSSPolicies *policiesOpt /* NULL for none */
)
{
    NSSCertificate **certs;
    NSSCertificate *rvCert = NULL;
    PORT_Assert(cc && cc->certStore);
    if (!cc || !cc->certStore) {
        return NULL;
    }
    certs = nssCertificateStore_FindCertificatesByNickname(cc->certStore,
                                                           name,
                                                           NULL, 0, NULL);
    if (certs) {
        rvCert = nssCertificateArray_FindBestCertificate(certs,
                                                         timeOpt,
                                                         usage,
                                                         policiesOpt);
        nssCertificateArray_Destroy(certs);
    }
    return rvCert;
}

NSS_IMPLEMENT NSSCertificate **
NSSCryptoContext_FindCertificatesByNickname(
    NSSCryptoContext *cc,
    NSSUTF8 *name,
    NSSCertificate *rvOpt[],
    PRUint32 maximumOpt, /* 0 for no max */
    NSSArena *arenaOpt)
{
    NSSCertificate **rvCerts;
    PORT_Assert(cc && cc->certStore);
    if (!cc || !cc->certStore) {
        return NULL;
    }
    rvCerts = nssCertificateStore_FindCertificatesByNickname(cc->certStore,
                                                             name,
                                                             rvOpt,
                                                             maximumOpt,
                                                             arenaOpt);
    return rvCerts;
}

NSS_IMPLEMENT NSSCertificate *
NSSCryptoContext_FindCertificateByIssuerAndSerialNumber(
    NSSCryptoContext *cc,
    NSSDER *issuer,
    NSSDER *serialNumber)
{
    PORT_Assert(cc && cc->certStore);
    if (!cc || !cc->certStore) {
        return NULL;
    }
    return nssCertificateStore_FindCertificateByIssuerAndSerialNumber(
        cc->certStore,
        issuer,
        serialNumber);
}

NSS_IMPLEMENT NSSCertificate *
NSSCryptoContext_FindBestCertificateBySubject(
    NSSCryptoContext *cc,
    NSSDER *subject,
    NSSTime *timeOpt,
    NSSUsage *usage,
    NSSPolicies *policiesOpt)
{
    NSSCertificate **certs;
    NSSCertificate *rvCert = NULL;
    PORT_Assert(cc && cc->certStore);
    if (!cc || !cc->certStore) {
        return NULL;
    }
    certs = nssCertificateStore_FindCertificatesBySubject(cc->certStore,
                                                          subject,
                                                          NULL, 0, NULL);
    if (certs) {
        rvCert = nssCertificateArray_FindBestCertificate(certs,
                                                         timeOpt,
                                                         usage,
                                                         policiesOpt);
        nssCertificateArray_Destroy(certs);
    }
    return rvCert;
}

NSS_IMPLEMENT NSSCertificate **
nssCryptoContext_FindCertificatesBySubject(
    NSSCryptoContext *cc,
    NSSDER *subject,
    NSSCertificate *rvOpt[],
    PRUint32 maximumOpt, /* 0 for no max */
    NSSArena *arenaOpt)
{
    NSSCertificate **rvCerts;
    PORT_Assert(cc && cc->certStore);
    if (!cc || !cc->certStore) {
        return NULL;
    }
    rvCerts = nssCertificateStore_FindCertificatesBySubject(cc->certStore,
                                                            subject,
                                                            rvOpt,
                                                            maximumOpt,
                                                            arenaOpt);
    return rvCerts;
}

NSS_IMPLEMENT NSSCertificate **
NSSCryptoContext_FindCertificatesBySubject(
    NSSCryptoContext *cc,
    NSSDER *subject,
    NSSCertificate *rvOpt[],
    PRUint32 maximumOpt, /* 0 for no max */
    NSSArena *arenaOpt)
{
    return nssCryptoContext_FindCertificatesBySubject(cc, subject,
                                                      rvOpt, maximumOpt,
                                                      arenaOpt);
}

NSS_IMPLEMENT NSSCertificate *
NSSCryptoContext_FindBestCertificateByNameComponents(
    NSSCryptoContext *cc,
    NSSUTF8 *nameComponents,
    NSSTime *timeOpt,
    NSSUsage *usage,
    NSSPolicies *policiesOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate **
NSSCryptoContext_FindCertificatesByNameComponents(
    NSSCryptoContext *cc,
    NSSUTF8 *nameComponents,
    NSSCertificate *rvOpt[],
    PRUint32 maximumOpt, /* 0 for no max */
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate *
NSSCryptoContext_FindCertificateByEncodedCertificate(
    NSSCryptoContext *cc,
    NSSBER *encodedCertificate)
{
    PORT_Assert(cc && cc->certStore);
    if (!cc || !cc->certStore) {
        return NULL;
    }
    return nssCertificateStore_FindCertificateByEncodedCertificate(
        cc->certStore,
        encodedCertificate);
}

NSS_IMPLEMENT NSSCertificate *
NSSCryptoContext_FindBestCertificateByEmail(
    NSSCryptoContext *cc,
    NSSASCII7 *email,
    NSSTime *timeOpt,
    NSSUsage *usage,
    NSSPolicies *policiesOpt)
{
    NSSCertificate **certs;
    NSSCertificate *rvCert = NULL;

    PORT_Assert(cc && cc->certStore);
    if (!cc || !cc->certStore) {
        return NULL;
    }
    certs = nssCertificateStore_FindCertificatesByEmail(cc->certStore,
                                                        email,
                                                        NULL, 0, NULL);
    if (certs) {
        rvCert = nssCertificateArray_FindBestCertificate(certs,
                                                         timeOpt,
                                                         usage,
                                                         policiesOpt);
        nssCertificateArray_Destroy(certs);
    }
    return rvCert;
}

NSS_IMPLEMENT NSSCertificate **
NSSCryptoContext_FindCertificatesByEmail(
    NSSCryptoContext *cc,
    NSSASCII7 *email,
    NSSCertificate *rvOpt[],
    PRUint32 maximumOpt, /* 0 for no max */
    NSSArena *arenaOpt)
{
    NSSCertificate **rvCerts;
    PORT_Assert(cc && cc->certStore);
    if (!cc || !cc->certStore) {
        return NULL;
    }
    rvCerts = nssCertificateStore_FindCertificatesByEmail(cc->certStore,
                                                          email,
                                                          rvOpt,
                                                          maximumOpt,
                                                          arenaOpt);
    return rvCerts;
}

NSS_IMPLEMENT NSSCertificate *
NSSCryptoContext_FindCertificateByOCSPHash(
    NSSCryptoContext *cc,
    NSSItem *hash)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate *
NSSCryptoContext_FindBestUserCertificate(
    NSSCryptoContext *cc,
    NSSTime *timeOpt,
    NSSUsage *usage,
    NSSPolicies *policiesOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate **
NSSCryptoContext_FindUserCertificates(
    NSSCryptoContext *cc,
    NSSTime *timeOpt,
    NSSUsage *usageOpt,
    NSSPolicies *policiesOpt,
    NSSCertificate **rvOpt,
    PRUint32 rvLimit, /* zero for no limit */
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate *
NSSCryptoContext_FindBestUserCertificateForSSLClientAuth(
    NSSCryptoContext *cc,
    NSSUTF8 *sslHostOpt,
    NSSDER *rootCAsOpt[],   /* null pointer for none */
    PRUint32 rootCAsMaxOpt, /* zero means list is null-terminated */
    NSSAlgorithmAndParameters *apOpt,
    NSSPolicies *policiesOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate **
NSSCryptoContext_FindUserCertificatesForSSLClientAuth(
    NSSCryptoContext *cc,
    NSSUTF8 *sslHostOpt,
    NSSDER *rootCAsOpt[],   /* null pointer for none */
    PRUint32 rootCAsMaxOpt, /* zero means list is null-terminated */
    NSSAlgorithmAndParameters *apOpt,
    NSSPolicies *policiesOpt,
    NSSCertificate **rvOpt,
    PRUint32 rvLimit, /* zero for no limit */
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate *
NSSCryptoContext_FindBestUserCertificateForEmailSigning(
    NSSCryptoContext *cc,
    NSSASCII7 *signerOpt,
    NSSASCII7 *recipientOpt,
    /* anything more here? */
    NSSAlgorithmAndParameters *apOpt,
    NSSPolicies *policiesOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate *
NSSCryptoContext_FindUserCertificatesForEmailSigning(
    NSSCryptoContext *cc,
    NSSASCII7 *signerOpt, /* fgmr or a more general name? */
    NSSASCII7 *recipientOpt,
    /* anything more here? */
    NSSAlgorithmAndParameters *apOpt,
    NSSPolicies *policiesOpt,
    NSSCertificate **rvOpt,
    PRUint32 rvLimit, /* zero for no limit */
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSTrust *
nssCryptoContext_FindTrustForCertificate(
    NSSCryptoContext *cc,
    NSSCertificate *cert)
{
    PORT_Assert(cc && cc->certStore);
    if (!cc || !cc->certStore) {
        return NULL;
    }
    return nssCertificateStore_FindTrustForCertificate(cc->certStore, cert);
}

NSS_IMPLEMENT nssSMIMEProfile *
nssCryptoContext_FindSMIMEProfileForCertificate(
    NSSCryptoContext *cc,
    NSSCertificate *cert)
{
    PORT_Assert(cc && cc->certStore);
    if (!cc || !cc->certStore) {
        return NULL;
    }
    return nssCertificateStore_FindSMIMEProfileForCertificate(cc->certStore,
                                                              cert);
}

NSS_IMPLEMENT PRStatus
NSSCryptoContext_GenerateKeyPair(
    NSSCryptoContext *cc,
    NSSAlgorithmAndParameters *ap,
    NSSPrivateKey **pvkOpt,
    NSSPublicKey **pbkOpt,
    PRBool privateKeyIsSensitive,
    NSSToken *destination,
    NSSCallback *uhhOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT NSSSymmetricKey *
NSSCryptoContext_GenerateSymmetricKey(
    NSSCryptoContext *cc,
    NSSAlgorithmAndParameters *ap,
    PRUint32 keysize,
    NSSToken *destination,
    NSSCallback *uhhOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSSymmetricKey *
NSSCryptoContext_GenerateSymmetricKeyFromPassword(
    NSSCryptoContext *cc,
    NSSAlgorithmAndParameters *ap,
    NSSUTF8 *passwordOpt, /* if null, prompt */
    NSSToken *destinationOpt,
    NSSCallback *uhhOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSSymmetricKey *
NSSCryptoContext_FindSymmetricKeyByAlgorithmAndKeyID(
    NSSCryptoContext *cc,
    NSSOID *algorithm,
    NSSItem *keyID,
    NSSCallback *uhhOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

struct token_session_str {
    NSSToken *token;
    nssSession *session;
};

NSS_IMPLEMENT NSSItem *
NSSCryptoContext_Decrypt(
    NSSCryptoContext *cc,
    NSSAlgorithmAndParameters *apOpt,
    NSSItem *encryptedData,
    NSSCallback *uhhOpt,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT PRStatus
NSSCryptoContext_BeginDecrypt(
    NSSCryptoContext *cc,
    NSSAlgorithmAndParameters *apOpt,
    NSSCallback *uhhOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT NSSItem *
NSSCryptoContext_ContinueDecrypt(
    NSSCryptoContext *cc,
    NSSItem *data,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSCryptoContext_FinishDecrypt(
    NSSCryptoContext *cc,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSCryptoContext_Sign(
    NSSCryptoContext *cc,
    NSSAlgorithmAndParameters *apOpt,
    NSSItem *data,
    NSSCallback *uhhOpt,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT PRStatus
NSSCryptoContext_BeginSign(
    NSSCryptoContext *cc,
    NSSAlgorithmAndParameters *apOpt,
    NSSCallback *uhhOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
NSSCryptoContext_ContinueSign(
    NSSCryptoContext *cc,
    NSSItem *data)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT NSSItem *
NSSCryptoContext_FinishSign(
    NSSCryptoContext *cc,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSCryptoContext_SignRecover(
    NSSCryptoContext *cc,
    NSSAlgorithmAndParameters *apOpt,
    NSSItem *data,
    NSSCallback *uhhOpt,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT PRStatus
NSSCryptoContext_BeginSignRecover(
    NSSCryptoContext *cc,
    NSSAlgorithmAndParameters *apOpt,
    NSSCallback *uhhOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT NSSItem *
NSSCryptoContext_ContinueSignRecover(
    NSSCryptoContext *cc,
    NSSItem *data,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSCryptoContext_FinishSignRecover(
    NSSCryptoContext *cc,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSSymmetricKey *
NSSCryptoContext_UnwrapSymmetricKey(
    NSSCryptoContext *cc,
    NSSAlgorithmAndParameters *apOpt,
    NSSItem *wrappedKey,
    NSSCallback *uhhOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSSymmetricKey *
NSSCryptoContext_DeriveSymmetricKey(
    NSSCryptoContext *cc,
    NSSPublicKey *bk,
    NSSAlgorithmAndParameters *apOpt,
    NSSOID *target,
    PRUint32 keySizeOpt, /* zero for best allowed */
    NSSOperations operations,
    NSSCallback *uhhOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSCryptoContext_Encrypt(
    NSSCryptoContext *cc,
    NSSAlgorithmAndParameters *apOpt,
    NSSItem *data,
    NSSCallback *uhhOpt,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT PRStatus
NSSCryptoContext_BeginEncrypt(
    NSSCryptoContext *cc,
    NSSAlgorithmAndParameters *apOpt,
    NSSCallback *uhhOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT NSSItem *
NSSCryptoContext_ContinueEncrypt(
    NSSCryptoContext *cc,
    NSSItem *data,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSCryptoContext_FinishEncrypt(
    NSSCryptoContext *cc,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT PRStatus
NSSCryptoContext_Verify(
    NSSCryptoContext *cc,
    NSSAlgorithmAndParameters *apOpt,
    NSSItem *data,
    NSSItem *signature,
    NSSCallback *uhhOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
NSSCryptoContext_BeginVerify(
    NSSCryptoContext *cc,
    NSSAlgorithmAndParameters *apOpt,
    NSSItem *signature,
    NSSCallback *uhhOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
NSSCryptoContext_ContinueVerify(
    NSSCryptoContext *cc,
    NSSItem *data)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
NSSCryptoContext_FinishVerify(
    NSSCryptoContext *cc)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT NSSItem *
NSSCryptoContext_VerifyRecover(
    NSSCryptoContext *cc,
    NSSAlgorithmAndParameters *apOpt,
    NSSItem *signature,
    NSSCallback *uhhOpt,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT PRStatus
NSSCryptoContext_BeginVerifyRecover(
    NSSCryptoContext *cc,
    NSSAlgorithmAndParameters *apOpt,
    NSSCallback *uhhOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT NSSItem *
NSSCryptoContext_ContinueVerifyRecover(
    NSSCryptoContext *cc,
    NSSItem *data,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSCryptoContext_FinishVerifyRecover(
    NSSCryptoContext *cc,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSCryptoContext_WrapSymmetricKey(
    NSSCryptoContext *cc,
    NSSAlgorithmAndParameters *apOpt,
    NSSSymmetricKey *keyToWrap,
    NSSCallback *uhhOpt,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSCryptoContext_Digest(
    NSSCryptoContext *cc,
    NSSAlgorithmAndParameters *apOpt,
    NSSItem *data,
    NSSCallback *uhhOpt,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    return nssToken_Digest(cc->token, cc->session, apOpt,
                           data, rvOpt, arenaOpt);
}

NSS_IMPLEMENT PRStatus
NSSCryptoContext_BeginDigest(
    NSSCryptoContext *cc,
    NSSAlgorithmAndParameters *apOpt,
    NSSCallback *uhhOpt)
{
    return nssToken_BeginDigest(cc->token, cc->session, apOpt);
}

NSS_IMPLEMENT PRStatus
NSSCryptoContext_ContinueDigest(
    NSSCryptoContext *cc,
    NSSAlgorithmAndParameters *apOpt,
    NSSItem *item)
{
    /*
    NSSAlgorithmAndParameters *ap;
    ap = (apOpt) ? apOpt : cc->ap;
    */
    /* why apOpt?  can't change it at this point... */
    return nssToken_ContinueDigest(cc->token, cc->session, item);
}

NSS_IMPLEMENT NSSItem *
NSSCryptoContext_FinishDigest(
    NSSCryptoContext *cc,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    return nssToken_FinishDigest(cc->token, cc->session, rvOpt, arenaOpt);
}

NSS_IMPLEMENT NSSCryptoContext *
NSSCryptoContext_Clone(NSSCryptoContext *cc)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}
