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

#ifndef PKIM_H
#define PKIM_H

#ifdef DEBUG
static const char PKIM_CVS_ID[] = "@(#) $RCSfile: pkim.h,v $ $Revision: 1.14 $ $Date: 2002/01/31 17:08:32 $ $Name:  $";
#endif /* DEBUG */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

#ifndef PKITM_H
#include "pkitm.h"
#endif /* PKITM_H */

PR_BEGIN_EXTERN_C

NSS_EXTERN NSSToken *
STAN_GetDefaultCryptoToken
(
  void
);

NSS_EXTERN nssHash *
nssHash_CreateCertificate
(
  NSSArena *arenaOpt,
  PRUint32 numBuckets
);

/* Token ordering routines */

/*
 * Given a crypto algorithm, return the preferred token for performing
 * the crypto operation.
 */
NSS_EXTERN NSSToken *
nssTrustDomain_GetCryptoToken
(
  NSSTrustDomain *td,
  NSSAlgorithmAndParameters *ap
);

/* The following routines are used to obtain the preferred token on which 
 * to store particular objects.
 */

/*
 * Find the preferred token for storing user certificates.
 */
NSS_EXTERN NSSToken *
nssTrustDomain_GetUserCertToken
(
  NSSTrustDomain *td
);

/*
 * Find the preferred token for storing email certificates.
 */
NSS_EXTERN NSSToken *
nssTrustDomain_GetEmailCertToken
(
  NSSTrustDomain *td
);

/*
 * Find the preferred token for storing SSL certificates.
 */
NSS_EXTERN NSSToken *
nssTrustDomain_GetSSLCertToken
(
  NSSTrustDomain *td
);

/*
 * Find the preferred token for storing root certificates.
 */
NSS_EXTERN NSSToken *
nssTrustDomain_GetRootCertToken
(
  NSSTrustDomain *td
);

/*
 * Find the preferred token for storing private keys. 
 */
NSS_EXTERN NSSToken *
nssTrustDomain_GetPrivateKeyToken
(
  NSSTrustDomain *td
);

/*
 * Find the preferred token for storing symmetric keys. 
 */
NSS_EXTERN NSSToken *
nssTrustDomain_GetSymmetricKeyToken
(
  NSSTrustDomain *td
);

/* Certificate cache routines */

NSS_EXTERN PRStatus
nssTrustDomain_AddCertsToCache
(
  NSSTrustDomain *td,
  NSSCertificate **certs,
  PRUint32 numCerts
);

NSS_EXTERN void
nssTrustDomain_RemoveCertFromCache
(
  NSSTrustDomain *td,
  NSSCertificate *cert
);

NSS_EXTERN void
nssTrustDomain_FlushCache
(
  NSSTrustDomain *td,
  PRFloat64 threshold
);

NSS_IMPLEMENT PRStatus
nssTrustDomain_DestroyCache
(
  NSSTrustDomain *td
);

/* 
 * Remove all certs for the given token from the cache.  This is
 * needed if the token is removed.
 */
NSS_EXTERN PRStatus
nssTrustDomain_RemoveTokenCertsFromCache
(
  NSSTrustDomain *td,
  NSSToken *token
);

/*
 * Find all cached certs with this nickname (label).
 */
NSS_EXTERN NSSCertificate **
nssTrustDomain_GetCertsForNicknameFromCache
(
  NSSTrustDomain *td,
  NSSUTF8 *nickname,
  nssList *certListOpt
);

/*
 * Find all cached certs with this email address.
 */
NSS_EXTERN NSSCertificate **
nssTrustDomain_GetCertsForEmailAddressFromCache
(
  NSSTrustDomain *td,
  NSSASCII7 *email,
  nssList *certListOpt
);

/*
 * Find all cached certs with this subject.
 */
NSS_EXTERN NSSCertificate **
nssTrustDomain_GetCertsForSubjectFromCache
(
  NSSTrustDomain *td,
  NSSDER *subject,
  nssList *certListOpt
);

/*
 * Look for a specific cert in the cache.
 */
NSS_EXTERN NSSCertificate *
nssTrustDomain_GetCertForIssuerAndSNFromCache
(
  NSSTrustDomain *td,
  NSSDER *issuer,
  NSSDER *serialNum
);

/*
 * Look for a specific cert in the cache.
 */
NSS_EXTERN NSSCertificate *
nssTrustDomain_GetCertByDERFromCache
(
  NSSTrustDomain *td,
  NSSDER *der
);

/* Get all certs from the cache */
/* XXX this is being included to make some old-style calls word, not to
 *     say we should keep it
 */
NSS_EXTERN NSSCertificate **
nssTrustDomain_GetCertsFromCache
(
  NSSTrustDomain *td,
  nssList *certListOpt
);

NSS_EXTERN PRStatus
nssCertificate_SetCertTrust
(
  NSSCertificate *c,
  NSSTrust *trust
);

NSS_EXTERN nssDecodedCert *
nssCertificate_GetDecoding
(
  NSSCertificate *c
);

NSS_EXTERN nssDecodedCert *
nssDecodedCert_Create
(
  NSSArena *arenaOpt,
  NSSDER *encoding,
  NSSCertificateType type
);

NSS_EXTERN PRStatus
nssDecodedCert_Destroy
(
  nssDecodedCert *dc
);

NSS_EXTERN void 
nssBestCertificate_SetArgs
(
  nssBestCertificateCB *best,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policies
);

NSS_EXTERN PRStatus 
nssBestCertificate_Callback
(
  NSSCertificate *c, 
  void *arg
);

NSS_EXTERN PRStatus
nssPKIObject_Initialize
(
  struct nssPKIObjectBaseStr *object,
  NSSArena *arena,
  NSSTrustDomain *td,
  NSSCryptoContext *cc
);

NSS_EXTERN void
nssPKIObject_AddRef
(
  struct nssPKIObjectBaseStr *object
);

NSS_EXTERN PRBool
nssPKIObject_Destroy
(
  struct nssPKIObjectBaseStr *object
);

NSS_EXTERN NSSTime *
NSSTime_Now
(
  NSSTime *timeOpt
);

NSS_EXTERN NSSTime *
NSSTime_SetPRTime
(
  NSSTime *timeOpt,
  PRTime prTime
);

NSS_EXTERN PRTime
NSSTime_GetPRTime
(
  NSSTime *time
);

NSS_EXTERN void
nssTrustDomain_DumpCacheInfo
(
  NSSTrustDomain *td,
  void (* cert_dump_iter)(const void *, void *, void *),
  void *arg
);

PR_END_EXTERN_C

#endif /* PKIM_H */
