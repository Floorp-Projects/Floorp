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
static const char CVS_ID[] = "@(#) $RCSfile: devutil.c,v $ $Revision: 1.4 $ $Date: 2002/04/15 16:20:39 $ $Name:  $";
#endif /* DEBUG */

#ifndef DEVM_H
#include "devm.h"
#endif /* DEVM_H */

#ifndef CKHELPER_H
#include "ckhelper.h"
#endif /* CKHELPER_H */

NSS_IMPLEMENT nssCryptokiObject *
nssCryptokiObject_Create
(
  NSSToken *t, 
  nssSession *session,
  CK_OBJECT_HANDLE h
)
{
    PRStatus status;
    NSSSlot *slot;
    nssCryptokiObject *object;
    CK_BBOOL *isTokenObject;
    CK_ATTRIBUTE cert_template[] = {
	{ CKA_TOKEN, NULL, 0 },
	{ CKA_LABEL, NULL, 0 }
    };
    slot = nssToken_GetSlot(t);
    status = nssCKObject_GetAttributes(h, cert_template, 2,
                                       NULL, session, slot);
    nssSlot_Destroy(slot);
    if (status != PR_SUCCESS) {
	/* a failure here indicates a device error */
	return (nssCryptokiObject *)NULL;
    }
    object = nss_ZNEW(NULL, nssCryptokiObject);
    if (!object) {
	return (nssCryptokiObject *)NULL;
    }
    object->handle = h;
    object->token = nssToken_AddRef(t);
    isTokenObject = (CK_BBOOL *)cert_template[0].pValue;
    object->isTokenObject = *isTokenObject;
    nss_ZFreeIf(isTokenObject);
    NSS_CK_ATTRIBUTE_TO_UTF8(&cert_template[1], object->label);
    return object;
}

NSS_IMPLEMENT void
nssCryptokiObject_Destroy
(
  nssCryptokiObject *object
)
{
    if (object) {
	nssToken_Destroy(object->token);
	nss_ZFreeIf(object->label);
	nss_ZFreeIf(object);
    }
}

NSS_IMPLEMENT nssCryptokiObject *
nssCryptokiObject_Clone
(
  nssCryptokiObject *object
)
{
    nssCryptokiObject *rvObject;
    rvObject = nss_ZNEW(NULL, nssCryptokiObject);
    if (rvObject) {
	rvObject->handle = object->handle;
	rvObject->token = nssToken_AddRef(object->token);
	rvObject->isTokenObject = object->isTokenObject;
	if (object->label) {
	    rvObject->label = nssUTF8_Duplicate(object->label, NULL);
	}
    }
    return rvObject;
}

NSS_EXTERN PRBool
nssCryptokiObject_Equal
(
  nssCryptokiObject *o1,
  nssCryptokiObject *o2
)
{
    return (o1->token == o2->token && o1->handle == o2->handle);
}

NSS_IMPLEMENT PRUint32
nssPKCS11String_Length(CK_CHAR *pkcs11Str, PRUint32 bufLen)
{
    PRInt32 i;
    for (i = bufLen - 1; i>=0; ) {
	if (pkcs11Str[i] != ' ' && pkcs11Str[i] != '\0') break;
	--i;
    }
    return (PRUint32)(i + 1);
}

/*
 * Slot arrays
 */

NSS_IMPLEMENT NSSSlot **
nssSlotArray_Clone
(
  NSSSlot **slots
)
{
    NSSSlot **rvSlots = NULL;
    NSSSlot **sp = slots;
    PRUint32 count = 0;
    while (sp && *sp) count++;
    if (count > 0) {
	rvSlots = nss_ZNEWARRAY(NULL, NSSSlot *, count + 1);
	if (rvSlots) {
	    sp = slots;
	    count = 0;
	    for (sp = slots; *sp; sp++) {
		rvSlots[count++] = nssSlot_AddRef(*sp);
	    }
	}
    }
    return rvSlots;
}

