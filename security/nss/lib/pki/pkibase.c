/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: pkibase.c,v $ $Revision: 1.36 $ $Date: 2012/07/27 21:41:52 $";
#endif /* DEBUG */

#ifndef DEV_H
#include "dev.h"
#endif /* DEV_H */

#ifndef PKIM_H
#include "pkim.h"
#endif /* PKIM_H */

#include "pki3hack.h"

extern const NSSError NSS_ERROR_NOT_FOUND;

NSS_IMPLEMENT void
nssPKIObject_Lock(nssPKIObject * object)
{
    switch (object->lockType) {
    case nssPKIMonitor:
        PZ_EnterMonitor(object->sync.mlock);
        break;
    case nssPKILock:
        PZ_Lock(object->sync.lock);
        break;
    default:
        PORT_Assert(0);
    }
}

NSS_IMPLEMENT void
nssPKIObject_Unlock(nssPKIObject * object)
{
    switch (object->lockType) {
    case nssPKIMonitor:
        PZ_ExitMonitor(object->sync.mlock);
        break;
    case nssPKILock:
        PZ_Unlock(object->sync.lock);
        break;
    default:
        PORT_Assert(0);
    }
}

NSS_IMPLEMENT PRStatus
nssPKIObject_NewLock(nssPKIObject * object, nssPKILockType lockType)
{
    object->lockType = lockType;
    switch (lockType) {
    case nssPKIMonitor:
        object->sync.mlock = PZ_NewMonitor(nssILockSSL);
        return (object->sync.mlock ? PR_SUCCESS : PR_FAILURE);
    case nssPKILock:
        object->sync.lock = PZ_NewLock(nssILockSSL);
        return (object->sync.lock ? PR_SUCCESS : PR_FAILURE);
    default:
        PORT_Assert(0);
        return PR_FAILURE;
    }
}

NSS_IMPLEMENT void
nssPKIObject_DestroyLock(nssPKIObject * object)
{
    switch (object->lockType) {
    case nssPKIMonitor:
        PZ_DestroyMonitor(object->sync.mlock);
        object->sync.mlock = NULL;
        break;
    case nssPKILock:
        PZ_DestroyLock(object->sync.lock);
        object->sync.lock = NULL;
        break;
    default:
        PORT_Assert(0);
    }
}



