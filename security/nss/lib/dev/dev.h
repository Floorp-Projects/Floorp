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
static const char DEV_CVS_ID[] = "@(#) $RCSfile: dev.h,v $ $Revision: 1.12 $ $Date: 2001/11/08 00:14:52 $ $Name:  $";
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

#ifndef DEVT_H
#include "devt.h"
#endif /* DEVT_H */

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

/* Given a raw attribute template, import an object 
 * (certificate, public key, private key, symmetric key)
 */
NSS_EXTERN CK_OBJECT_HANDLE
nssToken_ImportObject
(
  NSSToken *tok,
  nssSession *sessionOpt,
  CK_ATTRIBUTE_PTR objectTemplate,
  CK_ULONG otsize
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
  NSSToken *tok,
  nssSession *sessionOpt,
  CK_OBJECT_HANDLE object
);

NSS_EXTERN CK_OBJECT_HANDLE
nssToken_FindObjectByTemplate
(
  NSSToken *tok,
  nssSession *sessionOpt,
  CK_ATTRIBUTE_PTR cktemplate,
  CK_ULONG ctsize
);

NSS_EXTERN PRStatus
nssToken_TraverseCertificatesByTemplate
(
  NSSToken *tok,
  nssSession *sessionOpt,
  nssList *cachedList,
  CK_ATTRIBUTE_PTR cktemplate,
  CK_ULONG ctsize,
  PRStatus (*callback)(NSSCertificate *c, void *arg),
  void *arg
);

NSS_EXTERN PRStatus *
nssToken_TraverseCertificates
(
  NSSToken *tok,
  nssSession *sessionOpt,
  PRStatus (*callback)(NSSCertificate *c, void *arg),
  void *arg
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

#ifdef DEBUG
void nssModule_Debug(NSSModule *m);
#endif

PR_END_EXTERN_C

#endif /* DEV_H */