#ifdef PURE_STAN_BUILD
NSS_IMPLEMENT void
nssModuleArray_Destroy
(
  NSSModule **modules
)
{
    if (modules) {
	NSSModule **mp;
	for (mp = modules; *mp; mp++) {
	    nssModule_Destroy(*mp);
	}
	nss_ZFreeIf(modules);
    }
}
#endif

NSS_IMPLEMENT void
nssSlotArray_Destroy
(
  NSSSlot **slots
)
{
    if (slots) {
	NSSSlot **slotp;
	for (slotp = slots; *slotp; slotp++) {
	    nssSlot_Destroy(*slotp);
	}
	nss_ZFreeIf(slots);
    }
}

NSS_IMPLEMENT void
NSSSlotArray_Destroy
(
  NSSSlot **slots
)
{
    nssSlotArray_Destroy(slots);
}

NSS_IMPLEMENT void
nssTokenArray_Destroy
(
  NSSToken **tokens
)
{
    if (tokens) {
	NSSToken **tokenp;
	for (tokenp = tokens; *tokenp; tokenp++) {
	    nssToken_Destroy(*tokenp);
	}
	nss_ZFreeIf(tokens);
    }
}

NSS_IMPLEMENT void
NSSTokenArray_Destroy
(
  NSSToken **tokens
)
{
    nssTokenArray_Destroy(tokens);
}

NSS_IMPLEMENT void
nssCryptokiObjectArray_Destroy
(
  nssCryptokiObject **objects
)
{
    if (objects) {
	nssCryptokiObject **op;
	for (op = objects; *op; op++) {
	    nssCryptokiObject_Destroy(*op);
	}
	nss_ZFreeIf(objects);
    }
}

#ifdef PURE_STAN_BUILD
/*
 * Slot lists
 */

struct nssSlotListNodeStr
{
  PRCList link;
  NSSSlot *slot;
  PRUint32 order;
};

/* XXX separate slots with non-present tokens? */
struct nssSlotListStr 
{
  NSSArena *arena;
  PRBool i_allocated_arena;
  PZLock *lock;
  PRCList head;
  PRUint32 count;
};

NSS_IMPLEMENT nssSlotList *
nssSlotList_Create
(
  NSSArena *arenaOpt
)
{
    nssSlotList *rvList;
    NSSArena *arena;
    nssArenaMark *mark;
    if (arenaOpt) {
	arena = arenaOpt;
	mark = nssArena_Mark(arena);
	if (!mark) {
	    return (nssSlotList *)NULL;
	}
    } else {
	arena = nssArena_Create();
	if (!arena) {
	    return (nssSlotList *)NULL;
	}
    }
    rvList = nss_ZNEW(arena, nssSlotList);
    if (!rvList) {
	goto loser;
    }
    rvList->lock = PZ_NewLock(nssILockOther); /* XXX */
    if (!rvList->lock) {
	goto loser;
    }
    PR_INIT_CLIST(&rvList->head);
    rvList->arena = arena;
    rvList->i_allocated_arena = (arenaOpt == NULL);
    nssArena_Unmark(arena, mark);
    return rvList;
loser:
    if (arenaOpt) {
	nssArena_Release(arena, mark);
    } else {
	nssArena_Destroy(arena);
    }
    return (nssSlotList *)NULL;
}

NSS_IMPLEMENT void
nssSlotList_Destroy
(
  nssSlotList *slotList
)
{
    PRCList *link;
    struct nssSlotListNodeStr *node;
    if (slotList) {
	link = PR_NEXT_LINK(&slotList->head);
	while (link != &slotList->head) {
	    node = (struct nssSlotListNodeStr *)link;
	    nssSlot_Destroy(node->slot);
	    link = PR_NEXT_LINK(link);
	}
	if (slotList->i_allocated_arena) {
	    nssArena_Destroy(slotList->arena);
	}
    }
}

