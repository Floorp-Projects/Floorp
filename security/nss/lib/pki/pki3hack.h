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

#ifndef PKINSS3HACK_H
#define PKINSS3HACK_H

#ifdef DEBUG
static const char PKINSS3HACK_CVS_ID[] = "@(#) $RCSfile: pki3hack.h,v $ $Revision: 1.18 $ $Date: 2004/07/29 23:38:14 $ $Name:  $";
#endif /* DEBUG */

#ifndef NSSDEVT_H
#include "nssdevt.h"
#endif /* NSSDEVT_H */

#ifndef DEVT_H
#include "devt.h"
#endif /* DEVT_H */

#ifndef NSSPKIT_H
#include "nsspkit.h"
#endif /* NSSPKIT_H */

#include "base.h"

#include "cert.h"

PR_BEGIN_EXTERN_C

#define NSSITEM_FROM_SECITEM(nssit, secit)  \
    (nssit)->data = (void *)(secit)->data;  \
    (nssit)->size = (PRUint32)(secit)->len;

#define SECITEM_FROM_NSSITEM(secit, nssit)          \
    (secit)->data = (unsigned char *)(nssit)->data; \
    (secit)->len  = (unsigned int)(nssit)->size;

NSS_EXTERN NSSTrustDomain *
STAN_GetDefaultTrustDomain();

NSS_EXTERN NSSCryptoContext *
STAN_GetDefaultCryptoContext();

NSS_EXTERN PRStatus
STAN_InitTokenForSlotInfo(NSSTrustDomain *td, PK11SlotInfo *slot);

NSS_EXTERN PRStatus
STAN_ResetTokenInterator(NSSTrustDomain *td);

NSS_EXTERN PRStatus
STAN_LoadDefaultNSS3TrustDomain(void);

NSS_EXTERN PRStatus
STAN_Shutdown();

NSS_EXTERN SECStatus
STAN_AddModuleToDefaultTrustDomain(SECMODModule *module);

NSS_EXTERN SECStatus
STAN_RemoveModuleFromDefaultTrustDomain(SECMODModule *module);

NSS_EXTERN CERTCertificate *
STAN_ForceCERTCertificateUpdate(NSSCertificate *c);

NSS_EXTERN CERTCertificate *
STAN_GetCERTCertificate(NSSCertificate *c);

NSS_EXTERN CERTCertificate *
STAN_GetCERTCertificateOrRelease(NSSCertificate *c);

NSS_EXTERN NSSCertificate *
STAN_GetNSSCertificate(CERTCertificate *c);

NSS_EXTERN CERTCertTrust * 
nssTrust_GetCERTCertTrustForCert(NSSCertificate *c, CERTCertificate *cc);

NSS_EXTERN PRStatus
STAN_ChangeCertTrust(CERTCertificate *cc, CERTCertTrust *trust);

NSS_EXTERN PRStatus
nssPKIX509_GetIssuerAndSerialFromDER(NSSDER *der, NSSArena *arena, 
                                     NSSDER *issuer, NSSDER *serial);

NSS_EXTERN char *
STAN_GetCERTCertificateName(PLArenaPool *arenaOpt, NSSCertificate *c);

NSS_EXTERN char *
STAN_GetCERTCertificateNameForInstance(PLArenaPool *arenaOpt,
                                       NSSCertificate *c,
                                       nssCryptokiInstance *instance);

/* exposing this */
NSS_EXTERN NSSCertificate *
NSSCertificate_Create
(
  NSSArena *arenaOpt
);

/* This function is being put here because it is a hack for 
 * PK11_FindCertFromNickname.
 */
NSS_EXTERN NSSCertificate *
nssTrustDomain_FindBestCertificateByNicknameForToken
(
  NSSTrustDomain *td,
  NSSToken *token,
  NSSUTF8 *name,
  NSSTime *timeOpt, /* NULL for "now" */
  NSSUsage *usage,
  NSSPolicies *policiesOpt /* NULL for none */
);

/* This function is being put here because it is a hack for 
 * PK11_FindCertsFromNickname.
 */
NSS_EXTERN NSSCertificate **
nssTrustDomain_FindCertificatesByNicknameForToken
(
  NSSTrustDomain *td,
  NSSToken *token,
  NSSUTF8 *name,
  NSSCertificate *rvOpt[],
  PRUint32 maximumOpt, /* 0 for no max */
  NSSArena *arenaOpt
);

/* CERT_TraversePermCertsForSubject */
NSS_EXTERN PRStatus
nssTrustDomain_TraverseCertificatesBySubject
(
  NSSTrustDomain *td,
  NSSDER *subject,
  PRStatus (*callback)(NSSCertificate *c, void *arg),
  void *arg
);

/* CERT_TraversePermCertsForNickname */
NSS_EXTERN PRStatus
nssTrustDomain_TraverseCertificatesByNickname
(
  NSSTrustDomain *td,
  NSSUTF8 *nickname,
  PRStatus (*callback)(NSSCertificate *c, void *arg),
  void *arg
);

/* SEC_TraversePermCerts */
NSS_EXTERN PRStatus
nssTrustDomain_TraverseCertificates
(
  NSSTrustDomain *td,
  PRStatus (*callback)(NSSCertificate *c, void *arg),
  void *arg
);

/* CERT_AddTempCertToPerm */
NSS_EXTERN PRStatus
nssTrustDomain_AddTempCertToPerm
(
  NSSCertificate *c
);

PR_END_EXTERN_C

#endif /* PKINSS3HACK_H */
