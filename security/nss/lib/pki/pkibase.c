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
static const char CVS_ID[] = "@(#) $RCSfile: pkibase.c,v $ $Revision: 1.21 $ $Date: 2003/08/01 02:02:47 $ $Name:  $";
#endif /* DEBUG */

#ifndef DEV_H
#include "dev.h"
#endif /* DEV_H */

#ifndef PKIM_H
#include "pkim.h"
#endif /* PKIM_H */

#ifdef NSS_3_4_CODE
#include "pki3hack.h"
#endif

extern const NSSError NSS_ERROR_NOT_FOUND;

NSS_IMPLEMENT nssPKIObject *
nssPKIObject_Create (
  NSSArena *arenaOpt,
  nssCryptokiObject *instanceOpt,
  NSSTrustDomain *td,
  NSSCryptoContext *cc
)
{
    NSSArena *arena;
    nssArenaMark *mark = NULL;
    nssPKIObject *object;
    if (arenaOpt) {
	arena = arenaOpt;
	mark = nssArena_Mark(arena);
    } else {
	arena = nssArena_Create();
	if (!arena) {
	    return (nssPKIObject *)NULL;
	}
    }
    object = nss_ZNEW(arena, nssPKIObject);
    if (!object) {
	goto loser;
    }
    object->arena = arena;
    object->trustDomain = td; /* XXX */
    object->cryptoContext = cc;
    object->lock = PZ_NewLock(nssILockOther);
    if (!object->lock) {
	goto loser;
    }
    if (instanceOpt) {
	if (nssPKIObject_AddInstance(object, instanceOpt) != PR_SUCCESS) {
	    goto loser;
	}
    }
    PR_AtomicIncrement(&object->refCount);
    if (mark) {
	nssArena_Unmark(arena, mark);
    }
    return object;
loser:
    if (mark) {
	nssArena_Release(arena, mark);
    } else {
	nssArena_Destroy(arena);
    }
    return (nssPKIObject *)NULL;
}

NSS_IMPLEMENT PRBool
nssPKIObject_Destroy (
  nssPKIObject *object
)
{
    PRUint32 i;
    PR_ASSERT(object->refCount > 0);
    if (PR_AtomicDecrement(&object->refCount) == 0) {
	for (i=0; i<object->numInstances; i++) {
	    nssCryptokiObject_Destroy(object->instances[i]);
	}
	PZ_DestroyLock(object->lock);
	nssArena_Destroy(object->arena);
	return PR_TRUE;
    }
    return PR_FALSE;
}

NSS_IMPLEMENT nssPKIObject *
nssPKIObject_AddRef (
  nssPKIObject *object
)
{
    PR_AtomicIncrement(&object->refCount);
    return object;
}

