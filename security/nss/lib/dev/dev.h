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

#ifndef DEV_H
#define DEV_H

#ifdef DEBUG
static const char DEV_CVS_ID[] = "@(#) $RCSfile: dev.h,v $ $Revision: 1.21 $ $Date: 2002/03/07 23:21:32 $ $Name:  $";
#endif /* DEBUG */

#ifndef DEVT_H
#include "devt.h"
#endif /* DEVT_H */

#ifndef NSSCKT_H
#include "nssckt.h"
#endif /* NSSCKT_H */

#ifndef NSSPKIT_H
#include "nsspkit.h"
#endif /* NSSPKIT_H */

#ifndef BASET_H
#include "baset.h"
#endif /* BASET_H */

/*
 * nssdev.h
 *
 * This file prototypes the methods of the low-level cryptoki devices.
 *
 *  |-----------|---> NSSSlot <--> NSSToken
 *  | NSSModule |---> NSSSlot <--> NSSToken
 *  |-----------|---> NSSSlot <--> NSSToken
 */

PR_BEGIN_EXTERN_C

NSS_EXTERN NSSModule *
nssModule_Create
(
  NSSUTF8 *moduleOpt,
  NSSUTF8 *uriOpt,
  NSSUTF8 *opaqueOpt, /* XXX is this where the mech flags go??? */
  void    *reserved
  /* XXX more?  */
);

/* This is to use the new loading mechanism. */
NSS_EXTERN NSSModule *
nssModule_CreateFromSpec
(
  NSSUTF8 *moduleSpec
);

NSS_EXTERN PRStatus
nssModule_Destroy
(
  NSSModule *mod
);

NSS_EXTERN NSSModule *
nssModule_AddRef
(
  NSSModule *mod
);

NSS_EXTERN PRStatus
nssModule_Load
(
  NSSModule *mod
);

NSS_EXTERN PRStatus
nssModule_Unload
(
  NSSModule *mod
);

NSS_EXTERN PRStatus
nssModule_LogoutAllSlots
(
  NSSModule *mod
);

NSS_EXTERN NSSSlot **
nssModule_GetSlots
(
  NSSModule *mod
);

NSS_EXTERN NSSSlot *
nssModule_FindSlotByName
(
  NSSModule *mod,
  NSSUTF8 *slotName
);

NSS_EXTERN NSSToken *
nssModule_FindTokenByName
(
  NSSModule *mod,
  NSSUTF8 *tokenName
);

/* This descends from NSSTrustDomain_TraverseCertificates, a questionable
 * function.  Do we want NSS to have access to this at the module level?
 */
NSS_EXTERN PRStatus *
nssModule_TraverseCertificates
(
  NSSModule *mod,
  PRStatus (*callback)(NSSCertificate *c, void *arg),
  void *arg
);

NSS_EXTERN NSSSlot *
nssSlot_Create
(
  NSSArena *arenaOpt,
  CK_SLOT_ID slotId,
  NSSModule *parent
);

NSS_EXTERN PRStatus
nssSlot_Destroy
(
  NSSSlot *slot
);

NSS_EXTERN PRBool
nssSlot_IsPermanent
(
  NSSSlot *slot
);

NSS_EXTERN PRStatus
nssSlot_Refresh
(
  NSSSlot *slot
);

NSS_EXTERN NSSSlot *
nssSlot_AddRef
(
  NSSSlot *slot
);

NSS_EXTERN NSSUTF8 *
nssSlot_GetName
(
  NSSSlot *slot,
  NSSArena *arenaOpt
);

NSS_EXTERN PRStatus
nssSlot_Login
(
  NSSSlot *slot,
  PRBool asSO,
  NSSCallback *pwcb
);
extern const NSSError NSS_ERROR_INVALID_PASSWORD;
extern const NSSError NSS_ERROR_USER_CANCELED;

NSS_EXTERN PRStatus
nssSlot_Logout
(
  NSSSlot *slot,
  nssSession *sessionOpt
);

#define NSSSLOT_ASK_PASSWORD_FIRST_TIME -1
#define NSSSLOT_ASK_PASSWORD_EVERY_TIME  0
NSS_EXTERN void
nssSlot_SetPasswordDefaults
(
  NSSSlot *slot,
  PRInt32 askPasswordTimeout
);

NSS_EXTERN PRStatus
nssSlot_SetPassword
(
  NSSSlot *slot,
  NSSCallback *pwcb
);
extern const NSSError NSS_ERROR_INVALID_PASSWORD;
extern const NSSError NSS_ERROR_USER_CANCELED;

/*
 * nssSlot_IsLoggedIn
 */

NSS_EXTERN nssSession *
nssSlot_CreateSession
(
  NSSSlot *slot,
  NSSArena *arenaOpt,
  PRBool readWrite /* so far, this is the only flag used */
);

NSS_EXTERN NSSToken *
nssToken_Create
(
  NSSArena *arenaOpt,
  CK_SLOT_ID slotID,
  NSSSlot *parent
);

NSS_EXTERN PRStatus
nssToken_Destroy
(
  NSSToken *tok
);

NSS_EXTERN PRBool
nssToken_IsPresent
(
  NSSToken *token
);

NSS_EXTERN NSSToken *
nssToken_AddRef
(
  NSSToken *tok
);

