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

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: devutil.c,v $ $Revision: 1.25 $ $Date: 2004/09/22 01:45:26 $ $Name:  $";
#endif /* DEBUG */

#ifndef DEVM_H
#include "devm.h"
#endif /* DEVM_H */

#ifndef CKHELPER_H
#include "ckhelper.h"
#endif /* CKHELPER_H */

NSS_IMPLEMENT nssCryptokiObject *
nssCryptokiObject_Create (
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
nssCryptokiObject_Destroy (
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
nssCryptokiObject_Clone (
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
nssCryptokiObject_Equal (
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
nssSlotArray_Clone (
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
nssModuleArray_Destroy (
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
nssSlotArray_Destroy (
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
NSSSlotArray_Destroy (
  NSSSlot **slots
)
{
    nssSlotArray_Destroy(slots);
}

NSS_IMPLEMENT void
nssTokenArray_Destroy (
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
NSSTokenArray_Destroy (
  NSSToken **tokens
)
{
    nssTokenArray_Destroy(tokens);
}

NSS_IMPLEMENT void
nssCryptokiObjectArray_Destroy (
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
nssSlotList_Create (
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
nssSlotList_Destroy (
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
nssSlotList_Add (
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
nssSlotList_AddModuleSlots (
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
nssSlotList_GetSlots (
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
nssSlotList_GetBestSlotForAlgorithmAndParameters (
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
nssSlotList_GetBestSlot (
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
nssSlotList_FindSlotByName (
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
nssSlotList_FindTokenByName (
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

/* object cache for token */

typedef struct
{
  NSSArena *arena;
  nssCryptokiObject *object;
  CK_ATTRIBUTE_PTR attributes;
  CK_ULONG numAttributes;
}
nssCryptokiObjectAndAttributes;

enum {
  cachedCerts = 0,
  cachedTrust = 1,
  cachedCRLs = 2
} cachedObjectType;

struct nssTokenObjectCacheStr
{
  NSSToken *token;
  PZLock *lock;
  PRBool loggedIn;
  PRBool doObjectType[3];
  PRBool searchedObjectType[3];
  nssCryptokiObjectAndAttributes **objects[3];
};

NSS_IMPLEMENT nssTokenObjectCache *
nssTokenObjectCache_Create (
  NSSToken *token,
  PRBool cacheCerts,
  PRBool cacheTrust,
  PRBool cacheCRLs
)
{
    nssTokenObjectCache *rvCache;
    rvCache = nss_ZNEW(NULL, nssTokenObjectCache);
    if (!rvCache) {
	goto loser;
    }
    rvCache->lock = PZ_NewLock(nssILockOther); /* XXX */
    if (!rvCache->lock) {
	goto loser;
    }
    rvCache->doObjectType[cachedCerts] = cacheCerts;
    rvCache->doObjectType[cachedTrust] = cacheTrust;
    rvCache->doObjectType[cachedCRLs] = cacheCRLs;
    rvCache->token = token; /* cache goes away with token */
    return rvCache;
loser:
    return (nssTokenObjectCache *)NULL;
}

static void
clear_cache (
  nssTokenObjectCache *cache
)
{
    nssCryptokiObjectAndAttributes **oa;
    PRUint32 objectType;
    for (objectType = cachedCerts; objectType <= cachedCRLs; objectType++) {
	cache->searchedObjectType[objectType] = PR_FALSE;
	if (!cache->objects[objectType]) {
	    continue;
	}
	for (oa = cache->objects[objectType]; *oa; oa++) {
	    /* prevent the token from being destroyed */
	    (*oa)->object->token = NULL;
	    nssCryptokiObject_Destroy((*oa)->object);
	    nssArena_Destroy((*oa)->arena);
	}
	nss_ZFreeIf(cache->objects[objectType]);
	cache->objects[objectType] = NULL;
    }
}

NSS_IMPLEMENT void
nssTokenObjectCache_Clear (
  nssTokenObjectCache *cache
)
{
    if (cache) {
	PZ_Lock(cache->lock);
	clear_cache(cache);
	PZ_Unlock(cache->lock);
    }
}

NSS_IMPLEMENT void
nssTokenObjectCache_Destroy (
  nssTokenObjectCache *cache
)
{
    if (cache) {
	clear_cache(cache);
	PZ_DestroyLock(cache->lock);
	nss_ZFreeIf(cache);
    }
}

NSS_IMPLEMENT PRBool
nssTokenObjectCache_HaveObjectClass (
  nssTokenObjectCache *cache,
  CK_OBJECT_CLASS objclass
)
{
    PRBool haveIt;
    PZ_Lock(cache->lock);
    switch (objclass) {
    case CKO_CERTIFICATE:    haveIt = cache->doObjectType[cachedCerts]; break;
    case CKO_NETSCAPE_TRUST: haveIt = cache->doObjectType[cachedTrust]; break;
    case CKO_NETSCAPE_CRL:   haveIt = cache->doObjectType[cachedCRLs];  break;
    default:                 haveIt = PR_FALSE;
    }
    PZ_Unlock(cache->lock);
    return haveIt;
}

static nssCryptokiObjectAndAttributes **
create_object_array (
  nssCryptokiObject **objects,
  PRBool *doObjects,
  PRUint32 *numObjects,
  PRStatus *status
)
{
    nssCryptokiObjectAndAttributes **rvOandA = NULL;
    *numObjects = 0;
    /* There are no objects for this type */
    if (!objects || !*objects) {
	*status = PR_SUCCESS;
	return rvOandA;
    }
    while (*objects++) (*numObjects)++;
    if (*numObjects >= MAX_LOCAL_CACHE_OBJECTS) {
	/* Hit the maximum allowed, so don't use a cache (there are
	 * too many objects to make caching worthwhile, presumably, if
	 * the token can handle that many objects, it can handle searching.
	 */
	*doObjects = PR_FALSE;
	*status = PR_FAILURE;
	*numObjects = 0;
    } else {
	rvOandA = nss_ZNEWARRAY(NULL, 
	                        nssCryptokiObjectAndAttributes *, 
	                        *numObjects + 1);
	*status = rvOandA ? PR_SUCCESS : PR_FAILURE;
    }
    return rvOandA;
}

static nssCryptokiObjectAndAttributes *
create_object (
  nssCryptokiObject *object,
  const CK_ATTRIBUTE_TYPE *types,
  PRUint32 numTypes,
  PRStatus *status
)
{
    PRUint32 j;
    NSSArena *arena;
    NSSSlot *slot = NULL;
    nssSession *session = NULL;
    nssCryptokiObjectAndAttributes *rvCachedObject = NULL;

    slot = nssToken_GetSlot(object->token);
    session = nssToken_GetDefaultSession(object->token);

    arena = nssArena_Create();
    if (!arena) {
	goto loser;
    }
    rvCachedObject = nss_ZNEW(arena, nssCryptokiObjectAndAttributes);
    if (!rvCachedObject) {
	goto loser;
    }
    rvCachedObject->arena = arena;
    /* The cache is tied to the token, and therefore the objects
     * in it should not hold references to the token.
     */
    nssToken_Destroy(object->token);
    rvCachedObject->object = object;
    rvCachedObject->attributes = nss_ZNEWARRAY(arena, CK_ATTRIBUTE, numTypes);
    if (!rvCachedObject->attributes) {
	goto loser;
    }
    for (j=0; j<numTypes; j++) {
	rvCachedObject->attributes[j].type = types[j];
    }
    *status = nssCKObject_GetAttributes(object->handle,
                                        rvCachedObject->attributes,
                                        numTypes,
                                        arena,
                                        session,
                                        slot);
    if (*status != PR_SUCCESS) {
	goto loser;
    }
    rvCachedObject->numAttributes = numTypes;
    *status = PR_SUCCESS;
    if (slot) {
	nssSlot_Destroy(slot);
    }
    return rvCachedObject;
loser:
    *status = PR_FAILURE;
    if (slot) {
	nssSlot_Destroy(slot);
    }
    if (arena)
	nssArena_Destroy(arena);
    return (nssCryptokiObjectAndAttributes *)NULL;
}

/*
 *
 * State diagram for cache:
 *
 *            token !present            token removed
 *        +-------------------------+<----------------------+
 *        |                         ^                       |
 *        v                         |                       |
 *  +----------+   slot friendly    |  token present   +----------+ 
 *  |   cache  | -----------------> % ---------------> |   cache  |
 *  | unloaded |                                       |  loaded  |
 *  +----------+                                       +----------+
 *    ^   |                                                 ^   |
 *    |   |   slot !friendly           slot logged in       |   |
 *    |   +-----------------------> % ----------------------+   |
 *    |                             |                           |
 *    | slot logged out             v  slot !friendly           |
 *    +-----------------------------+<--------------------------+
 *
 */

/* This function must not be called with cache->lock locked. */
static PRBool
token_is_present (
  nssTokenObjectCache *cache
)
{
    NSSSlot *slot = nssToken_GetSlot(cache->token);
    PRBool tokenPresent = nssSlot_IsTokenPresent(slot);
    nssSlot_Destroy(slot);
    return tokenPresent;
}

static PRBool
search_for_objects (
  nssTokenObjectCache *cache
)
{
    PRBool doSearch = PR_FALSE;
    NSSSlot *slot = nssToken_GetSlot(cache->token);
    /* Handle non-friendly slots (slots which require login for objects) */
    if (!nssSlot_IsFriendly(slot)) {
	if (nssSlot_IsLoggedIn(slot)) {
	    /* Either no state change, or went from !logged in -> logged in */
	    cache->loggedIn = PR_TRUE;
	    doSearch = PR_TRUE;
	} else {
	    if (cache->loggedIn) {
		/* went from logged in -> !logged in, destroy cached objects */
		clear_cache(cache);
		cache->loggedIn = PR_FALSE;
	    } /* else no state change, still not logged in, so exit */
	}
    } else {
	/* slot is friendly, thus always available for search */
	doSearch = PR_TRUE;
    }
    nssSlot_Destroy(slot);
    return doSearch;
}

static nssCryptokiObjectAndAttributes *
create_cert (
  nssCryptokiObject *object,
  PRStatus *status
)
{
    static const CK_ATTRIBUTE_TYPE certAttr[] = {
	CKA_CLASS,
	CKA_TOKEN,
	CKA_LABEL,
	CKA_CERTIFICATE_TYPE,
	CKA_ID,
	CKA_VALUE,
	CKA_ISSUER,
	CKA_SERIAL_NUMBER,
	CKA_SUBJECT,
	CKA_NETSCAPE_EMAIL
    };
    static const PRUint32 numCertAttr = sizeof(certAttr) / sizeof(certAttr[0]);
    return create_object(object, certAttr, numCertAttr, status);
}

static PRStatus
get_token_certs_for_cache (
  nssTokenObjectCache *cache
)
{
    PRStatus status;
    nssCryptokiObject **objects;
    PRBool *doIt = &cache->doObjectType[cachedCerts];
    PRUint32 i, numObjects;

    if (!search_for_objects(cache) || 
         cache->searchedObjectType[cachedCerts] || 
        !cache->doObjectType[cachedCerts]) 
    {
	/* Either there was a state change that prevents a search
	 * (token logged out), or the search was already done,
	 * or certs are not being cached.
	 */
	return PR_SUCCESS;
    }
    objects = nssToken_FindCertificates(cache->token, NULL,
                                        nssTokenSearchType_TokenForced,
				        MAX_LOCAL_CACHE_OBJECTS, &status);
    if (status != PR_SUCCESS) {
	return status;
    }
    cache->objects[cachedCerts] = create_object_array(objects,
                                                      doIt,
                                                      &numObjects,
                                                      &status);
    if (status != PR_SUCCESS) {
	return status;
    }
    for (i=0; i<numObjects; i++) {
	cache->objects[cachedCerts][i] = create_cert(objects[i], &status);
	if (status != PR_SUCCESS) {
	    break;
	}
    }
    if (status == PR_SUCCESS) {
	nss_ZFreeIf(objects);
    } else {
	PRUint32 j;
	for (j=0; j<i; j++) {
	    /* sigh */
	    nssToken_AddRef(cache->objects[cachedCerts][j]->object->token);
	    nssArena_Destroy(cache->objects[cachedCerts][j]->arena);
	}
	nssCryptokiObjectArray_Destroy(objects);
    }
    cache->searchedObjectType[cachedCerts] = PR_TRUE;
    return status;
}

static nssCryptokiObjectAndAttributes *
create_trust (
  nssCryptokiObject *object,
  PRStatus *status
)
{
    static const CK_ATTRIBUTE_TYPE trustAttr[] = {
	CKA_CLASS,
	CKA_TOKEN,
	CKA_LABEL,
	CKA_CERT_SHA1_HASH,
	CKA_CERT_MD5_HASH,
	CKA_ISSUER,
	CKA_SUBJECT,
	CKA_TRUST_SERVER_AUTH,
	CKA_TRUST_CLIENT_AUTH,
	CKA_TRUST_EMAIL_PROTECTION,
	CKA_TRUST_CODE_SIGNING
    };
    static const PRUint32 numTrustAttr = sizeof(trustAttr) / sizeof(trustAttr[0]);
    return create_object(object, trustAttr, numTrustAttr, status);
}

static PRStatus
get_token_trust_for_cache (
  nssTokenObjectCache *cache
)
{
    PRStatus status;
    nssCryptokiObject **objects;
    PRBool *doIt = &cache->doObjectType[cachedTrust];
    PRUint32 i, numObjects;

    if (!search_for_objects(cache) || 
         cache->searchedObjectType[cachedTrust] || 
        !cache->doObjectType[cachedTrust]) 
    {
	/* Either there was a state change that prevents a search
	 * (token logged out), or the search was already done,
	 * or trust is not being cached.
	 */
	return PR_SUCCESS;
    }
    objects = nssToken_FindTrustObjects(cache->token, NULL,
                                        nssTokenSearchType_TokenForced,
				        MAX_LOCAL_CACHE_OBJECTS, &status);
    if (status != PR_SUCCESS) {
	return status;
    }
    cache->objects[cachedTrust] = create_object_array(objects,
                                                      doIt,
                                                      &numObjects,
                                                      &status);
    if (status != PR_SUCCESS) {
	return status;
    }
    for (i=0; i<numObjects; i++) {
	cache->objects[cachedTrust][i] = create_trust(objects[i], &status);
	if (status != PR_SUCCESS) {
	    break;
	}
    }
    if (status == PR_SUCCESS) {
	nss_ZFreeIf(objects);
    } else {
	PRUint32 j;
	for (j=0; j<i; j++) {
	    /* sigh */
	    nssToken_AddRef(cache->objects[cachedTrust][j]->object->token);
	    nssArena_Destroy(cache->objects[cachedTrust][j]->arena);
	}
	nssCryptokiObjectArray_Destroy(objects);
    }
    cache->searchedObjectType[cachedTrust] = PR_TRUE;
    return status;
}

static nssCryptokiObjectAndAttributes *
create_crl (
  nssCryptokiObject *object,
  PRStatus *status
)
{
    static const CK_ATTRIBUTE_TYPE crlAttr[] = {
	CKA_CLASS,
	CKA_TOKEN,
	CKA_LABEL,
	CKA_VALUE,
	CKA_SUBJECT,
	CKA_NETSCAPE_KRL,
	CKA_NETSCAPE_URL
    };
    static const PRUint32 numCRLAttr = sizeof(crlAttr) / sizeof(crlAttr[0]);
    return create_object(object, crlAttr, numCRLAttr, status);
}

static PRStatus
get_token_crls_for_cache (
  nssTokenObjectCache *cache
)
{
    PRStatus status;
    nssCryptokiObject **objects;
    PRBool *doIt = &cache->doObjectType[cachedCRLs];
    PRUint32 i, numObjects;

    if (!search_for_objects(cache) || 
         cache->searchedObjectType[cachedCRLs] || 
        !cache->doObjectType[cachedCRLs]) 
    {
	/* Either there was a state change that prevents a search
	 * (token logged out), or the search was already done,
	 * or CRLs are not being cached.
	 */
	return PR_SUCCESS;
    }
    objects = nssToken_FindCRLs(cache->token, NULL,
                                nssTokenSearchType_TokenForced,
				MAX_LOCAL_CACHE_OBJECTS, &status);
    if (status != PR_SUCCESS) {
	return status;
    }
    cache->objects[cachedCRLs] = create_object_array(objects,
                                                     doIt,
                                                     &numObjects,
                                                     &status);
    if (status != PR_SUCCESS) {
	return status;
    }
    for (i=0; i<numObjects; i++) {
	cache->objects[cachedCRLs][i] = create_crl(objects[i], &status);
	if (status != PR_SUCCESS) {
	    break;
	}
    }
    if (status == PR_SUCCESS) {
	nss_ZFreeIf(objects);
    } else {
	PRUint32 j;
	for (j=0; j<i; j++) {
	    /* sigh */
	    nssToken_AddRef(cache->objects[cachedCRLs][j]->object->token);
	    nssArena_Destroy(cache->objects[cachedCRLs][j]->arena);
	}
	nssCryptokiObjectArray_Destroy(objects);
    }
    cache->searchedObjectType[cachedCRLs] = PR_TRUE;
    return status;
}

static CK_ATTRIBUTE_PTR
find_attribute_in_object (
  nssCryptokiObjectAndAttributes *obj,
  CK_ATTRIBUTE_TYPE attrType
)
{
    PRUint32 j;
    for (j=0; j<obj->numAttributes; j++) {
	if (attrType == obj->attributes[j].type) {
	    return &obj->attributes[j];
	}
    }
    return (CK_ATTRIBUTE_PTR)NULL;
}

/* Find all objects in the array that match the supplied template */
static nssCryptokiObject **
find_objects_in_array (
  nssCryptokiObjectAndAttributes **objArray,
  CK_ATTRIBUTE_PTR ot,
  CK_ULONG otlen,
  PRUint32 maximumOpt
)
{
    PRIntn oi = 0;
    PRUint32 i;
    NSSArena *arena;
    PRUint32 size = 8;
    PRUint32 numMatches = 0;
    nssCryptokiObject **objects = NULL;
    nssCryptokiObjectAndAttributes **matches = NULL;
    CK_ATTRIBUTE_PTR attr;

    if (!objArray) {
	return (nssCryptokiObject **)NULL;
    }
    arena = nssArena_Create();
    if (!arena) {
	return (nssCryptokiObject **)NULL;
    }
    matches = nss_ZNEWARRAY(arena, nssCryptokiObjectAndAttributes *, size);
    if (!matches) {
	goto loser;
    }
    if (maximumOpt == 0) maximumOpt = ~0;
    /* loop over the cached objects */
    for (; *objArray && numMatches < maximumOpt; objArray++) {
	nssCryptokiObjectAndAttributes *obj = *objArray;
	/* loop over the test template */
	for (i=0; i<otlen; i++) {
	    /* see if the object has the attribute */
	    attr = find_attribute_in_object(obj, ot[i].type);
	    if (!attr) {
		/* nope, match failed */
		break;
	    }
	    /* compare the attribute against the test value */
	    if (ot[i].ulValueLen != attr->ulValueLen ||
	        !nsslibc_memequal(ot[i].pValue, 
	                          attr->pValue,
	                          attr->ulValueLen, NULL))
	    {
		/* nope, match failed */
		break;
	    }
	}
	if (i == otlen) {
	    /* all of the attributes in the test template were found
	     * in the object's template, and they all matched
	     */
	    matches[numMatches++] = obj;
	    if (numMatches == size) {
		size *= 2;
		matches = nss_ZREALLOCARRAY(matches, 
		                            nssCryptokiObjectAndAttributes *, 
		                            size);
		if (!matches) {
		    goto loser;
		}
	    }
	}
    }
    if (numMatches > 0) {
	objects = nss_ZNEWARRAY(NULL, nssCryptokiObject *, numMatches + 1);
	if (!objects) {
	    goto loser;
	}
	for (oi=0; oi<(PRIntn)numMatches; oi++) {
	    objects[oi] = nssCryptokiObject_Clone(matches[oi]->object);
	    if (!objects[oi]) {
		goto loser;
	    }
	}
    }
    nssArena_Destroy(arena);
    return objects;
loser:
    if (objects) {
	for (--oi; oi>=0; --oi) {
	    nssCryptokiObject_Destroy(objects[oi]);
	}
    }
    nssArena_Destroy(arena);
    return (nssCryptokiObject **)NULL;
}

NSS_IMPLEMENT nssCryptokiObject **
nssTokenObjectCache_FindObjectsByTemplate (
  nssTokenObjectCache *cache,
  CK_OBJECT_CLASS objclass,
  CK_ATTRIBUTE_PTR otemplate,
  CK_ULONG otlen,
  PRUint32 maximumOpt,
  PRStatus *statusOpt
)
{
    PRStatus status = PR_FAILURE;
    nssCryptokiObject **rvObjects = NULL;
    if (!token_is_present(cache)) {
	status = PR_SUCCESS;
	goto finish;
    }
    PZ_Lock(cache->lock);
    switch (objclass) {
    case CKO_CERTIFICATE:
	if (cache->doObjectType[cachedCerts]) {
	    status = get_token_certs_for_cache(cache);
	    if (status != PR_SUCCESS) {
		goto unlock;
	    }
	    rvObjects = find_objects_in_array(cache->objects[cachedCerts], 
	                                      otemplate, otlen, maximumOpt);
	}
	break;
    case CKO_NETSCAPE_TRUST:
	if (cache->doObjectType[cachedTrust]) {
	    status = get_token_trust_for_cache(cache);
	    if (status != PR_SUCCESS) {
		goto unlock;
	    }
	    rvObjects = find_objects_in_array(cache->objects[cachedTrust], 
	                                      otemplate, otlen, maximumOpt);
	}
	break;
    case CKO_NETSCAPE_CRL:
	if (cache->doObjectType[cachedCRLs]) {
	    status = get_token_crls_for_cache(cache);
	    if (status != PR_SUCCESS) {
		goto unlock;
	    }
	    rvObjects = find_objects_in_array(cache->objects[cachedCRLs], 
	                                      otemplate, otlen, maximumOpt);
	}
	break;
    default: break;
    }
unlock:
    PZ_Unlock(cache->lock);
finish:
    if (statusOpt) {
	*statusOpt = status;
    }
    return rvObjects;
}

static PRBool
cache_available_for_object_type (
  nssTokenObjectCache *cache,
  PRUint32 objectType
)
{
    if (!cache->doObjectType[objectType]) {
	/* not caching this object kind */
	return PR_FALSE;
    }
    if (!cache->searchedObjectType[objectType]) {
	/* objects are not cached yet */
	return PR_FALSE;
    }
    if (!search_for_objects(cache)) {
	/* not logged in */
	return PR_FALSE;
    }
    return PR_TRUE;
}

NSS_IMPLEMENT PRStatus
nssTokenObjectCache_GetObjectAttributes (
  nssTokenObjectCache *cache,
  NSSArena *arenaOpt,
  nssCryptokiObject *object,
  CK_OBJECT_CLASS objclass,
  CK_ATTRIBUTE_PTR atemplate,
  CK_ULONG atlen
)
{
    PRUint32 i, j;
    NSSArena *arena = NULL;
    nssArenaMark *mark = NULL;
    nssCryptokiObjectAndAttributes *cachedOA = NULL;
    nssCryptokiObjectAndAttributes **oa = NULL;
    PRUint32 objectType;
    if (!token_is_present(cache)) {
	return PR_FAILURE;
    }
    PZ_Lock(cache->lock);
    switch (objclass) {
    case CKO_CERTIFICATE:    objectType = cachedCerts; break;
    case CKO_NETSCAPE_TRUST: objectType = cachedTrust; break;
    case CKO_NETSCAPE_CRL:   objectType = cachedCRLs;  break;
    default: goto loser;
    }
    if (!cache_available_for_object_type(cache, objectType)) {
	goto loser;
    }
    oa = cache->objects[objectType];
    if (!oa) {
	goto loser;
    }
    for (; *oa; oa++) {
	if (nssCryptokiObject_Equal((*oa)->object, object)) {
	    cachedOA = *oa;
	    break;
	}
    }
    if (!cachedOA) {
	goto loser; /* don't have this object */
    }
    if (arenaOpt) {
	arena = arenaOpt;
	mark = nssArena_Mark(arena);
    }
    for (i=0; i<atlen; i++) {
	for (j=0; j<cachedOA->numAttributes; j++) {
	    if (atemplate[i].type == cachedOA->attributes[j].type) {
		CK_ATTRIBUTE_PTR attr = &cachedOA->attributes[j];
		if (cachedOA->attributes[j].ulValueLen == 0 ||
		    cachedOA->attributes[j].ulValueLen == (CK_ULONG)-1) 
		{
		    break; /* invalid attribute */
		}
		if (atemplate[i].ulValueLen > 0) {
		    if (atemplate[i].pValue == NULL ||
		        atemplate[i].ulValueLen < attr->ulValueLen) 
		    {
			goto loser;
		    }
		} else {
		    atemplate[i].pValue = nss_ZAlloc(arena, attr->ulValueLen);
		    if (!atemplate[i].pValue) {
			goto loser;
		    }
		}
		nsslibc_memcpy(atemplate[i].pValue,
		               attr->pValue, attr->ulValueLen);
		atemplate[i].ulValueLen = attr->ulValueLen;
		break;
	    }
	}
	if (j == cachedOA->numAttributes) {
	    atemplate[i].ulValueLen = (CK_ULONG)-1;
	}
    }
    PZ_Unlock(cache->lock);
    if (mark) {
	nssArena_Unmark(arena, mark);
    }
    return PR_SUCCESS;
loser:
    PZ_Unlock(cache->lock);
    if (mark) {
	nssArena_Release(arena, mark);
    }
    return PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
nssTokenObjectCache_ImportObject (
  nssTokenObjectCache *cache,
  nssCryptokiObject *object,
  CK_OBJECT_CLASS objclass,
  CK_ATTRIBUTE_PTR ot,
  CK_ULONG otlen
)
{
    PRStatus status = PR_SUCCESS;
    PRUint32 count;
    nssCryptokiObjectAndAttributes **oa, ***otype;
    PRUint32 objectType;
    PRBool haveIt = PR_FALSE;

    if (!token_is_present(cache)) {
	return PR_SUCCESS; /* cache not active, ignored */
    }
    PZ_Lock(cache->lock);
    switch (objclass) {
    case CKO_CERTIFICATE:    objectType = cachedCerts; break;
    case CKO_NETSCAPE_TRUST: objectType = cachedTrust; break;
    case CKO_NETSCAPE_CRL:   objectType = cachedCRLs;  break;
    default:
	PZ_Unlock(cache->lock);
	return PR_SUCCESS; /* don't need to import it here */
    }
    if (!cache_available_for_object_type(cache, objectType)) {
	PZ_Unlock(cache->lock);
	return PR_SUCCESS; /* cache not active, ignored */
    }
    count = 0;
    otype = &cache->objects[objectType]; /* index into array of types */
    oa = *otype; /* the array of objects for this type */
    while (oa && *oa) {
	if (nssCryptokiObject_Equal((*oa)->object, object)) {
	    haveIt = PR_TRUE;
	    break;
	}
	count++;
	oa++;
    }
    if (haveIt) {
	/* Destroy the old entry */
	(*oa)->object->token = NULL;
	nssCryptokiObject_Destroy((*oa)->object);
	nssArena_Destroy((*oa)->arena);
    } else {
	/* Create space for a new entry */
	if (count > 0) {
	    *otype = nss_ZREALLOCARRAY(*otype,
	                               nssCryptokiObjectAndAttributes *, 
	                               count + 2);
	} else {
	    *otype = nss_ZNEWARRAY(NULL, nssCryptokiObjectAndAttributes *, 2);
	}
    }
    if (*otype) {
	nssCryptokiObject *copyObject = nssCryptokiObject_Clone(object);
	if (objectType == cachedCerts) {
	    (*otype)[count] = create_cert(copyObject, &status);
	} else if (objectType == cachedTrust) {
	    (*otype)[count] = create_trust(copyObject, &status);
	} else if (objectType == cachedCRLs) {
	    (*otype)[count] = create_crl(copyObject, &status);
	}
    } else {
	status = PR_FAILURE;
    }
    PZ_Unlock(cache->lock);
    return status;
}

NSS_IMPLEMENT void
nssTokenObjectCache_RemoveObject (
  nssTokenObjectCache *cache,
  nssCryptokiObject *object
)
{
    PRUint32 oType;
    nssCryptokiObjectAndAttributes **oa, **swp = NULL;
    if (!token_is_present(cache)) {
	return;
    }
    PZ_Lock(cache->lock);
    for (oType=0; oType<3; oType++) {
	if (!cache_available_for_object_type(cache, oType) ||
	    !cache->objects[oType])
	{
	    continue;
	}
	for (oa = cache->objects[oType]; *oa; oa++) {
	    if (nssCryptokiObject_Equal((*oa)->object, object)) {
		swp = oa; /* the entry to remove */
		while (oa[1]) oa++; /* go to the tail */
		(*swp)->object->token = NULL;
		nssCryptokiObject_Destroy((*swp)->object);
		nssArena_Destroy((*swp)->arena); /* destroy it */
		*swp = *oa; /* swap the last with the removed */
		*oa = NULL; /* null-terminate the array */
		break;
	    }
	}
	if (swp) {
	    break;
	}
    }
    if ((oType <3) &&
		cache->objects[oType] && cache->objects[oType][0] == NULL) {
	nss_ZFreeIf(cache->objects[oType]); /* no entries remaining */
	cache->objects[oType] = NULL;
    }
    PZ_Unlock(cache->lock);
}

/* These two hash algorithms are presently sufficient.
** They are used for fingerprints of certs which are stored as the 
** CKA_CERT_SHA1_HASH and CKA_CERT_MD5_HASH attributes.
** We don't need to add SHAxxx to these now.
*/
/* XXX of course this doesn't belong here */
NSS_IMPLEMENT NSSAlgorithmAndParameters *
NSSAlgorithmAndParameters_CreateSHA1Digest (
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
NSSAlgorithmAndParameters_CreateMD5Digest (
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

