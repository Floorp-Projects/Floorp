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

#ifndef PKISTORE_H
#define PKISTORE_H

#ifdef DEBUG
static const char PKISTORE_CVS_ID[] = "@(#) $RCSfile: pkistore.h,v $ $Revision: 1.5 $ $Date: 2003/01/08 21:48:45 $ $Name:  $";
#endif /* DEBUG */

#ifndef NSSPKIT_H
#include "nsspkit.h"
#endif /* NSSPKIT_H */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

PR_BEGIN_EXTERN_C

/* 
 * PKI Stores
 *
 * This is a set of routines for managing local stores of PKI objects.
 * Currently, the only application is in crypto contexts, where the
 * certificate store is used.  In the future, methods should be added
 * here for storing local references to keys.
 */

/* 
 * nssCertificateStore
 *
 * Manages local store of certificate, trust, and S/MIME profile objects.
 * Within a crypto context, mappings of cert to trust and cert to S/MIME
 * profile are always 1-1.  Therefore, it is reasonable to store all objects
 * in a single collection, indexed by the certificate.
 */

NSS_EXTERN nssCertificateStore *
nssCertificateStore_Create
(
  NSSArena *arenaOpt
);

NSS_EXTERN PRStatus
nssCertificateStore_Destroy
(
  nssCertificateStore *store
);

NSS_EXTERN PRStatus
nssCertificateStore_Add
(
  nssCertificateStore *store,
  NSSCertificate *cert
);

NSS_EXTERN void
nssCertificateStore_RemoveCertLOCKED
(
  nssCertificateStore *store,
  NSSCertificate *cert
);

NSS_EXTERN void
nssCertificateStore_Lock (
  nssCertificateStore *store
);

NSS_EXTERN void
nssCertificateStore_Unlock (
  nssCertificateStore *store
);

NSS_EXTERN NSSCertificate **
nssCertificateStore_FindCertificatesBySubject
(
  nssCertificateStore *store,
  NSSDER *subject,
  NSSCertificate *rvOpt[],
  PRUint32 maximumOpt,
  NSSArena *arenaOpt
);

NSS_EXTERN NSSCertificate **
nssCertificateStore_FindCertificatesByNickname
(
  nssCertificateStore *store,
  NSSUTF8 *nickname,
  NSSCertificate *rvOpt[],
  PRUint32 maximumOpt,
  NSSArena *arenaOpt
);

NSS_EXTERN NSSCertificate **
nssCertificateStore_FindCertificatesByEmail
(
  nssCertificateStore *store,
  NSSASCII7 *email,
  NSSCertificate *rvOpt[],
  PRUint32 maximumOpt,
  NSSArena *arenaOpt
);

NSS_EXTERN NSSCertificate *
nssCertificateStore_FindCertificateByIssuerAndSerialNumber
(
  nssCertificateStore *store,
  NSSDER *issuer,
  NSSDER *serial
);

NSS_EXTERN NSSCertificate *
nssCertificateStore_FindCertificateByEncodedCertificate
(
  nssCertificateStore *store,
  NSSDER *encoding
);

NSS_EXTERN PRStatus
nssCertificateStore_AddTrust
(
  nssCertificateStore *store,
  NSSTrust *trust
);

NSS_EXTERN NSSTrust *
nssCertificateStore_FindTrustForCertificate
(
  nssCertificateStore *store,
  NSSCertificate *cert
);

NSS_EXTERN PRStatus
nssCertificateStore_AddSMIMEProfile
(
  nssCertificateStore *store,
  nssSMIMEProfile *profile
);

NSS_EXTERN nssSMIMEProfile *
nssCertificateStore_FindSMIMEProfileForCertificate
(
  nssCertificateStore *store,
  NSSCertificate *cert
);

NSS_EXTERN void
nssCertificateStore_DumpStoreInfo
(
  nssCertificateStore *store,
  void (* cert_dump_iter)(const void *, void *, void *),
  void *arg
);

PR_END_EXTERN_C

#endif /* PKISTORE_H */
