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
static const char CVS_ID[] = "@(#) $RCSfile: slot.c,v $ $Revision: 1.2 $ $Date: 2001/09/13 22:14:22 $ $Name:  $";
#endif /* DEBUG */

#ifndef DEV_H
#include "dev.h"
#endif /* DEV_H */

#ifndef DEVM_H
#include "devm.h"
#endif /* DEVM_H */

#ifndef NSSCKEPV_H
#include "nssckepv.h"
#endif /* NSSCKEPV_H */

#ifndef CKHELPER_H
#include "ckhelper.h"
#endif /* CKHELPER_H */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

/* maybe this should really inherit completely from the module...  I dunno,
 * any uses of slots where independence is needed?
 */
NSS_IMPLEMENT NSSSlot *
NSSSlot_Create
(
  NSSArena *arenaOpt,
  CK_SLOT_ID slotID,
  NSSModule *parent
)
{
    NSSArena *arena;
    nssArenaMark *mark;
    NSSSlot *rvSlot;
    NSSToken *token;
    NSSUTF8 *slotName = NULL;
    PRUint32 length;
    PRBool newArena;
    PRStatus nssrv;
    CK_SLOT_INFO slotInfo;
    CK_RV ckrv;
    if (arenaOpt) {
	arena = arenaOpt;
#ifdef arena_mark_bug_fixed
	mark = nssArena_Mark(arena);
	if (!mark) {
	    return PR_FAILURE;
	}
#endif
	newArena = PR_FALSE;
    } else {
	arena = NSSArena_Create();
	if(!arena) {
	    return (NSSSlot *)NULL;
	}
	newArena = PR_TRUE;
    }
    rvSlot = nss_ZNEW(arena, NSSSlot);
    if (!rvSlot) {
	goto loser;
    }
    /* Get slot information */
    ckrv = CKAPI(parent)->C_GetSlotInfo(slotID, &slotInfo);
    if (ckrv != CKR_OK) {
	/* set an error here, eh? */
	goto loser;
    }
    /* Grab the slot description from the PKCS#11 fixed-length buffer */
    length = nssPKCS11StringLength(slotInfo.slotDescription, 
                                   sizeof(slotInfo.slotDescription));
    if (length > 0) {
	slotName = nssUTF8_Create(arena, nssStringType_UTF8String, 
	                          (void *)slotInfo.slotDescription, length);
	if (!slotName) {
	    goto loser;
	}
    }
    if (!arenaOpt) {
	/* Avoid confusion now - only set the slot's arena to a non-NULL value
	 * if a new arena is created.  Otherwise, depend on the caller (having
	 * passed arenaOpt) to free the arena.
	 */
	rvSlot->arena = arena;
    }
    rvSlot->refCount = 1;
    rvSlot->epv = parent->epv;
    rvSlot->module = parent;
    rvSlot->name = slotName;
    rvSlot->ckFlags = slotInfo.flags;
    /* Initialize the token if present. */
    if (slotInfo.flags & CKF_TOKEN_PRESENT) {
	token = NSSToken_Create(arena, slotID, rvSlot);
	if (!token) {
	    goto loser;
	}
    }
    rvSlot->token = token;
#ifdef arena_mark_bug_fixed
    nssrv = nssArena_Unmark(arena, mark);
    if (nssrv != PR_SUCCESS) {
	goto loser;
    }
#endif
    return rvSlot;
loser:
    if (newArena) {
	nssArena_Destroy(arena);
    } else {
#ifdef arena_mark_bug_fixed
	if (mark) {
	    nssArena_Release(arena, mark);
	}
#endif
    }
    /* everything was created in the arena, nothing to see here, move along */
    return (NSSSlot *)NULL;
}

NSS_IMPLEMENT PRStatus
NSSSlot_Destroy
(
  NSSSlot *slot
)
{
    if (--slot->refCount == 0) {
	/*NSSToken_Destroy(slot->token);*/
	return NSSArena_Destroy(slot->arena);
    }
    return PR_SUCCESS;
}