NSS_IMPLEMENT nssPKIObject *
nssPKIObject_Create (
  NSSArena *arenaOpt,
  nssCryptokiObject *instanceOpt,
  NSSTrustDomain *td,
  NSSCryptoContext *cc,
  nssPKILockType lockType
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
    if (PR_SUCCESS != nssPKIObject_NewLock(object, lockType)) {
	goto loser;
    }
    if (instanceOpt) {
	if (nssPKIObject_AddInstance(object, instanceOpt) != PR_SUCCESS) {
	    goto loser;
	}
    }
    PR_ATOMIC_INCREMENT(&object->refCount);
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
    if (PR_ATOMIC_DECREMENT(&object->refCount) == 0) {
	for (i=0; i<object->numInstances; i++) {
	    nssCryptokiObject_Destroy(object->instances[i]);
	}
	nssPKIObject_DestroyLock(object);
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
    PR_ATOMIC_INCREMENT(&object->refCount);
    return object;
}

NSS_IMPLEMENT PRStatus
nssPKIObject_AddInstance (
  nssPKIObject *object,
  nssCryptokiObject *instance
)
{
    nssCryptokiObject **newInstances = NULL;

    nssPKIObject_Lock(object);
    if (object->numInstances == 0) {
	newInstances = nss_ZNEWARRAY(object->arena,
				     nssCryptokiObject *,
				     object->numInstances + 1);
    } else {
	PRBool found = PR_FALSE;
	PRUint32 i;
	for (i=0; i<object->numInstances; i++) {
	    if (nssCryptokiObject_Equal(object->instances[i], instance)) {
		found = PR_TRUE;
		break;
	    }
	}
	if (found) {
	    /* The new instance is identical to one in the array, except
	     * perhaps that the label may be different.  So replace 
	     * the label in the array instance with the label from the 
	     * new instance, and discard the new instance.
	     */
	    nss_ZFreeIf(object->instances[i]->label);
	    object->instances[i]->label = instance->label;
	    nssPKIObject_Unlock(object);
	    instance->label = NULL;
	    nssCryptokiObject_Destroy(instance);
	    return PR_SUCCESS;
	}
	newInstances = nss_ZREALLOCARRAY(object->instances,
					 nssCryptokiObject *,
					 object->numInstances + 1);
    }
    if (newInstances) {
	object->instances = newInstances;
	newInstances[object->numInstances++] = instance;
    }
    nssPKIObject_Unlock(object);
    return (newInstances ? PR_SUCCESS : PR_FAILURE);
}

NSS_IMPLEMENT PRBool
nssPKIObject_HasInstance (
  nssPKIObject *object,
  nssCryptokiObject *instance
)
{
    PRUint32 i;
    PRBool hasIt = PR_FALSE;;
    nssPKIObject_Lock(object);
    for (i=0; i<object->numInstances; i++) {
	if (nssCryptokiObject_Equal(object->instances[i], instance)) {
	    hasIt = PR_TRUE;
	    break;
	}
    }
    nssPKIObject_Unlock(object);
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
    nssPKIObject_Lock(object);
    if (object->numInstances == 0) {
	nssPKIObject_Unlock(object);
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
	nssCryptokiObject **instances = nss_ZREALLOCARRAY(object->instances,
	                                      nssCryptokiObject *,
	                                      object->numInstances);
	if (instances) {
	    object->instances = instances;
	}
    } else {
	nss_ZFreeIf(object->instances);
    }
    nssCryptokiObject_Destroy(instanceToRemove);
    nssPKIObject_Unlock(object);
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
    numNotDestroyed = 0;
    nssPKIObject_Lock(object);
    for (i=0; i<object->numInstances; i++) {
	nssCryptokiObject *instance = object->instances[i];
	status = nssToken_DeleteStoredObject(instance);
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
    nssPKIObject_Unlock(object);
    return status;
}

NSS_IMPLEMENT NSSToken **
nssPKIObject_GetTokens (
  nssPKIObject *object,
  PRStatus *statusOpt
)
{
    NSSToken **tokens = NULL;
    nssPKIObject_Lock(object);
    if (object->numInstances > 0) {
	tokens = nss_ZNEWARRAY(NULL, NSSToken *, object->numInstances + 1);
	if (tokens) {
	    PRUint32 i;
	    for (i=0; i<object->numInstances; i++) {
		tokens[i] = nssToken_AddRef(object->instances[i]->token);
	    }
	}
    }
    nssPKIObject_Unlock(object);
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
    nssPKIObject_Lock(object);
    for (i=0; i<object->numInstances; i++) {
	if ((!tokenOpt && object->instances[i]->label) ||
	    (object->instances[i]->token == tokenOpt)) 
	{
            /* Must copy, see bug 745548 */
	    nickname = nssUTF8_Duplicate(object->instances[i]->label, NULL);
	    break;
	}
    }
    nssPKIObject_Unlock(object);
    return nickname;
}

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
    nssPKIObject_Lock(object);
    instances = nss_ZNEWARRAY(NULL, nssCryptokiObject *, 
                              object->numInstances + 1);
    if (instances) {
	for (i=0; i<object->numInstances; i++) {
	    instances[i] = nssCryptokiObject_Clone(object->instances[i]);
	}
    }
    nssPKIObject_Unlock(object);
    return instances;
}

NSS_IMPLEMENT void
nssCertificateArray_Destroy (
  NSSCertificate **certs
)
{
    if (certs) {
	NSSCertificate **certp;
	for (certp = certs; *certp; certp++) {
	    if ((*certp)->decoding) {
		CERTCertificate *cc = STAN_GetCERTCertificate(*certp);
		if (cc) {
		    CERT_DestroyCertificate(cc);
		}
		continue;
	    }
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
  const NSSUsage *usage,
  NSSPolicies *policiesOpt
)
{
    NSSCertificate *bestCert = NULL;
    nssDecodedCert *bestdc = NULL;
    NSSTime *time, sTime;
    PRBool bestCertMatches = PR_FALSE;
    PRBool thisCertMatches;
    PRBool bestCertIsValidAtTime = PR_FALSE;
    PRBool bestCertIsTrusted = PR_FALSE;

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
	nssDecodedCert *dc;
	NSSCertificate *c = *certs;
	dc = nssCertificate_GetDecoding(c);
	if (!dc) continue;
	thisCertMatches = dc->matchUsage(dc, usage);
	if (!bestCert) {
	    /* always take the first cert, but remember whether or not
	     * the usage matched 
	     */
	    bestCert = nssCertificate_AddRef(c);
	    bestCertMatches = thisCertMatches;
	    bestdc = dc;
	    continue;
	} else {
	    if (bestCertMatches && !thisCertMatches) {
		/* if already have a cert for this usage, and if this cert 
		 * doesn't have the correct usage, continue
		 */
		continue;
	    } else if (!bestCertMatches && thisCertMatches) {
		/* this one does match usage, replace the other */
		nssCertificate_Destroy(bestCert);
		bestCert = nssCertificate_AddRef(c);
		bestCertMatches = thisCertMatches;
		bestdc = dc;
		continue;
	    }
	    /* this cert match as well as any cert we've found so far, 
	     * defer to time/policies 
	     * */
	}
	/* time */
	if (bestCertIsValidAtTime || bestdc->isValidAtTime(bestdc, time)) {
	    /* The current best cert is valid at time */
	    bestCertIsValidAtTime = PR_TRUE;
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
		bestdc = dc;
		bestCertIsValidAtTime = PR_TRUE;
		continue;
	    }
	}
	/* Either they are both valid at time, or neither valid.
	 * If only one is trusted for this usage, take it.
	 */
	if (bestCertIsTrusted || bestdc->isTrustedForUsage(bestdc, usage)) {
	    bestCertIsTrusted = PR_TRUE;
	    if (!dc->isTrustedForUsage(dc, usage)) {
	        continue;
	    }
	} else {
	    /* The current best cert is not trusted */
	    if (dc->isTrustedForUsage(dc, usage)) {
		/* If the new cert is trusted, it's better */
		nssCertificate_Destroy(bestCert);
		bestCert = nssCertificate_AddRef(c);
		bestdc = dc;
		bestCertIsTrusted = PR_TRUE;
	        continue;
	    }
	}
	/* Otherwise, take the newer one. */
	if (!bestdc->isNewerThan(bestdc, dc)) {
	    nssCertificate_Destroy(bestCert);
	    bestCert = nssCertificate_AddRef(c);
	    bestdc = dc;
	    continue;
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
  nssPKILockType lockType; /* type of lock to use for new proto-objects */
};

static nssPKIObjectCollection *
nssPKIObjectCollection_Create (
  NSSTrustDomain *td,
  NSSCryptoContext *ccOpt,
  nssPKILockType lockType
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
    rvCollection->lockType = lockType;
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
	/* This is an object with multiple instances */
	status = nssPKIObject_AddInstance(node->object, instance);
    } else {
	/* This is a completely new object.  Create a node for it. */
	node = nss_ZNEW(collection->arena, pkiObjectCollectionNode);
	if (!node) {
	    goto loser;
	}
	node->object = nssPKIObject_Create(NULL, instance, 
	                                   collection->td, collection->cc,
                                           collection->lockType);
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
	while ((!numInstances || i < numInstances) && *instances) {
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
	    instances++;
	    i++;
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
    } else if (!foundIt) {
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
    return PR_SUCCESS;
}

/*
 * Certificate collections
 */

static void
cert_destroyObject(nssPKIObject *o)
{
    NSSCertificate *c = (NSSCertificate *)o;
    if (c->decoding) {
	CERTCertificate *cc = STAN_GetCERTCertificate(c);
	if (cc) {
	    CERT_DestroyCertificate(cc);
	    return;
	} /* else destroy it as NSSCertificate below */
    }
    nssCertificate_Destroy(c);
}

static PRStatus
cert_getUIDFromObject(nssPKIObject *o, NSSItem *uid)
{
    NSSCertificate *c = (NSSCertificate *)o;
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
    return PR_SUCCESS;
}

static PRStatus
cert_getUIDFromInstance(nssCryptokiObject *instance, NSSItem *uid, 
                        NSSArena *arena)
{
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
}

static nssPKIObject *
cert_createObject(nssPKIObject *o)
{
    NSSCertificate *cert;
    cert = nssCertificate_Create(o);
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
    collection = nssPKIObjectCollection_Create(td, NULL, nssPKIMonitor);
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
    if (!encoding) {
        nss_SetError(NSS_ERROR_INVALID_ARGUMENT);
        return PR_FALSE;
    }
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
    collection = nssPKIObjectCollection_Create(td, NULL, nssPKILock);
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
    if (rvTime) {
        rvTime->prTime = prTime;
    }
    return rvTime;
}

NSS_IMPLEMENT PRTime
NSSTime_GetPRTime (
  NSSTime *time
)
{
  return time->prTime;
}