NSS_EXTERN NSSUTF8 *
nssToken_GetName
(
  NSSToken *tok
);

NSS_EXTERN PRStatus
nssToken_ImportCertificate
(
  NSSToken *tok,
  nssSession *sessionOpt,
  NSSCertificate *cert,
  NSSUTF8 *nickname,
  PRBool asTokenObject
);
 
NSS_EXTERN PRStatus
nssToken_ImportTrust
(
  NSSToken *tok,
  nssSession *sessionOpt,
  NSSTrust *trust,
  PRBool asTokenObject
);

NSS_EXTERN PRStatus
nssToken_SetTrustCache
(
  NSSToken *tok
);

NSS_EXTERN PRStatus
nssToken_SetCrlCache
(
  NSSToken *tok
);

NSS_EXTERN PRBool
nssToken_HasCrls
(
  NSSToken *tok
);

NSS_EXTERN PRStatus
nssToken_SetHasCrls
(
  NSSToken *tok
);

NSS_EXTERN NSSPublicKey *
nssToken_GenerateKeyPair
(
  NSSToken *tok,
  nssSession *sessionOpt
  /* algorithm and parameters */
);

NSS_EXTERN NSSSymmetricKey *
nssToken_GenerateSymmetricKey
(
  NSSToken *tok,
  nssSession *sessionOpt
  /* algorithm and parameters */
);

/* Permanently remove an object from the token. */
NSS_EXTERN PRStatus
nssToken_DeleteStoredObject
(
  nssCryptokiInstance *instance
);

NSS_EXTERN NSSTrust *
nssToken_FindTrustForCert
(
  NSSToken *token,
  nssSession *sessionOpt,
  NSSCertificate *c,
  nssTokenSearchType searchType
);

NSS_EXTERN PRStatus
nssToken_TraverseCertificates
(
  NSSToken *tok,
  nssSession *sessionOpt,
  nssTokenCertSearch *search
);

NSS_EXTERN PRStatus
nssToken_TraverseCertificatesBySubject
(
  NSSToken *token,
  nssSession *sessionOpt,
  NSSDER *subject,
  nssTokenCertSearch *search
);

NSS_EXTERN PRStatus
nssToken_TraverseCertificatesByNickname
(
  NSSToken *token,
  nssSession *sessionOpt,
  NSSUTF8 *name,
  nssTokenCertSearch *search
);

NSS_EXTERN PRStatus
nssToken_TraverseCertificatesByEmail
(
  NSSToken *token,
  nssSession *sessionOpt,
  NSSASCII7 *email,
  nssTokenCertSearch *search
);

NSS_EXTERN NSSCertificate *
nssToken_FindCertificateByIssuerAndSerialNumber
(
  NSSToken *token,
  nssSession *sessionOpt,
  NSSDER *issuer,
  NSSDER *serial,
  nssTokenSearchType searchType
);

NSS_EXTERN NSSCertificate *
nssToken_FindCertificateByEncodedCertificate
(
  NSSToken *token,
  nssSession *sessionOpt,
  NSSBER *encodedCertificate,
  nssTokenSearchType searchType
);

NSS_EXTERN NSSTrust *
nssToken_FindTrustForCert
(
  NSSToken *token,
  nssSession *session,
  NSSCertificate *c,
  nssTokenSearchType searchType
);

NSS_EXTERN NSSItem *
nssToken_Digest
(
  NSSToken *tok,
  nssSession *sessionOpt,
  NSSAlgorithmAndParameters *ap,
  NSSItem *data,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
);

NSS_EXTERN PRStatus
nssToken_BeginDigest
(
  NSSToken *tok,
  nssSession *sessionOpt,
  NSSAlgorithmAndParameters *ap
);

NSS_EXTERN PRStatus
nssToken_ContinueDigest
(
  NSSToken *tok,
  nssSession *sessionOpt,
  NSSItem *item
);

NSS_EXTERN NSSItem *
nssToken_FinishDigest
(
  NSSToken *tok,
  nssSession *sessionOpt,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
);

NSS_EXTERN PRStatus
nssSession_Destroy
(
  nssSession *s
);

/* would like to inline */
NSS_EXTERN PRStatus
nssSession_EnterMonitor
(
  nssSession *s
);

/* would like to inline */
NSS_EXTERN PRStatus
nssSession_ExitMonitor
(
  nssSession *s
);

/* would like to inline */
NSS_EXTERN PRBool
nssSession_IsReadWrite
(
  nssSession *s
);

NSS_EXTERN NSSAlgorithmAndParameters *
NSSAlgorithmAndParameters_CreateSHA1Digest
(
  NSSArena *arenaOpt
);

NSS_EXTERN NSSAlgorithmAndParameters *
NSSAlgorithmAndParameters_CreateMD5Digest
(
  NSSArena *arenaOpt
);

#ifdef NSS_3_4_CODE
/* exposing this for the smart card cache code */
NSS_EXTERN nssCryptokiInstance *
nssCryptokiInstance_Create
(
  NSSArena *arena,
  NSSToken *t, 
  CK_OBJECT_HANDLE h,
  PRBool isTokenObject
);
#endif

PR_END_EXTERN_C

#endif /* DEV_H */
