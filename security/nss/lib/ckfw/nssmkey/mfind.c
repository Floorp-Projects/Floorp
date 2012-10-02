/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: mfind.c,v $ $Revision: 1.2 $ $Date: 2012/04/25 14:49:40 $";
#endif /* DEBUG */

#ifndef CKMK_H
#include "ckmk.h"
#endif /* CKMK_H */

/*
 * nssmkey/mfind.c
 *
 * This file implements the NSSCKMDFindObjects object for the
 * "nssmkey" cryptoki module.
 */

struct ckmkFOStr {
  NSSArena *arena;
  CK_ULONG n;
  CK_ULONG i;
  ckmkInternalObject **objs;
};

static void
ckmk_mdFindObjects_Final
(
  NSSCKMDFindObjects *mdFindObjects,
  NSSCKFWFindObjects *fwFindObjects,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  struct ckmkFOStr *fo = (struct ckmkFOStr *)mdFindObjects->etc;
  NSSArena *arena = fo->arena;
  PRUint32 i;

  /* walk down an free the unused 'objs' */
  for (i=fo->i; i < fo->n ; i++) {
    nss_ckmk_DestroyInternalObject(fo->objs[i]);
  }

  nss_ZFreeIf(fo->objs);
  nss_ZFreeIf(fo);
  nss_ZFreeIf(mdFindObjects);
  if ((NSSArena *)NULL != arena) {
    NSSArena_Destroy(arena);
  }

  return;
}

static NSSCKMDObject *
ckmk_mdFindObjects_Next
(
  NSSCKMDFindObjects *mdFindObjects,
  NSSCKFWFindObjects *fwFindObjects,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  NSSArena *arena,
  CK_RV *pError
)
{
  struct ckmkFOStr *fo = (struct ckmkFOStr *)mdFindObjects->etc;
  ckmkInternalObject *io;

  if( fo->i == fo->n ) {
    *pError = CKR_OK;
    return (NSSCKMDObject *)NULL;
  }

  io = fo->objs[ fo->i ];
  fo->i++;

  return nss_ckmk_CreateMDObject(arena, io, pError);
}

static CK_BBOOL
ckmk_attrmatch
(
  CK_ATTRIBUTE_PTR a,
  ckmkInternalObject *o
)
{
  PRBool prb;
  const NSSItem *b;
  CK_RV error;

  b = nss_ckmk_FetchAttribute(o, a->type,  &error);
  if (b == NULL) {
    return CK_FALSE;
  }

  if( a->ulValueLen != b->size ) {
    /* match a decoded serial number */
    if ((a->type == CKA_SERIAL_NUMBER) && (a->ulValueLen < b->size)) {
	int len;
	unsigned char *data;

	data = nss_ckmk_DERUnwrap(b->data, b->size, &len, NULL);
	if ((len == a->ulValueLen) && 
		nsslibc_memequal(a->pValue, data, len, (PRStatus *)NULL)) {
	    return CK_TRUE;
	}
    }
    return CK_FALSE;
  }

  prb = nsslibc_memequal(a->pValue, b->data, b->size, (PRStatus *)NULL);

  if( PR_TRUE == prb ) {
    return CK_TRUE;
  } else {
    return CK_FALSE;
  }
}


static CK_BBOOL
ckmk_match
(
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulAttributeCount,
  ckmkInternalObject *o
)
{
  CK_ULONG i;

  for( i = 0; i < ulAttributeCount; i++ ) {
    if (CK_FALSE == ckmk_attrmatch(&pTemplate[i], o)) {
      return CK_FALSE;
    }
  }

  /* Every attribute passed */
  return CK_TRUE;
}

#define CKMK_ITEM_CHUNK  20

#define PUT_OBJECT(obj, err, size, count, list) \
  { \
    if (count >= size) { \
    (list) = (list) ? \
		nss_ZREALLOCARRAY(list, ckmkInternalObject *, \
		               ((size)+CKMK_ITEM_CHUNK) ) : \
		nss_ZNEWARRAY(NULL, ckmkInternalObject *, \
		               ((size)+CKMK_ITEM_CHUNK) ) ; \
      if ((ckmkInternalObject **)NULL == list) { \
        err = CKR_HOST_MEMORY; \
        goto loser; \
      } \
      (size) += CKMK_ITEM_CHUNK; \
    } \
    (list)[ count ] = (obj); \
    count++; \
  }


/* find all the certs that represent the appropriate object (cert, priv key, or
 *  pub key) in the cert store.
 */
static PRUint32
collect_class(
  CK_OBJECT_CLASS objClass,
  SecItemClass itemClass,
  CK_ATTRIBUTE_PTR pTemplate, 
  CK_ULONG ulAttributeCount, 
  ckmkInternalObject ***listp,
  PRUint32 *sizep,
  PRUint32 count,
  CK_RV *pError
)
{
  ckmkInternalObject *next = NULL;
  SecKeychainSearchRef searchRef = 0;
  SecKeychainItemRef itemRef = 0;
  OSStatus error;

  /* future, build the attribute list based on the template
   * so we can refine the search */
  error = SecKeychainSearchCreateFromAttributes( 
		NULL, itemClass, NULL, &searchRef);

  while (noErr == SecKeychainSearchCopyNext(searchRef, &itemRef)) {
    /* if we don't have an internal object structure, get one */
    if ((ckmkInternalObject *)NULL == next) {
      next = nss_ZNEW(NULL, ckmkInternalObject);
      if ((ckmkInternalObject *)NULL == next) {
        *pError = CKR_HOST_MEMORY;
        goto loser;
      }
    }
    /* fill in the relevant object data */
    next->type = ckmkItem;
    next->objClass = objClass;
    next->u.item.itemRef = itemRef;
    next->u.item.itemClass = itemClass;

    /* see if this is one of the objects we are looking for */
    if( CK_TRUE == ckmk_match(pTemplate, ulAttributeCount, next) ) {
      /* yes, put it on the list */
      PUT_OBJECT(next, *pError, *sizep, count, *listp);
      next = NULL; /* this one is on the list, need to allocate a new one now */
    } else {
      /* no , release the current item and clear out the structure for reuse */
      CFRelease(itemRef);
      /* don't cache the values we just loaded */
      nsslibc_memset(next, 0, sizeof(*next));
    }
  }
loser:
  if (searchRef) {
    CFRelease(searchRef);
  }
  nss_ZFreeIf(next);
  return count;
}

