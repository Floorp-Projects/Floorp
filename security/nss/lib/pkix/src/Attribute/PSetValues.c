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
static const char CVS_ID[] = "@(#) $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/security/nss/lib/pkix/src/Attribute/Attic/PSetValues.c,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:12:32 $ $Name:  $";
#endif /* DEBUG */

#ifndef PKIX_H
#include "pkix.h"
#endif /* PKIX_H */

/*
 * nssPKIXAttribute_SetValues
 *
 * This routine sets all of the values of the specified 
 * NSSPKIXAttribute to the values in the specified NSSItem array.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 *  NSS_ERROR_INVALID_POINTER
 *  NSS_ERROR_ARRAY_TOO_SMALL
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_IMPLEMENT PRStatus
nssPKIXAttribute_SetValues
(
  NSSPKIXAttribute *a,
  NSSPKIXAttributeValue values[],
  PRInt32 count
)
{
  nssASN1Item **ip;
  nssASN1Item *newarray;
  PRUint32 i;
  nssArenaMark *mark;

#ifdef NSSDEBUG
  if( PR_SUCCESS != nssPKIXAttribute_verifyPointer(a) ) {
    return PR_FAILURE;
  }

  if( (NSSPKIXAttributeValue *)NULL == values ) {
    nss_SetError(NSS_ERROR_INVALID_POINTER);
    return PR_FAILURE;
  }

  if( count < 1 ) {
    nss_SetError(NSS_ERROR_ARRAY_TOO_SMALL);
    return PR_FAILURE;
  }
#endif /* NSSDEBUG */

  PR_ASSERT((nssASN1Item **)NULL != a->asn1values);
  if( (nssASN1Item **)NULL == a->asn1values ) {
    nss_SetError(NSS_ERROR_ASSERTION_FAILED);
    return PR_FAILURE;
  }

  mark = nssArena_Mark(a->arena);
  if( (nssArenaMark *)NULL == mark ) {
    return PR_FAILURE;
  }

  newarray = nss_ZNEWARRAY(a->arena, nssASN1Item *, count);
  if( (nssASN1Item *)NULL == newarray ) {
    return PR_FAILURE;
  }

  for( i = 0; i < count; i++ ) {
    NSSItem tmp;

    newarray[i] = nss_ZNEW(a->arena, nssASN1Item);
    if( (nssASN1Item *)NULL == newarray[i] ) {
      goto loser;
    }

    if( (NSSItem *)NULL == nssItem_Duplicate(&values[i], a->arena, &tmp) ) {
      goto loser;
    }

    newarray[i]->size = tmp.size;
    newarray[i]->data = tmp.data;
  }

  for( ip = &a->asn1values[0]; *ip; ip++ ) {
    nss_ZFreeIf((*ip)->data);
    nss_ZFreeIf(*ip);
  }

  nss_ZFreeIf(a->asn1values);
  
  a->asn1values = newarray;
  a->valuesCount = count;

  (void)nss_pkix_Attribute_Clear(a);
  return nssArena_Unmark(a->arena, mark);

 loser:
  (void)nssArena_Release(a->arena, mark);
  return PR_FAILURE;
}
