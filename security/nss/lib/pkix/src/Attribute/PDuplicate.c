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
static const char CVS_ID[] = "@(#) $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/security/nss/lib/pkix/src/Attribute/Attic/PDuplicate.c,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:10:50 $ $Name:  $";
#endif /* DEBUG */

#ifndef PKIX_H
#include "pkix.h"
#endif /* PKIX_H */

/*
 * nssPKIXAttribute_Duplicate
 *
 * This routine duplicates an NSSPKIXAttribute.  {arenaOpt}
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXAttribute upon success
 *  NULL upon failure.
 */

NSS_IMPLEMENT NSSPKIXAttribute *
nssPKIXAttribute_Duplicate
(
  NSSPKIXAttribute *attribute,
  NSSArena *arenaOpt
)
{
  NSSArena *arena;
  PRBool arena_allocated = PR_FALSE;
  nssArenaMark *mark = (nssArenaMark *)NULL;
  NSSPKIXAttribute *rv = (NSSPKIXAttribute *)NULL;

#ifdef NSSDEBUG
  if( PR_SUCCESS != nssPKIXAttribute_verifyPointer(attribute) ) {
    return PR_FAILURE;
  }

  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return PR_FAILURE;
    }
  }
#endif /* NSSDEBUG */

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

  if( (NSSBER *)NULL != attribute->ber ) {
    rv->ber = nssItem_Duplicate(arena, attribute->ber, (NSSItem *)NULL);
    if( (NSSBER *)NULL == rv->ber ) {
      goto loser;
    }
  }

  if( (NSSDER *)NULL != attribute->der ) {
    rv->der = nssItem_Duplicate(arena, attribute->der, (NSSItem *)NULL);
    if( (NSSDER *)NULL == rv->der ) {
      goto loser;
    }
  }

  if( (void *)NULL != attribute->asn1type.data ) {
    rv->asn1type.size = attribute->asn1type.size;
    rv->asn1type.data = nss_ZAlloc(arena, attribute->asn1type.size);
    if( (void *)NULL == rv->asn1type.data ) {
      goto loser;
    }
    (void)nsslibc_memcpy(rv->asn1type.data, attribute->asn1type.data, 
                         rv->asn1type.size);
  }
    
  if( (nssASN1Item **)NULL != attribute->asn1values ) {
    rv->asn1values = nss_ZNEWARRAY(arena, nssASN1Item *, attribute->valuesCount);
    if( (nssASN1Item **)NULL == rv->asn1values ) {
      goto loser;
    }

    for( i = 0; i < attribute->valuesCount; i++ ) {
      nssASN1Item *src;
      nssASN1Item *dst;

      src = attribute->asn1values[i];
      dst = nss_ZNEW(arena, nssASN1Item);
      if( (nssASN1Item *)NULL == dst ) {
        goto loser;
      }

      rv->asn1values[i] = dst;

      dst->size = src->size;
      dst->data = nss_ZAlloc(arena, dst->size);
      if( (void *)NULL == dst->data ) {
        goto loser;
      }
      (void)nsslibc_memcpy(dst->data, src->data, src->size);
    }
  }

  rv->type = attribute->type; /* NULL or otherwise */
  rv->valuescount = attribute->valuesCount;

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
