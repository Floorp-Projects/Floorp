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
static const char CVS_ID[] = "@(#) $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/security/nss/lib/pkix/src/RelativeDistinguishedName/Attic/PGetAttributeTypeAndValues.c,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:14:26 $ $Name:  $";
#endif /* DEBUG */

#ifndef PKIX_H
#include "pkix.h"
#endif /* PKIX_H */

/*
 * nssPKIXRelativeDistinguishedName_GetAttributeTypeAndValues
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARRAY_TOO_SMALL
 *
 * Return value:
 *  A valid pointer to an array of NSSPKIXAttributeTypeAndValue 
 *      pointers upon success
 *  NULL upon failure
 */

NSS_IMPLEMENT NSSPKIXAttributeTypeAndValue **
nssPKIXRelativeDistinguishedName_GetAttributeTypeAndValues
(
  NSSPKIXRelativeDistinguishedName *rdn,
  NSSPKIXAttributeTypeAndValue *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
)
{
  NSSPKIXAttributeTypeAndValue **rv = (NSSPKIXAttributeTypeAndValue **)NULL;
  PRUint32 i;

#ifdef NSSDEBUG
  if( PR_SUCCESS != nssPKIXRelativeDistinguishedName_verifyPointer(rdn) ) {
    return (NSSPKIXAttributeTypeAndValue **)NULL;
  }

  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyOpt(attribute) ) {
      return (NSSPKIXAttributeTypeAndValue **)NULL;
    }
  }
#endif /* NSSDEBUG */

  if( 0 == rdn->count ) {
    nss_pkix_RelativeDistinguishedName_Count(rdn);
  }

#ifdef PEDANTIC
  if( 0 == rdn->count ) {
    nss_SetError(NSS_ERROR_VALUE_OUT_OF_RANGE);
    return (NSSPKIXAttributeTypeAndValue **)NULL;
  }
#endif /* PEDANTIC */

  if( (limit < rdn->count) &&
      !((0 == limit) && ((NSSPKIXAttributeTypeAndValue **)NULL == rvOpt)) ) {
    nss_SetError(NSS_ERROR_ARRAY_TOO_SMALL);
    return (NSSPKIXAttributeTypeAndValue **)NULL;
  }

  limit = rdn->count;
  if( (NSSPKIXAttributeTypeAndValue **)NULL == rvOpt ) {
    rv = nss_ZNEWARRAY(arenaOpt, NSSPKIXAttributeTypeAndValue *, limit);
    if( (NSSPKIXAttributeTypeAndValue **)NULL == rv ) {
      return (NSSPKIXAttributeTypeAndValue **)NULL;
    }
  } else {
    rv = rvOpt;
  }

  for( i = 0; i < limit; i++ ) {
    rv[i] = nssPKIXAttributeTypeAndValue_Duplicate(rdn->atav[i], arenaOpt);
    if( (NSSPKIXAttributeTypeAndValue *)NULL == rv[i] ) {
      goto loser;
    }
  }

  return rv;

 loser:
  for( i = 0; i < limit; i++ ) {
    NSSPKIXAttributeTypeAndValue *x = rv[i];
    if( (NSSPKIXAttributeTypeAndValue *)NULL == x ) {
      break;
    }
    (void)nssPKIXAttributeTypeAndValue_Destroy(x);
  }

  if( rv != rvOpt ) {
    nss_ZFreeIf(rv);
  }

  return (NSSPKIXAttributeTypeAndValue **)NULL;
}