/* XXX should do allocs outside of lock */
NSS_IMPLEMENT PRStatus
nssSlotList_Add
(
  nssSlotList *slotList,
  NSSSlot *slot,
  PRUint32 order
)
{
    PRCList *link;
    struct nssSlotListNodeStr *node;
    PZ_Lock(slotList->lock);
    link = PR_NEXT_LINK(&slotList->head);
    while (link != &slotList->head) {
	node = (struct nssSlotListNodeStr *)link;
	if (order < node->order) {
	    break;
	}
	link = PR_NEXT_LINK(link);
    }
    node = nss_ZNEW(slotList->arena, struct nssSlotListNodeStr);
    if (!node) {
	return PR_FAILURE;
    }
    PR_INIT_CLIST(&node->link);
    node->slot = nssSlot_AddRef(slot);
    node->order = order;
    PR_INSERT_AFTER(&node->link, link);
    slotList->count++;
    PZ_Unlock(slotList->lock);
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRStatus
nssSlotList_AddModuleSlots
(
  nssSlotList *slotList,
  NSSModule *module,
  PRUint32 order
)
{
    nssArenaMark *mark = NULL;
    NSSSlot **sp, **slots = NULL;
    PRCList *link;
    struct nssSlotListNodeStr *node;
    PZ_Lock(slotList->lock);
    link = PR_NEXT_LINK(&slotList->head);
    while (link != &slotList->head) {
	node = (struct nssSlotListNodeStr *)link;
	if (order < node->order) {
	    break;
	}
	link = PR_NEXT_LINK(link);
    }
    slots = nssModule_GetSlots(module);
    if (!slots) {
	PZ_Unlock(slotList->lock);
	return PR_SUCCESS;
    }
    mark = nssArena_Mark(slotList->arena);
    if (!mark) {
	goto loser;
    }
    for (sp = slots; *sp; sp++) {
	node = nss_ZNEW(slotList->arena, struct nssSlotListNodeStr);
	if (!node) {
	    goto loser;
	}
	PR_INIT_CLIST(&node->link);
	node->slot = *sp; /* have ref from nssModule_GetSlots */
	node->order = order;
	PR_INSERT_AFTER(&node->link, link);
	slotList->count++;
    }
    PZ_Unlock(slotList->lock);
    nssArena_Unmark(slotList->arena, mark);
    return PR_SUCCESS;
loser:
    PZ_Unlock(slotList->lock);
    if (mark) {
	nssArena_Release(slotList->arena, mark);
    }
    if (slots) {
	nssSlotArray_Destroy(slots);
    }
    return PR_FAILURE;
}

NSS_IMPLEMENT NSSSlot **
nssSlotList_GetSlots
(
  nssSlotList *slotList
)
{
    PRUint32 i;
    PRCList *link;
    struct nssSlotListNodeStr *node;
    NSSSlot **rvSlots = NULL;
    PZ_Lock(slotList->lock);
    rvSlots = nss_ZNEWARRAY(NULL, NSSSlot *, slotList->count + 1);
    if (!rvSlots) {
	PZ_Unlock(slotList->lock);
	return (NSSSlot **)NULL;
    }
    i = 0;
    link = PR_NEXT_LINK(&slotList->head);
    while (link != &slotList->head) {
	node = (struct nssSlotListNodeStr *)link;
	rvSlots[i] = nssSlot_AddRef(node->slot);
	link = PR_NEXT_LINK(link);
	i++;
    }
    PZ_Unlock(slotList->lock);
    return rvSlots;
}

#if 0
NSS_IMPLEMENT NSSSlot *
nssSlotList_GetBestSlotForAlgorithmAndParameters
(
  nssSlotList *slotList,
  NSSAlgorithmAndParameters *ap
)
{
    PRCList *link;
    struct nssSlotListNodeStr *node;
    NSSSlot *rvSlot = NULL;
    PZ_Lock(slotList->lock);
    link = PR_NEXT_LINK(&slotList->head);
    while (link != &slotList->head) {
	node = (struct nssSlotListNodeStr *)link;
	if (nssSlot_DoesAlgorithmAndParameters(ap)) {
	    rvSlot = nssSlot_AddRef(node->slot); /* XXX check isPresent? */
	}
	link = PR_NEXT_LINK(link);
    }
    PZ_Unlock(slotList->lock);
    return rvSlot;
}
#endif

NSS_IMPLEMENT NSSSlot *
nssSlotList_GetBestSlot
(
  nssSlotList *slotList
)
{
    PRCList *link;
    struct nssSlotListNodeStr *node;
    NSSSlot *rvSlot = NULL;
    PZ_Lock(slotList->lock);
    if (PR_CLIST_IS_EMPTY(&slotList->head)) {
	PZ_Unlock(slotList->lock);
	return (NSSSlot *)NULL;
    }
    link = PR_NEXT_LINK(&slotList->head);
    node = (struct nssSlotListNodeStr *)link;
    rvSlot = nssSlot_AddRef(node->slot); /* XXX check isPresent? */
    PZ_Unlock(slotList->lock);
    return rvSlot;
}

NSS_IMPLEMENT NSSSlot *
nssSlotList_FindSlotByName
(
  nssSlotList *slotList,
  NSSUTF8 *slotName
)
{
    PRCList *link;
    struct nssSlotListNodeStr *node;
    NSSSlot *rvSlot = NULL;
    PZ_Lock(slotList->lock);
    link = PR_NEXT_LINK(&slotList->head);
    while (link != &slotList->head) {
	NSSUTF8 *sName;
	node = (struct nssSlotListNodeStr *)link;
	sName = nssSlot_GetName(node->slot);
	if (nssUTF8_Equal(sName, slotName, NULL)) {
	    rvSlot = nssSlot_AddRef(node->slot);
	    break;
	}
	link = PR_NEXT_LINK(link);
    }
    PZ_Unlock(slotList->lock);
    return rvSlot;
}

NSS_IMPLEMENT NSSToken *
nssSlotList_FindTokenByName
(
  nssSlotList *slotList,
  NSSUTF8 *tokenName
)
{
    PRCList *link;
    struct nssSlotListNodeStr *node;
    NSSToken *rvToken = NULL;
    PZ_Lock(slotList->lock);
    link = PR_NEXT_LINK(&slotList->head);
    while (link != &slotList->head) {
	NSSUTF8 *tName;
	node = (struct nssSlotListNodeStr *)link;
	tName = nssSlot_GetTokenName(node->slot);
	if (nssUTF8_Equal(tName, tokenName, NULL)) {
	    rvToken = nssSlot_GetToken(node->slot);
	    break;
	}
	link = PR_NEXT_LINK(link);
    }
    PZ_Unlock(slotList->lock);
    return rvToken;
}

#endif /* PURE_STAN_BUILD */

/* XXX of course this doesn't belong here */
NSS_IMPLEMENT NSSAlgorithmAndParameters *
NSSAlgorithmAndParameters_CreateSHA1Digest
(
  NSSArena *arenaOpt
)
{
    NSSAlgorithmAndParameters *rvAP = NULL;
    rvAP = nss_ZNEW(arenaOpt, NSSAlgorithmAndParameters);
    if (rvAP) {
	rvAP->mechanism.mechanism = CKM_SHA_1;
	rvAP->mechanism.pParameter = NULL;
	rvAP->mechanism.ulParameterLen = 0;
    }
    return rvAP;
}

NSS_IMPLEMENT NSSAlgorithmAndParameters *
NSSAlgorithmAndParameters_CreateMD5Digest
(
  NSSArena *arenaOpt
)
{
    NSSAlgorithmAndParameters *rvAP = NULL;
    rvAP = nss_ZNEW(arenaOpt, NSSAlgorithmAndParameters);
    if (rvAP) {
	rvAP->mechanism.mechanism = CKM_MD5;
	rvAP->mechanism.pParameter = NULL;
	rvAP->mechanism.ulParameterLen = 0;
    }
    return rvAP;
}

