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

#ifndef PKINSS3HACK_H
#define PKINSS3HACK_H

#ifdef DEBUG
static const char PKINSS3HACK_CVS_ID[] = "@(#) $RCSfile: pkinss3hack.h,v $ $Revision: 1.2 $ $Date: 2001/10/17 14:40:22 $ $Name:  $";
#endif /* DEBUG */

#ifndef NSSPKIT_H
#include "nsspkit.h"
#endif /* NSSPKIT_H */

#include "cert.h"

PR_BEGIN_EXTERN_C

NSS_EXTERN NSSTrustDomain *
STAN_GetDefaultTrustDomain();

NSS_IMPLEMENT void
STAN_LoadDefaultNSS3TrustDomain
(
  void
);

NSS_EXTERN PRStatus
STAN_AddNewSlotToDefaultTD
(
  PK11SlotInfo *sl
);

NSS_EXTERN CERTCertificate *
STAN_GetCERTCertificate(NSSCertificate *c);

NSS_EXTERN NSSCertificate *
STAN_GetNSSCertificate(CERTCertificate *c);

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

PR_END_EXTERN_C

#endif /* PKINSS3HACK_H */
