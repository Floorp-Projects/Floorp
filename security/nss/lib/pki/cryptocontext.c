/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: cryptocontext.c,v $ $Revision: 1.1 $ $Date: 2001/07/19 20:41:37 $ $Name:  $";
#endif /* DEBUG */

#ifndef NSSPKI_H
#include "nsspki.h"
#endif /* NSSPKI_H */

PRStatus
NSSCryptoContext_Destroy
(
  NSSCryptoContext *td
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

PRStatus
NSSCryptoContext_SetDefaultCallback
(
  NSSCryptoContext *td,
  NSSCallback *newCallback,
  NSSCallback **oldCallbackOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSSCallback *
NSSCryptoContext_GetDefaultCallback
(
  NSSCryptoContext *td,
  PRStatus *statusOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSTrustDomain *
NSSCryptoContext_GetTrustDomain
(
  NSSCryptoContext *td
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

PRStatus
NSSCryptoContext_ImportCertificate
(
  NSSCryptoContext *cc,
  NSSCertificate *c
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSSCertificate *
NSSCryptoContext_ImportPKIXCertificate
(
  NSSCryptoContext *cc,
  struct NSSPKIXCertificateStr *pc
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate *
NSSCryptoContext_ImportEncodedCertificate
(
  NSSCryptoContext *cc,
  NSSBER *ber
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

PRStatus
NSSCryptoContext_ImportEncodedPKIXCertificateChain
(
  NSSCryptoContext *cc,
  NSSBER *ber
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSSCertificate *
NSSCryptoContext_FindBestCertificateByNickname
(
  NSSCryptoContext *cc,
  NSSUTF8 *name,
  NSSTime *timeOpt, /* NULL for "now" */
  NSSUsage *usage,
  NSSPolicies *policiesOpt /* NULL for none */
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate **
NSSCryptoContext_FindCertificatesByNickname
(
  NSSCryptoContext *cc,
  NSSUTF8 *name,
  NSSCertificate *rvOpt[],
  PRUint32 maximumOpt, /* 0 for no max */
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate *
NSSCryptoContext_FindCertificateByIssuerAndSerialNumber
(
  NSSCryptoContext *cc,
  NSSDER *issuer,
  NSSDER *serialNumber
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate *
NSSCryptoContext_FindBestCertificateBySubject
(
  NSSCryptoContext *cc,
  NSSUTF8 *subject,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate **
NSSCryptoContext_FindCertificatesBySubject
(
  NSSCryptoContext *cc,
  NSSUTF8 *subject,
  NSSCertificate *rvOpt[],
  PRUint32 maximumOpt, /* 0 for no max */
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate *
NSSCryptoContext_FindBestCertificateByNameComponents
(
  NSSCryptoContext *cc,
  NSSUTF8 *nameComponents,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate **
NSSCryptoContext_FindCertificatesByNameComponents
(
  NSSCryptoContext *cc,
  NSSUTF8 *nameComponents,
  NSSCertificate *rvOpt[],
  PRUint32 maximumOpt, /* 0 for no max */
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate *
NSSCryptoContext_FindCertificateByEncodedCertificate
(
  NSSCryptoContext *cc,
  NSSBER *encodedCertificate
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate *
NSSCryptoContext_FindBestCertificateByEmail
(
  NSSCryptoContext *cc,
  NSSASCII7 *email
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate *
NSSCryptoContext_FindCertificatesByEmail
(
  NSSCryptoContext *cc,
  NSSASCII7 *email,
  NSSCertificate *rvOpt[],
  PRUint32 maximumOpt, /* 0 for no max */
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate *
NSSCryptoContext_FindCertificateByOCSPHash
(
  NSSCryptoContext *cc,
  NSSItem *hash
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate *
NSSCryptoContext_FindBestUserCertificate
(
  NSSCryptoContext *cc,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate **
NSSCryptoContext_FindUserCertificates
(
  NSSCryptoContext *cc,
  NSSTime *timeOpt,
  NSSUsage *usageOpt,
  NSSPolicies *policiesOpt,
  NSSCertificate **rvOpt,
  PRUint32 rvLimit, /* zero for no limit */
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate *
NSSCryptoContext_FindBestUserCertificateForSSLClientAuth
(
  NSSCryptoContext *cc,
  NSSUTF8 *sslHostOpt,
  NSSDER *rootCAsOpt[], /* null pointer for none */
  PRUint32 rootCAsMaxOpt, /* zero means list is null-terminated */
  NSSAlgorithmAndParameters *apOpt,
  NSSPolicies *policiesOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate **
NSSCryptoContext_FindUserCertificatesForSSLClientAuth
(
  NSSCryptoContext *cc,
  NSSUTF8 *sslHostOpt,
  NSSDER *rootCAsOpt[], /* null pointer for none */
  PRUint32 rootCAsMaxOpt, /* zero means list is null-terminated */
  NSSAlgorithmAndParameters *apOpt,
  NSSPolicies *policiesOpt,
  NSSCertificate **rvOpt,
  PRUint32 rvLimit, /* zero for no limit */
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate *
NSSCryptoContext_FindBestUserCertificateForEmailSigning
(
  NSSCryptoContext *cc,
  NSSASCII7 *signerOpt,
  NSSASCII7 *recipientOpt,
  /* anything more here? */
  NSSAlgorithmAndParameters *apOpt,
  NSSPolicies *policiesOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate *
NSSCryptoContext_FindUserCertificatesForEmailSigning
(
  NSSCryptoContext *cc,
  NSSASCII7 *signerOpt, /* fgmr or a more general name? */
  NSSASCII7 *recipientOpt,
  /* anything more here? */
  NSSAlgorithmAndParameters *apOpt,
  NSSPolicies *policiesOpt,
  NSSCertificate **rvOpt,
  PRUint32 rvLimit, /* zero for no limit */
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

PRStatus
NSSCryptoContext_GenerateKeyPair
(
  NSSCryptoContext *cc,
  NSSAlgorithmAndParameters *ap,
  NSSPrivateKey **pvkOpt,
  NSSPublicKey **pbkOpt,
  PRBool privateKeyIsSensitive,
  NSSToken *destination,
  NSSCallback *uhhOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSSSymmetricKey *
NSSCryptoContext_GenerateSymmetricKey
(
  NSSCryptoContext *cc,
  NSSAlgorithmAndParameters *ap,
  PRUint32 keysize,
  NSSToken *destination,
  NSSCallback *uhhOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSSymmetricKey *
NSSCryptoContext_GenerateSymmetricKeyFromPassword
(
  NSSCryptoContext *cc,
  NSSAlgorithmAndParameters *ap,
  NSSUTF8 *passwordOpt, /* if null, prompt */
  NSSToken *destinationOpt,
  NSSCallback *uhhOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSSymmetricKey *
NSSCryptoContext_FindSymmetricKeyByAlgorithmAndKeyID
(
  NSSCryptoContext *cc,
  NSSOID *algorithm,
  NSSItem *keyID,
  NSSCallback *uhhOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSItem *
NSSCryptoContext_Decrypt
(
  NSSCryptoContext *cc,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *encryptedData,
  NSSCallback *uhhOpt,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

PRStatus
NSSCryptoContext_BeginDecrypt
(
  NSSCryptoContext *cc,
  NSSAlgorithmAndParameters *apOpt,
  NSSCallback *uhhOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSSItem *
NSSCryptoContext_ContinueDecrypt
(
  NSSCryptoContext *cc,
  NSSItem *data,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSItem *
NSSCryptoContext_FinishDecrypt
(
  NSSCryptoContext *cc,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSItem *
NSSCryptoContext_Sign
(
  NSSCryptoContext *cc,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *data,
  NSSCallback *uhhOpt,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

PRStatus
NSSCryptoContext_BeginSign
(
  NSSCryptoContext *cc,
  NSSAlgorithmAndParameters *apOpt,
  NSSCallback *uhhOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

PRStatus
NSSCryptoContext_ContinueSign
(
  NSSCryptoContext *cc,
  NSSItem *data
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSSItem *
NSSCryptoContext_FinishSign
(
  NSSCryptoContext *cc,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSItem *
NSSCryptoContext_SignRecover
(
  NSSCryptoContext *cc,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *data,
  NSSCallback *uhhOpt,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

PRStatus
NSSCryptoContext_BeginSignRecover
(
  NSSCryptoContext *cc,
  NSSAlgorithmAndParameters *apOpt,
  NSSCallback *uhhOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSSItem *
NSSCryptoContext_ContinueSignRecover
(
  NSSCryptoContext *cc,
  NSSItem *data,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSItem *
NSSCryptoContext_FinishSignRecover
(
  NSSCryptoContext *cc,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSSymmetricKey *
NSSCryptoContext_UnwrapSymmetricKey
(
  NSSCryptoContext *cc,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *wrappedKey,
  NSSCallback *uhhOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSSymmetricKey *
NSSCryptoContext_DeriveSymmetricKey
(
  NSSCryptoContext *cc,
  NSSPublicKey *bk,
  NSSAlgorithmAndParameters *apOpt,
  NSSOID *target,
  PRUint32 keySizeOpt, /* zero for best allowed */
  NSSOperations operations,
  NSSCallback *uhhOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSItem *
NSSCryptoContext_Encrypt
(
  NSSCryptoContext *cc,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *data,
  NSSCallback *uhhOpt,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

PRStatus
NSSCryptoContext_BeginEncrypt
(
  NSSCryptoContext *cc,
  NSSAlgorithmAndParameters *apOpt,
  NSSCallback *uhhOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSSItem *
NSSCryptoContext_ContinueEncrypt
(
  NSSCryptoContext *cc,
  NSSItem *data,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSItem *
NSSCryptoContext_FinishEncrypt
(
  NSSCryptoContext *cc,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

PRStatus
NSSCryptoContext_Verify
(
  NSSCryptoContext *cc,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *data,
  NSSItem *signature,
  NSSCallback *uhhOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

PRStatus
NSSCryptoContext_BeginVerify
(
  NSSCryptoContext *cc,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *signature,
  NSSCallback *uhhOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

PRStatus
NSSCryptoContext_ContinueVerify
(
  NSSCryptoContext *cc,
  NSSItem *data
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

PRStatus
NSSCryptoContext_FinishVerify
(
  NSSCryptoContext *cc
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSSItem *
NSSCryptoContext_VerifyRecover
(
  NSSCryptoContext *cc,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *signature,
  NSSCallback *uhhOpt,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

PRStatus
NSSCryptoContext_BeginVerifyRecover
(
  NSSCryptoContext *cc,
  NSSAlgorithmAndParameters *apOpt,
  NSSCallback *uhhOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSSItem *
NSSCryptoContext_ContinueVerifyRecover
(
  NSSCryptoContext *cc,
  NSSItem *data,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSItem *
NSSCryptoContext_FinishVerifyRecover
(
  NSSCryptoContext *cc,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSItem *
NSSCryptoContext_WrapSymmetricKey
(
  NSSCryptoContext *cc,
  NSSAlgorithmAndParameters *apOpt,
  NSSSymmetricKey *keyToWrap,
  NSSCallback *uhhOpt,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSItem *
NSSCryptoContext_Digest
(
  NSSCryptoContext *cc,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *data,
  NSSCallback *uhhOpt,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

PRStatus
NSSCryptoContext_BeginDigest
(
  NSSCryptoContext *cc,
  NSSAlgorithmAndParameters *apOpt,
  NSSCallback *uhhOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

PRStatus
NSSCryptoContext_ContinueDigest
(
  NSSCryptoContext *cc,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *item
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSSItem *
NSSCryptoContext_FinishDigest
(
  NSSCryptoContext *cc,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCryptoContext *
NSSCryptoContext_Clone
(
  NSSCryptoContext *cc
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

