/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef PKI_H
#define PKI_H

#ifdef DEBUG
static const char PKI_CVS_ID[] = "@(#) $RCSfile: pki.h,v $ $Revision: 1.13.196.1 $ $Date: 2012/05/17 21:40:54 $";
#endif /* DEBUG */

#ifndef NSSDEVT_H
#include "nssdevt.h"
#endif /* NSSDEVT_H */

#ifndef NSSPKI_H
#include "nsspki.h"
#endif /* NSSPKI_H */

#ifndef PKIT_H
#include "pkit.h"
#endif /* PKIT_H */

PR_BEGIN_EXTERN_C

NSS_EXTERN NSSCallback *
nssTrustDomain_GetDefaultCallback
(
  NSSTrustDomain *td,
  PRStatus *statusOpt
);

NSS_EXTERN NSSCertificate **
nssTrustDomain_FindCertificatesBySubject
(
  NSSTrustDomain *td,
  NSSDER *subject,
  NSSCertificate *rvOpt[],
  PRUint32 maximumOpt,
  NSSArena *arenaOpt
);

NSS_EXTERN NSSTrust *
nssTrustDomain_FindTrustForCertificate
(
  NSSTrustDomain *td,
  NSSCertificate *c
);

NSS_EXTERN NSSCertificate *
nssCertificate_AddRef
(
  NSSCertificate *c
);

NSS_EXTERN PRStatus
nssCertificate_Destroy
(
  NSSCertificate *c
);

NSS_EXTERN NSSDER *
nssCertificate_GetEncoding
(
  NSSCertificate *c
);

NSS_EXTERN NSSDER *
nssCertificate_GetIssuer
(
  NSSCertificate *c
);

NSS_EXTERN NSSDER *
nssCertificate_GetSerialNumber
(
  NSSCertificate *c
);

NSS_EXTERN NSSDER *
nssCertificate_GetSubject
(
  NSSCertificate *c
);

/* Returns a copy, Caller must free using nss_ZFreeIf */
NSS_EXTERN NSSUTF8 *
nssCertificate_GetNickname
(
  NSSCertificate *c,
  NSSToken *tokenOpt
);

NSS_EXTERN NSSASCII7 *
nssCertificate_GetEmailAddress
(
  NSSCertificate *c
);

NSS_EXTERN PRBool
nssCertificate_IssuerAndSerialEqual
(
  NSSCertificate *c1,
  NSSCertificate *c2
);

NSS_EXTERN NSSPrivateKey *
nssPrivateKey_AddRef
(
  NSSPrivateKey *vk
);

NSS_EXTERN PRStatus
nssPrivateKey_Destroy
(
  NSSPrivateKey *vk
);

NSS_EXTERN NSSItem *
nssPrivateKey_GetID
(
  NSSPrivateKey *vk
);

NSS_EXTERN NSSUTF8 *
nssPrivateKey_GetNickname
(
  NSSPrivateKey *vk,
  NSSToken *tokenOpt
);

NSS_EXTERN PRStatus
nssPublicKey_Destroy
(
  NSSPublicKey *bk
);

NSS_EXTERN NSSItem *
nssPublicKey_GetID
(
  NSSPublicKey *vk
);

NSS_EXTERN NSSCertificate **
nssCryptoContext_FindCertificatesBySubject
(
  NSSCryptoContext *cc,
  NSSDER *subject,
  NSSCertificate *rvOpt[],
  PRUint32 maximumOpt, /* 0 for no max */
  NSSArena *arenaOpt
);

/* putting here for now, needs more thought */
NSS_EXTERN PRStatus
nssCryptoContext_ImportTrust
(
  NSSCryptoContext *cc,
  NSSTrust *trust
);

NSS_EXTERN NSSTrust *
nssCryptoContext_FindTrustForCertificate
(
  NSSCryptoContext *cc,
  NSSCertificate *cert
);

NSS_EXTERN PRStatus
nssCryptoContext_ImportSMIMEProfile
(
  NSSCryptoContext *cc,
  nssSMIMEProfile *profile
);

NSS_EXTERN nssSMIMEProfile *
nssCryptoContext_FindSMIMEProfileForCertificate
(
  NSSCryptoContext *cc,
  NSSCertificate *cert
);

NSS_EXTERN NSSTrust *
nssTrust_AddRef
(
  NSSTrust *trust
);

NSS_EXTERN PRStatus
nssTrust_Destroy
(
  NSSTrust *trust
);

NSS_EXTERN nssSMIMEProfile *
nssSMIMEProfile_AddRef
(
  nssSMIMEProfile *profile
);

NSS_EXTERN PRStatus
nssSMIMEProfile_Destroy
(
  nssSMIMEProfile *profile
);

NSS_EXTERN nssSMIMEProfile *
nssSMIMEProfile_Create
(
  NSSCertificate *cert,
  NSSItem *profileTime,
  NSSItem *profileData
);

PR_END_EXTERN_C

#endif /* PKI_H */