NSS_IMPLEMENT PRStatus
nssPKIObject_AddInstance (
  nssPKIObject *object,
  nssCryptokiObject *instance
)
{
    PZ_Lock(object->lock);
    if (object->numInstances == 0) {
	object->instances = nss_ZNEWARRAY(object->arena,
	                                  nssCryptokiObject *,
	                                  object->numInstances + 1);
    } else {
	PRUint32 i;
	for (i=0; i<object->numInstances; i++) {
	    if (nssCryptokiObject_Equal(object->instances[i], instance)) {
		PZ_Unlock(object->lock);
		if (instance->label) {
		    if (!object->instances[i]->label ||
		        !nssUTF8_Equal(instance->label,
		                       object->instances[i]->label, NULL))
		    {
			/* Either the old instance did not have a label,
			 * or the label has changed.
			 */
			nss_ZFreeIf(object->instances[i]->label);
			object->instances[i]->label = instance->label;
			instance->label = NULL;
		    }
		} else if (object->instances[i]->label) {
		    /* The old label was removed */
		    nss_ZFreeIf(object->instances[i]->label);
		    object->instances[i]->label = NULL;
		}
		nssCryptokiObject_Destroy(instance);
		return PR_SUCCESS;
	    }
	}
	object->instances = nss_ZREALLOCARRAY(object->instances,
	                                      nssCryptokiObject *,
	                                      object->numInstances + 1);
    }
    if (!object->instances) {
	PZ_Unlock(object->lock);
	return PR_FAILURE;
    }
    object->instances[object->numInstances++] = instance;
    PZ_Unlock(object->lock);
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRBool
nssPKIObject_HasInstance (
  nssPKIObject *object,
  nssCryptokiObject *instance
)
{
    PRUint32 i;
    PRBool hasIt = PR_FALSE;;
    PZ_Lock(object->lock);
    for (i=0; i<object->numInstances; i++) {
	if (nssCryptokiObject_Equal(object->instances[i], instance)) {
	    hasIt = PR_TRUE;
	    break;
	}
    }
    PZ_Unlock(object->lock);
    return hasIt;
}

NSS_IMPLEMENT PRStatus
nssPKIObject_RemoveInstanceForToken (
  nssPKIObject *object,
  NSSToken *token
)
{
    PRUint32 i;
    nssCryptokiObject *instanceToRemove = NULL;
    PZ_Lock(object->lock);
    if (object->numInstances == 0) {
	PZ_Unlock(object->lock);
	return PR_SUCCESS;
    }
    for (i=0; i<object->numInstances; i++) {
	if (object->instances[i]->token == token) {
	    instanceToRemove = object->instances[i];
	    object->instances[i] = object->instances[object->numInstances-1];
	    object->instances[object->numInstances-1] = NULL;
	    break;
	}
    }
    if (--object->numInstances > 0) {
	object->instances = nss_ZREALLOCARRAY(object->instances,
	                                      nssCryptokiObject *,
	                                      object->numInstances);
	if (!object->instances) {
	    PZ_Unlock(object->lock);
	    return PR_FAILURE;
	}
    } else {
	nss_ZFreeIf(object->instances);
    }
    nssCryptokiObject_Destroy(instanceToRemove);
    PZ_Unlock(object->lock);
    return PR_SUCCESS;
}

/* this needs more thought on what will happen when there are multiple
 * instances
 */
NSS_IMPLEMENT PRStatus
nssPKIObject_DeleteStoredObject (
  nssPKIObject *object,
  NSSCallback *uhh,
  PRBool isFriendly
)
{
    PRUint32 i, numNotDestroyed;
    PRStatus status = PR_SUCCESS;
    NSSTrustDomain *td = object->trustDomain;
    NSSCallback *pwcb = uhh ?  /* is this optional? */
                        uhh : 
                        nssTrustDomain_GetDefaultCallback(td, NULL);
    numNotDestroyed = 0;
    PZ_Lock(object->lock);
    for (i=0; i<object->numInstances; i++) {
	nssCryptokiObject *instance = object->instances[i];
#ifndef NSS_3_4_CODE
	NSSSlot *slot = nssToken_GetSlot(instance->token);
	/* If both the operation and the slot are friendly, login is
	 * not required.  If either or both are not friendly, it is
	 * required.
	 */
	if (!(isFriendly && nssSlot_IsFriendly(slot))) {
	    status = nssSlot_Login(slot, pwcb);
	    nssSlot_Destroy(slot);
	    if (status == PR_SUCCESS) {
		/* XXX this should be fixed to understand read-only tokens,
		 * for now, to handle the builtins, just make the attempt.
		 */
		status = nssToken_DeleteStoredObject(instance);
	    }
	}
#else
	status = nssToken_DeleteStoredObject(instance);
#endif
	object->instances[i] = NULL;
	if (status == PR_SUCCESS) {
	    nssCryptokiObject_Destroy(instance);
	} else {
	    object->instances[numNotDestroyed++] = instance;
	}
    }
    if (numNotDestroyed == 0) {
	nss_ZFreeIf(object->instances);
	object->numInstances = 0;
    } else {
	object->numInstances = numNotDestroyed;
    }
    PZ_Unlock(object->lock);
    return status;
}

NSS_IMPLEMENT NSSToken **
nssPKIObject_GetTokens (
  nssPKIObject *object,
  PRStatus *statusOpt
)
{
    NSSToken **tokens = NULL;
    PZ_Lock(object->lock);
    if (object->numInstances > 0) {
	tokens = nss_ZNEWARRAY(NULL, NSSToken *, object->numInstances + 1);
	if (tokens) {
	    PRUint32 i;
	    for (i=0; i<object->numInstances; i++) {
		tokens[i] = nssToken_AddRef(object->instances[i]->token);
	    }
	}
    }
    PZ_Unlock(object->lock);
    if (statusOpt) *statusOpt = PR_SUCCESS; /* until more logic here */
    return tokens;
}

NSS_IMPLEMENT NSSUTF8 *
nssPKIObject_GetNicknameForToken (
  nssPKIObject *object,
  NSSToken *tokenOpt
)
{
    PRUint32 i;
    NSSUTF8 *nickname = NULL;
    PZ_Lock(object->lock);
    for (i=0; i<object->numInstances; i++) {
	if ((!tokenOpt && object->instances[i]->label) ||
	    (object->instances[i]->token == tokenOpt)) 
	{
            /* XXX should be copy? safe as long as caller has reference */
	    nickname = object->instances[i]->label; 
	    break;
	}
    }
    PZ_Unlock(object->lock);
    return nickname;
}

#ifdef NSS_3_4_CODE
NSS_IMPLEMENT nssCryptokiObject **
nssPKIObject_GetInstances (
  nssPKIObject *object
)
{
    nssCryptokiObject **instances = NULL;
    PRUint32 i;
    if (object->numInstances == 0) {
	return (nssCryptokiObject **)NULL;
    }
    PZ_Lock(object->lock);
    instances = nss_ZNEWARRAY(NULL, nssCryptokiObject *, 
                              object->numInstances + 1);
    if (instances) {
	for (i=0; i<object->numInstances; i++) {
	    instances[i] = nssCryptokiObject_Clone(object->instances[i]);
	}
    }
    PZ_Unlock(object->lock);
    return instances;
}
#endif

NSS_IMPLEMENT void
nssCertificateArray_Destroy (
  NSSCertificate **certs
)
{
    if (certs) {
	NSSCertificate **certp;
	for (certp = certs; *certp; certp++) {
#ifdef NSS_3_4_CODE
	    if ((*certp)->decoding) {
		CERTCertificate *cc = STAN_GetCERTCertificate(*certp);
		if (cc) {
		    CERT_DestroyCertificate(cc);
		}
		continue;
	    }
#endif
	    nssCertificate_Destroy(*certp);
	}
	nss_ZFreeIf(certs);
    }
}

NSS_IMPLEMENT void
NSSCertificateArray_Destroy (
  NSSCertificate **certs
)
{
    nssCertificateArray_Destroy(certs);
}

NSS_IMPLEMENT NSSCertificate **
nssCertificateArray_Join (
  NSSCertificate **certs1,
  NSSCertificate **certs2
)
{
    if (certs1 && certs2) {
	NSSCertificate **certs, **cp;
	PRUint32 count = 0;
	PRUint32 count1 = 0;
	cp = certs1;
	while (*cp++) count1++;
	count = count1;
	cp = certs2;
	while (*cp++) count++;
	certs = nss_ZREALLOCARRAY(certs1, NSSCertificate *, count + 1);
	if (!certs) {
	    nss_ZFreeIf(certs1);
	    nss_ZFreeIf(certs2);
	    return (NSSCertificate **)NULL;
	}
	for (cp = certs2; *cp; cp++, count1++) {
	    certs[count1] = *cp;
	}
	nss_ZFreeIf(certs2);
	return certs;
    } else if (certs1) {
	return certs1;
    } else {
	return certs2;
    }
}

NSS_IMPLEMENT NSSCertificate * 
nssCertificateArray_FindBestCertificate (
  NSSCertificate **certs, 
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt
)
{
    NSSCertificate *bestCert = NULL;
    NSSTime *time, sTime;
    PRBool haveUsageMatch = PR_FALSE;
    PRBool thisCertMatches;

    if (timeOpt) {
	time = timeOpt;
    } else {
	NSSTime_Now(&sTime);
	time = &sTime;
    }
    if (!certs) {
	return (NSSCertificate *)NULL;
    }
    for (; *certs; certs++) {
	nssDecodedCert *dc, *bestdc;
	NSSCertificate *c = *certs;
	dc = nssCertificate_GetDecoding(c);
	if (!dc) continue;
	thisCertMatches = dc->matchUsage(dc, usage);
	if (!bestCert) {
	    /* always take the first cert, but remember whether or not
	     * the usage matched 
	     */
	    bestCert = nssCertificate_AddRef(c);
	    haveUsageMatch = thisCertMatches;
	    continue;
	} else {
	    if (haveUsageMatch && !thisCertMatches) {
		/* if already have a cert for this usage, and if this cert 
		 * doesn't have the correct usage, continue
		 */
		continue;
	    } else if (!haveUsageMatch && thisCertMatches) {
		/* this one does match usage, replace the other */
		nssCertificate_Destroy(bestCert);
		bestCert = nssCertificate_AddRef(c);
		haveUsageMatch = PR_TRUE;
		continue;
	    }
	    /* this cert match as well as any cert we've found so far, 
	     * defer to time/policies 
	     * */
	}
	bestdc = nssCertificate_GetDecoding(bestCert);
	/* time */
	if (bestdc->isValidAtTime(bestdc, time)) {
	    /* The current best cert is valid at time */
	    if (!dc->isValidAtTime(dc, time)) {
		/* If the new cert isn't valid at time, it's not better */
		continue;
	    }
	} else {
	    /* The current best cert is not valid at time */
	    if (dc->isValidAtTime(dc, time)) {
		/* If the new cert is valid at time, it's better */
		nssCertificate_Destroy(bestCert);
		bestCert = nssCertificate_AddRef(c);
	    }
	}
	/* either they are both valid at time, or neither valid; 
	 * take the newer one
	 */
	if (!bestdc->isNewerThan(bestdc, dc)) {
	    nssCertificate_Destroy(bestCert);
	    bestCert = nssCertificate_AddRef(c);
	}
	/* policies */
	/* XXX later -- defer to policies */
    }
    return bestCert;
}

NSS_IMPLEMENT PRStatus
nssCertificateArray_Traverse (
  NSSCertificate **certs,
  PRStatus (* callback)(NSSCertificate *c, void *arg),
  void *arg
)
{
    PRStatus status = PR_SUCCESS;
    if (certs) {
	NSSCertificate **certp;
	for (certp = certs; *certp; certp++) {
	    status = (*callback)(*certp, arg);
	    if (status != PR_SUCCESS) {
		break;
	    }
	}
    }
    return status;
}


NSS_IMPLEMENT void
nssCRLArray_Destroy (
  NSSCRL **crls
)
{
    if (crls) {
	NSSCRL **crlp;
	for (crlp = crls; *crlp; crlp++) {
	    nssCRL_Destroy(*crlp);
	}
	nss_ZFreeIf(crls);
    }
}

/*
 * Object collections
 */

typedef enum
{
  pkiObjectType_Certificate = 0,
  pkiObjectType_CRL = 1,
  pkiObjectType_PrivateKey = 2,
  pkiObjectType_PublicKey = 3
} pkiObjectType;

/* Each object is defined by a set of items that uniquely identify it.
 * Here are the uid sets:
 *
 * NSSCertificate ==>  { issuer, serial }
 * NSSPrivateKey
 *         (RSA) ==> { modulus, public exponent }
 *
 */
#define MAX_ITEMS_FOR_UID 2

/* pkiObjectCollectionNode
 *
 * A node in the collection is the set of unique identifiers for a single
 * object, along with either the actual object or a proto-object.
 */
typedef struct
{
  PRCList link;
  PRBool haveObject;
  nssPKIObject *object;
  NSSItem uid[MAX_ITEMS_FOR_UID];
} 
pkiObjectCollectionNode;

/* nssPKIObjectCollection
 *
 * The collection is the set of all objects, plus the interfaces needed
 * to manage the objects.
 *
 */
struct nssPKIObjectCollectionStr
{
  NSSArena *arena;
  NSSTrustDomain *td;
  NSSCryptoContext *cc;
  PRCList head; /* list of pkiObjectCollectionNode's */
  PRUint32 size;
  pkiObjectType objectType;
  void           (*      destroyObject)(nssPKIObject *o);
  PRStatus       (*   getUIDFromObject)(nssPKIObject *o, NSSItem *uid);
  PRStatus       (* getUIDFromInstance)(nssCryptokiObject *co, NSSItem *uid, 
                                        NSSArena *arena);
  nssPKIObject * (*       createObject)(nssPKIObject *o);
};

static nssPKIObjectCollection *
nssPKIObjectCollection_Create (
  NSSTrustDomain *td,
  NSSCryptoContext *ccOpt
)
{
    NSSArena *arena;
    nssPKIObjectCollection *rvCollection = NULL;
    arena = nssArena_Create();
    if (!arena) {
	return (nssPKIObjectCollection *)NULL;
    }
    rvCollection = nss_ZNEW(arena, nssPKIObjectCollection);
    if (!rvCollection) {
	goto loser;
    }
    PR_INIT_CLIST(&rvCollection->head);
    rvCollection->arena = arena;
    rvCollection->td = td; /* XXX */
    rvCollection->cc = ccOpt;
    return rvCollection;
loser:
    nssArena_Destroy(arena);
    return (nssPKIObjectCollection *)NULL;
}

NSS_IMPLEMENT void
nssPKIObjectCollection_Destroy (
  nssPKIObjectCollection *collection
)
{
    if (collection) {
	PRCList *link;
	pkiObjectCollectionNode *node;
	/* first destroy any objects in the collection */
	link = PR_NEXT_LINK(&collection->head);
	while (link != &collection->head) {
	    node = (pkiObjectCollectionNode *)link;
	    if (node->haveObject) {
		(*collection->destroyObject)(node->object);
	    } else {
		nssPKIObject_Destroy(node->object);
	    }
	    link = PR_NEXT_LINK(link);
	}
	/* then destroy it */
	nssArena_Destroy(collection->arena);
    }
}

NSS_IMPLEMENT PRUint32
nssPKIObjectCollection_Count (
  nssPKIObjectCollection *collection
)
{
    return collection->size;
}

NSS_IMPLEMENT PRStatus
nssPKIObjectCollection_AddObject (
  nssPKIObjectCollection *collection,
  nssPKIObject *object
)
{
    pkiObjectCollectionNode *node;
    node = nss_ZNEW(collection->arena, pkiObjectCollectionNode);
    if (!node) {
	return PR_FAILURE;
    }
    node->haveObject = PR_TRUE;
    node->object = nssPKIObject_AddRef(object);
    (*collection->getUIDFromObject)(object, node->uid);
    PR_INIT_CLIST(&node->link);
    PR_INSERT_BEFORE(&node->link, &collection->head);
    collection->size++;
    return PR_SUCCESS;
}

static pkiObjectCollectionNode *
find_instance_in_collection (
  nssPKIObjectCollection *collection,
  nssCryptokiObject *instance
)
{
    PRCList *link;
    pkiObjectCollectionNode *node;
    link = PR_NEXT_LINK(&collection->head);
    while (link != &collection->head) {
	node = (pkiObjectCollectionNode *)link;
	if (nssPKIObject_HasInstance(node->object, instance)) {
	    return node;
	}
	link = PR_NEXT_LINK(link);
    }
    return (pkiObjectCollectionNode *)NULL;
}

static pkiObjectCollectionNode *
find_object_in_collection (
  nssPKIObjectCollection *collection,
  NSSItem *uid
)
{
    PRUint32 i;
    PRStatus status;
    PRCList *link;
    pkiObjectCollectionNode *node;
    link = PR_NEXT_LINK(&collection->head);
    while (link != &collection->head) {
	node = (pkiObjectCollectionNode *)link;
	for (i=0; i<MAX_ITEMS_FOR_UID; i++) {
	    if (!nssItem_Equal(&node->uid[i], &uid[i], &status)) {
		break;
	    }
	}
	if (i == MAX_ITEMS_FOR_UID) {
	    return node;
	}
	link = PR_NEXT_LINK(link);
    }
    return (pkiObjectCollectionNode *)NULL;
}

static pkiObjectCollectionNode *
add_object_instance (
  nssPKIObjectCollection *collection,
  nssCryptokiObject *instance,
  PRBool *foundIt
)
{
    PRUint32 i;
    PRStatus status;
    pkiObjectCollectionNode *node;
    nssArenaMark *mark = NULL;
    NSSItem uid[MAX_ITEMS_FOR_UID];
    nsslibc_memset(uid, 0, sizeof uid);
    /* The list is traversed twice, first (here) looking to match the
     * { token, handle } tuple, and if that is not found, below a search
     * for unique identifier is done.  Here, a match means this exact object
     * instance is already in the collection, and we have nothing to do.
     */
    *foundIt = PR_FALSE;
    node = find_instance_in_collection(collection, instance);
    if (node) {
	/* The collection is assumed to take over the instance.  Since we
	 * are not using it, it must be destroyed.
	 */
	nssCryptokiObject_Destroy(instance);
	*foundIt = PR_TRUE;
	return node;
    }
    mark = nssArena_Mark(collection->arena);
    if (!mark) {
	goto loser;
    }
    status = (*collection->getUIDFromInstance)(instance, uid, 
                                               collection->arena);
    if (status != PR_SUCCESS) {
	goto loser;
    }
    /* Search for unique identifier.  A match here means the object exists 
     * in the collection, but does not have this instance, so the instance 
     * needs to be added.
     */
    node = find_object_in_collection(collection, uid);
    if (node) {
	/* This is a object with multiple instances */
	status = nssPKIObject_AddInstance(node->object, instance);
    } else {
	/* This is a completely new object.  Create a node for it. */
	node = nss_ZNEW(collection->arena, pkiObjectCollectionNode);
	if (!node) {
	    goto loser;
	}
	node->object = nssPKIObject_Create(NULL, instance, 
	                                   collection->td, collection->cc);
	if (!node->object) {
	    goto loser;
	}
	for (i=0; i<MAX_ITEMS_FOR_UID; i++) {
	    node->uid[i] = uid[i];
	}
	node->haveObject = PR_FALSE;
	PR_INIT_CLIST(&node->link);
	PR_INSERT_BEFORE(&node->link, &collection->head);
	collection->size++;
	status = PR_SUCCESS;
    }
    nssArena_Unmark(collection->arena, mark);
    return node;
loser:
    if (mark) {
	nssArena_Release(collection->arena, mark);
    }
    nssCryptokiObject_Destroy(instance);
    return (pkiObjectCollectionNode *)NULL;
}

NSS_IMPLEMENT PRStatus
nssPKIObjectCollection_AddInstances (
  nssPKIObjectCollection *collection,
  nssCryptokiObject **instances,
  PRUint32 numInstances
)
{
    PRStatus status = PR_SUCCESS;
    PRUint32 i = 0;
    PRBool foundIt;
    pkiObjectCollectionNode *node;
    if (instances) {
	for (; *instances; instances++, i++) {
	    if (numInstances > 0 && i == numInstances) {
		break;
	    }
	    if (status == PR_SUCCESS) {
		node = add_object_instance(collection, *instances, &foundIt);
		if (node == NULL) {
		    /* add_object_instance freed the current instance */
		    /* free the remaining instances */
		    status = PR_FAILURE;
		}
	    } else {
		nssCryptokiObject_Destroy(*instances);
	    }
	}
    }
    return status;
}

static void
nssPKIObjectCollection_RemoveNode (
   nssPKIObjectCollection *collection,
   pkiObjectCollectionNode *node
)
{
    PR_REMOVE_LINK(&node->link); 
    collection->size--;
}

static PRStatus
nssPKIObjectCollection_GetObjects (
  nssPKIObjectCollection *collection,
  nssPKIObject **rvObjects,
  PRUint32 rvSize
)
{
    PRUint32 i = 0;
    PRCList *link = PR_NEXT_LINK(&collection->head);
    pkiObjectCollectionNode *node;
    int error=0;
    while ((i < rvSize) && (link != &collection->head)) {
	node = (pkiObjectCollectionNode *)link;
	if (!node->haveObject) {
	    /* Convert the proto-object to an object */
	    node->object = (*collection->createObject)(node->object);
	    if (!node->object) {
		link = PR_NEXT_LINK(link);
		/*remove bogus object from list*/
		nssPKIObjectCollection_RemoveNode(collection,node);
		error++;
		continue;
	    }
	    node->haveObject = PR_TRUE;
	}
	rvObjects[i++] = nssPKIObject_AddRef(node->object);
	link = PR_NEXT_LINK(link);
    }
    if (!error && *rvObjects == NULL) {
	nss_SetError(NSS_ERROR_NOT_FOUND);
    }
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRStatus
nssPKIObjectCollection_Traverse (
  nssPKIObjectCollection *collection,
  nssPKIObjectCallback *callback
)
{
    PRStatus status;
    PRCList *link = PR_NEXT_LINK(&collection->head);
    pkiObjectCollectionNode *node;
    while (link != &collection->head) {
	node = (pkiObjectCollectionNode *)link;
	if (!node->haveObject) {
	    node->object = (*collection->createObject)(node->object);
	    if (!node->object) {
		link = PR_NEXT_LINK(link);
		/*remove bogus object from list*/
		nssPKIObjectCollection_RemoveNode(collection,node);
		continue;
	    }
	    node->haveObject = PR_TRUE;
	}
	switch (collection->objectType) {
	case pkiObjectType_Certificate: 
	    status = (*callback->func.cert)((NSSCertificate *)node->object, 
	                                    callback->arg);
	    break;
	case pkiObjectType_CRL: 
	    status = (*callback->func.crl)((NSSCRL *)node->object, 
	                                   callback->arg);
	    break;
	case pkiObjectType_PrivateKey: 
	    status = (*callback->func.pvkey)((NSSPrivateKey *)node->object, 
	                                     callback->arg);
	    break;
	case pkiObjectType_PublicKey: 
	    status = (*callback->func.pbkey)((NSSPublicKey *)node->object, 
	                                     callback->arg);
	    break;
	}
	link = PR_NEXT_LINK(link);
    }
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRStatus
nssPKIObjectCollection_AddInstanceAsObject (
  nssPKIObjectCollection *collection,
  nssCryptokiObject *instance
)
{
    pkiObjectCollectionNode *node;
    PRBool foundIt;
    node = add_object_instance(collection, instance, &foundIt);
    if (node == NULL) {
	return PR_FAILURE;
    }
    if (!node->haveObject) {
	node->object = (*collection->createObject)(node->object);
	if (!node->object) {
	    /*remove bogus object from list*/
	    nssPKIObjectCollection_RemoveNode(collection,node);
	    return PR_FAILURE;
	}
	node->haveObject = PR_TRUE;
    }
#ifdef NSS_3_4_CODE
    else if (!foundIt) {
	/* The instance was added to a pre-existing node.  This
	 * function is *only* being used for certificates, and having
	 * multiple instances of certs in 3.X requires updating the
	 * CERTCertificate.
	 * But only do it if it was a new instance!!!  If the same instance
	 * is encountered, we set *foundIt to true.  Detect that here and
	 * ignore it.
	 */
	STAN_ForceCERTCertificateUpdate((NSSCertificate *)node->object);
    }
#endif
    return PR_SUCCESS;
}

/*
 * Certificate collections
 */

static void
cert_destroyObject(nssPKIObject *o)
{
    NSSCertificate *c = (NSSCertificate *)o;
#ifdef NSS_3_4_CODE
    if (c->decoding) {
	CERTCertificate *cc = STAN_GetCERTCertificate(c);
	if (cc) {
	    CERT_DestroyCertificate(cc);
	    return;
	} /* else destroy it as NSSCertificate below */
    }
#endif
    nssCertificate_Destroy(c);
}

static PRStatus
cert_getUIDFromObject(nssPKIObject *o, NSSItem *uid)
{
    NSSCertificate *c = (NSSCertificate *)o;
#ifdef NSS_3_4_CODE
    /* The builtins are still returning decoded serial numbers.  Until
     * this compatibility issue is resolved, use the full DER of the
     * cert to uniquely identify it.
     */
    NSSDER *derCert;
    derCert = nssCertificate_GetEncoding(c);
    uid[0].data = NULL; uid[0].size = 0;
    uid[1].data = NULL; uid[1].size = 0;
    if (derCert != NULL) {
	uid[0] = *derCert;
    }
#else
    NSSDER *issuer, *serial;
    issuer = nssCertificate_GetIssuer(c);
    serial = nssCertificate_GetSerialNumber(c);
    uid[0] = *issuer;
    uid[1] = *serial;
#endif /* NSS_3_4_CODE */
    return PR_SUCCESS;
}

static PRStatus
cert_getUIDFromInstance(nssCryptokiObject *instance, NSSItem *uid, 
                        NSSArena *arena)
{
#ifdef NSS_3_4_CODE
    /* The builtins are still returning decoded serial numbers.  Until
     * this compatibility issue is resolved, use the full DER of the
     * cert to uniquely identify it.
     */
    uid[1].data = NULL; uid[1].size = 0;
    return nssCryptokiCertificate_GetAttributes(instance,
                                                NULL,  /* XXX sessionOpt */
                                                arena, /* arena    */
                                                NULL,  /* type     */
                                                NULL,  /* id       */
                                                &uid[0], /* encoding */
                                                NULL,  /* issuer   */
                                                NULL,  /* serial   */
                                                NULL);  /* subject  */
#else
    return nssCryptokiCertificate_GetAttributes(instance,
                                                NULL,  /* XXX sessionOpt */
                                                arena, /* arena    */
                                                NULL,  /* type     */
                                                NULL,  /* id       */
                                                NULL,  /* encoding */
                                                &uid[0], /* issuer */
                                                &uid[1], /* serial */
                                                NULL);  /* subject  */
#endif /* NSS_3_4_CODE */
}

static nssPKIObject *
cert_createObject(nssPKIObject *o)
{
    NSSCertificate *cert;
    cert = nssCertificate_Create(o);
#ifdef NSS_3_4_CODE
/*    if (STAN_GetCERTCertificate(cert) == NULL) {
	nssCertificate_Destroy(cert);
	return (nssPKIObject *)NULL;
    } */
    /* In 3.4, have to maintain uniqueness of cert pointers by caching all
     * certs.  Cache the cert here, before returning.  If it is already
     * cached, take the cached entry.
     */
    {
	NSSTrustDomain *td = o->trustDomain;
	nssTrustDomain_AddCertsToCache(td, &cert, 1);
    }
#endif
    return (nssPKIObject *)cert;
}

NSS_IMPLEMENT nssPKIObjectCollection *
nssCertificateCollection_Create (
  NSSTrustDomain *td,
  NSSCertificate **certsOpt
)
{
    PRStatus status;
    nssPKIObjectCollection *collection;
    collection = nssPKIObjectCollection_Create(td, NULL);
    collection->objectType = pkiObjectType_Certificate;
    collection->destroyObject = cert_destroyObject;
    collection->getUIDFromObject = cert_getUIDFromObject;
    collection->getUIDFromInstance = cert_getUIDFromInstance;
    collection->createObject = cert_createObject;
    if (certsOpt) {
	for (; *certsOpt; certsOpt++) {
	    nssPKIObject *object = (nssPKIObject *)(*certsOpt);
	    status = nssPKIObjectCollection_AddObject(collection, object);
	}
    }
    return collection;
}

NSS_IMPLEMENT NSSCertificate **
nssPKIObjectCollection_GetCertificates (
  nssPKIObjectCollection *collection,
  NSSCertificate **rvOpt,
  PRUint32 maximumOpt,
  NSSArena *arenaOpt
)
{
    PRStatus status;
    PRUint32 rvSize;
    PRBool allocated = PR_FALSE;
    if (collection->size == 0) {
	return (NSSCertificate **)NULL;
    }
    if (maximumOpt == 0) {
	rvSize = collection->size;
    } else {
	rvSize = PR_MIN(collection->size, maximumOpt);
    }
    if (!rvOpt) {
	rvOpt = nss_ZNEWARRAY(arenaOpt, NSSCertificate *, rvSize + 1);
	if (!rvOpt) {
	    return (NSSCertificate **)NULL;
	}
	allocated = PR_TRUE;
    }
    status = nssPKIObjectCollection_GetObjects(collection, 
                                               (nssPKIObject **)rvOpt, 
                                               rvSize);
    if (status != PR_SUCCESS) {
	if (allocated) {
	    nss_ZFreeIf(rvOpt);
	}
	return (NSSCertificate **)NULL;
    }
    return rvOpt;
}

/*
 * CRL/KRL collections
 */

static void
crl_destroyObject(nssPKIObject *o)
{
    NSSCRL *crl = (NSSCRL *)o;
    nssCRL_Destroy(crl);
}

static PRStatus
crl_getUIDFromObject(nssPKIObject *o, NSSItem *uid)
{
    NSSCRL *crl = (NSSCRL *)o;
    NSSDER *encoding;
    encoding = nssCRL_GetEncoding(crl);
    uid[0] = *encoding;
    uid[1].data = NULL; uid[1].size = 0;
    return PR_SUCCESS;
}

static PRStatus
crl_getUIDFromInstance(nssCryptokiObject *instance, NSSItem *uid, 
                       NSSArena *arena)
{
    return nssCryptokiCRL_GetAttributes(instance,
                                        NULL,    /* XXX sessionOpt */
                                        arena,   /* arena    */
                                        &uid[0], /* encoding */
                                        NULL,    /* subject  */
                                        NULL,    /* class    */
                                        NULL,    /* url      */
                                        NULL);   /* isKRL    */
}

static nssPKIObject *
crl_createObject(nssPKIObject *o)
{
    return (nssPKIObject *)nssCRL_Create(o);
}

NSS_IMPLEMENT nssPKIObjectCollection *
nssCRLCollection_Create (
  NSSTrustDomain *td,
  NSSCRL **crlsOpt
)
{
    PRStatus status;
    nssPKIObjectCollection *collection;
    collection = nssPKIObjectCollection_Create(td, NULL);
    collection->objectType = pkiObjectType_CRL;
    collection->destroyObject = crl_destroyObject;
    collection->getUIDFromObject = crl_getUIDFromObject;
    collection->getUIDFromInstance = crl_getUIDFromInstance;
    collection->createObject = crl_createObject;
    if (crlsOpt) {
	for (; *crlsOpt; crlsOpt++) {
	    nssPKIObject *object = (nssPKIObject *)(*crlsOpt);
	    status = nssPKIObjectCollection_AddObject(collection, object);
	}
    }
    return collection;
}

NSS_IMPLEMENT NSSCRL **
nssPKIObjectCollection_GetCRLs (
  nssPKIObjectCollection *collection,
  NSSCRL **rvOpt,
  PRUint32 maximumOpt,
  NSSArena *arenaOpt
)
{
    PRStatus status;
    PRUint32 rvSize;
    PRBool allocated = PR_FALSE;
    if (collection->size == 0) {
	return (NSSCRL **)NULL;
    }
    if (maximumOpt == 0) {
	rvSize = collection->size;
    } else {
	rvSize = PR_MIN(collection->size, maximumOpt);
    }
    if (!rvOpt) {
	rvOpt = nss_ZNEWARRAY(arenaOpt, NSSCRL *, rvSize + 1);
	if (!rvOpt) {
	    return (NSSCRL **)NULL;
	}
	allocated = PR_TRUE;
    }
    status = nssPKIObjectCollection_GetObjects(collection, 
                                               (nssPKIObject **)rvOpt, 
                                               rvSize);
    if (status != PR_SUCCESS) {
	if (allocated) {
	    nss_ZFreeIf(rvOpt);
	}
	return (NSSCRL **)NULL;
    }
    return rvOpt;
}

#ifdef PURE_STAN_BUILD
/*
 * PrivateKey collections
 */

static void
privkey_destroyObject(nssPKIObject *o)
{
    NSSPrivateKey *pvk = (NSSPrivateKey *)o;
    nssPrivateKey_Destroy(pvk);
}

static PRStatus
privkey_getUIDFromObject(nssPKIObject *o, NSSItem *uid)
{
    NSSPrivateKey *pvk = (NSSPrivateKey *)o;
    NSSItem *id;
    id = nssPrivateKey_GetID(pvk);
    uid[0] = *id;
    return PR_SUCCESS;
}

static PRStatus
privkey_getUIDFromInstance(nssCryptokiObject *instance, NSSItem *uid, 
                           NSSArena *arena)
{
    return nssCryptokiPrivateKey_GetAttributes(instance,
                                               NULL,  /* XXX sessionOpt */
                                               arena,
                                               NULL, /* type */
                                               &uid[0]);
}

static nssPKIObject *
privkey_createObject(nssPKIObject *o)
{
    NSSPrivateKey *pvk;
    pvk = nssPrivateKey_Create(o);
    return (nssPKIObject *)pvk;
}

NSS_IMPLEMENT nssPKIObjectCollection *
nssPrivateKeyCollection_Create (
  NSSTrustDomain *td,
  NSSPrivateKey **pvkOpt
)
{
    PRStatus status;
    nssPKIObjectCollection *collection;
    collection = nssPKIObjectCollection_Create(td, NULL);
    collection->objectType = pkiObjectType_PrivateKey;
    collection->destroyObject = privkey_destroyObject;
    collection->getUIDFromObject = privkey_getUIDFromObject;
    collection->getUIDFromInstance = privkey_getUIDFromInstance;
    collection->createObject = privkey_createObject;
    if (pvkOpt) {
	for (; *pvkOpt; pvkOpt++) {
	    nssPKIObject *o = (nssPKIObject *)(*pvkOpt);
	    status = nssPKIObjectCollection_AddObject(collection, o);
	}
    }
    return collection;
}

NSS_IMPLEMENT NSSPrivateKey **
nssPKIObjectCollection_GetPrivateKeys (
  nssPKIObjectCollection *collection,
  NSSPrivateKey **rvOpt,
  PRUint32 maximumOpt,
  NSSArena *arenaOpt
)
{
    PRStatus status;
    PRUint32 rvSize;
    PRBool allocated = PR_FALSE;
    if (collection->size == 0) {
	return (NSSPrivateKey **)NULL;
    }
    if (maximumOpt == 0) {
	rvSize = collection->size;
    } else {
	rvSize = PR_MIN(collection->size, maximumOpt);
    }
    if (!rvOpt) {
	rvOpt = nss_ZNEWARRAY(arenaOpt, NSSPrivateKey *, rvSize + 1);
	if (!rvOpt) {
	    return (NSSPrivateKey **)NULL;
	}
	allocated = PR_TRUE;
    }
    status = nssPKIObjectCollection_GetObjects(collection, 
                                               (nssPKIObject **)rvOpt, 
                                               rvSize);
    if (status != PR_SUCCESS) {
	if (allocated) {
	    nss_ZFreeIf(rvOpt);
	}
	return (NSSPrivateKey **)NULL;
    }
    return rvOpt;
}

/*
 * PublicKey collections
 */

static void
pubkey_destroyObject(nssPKIObject *o)
{
    NSSPublicKey *pubk = (NSSPublicKey *)o;
    nssPublicKey_Destroy(pubk);
}

static PRStatus
pubkey_getUIDFromObject(nssPKIObject *o, NSSItem *uid)
{
    NSSPublicKey *pubk = (NSSPublicKey *)o;
    NSSItem *id;
    id = nssPublicKey_GetID(pubk);
    uid[0] = *id;
    return PR_SUCCESS;
}

static PRStatus
pubkey_getUIDFromInstance(nssCryptokiObject *instance, NSSItem *uid, 
                          NSSArena *arena)
{
    return nssCryptokiPublicKey_GetAttributes(instance,
                                              NULL,  /* XXX sessionOpt */
                                              arena,
                                              NULL, /* type */
                                              &uid[0]);
}

static nssPKIObject *
pubkey_createObject(nssPKIObject *o)
{
    NSSPublicKey *pubk;
    pubk = nssPublicKey_Create(o);
    return (nssPKIObject *)pubk;
}

NSS_IMPLEMENT nssPKIObjectCollection *
nssPublicKeyCollection_Create (
  NSSTrustDomain *td,
  NSSPublicKey **pubkOpt
)
{
    PRStatus status;
    nssPKIObjectCollection *collection;
    collection = nssPKIObjectCollection_Create(td, NULL);
    collection->objectType = pkiObjectType_PublicKey;
    collection->destroyObject = pubkey_destroyObject;
    collection->getUIDFromObject = pubkey_getUIDFromObject;
    collection->getUIDFromInstance = pubkey_getUIDFromInstance;
    collection->createObject = pubkey_createObject;
    if (pubkOpt) {
	for (; *pubkOpt; pubkOpt++) {
	    nssPKIObject *o = (nssPKIObject *)(*pubkOpt);
	    status = nssPKIObjectCollection_AddObject(collection, o);
	}
    }
    return collection;
}

NSS_IMPLEMENT NSSPublicKey **
nssPKIObjectCollection_GetPublicKeys (
  nssPKIObjectCollection *collection,
  NSSPublicKey **rvOpt,
  PRUint32 maximumOpt,
  NSSArena *arenaOpt
)
{
    PRStatus status;
    PRUint32 rvSize;
    PRBool allocated = PR_FALSE;
    if (collection->size == 0) {
	return (NSSPublicKey **)NULL;
    }
    if (maximumOpt == 0) {
	rvSize = collection->size;
    } else {
	rvSize = PR_MIN(collection->size, maximumOpt);
    }
    if (!rvOpt) {
	rvOpt = nss_ZNEWARRAY(arenaOpt, NSSPublicKey *, rvSize + 1);
	if (!rvOpt) {
	    return (NSSPublicKey **)NULL;
	}
	allocated = PR_TRUE;
    }
    status = nssPKIObjectCollection_GetObjects(collection, 
                                               (nssPKIObject **)rvOpt, 
                                               rvSize);
    if (status != PR_SUCCESS) {
	if (allocated) {
	    nss_ZFreeIf(rvOpt);
	}
	return (NSSPublicKey **)NULL;
    }
    return rvOpt;
}
#endif /* PURE_STAN_BUILD */

/* how bad would it be to have a static now sitting around, updated whenever
 * this was called?  would avoid repeated allocs...
 */
NSS_IMPLEMENT NSSTime *
NSSTime_Now (
  NSSTime *timeOpt
)
{
    return NSSTime_SetPRTime(timeOpt, PR_Now());
}

NSS_IMPLEMENT NSSTime *
NSSTime_SetPRTime (
  NSSTime *timeOpt,
  PRTime prTime
)
{
    NSSTime *rvTime;
    rvTime = (timeOpt) ? timeOpt : nss_ZNEW(NULL, NSSTime);
    rvTime->prTime = prTime;
    return rvTime;
}

NSS_IMPLEMENT PRTime
NSSTime_GetPRTime (
  NSSTime *time
)
{
  return time->prTime;
}

