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
static const char CVS_ID[] = "@(#) $RCSfile: dev3hack.c,v $ $Revision: 1.1 $ $Date: 2001/11/08 00:15:05 $ $Name:  $";
#endif /* DEBUG */

#ifndef NSS_3_4_CODE
#define NSS_3_4_CODE
#endif /* NSS_3_4_CODE */

#ifndef PKIT_H
#include "pkit.h"
#endif /* PKIT_H */

#ifndef DEVT_H
#include "devt.h"
#endif /* DEVT_H */

#include "dev3hack.h"

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

#include "pk11func.h"
#include "secmodti.h"

NSS_IMPLEMENT nssSession *
nssSession_ImportNSS3Session(NSSArena *arenaOpt,
                             CK_SESSION_HANDLE session, 
                             PZLock *lock, PRBool rw)
{
    nssSession *rvSession;
    rvSession = nss_ZNEW(arenaOpt, nssSession);
    rvSession->handle = session;
    rvSession->lock = lock;
    rvSession->isRW = rw;
    return rvSession;
}

NSS_IMPLEMENT nssSession *
nssSlot_CreateSession
(
  NSSSlot *slot,
  NSSArena *arenaOpt,
  PRBool readWrite
)
{
    nssSession *rvSession;
    rvSession = nss_ZNEW(arenaOpt, nssSession);
    if (!rvSession) {
	return (nssSession *)NULL;
    }
    if (readWrite) {
	rvSession->handle = PK11_GetRWSession(slot->pk11slot);
	rvSession->isRW = PR_TRUE;
	rvSession->slot = slot;
	return rvSession;
    } else {
	return NULL;
    }
}

NSS_IMPLEMENT PRStatus
nssSession_Destroy
(
  nssSession *s
)
{
    CK_RV ckrv = CKR_OK;
    if (s) {
	if (s->isRW) {
	    PK11_RestoreROSession(s->slot->pk11slot, s->handle);
	}
	nss_ZFreeIf(s);
    }
    return (ckrv == CKR_OK) ? PR_SUCCESS : PR_FAILURE;
}

static NSSSlot *
nssSlot_CreateFromPK11SlotInfo(NSSTrustDomain *td, PK11SlotInfo *nss3slot)
{
    PRUint32 length;
    NSSSlot *rvSlot;
    rvSlot = nss_ZNEW(td->arena, NSSSlot);
    if (!rvSlot) {
	return NULL;
    }
    rvSlot->refCount = 1;
    rvSlot->pk11slot = nss3slot;
    rvSlot->epv = nss3slot->functionList;
    rvSlot->slotID = nss3slot->slotID;
    rvSlot->trustDomain = td;
    /* Grab the slot name from the PKCS#11 fixed-length buffer */
    length = nssPKCS11StringLength(nss3slot->slot_name,
                                   sizeof(nss3slot->slot_name));
    if (length > 0) {
	rvSlot->name = nssUTF8_Create(td->arena, nssStringType_UTF8String, 
	                              (void *)nss3slot->slot_name, length);
    }
    return rvSlot;
}

NSS_IMPLEMENT NSSToken *
nssToken_CreateFromPK11SlotInfo(NSSTrustDomain *td, PK11SlotInfo *nss3slot)
{
    PRUint32 length;
    NSSToken *rvToken;
    rvToken = nss_ZNEW(td->arena, NSSToken);
    if (!rvToken) {
	return NULL;
    }
    rvToken->refCount = 1;
    rvToken->pk11slot = nss3slot;
    rvToken->epv = nss3slot->functionList;
    rvToken->defaultSession = nssSession_ImportNSS3Session(td->arena,
                                                       nss3slot->session,
                                                       nss3slot->sessionLock,
                                                       nss3slot->defRWSession);
    rvToken->trustDomain = td;
    /* Grab the token name from the PKCS#11 fixed-length buffer */
    length = nssPKCS11StringLength(nss3slot->token_name,
                                   sizeof(nss3slot->token_name));
    if (length > 0) {
	rvToken->name = nssUTF8_Create(td->arena, nssStringType_UTF8String, 
	                               (void *)nss3slot->token_name, length);
    }
    rvToken->slot = nssSlot_CreateFromPK11SlotInfo(td, nss3slot);
    rvToken->slot->token = rvToken;
    rvToken->defaultSession->slot = rvToken->slot;
    return rvToken;
}


NSSTrustDomain *
nssToken_GetTrustDomain(NSSToken *token)
{
    return token->trustDomain;
}

typedef enum {
    nssPK11Event_DefaultSessionRO = 0,
    nssPK11Event_DefaultSessionRW = 1
} nssPK11Event;

NSS_IMPLEMENT PRStatus
nssToken_Nofify
(
  NSSToken *tok,
  nssPK11Event event
)
{
    switch (event) {
    default:
	return PR_FAILURE;
    }
    return PR_FAILURE;
}

