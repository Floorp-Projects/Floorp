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
static const char CVS_ID[] = "@(#) $RCSfile: trustdomain.c,v $ $Revision: 1.1 $ $Date: 2001/07/19 20:41:38 $ $Name:  $";
#endif /* DEBUG */

#ifndef NSSPKI_H
#include "nsspki.h"
#endif /* NSSPKI_H */

NSSTrustDomain *
NSSTrustDomain_Create
(
  NSSUTF8 *moduleOpt,
  NSSUTF8 *uriOpt,
  NSSUTF8 *opaqueOpt,
  void *reserved
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

PRStatus
NSSTrustDomain_Destroy
(
  NSSTrustDomain *td
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

PRStatus
NSSTrustDomain_SetDefaultCallback
(
  NSSTrustDomain *td,
  NSSCallback *newCallback,
  NSSCallback **oldCallbackOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSSCallback *
NSSTrustDomain_GetDefaultCallback
(
  NSSTrustDomain *td,
  PRStatus *statusOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

PRStatus
NSSTrustDomain_LoadModule
(
  NSSUTF8 *moduleOpt,
  NSSUTF8 *uriOpt,
  NSSUTF8 *opaqueOpt,
  void *reserved
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

PRStatus
NSSTrustDomain_DisableToken
(
  NSSTrustDomain *td,
  NSSToken *token,
  NSSError why
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

PRStatus
NSSTrustDomain_EnableToken
(
  NSSTrustDomain *td,
  NSSToken *token
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

PRStatus
NSSTrustDomain_IsTokenEnabled
(
  NSSTrustDomain *td,
  NSSToken *token,
  NSSError *whyOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSSSlot *
NSSTrustDomain_FindSlotByName
(
  NSSTrustDomain *td,
  NSSUTF8 *slotName
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSToken *
NSSTrustDomain_FindTokenByName
(
  NSSTrustDomain *td,
  NSSUTF8 *tokenName
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSToken *
NSSTrustDomain_FindTokenBySlotName
(
  NSSTrustDomain *td,
  NSSUTF8 *slotName
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSToken *
NSSTrustDomain_FindTokenForAlgorithm
(
  NSSTrustDomain *td,
  NSSOID *algorithm
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSToken *
NSSTrustDomain_FindBestTokenForAlgorithms
(
  NSSTrustDomain *td,
  NSSOID *algorithms[], /* may be null-terminated */
  PRUint32 nAlgorithmsOpt /* limits the array if nonzero */
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

PRStatus
NSSTrustDomain_Login
(
  NSSTrustDomain *td,
  NSSCallback *uhhOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

PRStatus
NSSTrustDomain_Logout
(
  NSSTrustDomain *td
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSSCertificate *
NSSTrustDomain_ImportCertificate
(
  NSSTrustDomain *td,
  NSSCertificate *c
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate *
NSSTrustDomain_ImportPKIXCertificate
(
  NSSTrustDomain *td,
  /* declared as a struct until these "data types" are defined */
  struct NSSPKIXCertificateStr *pc
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate *
NSSTrustDomain_ImportEncodedCertificate
(
  NSSTrustDomain *td,
  NSSBER *ber
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate **
NSSTrustDomain_ImportEncodedCertificateChain
(
  NSSTrustDomain *td,
  NSSBER *ber,
  NSSCertificate *rvOpt[],
  PRUint32 maximumOpt, /* 0 for no max */
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSPrivateKey *
NSSTrustDomain_ImportEncodedPrivateKey
(
  NSSTrustDomain *td,
  NSSBER *ber,
  NSSItem *passwordOpt, /* NULL will cause a callback */
  NSSCallback *uhhOpt,
  NSSToken *destination
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSPublicKey *
NSSTrustDomain_ImportEncodedPublicKey
(
  NSSTrustDomain *td,
  NSSBER *ber
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate *
NSSTrustDomain_FindBestCertificateByNickname
(
  NSSTrustDomain *td,
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
NSSTrustDomain_FindCertificatesByNickname
(
  NSSTrustDomain *td,
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
NSSTrustDomain_FindCertificateByIssuerAndSerialNumber
(
  NSSTrustDomain *td,
  NSSDER *issuer,
  NSSDER *serialNumber
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate *
NSSTrustDomain_FindBestCertificateBySubject
(
  NSSTrustDomain *td,
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
NSSTrustDomain_FindCertificatesBySubject
(
  NSSTrustDomain *td,
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
NSSTrustDomain_FindBestCertificateByNameComponents
(
  NSSTrustDomain *td,
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
NSSTrustDomain_FindCertificatesByNameComponents
(
  NSSTrustDomain *td,
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
NSSTrustDomain_FindCertificateByEncodedCertificate
(
  NSSTrustDomain *td,
  NSSBER *encodedCertificate
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate *
NSSTrustDomain_FindCertificateByEmail
(
  NSSTrustDomain *td,
  NSSASCII7 *email,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate **
NSSTrustDomain_FindCertificatesByEmail
(
  NSSTrustDomain *td,
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
NSSTrustDomain_FindCertificateByOCSPHash
(
  NSSTrustDomain *td,
  NSSItem *hash
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate *
NSSTrustDomain_FindBestUserCertificate
(
  NSSTrustDomain *td,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCertificate **
NSSTrustDomain_FindUserCertificates
(
  NSSTrustDomain *td,
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
NSSTrustDomain_FindBestUserCertificateForSSLClientAuth
(
  NSSTrustDomain *td,
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
NSSTrustDomain_FindUserCertificatesForSSLClientAuth
(
  NSSTrustDomain *td,
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
NSSTrustDomain_FindBestUserCertificateForEmailSigning
(
  NSSTrustDomain *td,
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

NSSCertificate **
NSSTrustDomain_FindUserCertificatesForEmailSigning
(
  NSSTrustDomain *td,
  NSSASCII7 *signerOpt,
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
NSSTrustDomain_GenerateKeyPair
(
  NSSTrustDomain *td,
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
NSSTrustDomain_GenerateSymmetricKey
(
  NSSTrustDomain *td,
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
NSSTrustDomain_GenerateSymmetricKeyFromPassword
(
  NSSTrustDomain *td,
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
NSSTrustDomain_FindSymmetricKeyByAlgorithmAndKeyID
(
  NSSTrustDomain *td,
  NSSOID *algorithm,
  NSSItem *keyID,
  NSSCallback *uhhOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCryptoContext *
NSSTrustDomain_CreateCryptoContext
(
  NSSTrustDomain *td,
  NSSCallback *uhhOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCryptoContext *
NSSTrustDomain_CreateCryptoContextForAlgorithm
(
  NSSTrustDomain *td,
  NSSOID *algorithm
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSSCryptoContext *
NSSTrustDomain_CreateCryptoContextForAlgorithmAndParameters
(
  NSSTrustDomain *td,
  NSSAlgorithmAndParameters *ap
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

