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
static const char CVS_ID[] = "@(#) $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/security/nss/lib/pkix/src/RDNSequence/Attic/PDuplicate.c,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:14:14 $ $Name:  $";
#endif /* DEBUG */

#ifndef PKIX_H
#include "pkix.h"
#endif /* PKIX_H */

/*
 * nssPKIXRDNSequence_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN_SEQUENCE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXRDNSequence upon success
 *  NULL upon failure
 */

NSS_IMPLEMENT NSSPKIXRDNSequence *
nssPKIXRDNSequence_Duplicate
(
  NSSPKIXRDNSequence *rdnseq,
  NSSArena *arenaOpt
)
{
  NSSArena *arena;
  PRBool arena_allocated = PR_FALSE;
  nssArenaMark *mark = (nssArenaMark *)NULL;
  NSSPKIXRDNSequence *rv = (NSSPKIXRDNSequence *)NULL;
  PRStatus status;
  PRUint32 i;

#ifdef NSSDEBUG
  if( PR_SUCCESS != nssPKIXRDNSequence_verifyPointer(rdnseq) ) {
    return (NSSPKIXRelativeDistinguishedName *)NULL;
  }

  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyOpt(attribute) ) {
      return (NSSPKIXRelativeDistinguishedName *)NULL;
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

  rv = nss_ZNEW(arena, NSSPKIXRDNSequence);
  if( (NSSPKIXRDNSequence *)NULL == rv ) {
    goto loser;
  }

  rv->arena = arena;
  rv->i_allocated_arena = arena_allocated;

  if( (NSSBER *)NULL != rdnseq->ber ) {
    rv->ber = nssItem_Duplicate(rdnseq->ber, arena, (NSSItem *)NULL);
    if( (NSSBER *)NULL == rv->ber ) {
      goto loser;
    }
  }

  if( (NSSDER *)NULL != rdnseq->der ) {
    rv->der = nssItem_Duplicate(rdnseq->der, arena, (NSSItem *)NULL);
    if( (NSSDER *)NULL == rv->der ) {
      goto loser;
    }
  }

  if( (NSSUTF8 *)NULL != rdnseq->utf8 ) {
    rv->utf8 = nssUTF8_duplicate(rdnseq->utf8, arena);
    if( (NSSUTF8 *)NULL == rv->utf8 ) {
      goto loser;
    }
  }

  if( 0 == rdnseq->count ) {
    nss_pkix_RDNSequence_Count(rdnseq);
  }

#ifdef PEDANTIC
  if( 0 == rdnseq->count ) {
    nss_SetError(NSS_ERROR_VALUE_OUT_OF_RANGE);
    return (NSSPKIXRelativeDistinguishedName *)NULL;
  }
#endif /* PEDANTIC */

  rv->count = rdnseq->count;

  rv->rdns = nss_ZNEWARRAY(arena, NSSPKIXRelativeDistinguishedName *, (rv->count+1));
  if( (NSSPKIXRelativeDistinguishedName **)NULL == rv->rdns ) {
    goto loser;
  }

  for( i = 0; i < count; i++ ) {
    NSSPKIXRelativeDistinguishedName *v = rdnseq->rdns[i];

    rv->rdns[i] = nssPKIXRelativeDistinguishedName_Duplicate(v, arena);
    if( (NSSPKIXRelativeDistinguishedName *)NULL == rv->rdns[i] ) {
      goto loser;
    }
  }

  if( (nssArenaMark *)NULL != mark ) {
    if( PR_SUCCESS != nssArena_Unmark(arena, mark) ) {
      goto loser;
    }
  }

#ifdef DEBUG
  if( PR_SUCCESS != nss_pkix_RDNSequence_add_pointer(rv) ) {
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

  return (NSSPKIXRDNSequence *)NULL;
}
