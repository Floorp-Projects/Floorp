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
static const char CVS_ID[] = "@(#) $RCSfile: find.c,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:43:49 $ $Name:  $";
#endif /* DEBUG */

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

  nss_ZFreeIf(fo->objs);
  nss_ZFreeIf(fo);

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

static CK_BBOOL
builtins_attrmatch
(
  CK_ATTRIBUTE_PTR a,
  NSSItem *b
)
{
  PRBool prb;

  if( a->ulValueLen != b->size ) {
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

  arena = NSSCKFWSession_GetArena(fwSession, pError);
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

  if( 0 == fo->n ) {
    *pError = CKR_OK;
    goto loser;
  }

  fo->objs = nss_ZNEWARRAY(arena, builtinsInternalObject *, fo->n);
  if( (builtinsInternalObject **)NULL == temp ) {
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
  return (NSSCKMDFindObjects *)NULL;
}

