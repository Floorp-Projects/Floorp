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
static const char CVS_ID[] = "@(#) $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/security/nss/lib/pkix/src/Attribute/Attic/MVCreate.c,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:10:27 $ $Name:  $";
#endif /* DEBUG */

#ifndef PKIXM_H
#include "pkixm.h"
#endif /* PKIXM_H */

/*
 * nss_pkix_Attribute_V_Create
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_OID
 *  NSS_ERROR_INVALID_ITEM
 *  NSS_ERROR_INTERNAL_ERROR;
 *
 * Return value:
 *  A valid pointer to an NSSPKIXAttribute upon success
 *  NULL upon failure.
 */

NSS_IMPLEMENT NSSPKIXAttribute *
nss_pkix_Attribute_V_Create
(
  NSSArena *arenaOpt,
  NSSPKIXAttributeType *typeOid,
  PRUint32 count,
  va_list ap
)
{
  NSSArena *arena;
  PRBool arena_allocated = PR_FALSE;
  nssArenaMark *mark = (nssArenaMark *)NULL;
  NSSPKIXAttribute *rv = (NSSPKIXAttribute *)NULL;
  PRStatus status;
  PRUint32 i;

  if( (NSSArena *)NULL == arenaOpt ) {
    arena = nssArena_Create();
    if( (NSSArena *)NULL == arena ) {
      goto loser;
    }
    arena_allocated = PR_TRUE;
  } else {
    arena = arenaOpt;
    mark = nssArena_Mark(arena);
    if( (nssArenaMark *)NULL == mark ) {
      goto loser;
    }
  }

  rv = nss_ZNEW(arena, NSSPKIXAttribute);
  if( (NSSPKIXAttribute *)NULL == rv ) {
    goto loser;
  }

  rv->arena = arena;
  rv->i_allocated_arena = arena_allocated;

  rv->type = typeOid;
  {
    NSSItem typeder;
    NSSItem *x = nssOID_GetDEREncoding(rv->type, &typeder, arena);
    if( (NSSItem *)NULL == x ) {
      goto loser;
    }

    rv->asn1type.size = typeder.size;
    rv->asn1type.data = typeder.data;
  }

  rv->valuesCount = count;

  rv->asn1values = nss_ZNEWARRAY(arena, nssASN1Item *, count);
  if( (nssASN1Item **)NULL == rv->asn1values ) {
    goto loser;
  }

  for( i = 0; i < count; i++ ) {
    NSSItem tmp;
    NSSPKIXAttributeValue *v = (NSSPKIXAttributeValue *)
      va_arg(ap, NSSPKIXAttributeValue *);

    rv->asn1values[i] = nss_ZNEW(arena, nssASN1Item);
    if( (nssASN1Item *)NULL == rv->asn1values[i] ) {
      goto loser;
    }

    if( (NSSItem *)NULL == nssItem_Duplicate(v, arena, &tmp) ) {
      goto loser;
    }

    rv->asn1values[i].size = tmp.size;
    rv->asn1values[i].data = tmp.data;
  }

  if( (nssArenaMark *)NULL != mark ) {
    if( PR_SUCCESS != nssArena_Unmark(arena, mark) ) {
      goto loser;
    }
  }

#ifdef DEBUG
  if( PR_SUCCESS != nss_pkix_Attribute_add_pointer(rv) ) {
    goto loser;
  }

  if( PR_SUCCESS != 
      nssArena_registerDestructor(arena,
                                  nss_pkix_Attribute_remove_pointer, 
                                  rv) ) {
    (void)nss_pkix_Attribute_remove_pointer(rv);
    goto loser;
  }
#endif /* DEBUG */

  return rv;

 loser:
  if( (nssArenaMark *)NULL != mark ) {
    (void)nssArena_Release(arena, mark);
  }

  if( PR_TRUE == arena_allocated ) {
    (void)nssArena_Destroy(arena);
  }

  return (NSSPKIXAttribute *)NULL;
}
