/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BUILTINS_H
#include "builtins.h"
#endif /* BUILTINS_H */

/*
 * builtins/find.c
 *
 * This file implements the NSSCKMDFindObjects object for the
 * "builtin objects" cryptoki module.
 */

struct builtinsFOStr {
  NSSArena *arena;
  CK_ULONG n;
  CK_ULONG i;
  builtinsInternalObject **objs;
};

static void
builtins_mdFindObjects_Final
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
  struct builtinsFOStr *fo = (struct builtinsFOStr *)mdFindObjects->etc;
  NSSArena *arena = fo->arena;

  nss_ZFreeIf(fo->objs);
  nss_ZFreeIf(fo);
  nss_ZFreeIf(mdFindObjects);
  if ((NSSArena *)NULL != arena) {
    NSSArena_Destroy(arena);
  }

  return;
}

static NSSCKMDObject *
builtins_mdFindObjects_Next
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
  struct builtinsFOStr *fo = (struct builtinsFOStr *)mdFindObjects->etc;
  builtinsInternalObject *io;

  if( fo->i == fo->n ) {
    *pError = CKR_OK;
    return (NSSCKMDObject *)NULL;
  }

  io = fo->objs[ fo->i ];
  fo->i++;

  return nss_builtins_CreateMDObject(arena, io, pError);
}

static int
builtins_derUnwrapInt(unsigned char *src, int size, unsigned char **dest) {
    unsigned char *start = src;
    int len = 0;

    if (*src ++ != 2) {
	return 0;
    }
    len = *src++;
    if (len & 0x80) {
	int count = len & 0x7f;
	len =0;

	if (count+2 > size) {
	    return 0;
	}
	while (count-- > 0) {
	    len = (len << 8) | *src++;
	}
    }
    if (len + (src-start) != size) {
	return 0;
    }
    *dest = src;
    return len;
}

static CK_BBOOL
builtins_attrmatch
(
  CK_ATTRIBUTE_PTR a,
  const NSSItem *b
)
{
  PRBool prb;

  if( a->ulValueLen != b->size ) {
    /* match a decoded serial number */
    if ((a->type == CKA_SERIAL_NUMBER) && (a->ulValueLen < b->size)) {
	int len;
	unsigned char *data;

	len = builtins_derUnwrapInt(b->data,b->size,&data);
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
builtins_match
(
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulAttributeCount,
  builtinsInternalObject *o
)
{
  CK_ULONG i;

  for( i = 0; i < ulAttributeCount; i++ ) {
    CK_ULONG j;

    for( j = 0; j < o->n; j++ ) {
      if( o->types[j] == pTemplate[i].type ) {
        if( CK_FALSE == builtins_attrmatch(&pTemplate[i], &o->items[j]) ) {
          return CK_FALSE;
        } else {
          break;
        }
      }
    }

    if( j == o->n ) {
      /* Loop ran to the end: no matching attribute */
      return CK_FALSE;
    }
  }

  /* Every attribute passed */
  return CK_TRUE;
}

NSS_IMPLEMENT NSSCKMDFindObjects *
nss_builtins_FindObjectsInit
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
  struct builtinsFOStr *fo = (struct builtinsFOStr *)NULL;
  builtinsInternalObject **temp = (builtinsInternalObject **)NULL;
  PRUint32 i;

  arena = NSSArena_Create();
  if( (NSSArena *)NULL == arena ) {
    goto loser;
  }

  rv = nss_ZNEW(arena, NSSCKMDFindObjects);
  if( (NSSCKMDFindObjects *)NULL == rv ) {
    *pError = CKR_HOST_MEMORY;
    goto loser;
  }

  fo = nss_ZNEW(arena, struct builtinsFOStr);
  if( (struct builtinsFOStr *)NULL == fo ) {
    *pError = CKR_HOST_MEMORY;
    goto loser;
  }

  fo->arena = arena;
  /* fo->n and fo->i are already zero */

  rv->etc = (void *)fo;
  rv->Final = builtins_mdFindObjects_Final;
  rv->Next = builtins_mdFindObjects_Next;
  rv->null = (void *)NULL;

  temp = nss_ZNEWARRAY((NSSArena *)NULL, builtinsInternalObject *, 
                       nss_builtins_nObjects);
  if( (builtinsInternalObject **)NULL == temp ) {
    *pError = CKR_HOST_MEMORY;
    goto loser;
  }

  for( i = 0; i < nss_builtins_nObjects; i++ ) {
    builtinsInternalObject *o = (builtinsInternalObject *)&nss_builtins_data[i];

    if( CK_TRUE == builtins_match(pTemplate, ulAttributeCount, o) ) {
      temp[ fo->n ] = o;
      fo->n++;
    }
  }

  fo->objs = nss_ZNEWARRAY(arena, builtinsInternalObject *, fo->n);
  if( (builtinsInternalObject **)NULL == fo->objs ) {
    *pError = CKR_HOST_MEMORY;
    goto loser;
  }

  (void)nsslibc_memcpy(fo->objs, temp, sizeof(builtinsInternalObject *) * fo->n);
  nss_ZFreeIf(temp);
  temp = (builtinsInternalObject **)NULL;

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

