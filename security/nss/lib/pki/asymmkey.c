/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSSPKI_H
#include "nsspki.h"
#endif /* NSSPKI_H */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

extern const NSSError NSS_ERROR_NOT_FOUND;

NSS_IMPLEMENT PRStatus
NSSPrivateKey_Destroy(
    NSSPrivateKey *vk)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
NSSPrivateKey_DeleteStoredObject(
    NSSPrivateKey *vk,
    NSSCallback *uhh)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT PRUint32
NSSPrivateKey_GetSignatureLength(
    NSSPrivateKey *vk)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return -1;
}

NSS_IMPLEMENT PRUint32
NSSPrivateKey_GetPrivateModulusLength(
    NSSPrivateKey *vk)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return -1;
}

NSS_IMPLEMENT PRBool
NSSPrivateKey_IsStillPresent(
    NSSPrivateKey *vk,
    PRStatus *statusOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FALSE;
}

NSS_IMPLEMENT NSSItem *
NSSPrivateKey_Encode(
    NSSPrivateKey *vk,
    NSSAlgorithmAndParameters *ap,
    NSSItem *passwordOpt, /* NULL will cause a callback; "" for no password */
    NSSCallback *uhhOpt,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSTrustDomain *
NSSPrivateKey_GetTrustDomain(
    NSSPrivateKey *vk,
    PRStatus *statusOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSToken *
NSSPrivateKey_GetToken(NSSPrivateKey *vk)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSSlot *
NSSPrivateKey_GetSlot(NSSPrivateKey *vk)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSModule *
NSSPrivateKey_GetModule(
    NSSPrivateKey *vk)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSPrivateKey_Decrypt(
    NSSPrivateKey *vk,
    NSSAlgorithmAndParameters *apOpt,
    NSSItem *encryptedData,
    NSSCallback *uhh,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSPrivateKey_Sign(
    NSSPrivateKey *vk,
    NSSAlgorithmAndParameters *apOpt,
    NSSItem *data,
    NSSCallback *uhh,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSPrivateKey_SignRecover(
    NSSPrivateKey *vk,
    NSSAlgorithmAndParameters *apOpt,
    NSSItem *data,
    NSSCallback *uhh,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSSymmetricKey *
NSSPrivateKey_UnwrapSymmetricKey(
    NSSPrivateKey *vk,
    NSSAlgorithmAndParameters *apOpt,
    NSSItem *wrappedKey,
    NSSCallback *uhh)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSSymmetricKey *
NSSPrivateKey_DeriveSymmetricKey(
    NSSPrivateKey *vk,
    NSSPublicKey *bk,
    NSSAlgorithmAndParameters *apOpt,
    NSSOID *target,
    PRUint32 keySizeOpt, /* zero for best allowed */
    NSSOperations operations,
    NSSCallback *uhh)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSPublicKey *
NSSPrivateKey_FindPublicKey(
    NSSPrivateKey *vk
    /* { don't need the callback here, right? } */
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCryptoContext *
NSSPrivateKey_CreateCryptoContext(
    NSSPrivateKey *vk,
    NSSAlgorithmAndParameters *apOpt,
    NSSCallback *uhh)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate **
NSSPrivateKey_FindCertificates(
    NSSPrivateKey *vk,
    NSSCertificate *rvOpt[],
    PRUint32 maximumOpt, /* 0 for no max */
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate *
NSSPrivateKey_FindBestCertificate(
    NSSPrivateKey *vk,
    NSSTime *timeOpt,
    NSSUsage *usageOpt,
    NSSPolicies *policiesOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT PRStatus
NSSPublicKey_Destroy(NSSPublicKey *bk)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
NSSPublicKey_DeleteStoredObject(
    NSSPublicKey *bk,
    NSSCallback *uhh)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT NSSItem *
NSSPublicKey_Encode(
    NSSPublicKey *bk,
    NSSAlgorithmAndParameters *ap,
    NSSCallback *uhhOpt,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSTrustDomain *
NSSPublicKey_GetTrustDomain(
    NSSPublicKey *bk,
    PRStatus *statusOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSToken *
NSSPublicKey_GetToken(
    NSSPublicKey *bk,
    PRStatus *statusOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSSlot *
NSSPublicKey_GetSlot(
    NSSPublicKey *bk,
    PRStatus *statusOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSModule *
NSSPublicKey_GetModule(
    NSSPublicKey *bk,
    PRStatus *statusOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSPublicKey_Encrypt(
    NSSPublicKey *bk,
    NSSAlgorithmAndParameters *apOpt,
    NSSItem *data,
    NSSCallback *uhh,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT PRStatus
NSSPublicKey_Verify(
    NSSPublicKey *bk,
    NSSAlgorithmAndParameters *apOpt,
    NSSItem *data,
    NSSItem *signature,
    NSSCallback *uhh)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT NSSItem *
NSSPublicKey_VerifyRecover(
    NSSPublicKey *bk,
    NSSAlgorithmAndParameters *apOpt,
    NSSItem *signature,
    NSSCallback *uhh,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSPublicKey_WrapSymmetricKey(
    NSSPublicKey *bk,
    NSSAlgorithmAndParameters *apOpt,
    NSSSymmetricKey *keyToWrap,
    NSSCallback *uhh,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCryptoContext *
NSSPublicKey_CreateCryptoContext(
    NSSPublicKey *bk,
    NSSAlgorithmAndParameters *apOpt,
    NSSCallback *uhh)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate **
NSSPublicKey_FindCertificates(
    NSSPublicKey *bk,
    NSSCertificate *rvOpt[],
    PRUint32 maximumOpt, /* 0 for no max */
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate *
NSSPublicKey_FindBestCertificate(
    NSSPublicKey *bk,
    NSSTime *timeOpt,
    NSSUsage *usageOpt,
    NSSPolicies *policiesOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSPrivateKey *
NSSPublicKey_FindPrivateKey(
    NSSPublicKey *bk,
    NSSCallback *uhh)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}