static PRUint32
collect_objects(
  CK_ATTRIBUTE_PTR pTemplate, 
  CK_ULONG ulAttributeCount, 
  ckmkInternalObject ***listp,
  CK_RV *pError
)
{
  PRUint32 i;
  PRUint32 count = 0;
  PRUint32 size = 0;
  CK_OBJECT_CLASS objClass;

  /*
   * first handle the static build in objects (if any)
   */
  for( i = 0; i < nss_ckmk_nObjects; i++ ) {
    ckmkInternalObject *o = (ckmkInternalObject *)&nss_ckmk_data[i];

    if( CK_TRUE == ckmk_match(pTemplate, ulAttributeCount, o) ) {
      PUT_OBJECT(o, *pError, size, count, *listp);
    }
  }

  /*
   * now handle the various object types
   */
  objClass = nss_ckmk_GetULongAttribute(CKA_CLASS, 
		pTemplate, ulAttributeCount, pError);
  if (CKR_OK != *pError) {
    objClass = CK_INVALID_HANDLE;
  }
  *pError = CKR_OK;
  switch (objClass) {
  case CKO_CERTIFICATE:
    count = collect_class(objClass, kSecCertificateItemClass,
                              pTemplate, ulAttributeCount, listp, 
                              &size, count, pError);
    break;
  case CKO_PUBLIC_KEY:
    count = collect_class(objClass,  CSSM_DL_DB_RECORD_PUBLIC_KEY,
                              pTemplate, ulAttributeCount, listp, 
                              &size, count, pError);
    break;
  case CKO_PRIVATE_KEY:
    count = collect_class(objClass,  CSSM_DL_DB_RECORD_PRIVATE_KEY,
                              pTemplate, ulAttributeCount, listp, 
                              &size, count, pError);
    break;
  /* all of them */
  case CK_INVALID_HANDLE:
    count = collect_class(CKO_CERTIFICATE, kSecCertificateItemClass,
                              pTemplate, ulAttributeCount, listp, 
                              &size, count, pError);
    count = collect_class(CKO_PUBLIC_KEY,  CSSM_DL_DB_RECORD_PUBLIC_KEY,
                              pTemplate, ulAttributeCount, listp, 
                              &size, count, pError);
    count = collect_class(CKO_PUBLIC_KEY,  CSSM_DL_DB_RECORD_PRIVATE_KEY,
                              pTemplate, ulAttributeCount, listp, 
                              &size, count, pError);
    break;
  default:
    break;
  }
  if (CKR_OK != *pError) {
    goto loser;
  }

  return count;
loser:
  nss_ZFreeIf(*listp);
  return 0;
}


NSS_IMPLEMENT NSSCKMDFindObjects *
nss_ckmk_FindObjectsInit
(
  NSSCKFWSession *fwSession,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulAttributeCount,
  CK_RV *pError
)
{
  /* This could be made more efficient.  I'm rather rushed. */
  NSSArena *arena;
  NSSCKMDFindObjects *rv = (NSSCKMDFindObjects *)NULL;
  struct ckmkFOStr *fo = (struct ckmkFOStr *)NULL;
  ckmkInternalObject **temp = (ckmkInternalObject **)NULL;

  arena = NSSArena_Create();
  if( (NSSArena *)NULL == arena ) {
    goto loser;
  }

  rv = nss_ZNEW(arena, NSSCKMDFindObjects);
  if( (NSSCKMDFindObjects *)NULL == rv ) {
    *pError = CKR_HOST_MEMORY;
    goto loser;
  }

  fo = nss_ZNEW(arena, struct ckmkFOStr);
  if( (struct ckmkFOStr *)NULL == fo ) {
    *pError = CKR_HOST_MEMORY;
    goto loser;
  }

  fo->arena = arena;
  /* fo->n and fo->i are already zero */

  rv->etc = (void *)fo;
  rv->Final = ckmk_mdFindObjects_Final;
  rv->Next = ckmk_mdFindObjects_Next;
  rv->null = (void *)NULL;

  fo->n = collect_objects(pTemplate, ulAttributeCount, &temp, pError);
  if (*pError != CKR_OK) {
    goto loser;
  }

  fo->objs = nss_ZNEWARRAY(arena, ckmkInternalObject *, fo->n);
  if( (ckmkInternalObject **)NULL == fo->objs ) {
    *pError = CKR_HOST_MEMORY;
    goto loser;
  }

  (void)nsslibc_memcpy(fo->objs, temp, sizeof(ckmkInternalObject *) * fo->n);
  nss_ZFreeIf(temp);
  temp = (ckmkInternalObject **)NULL;

  return rv;

 loser:
  nss_ZFreeIf(temp);
  nss_ZFreeIf(fo);
  nss_ZFreeIf(rv);
  if ((NSSArena *)NULL != arena) {
     NSSArena_Destroy(arena);
  }
  return (NSSCKMDFindObjects *)NULL;
}

