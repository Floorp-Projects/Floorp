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
static const char CVS_ID[] = "@(#) $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/security/nss/lib/pkix/src/Attribute/Attic/PAddValue.c,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:10:30 $ $Name:  $";
#endif /* DEBUG */

#ifndef PKIX_H
#include "pkix.h"
#endif /* PKIX_H */

/*
 * nssPKIXAttribute_AddValue
 *
 * This routine adds the specified attribute value to the set in
 * the specified NSSPKIXAttribute.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 *  NSS_ERROR_INVALID_ITEM
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_IMPLEMENT PRStatus
nssPKIXAttribute_AddValue
(
  NSSPKIXAttribute *a,
  NSSPKIXAttributeValue *value
)
{
  PRUint32 newcount;
  nssASN1Item **new_asn1_items = (nssASN1Item **)NULL;
  NSSItem tmp;

#ifdef NSSDEBUG
  if( PR_SUCCESS != nssPKIXAttribute_verifyPointer(a) ) {
    return PR_FAILURE;
  }

  if( PR_SUCCESS != nssItem_verifyPointer(value) ) {
    return PR_FAILURE;
  }
#endif /* NSSDEBUG */

  PR_ASSERT((nssASN1Item **)NULL != a->asn1values);
  if( (nssASN1Item **)NULL == a->asn1values ) {
    nss_SetError(NSS_ERROR_INTERNAL_ERROR);
    return PR_FAILURE;
  }

  if( 0 == a->valuesCount ) {
    PRUint32 i;

    for( i = 0; i < 0xFFFFFFFF; i++ ) {
      if( (nssASN1Item *)NULL == a->asn1values[i] ) {
        break;
      }
    }

#ifdef PEDANTIC
    if( 0xFFFFFFFF == i ) {
      nss_SetError(NSS_ERROR_INTERNAL_ERROR);
      /* Internal error is that we don't handle them this big */
      return PR_FAILURE;
    }
#endif /* PEDANTIC */

    a->valuesCount = i;
  }

  newcount = a->valuesCount + 1;
  /* Check newcount for a rollover. */

  /* Remember that our asn1values array is NULL-terminated */
  new_asn1_items = (nssASN1Item **)nss_ZRealloc(a->asn1values,
                     ((newcount+1) * sizeof(nssASN1Item *)));
  if( (nssASN1Item **)NULL == new_asn1_items ) {
    goto loser;
  }

  new_asn1_items[ a->valuesCount ] = nss_ZNEW(a->arena, nssASN1Item);
  if( (nssASN1Item *)NULL == new_asn1_items[ a->valuesCount ] ) {
    goto loser;
  }

  if( (NSSItem *)NULL == nssItem_Duplicate(value, a->arena, &tmp) ) {
    goto loser;
  }

  new_asn1_items[ a->valuesCount ]->size = tmp.size;
  new_asn1_items[ a->valuesCount ]->data = tmp.data;

  a->valuesCount++;
  a->asn1values = new_asn1_items;

  return nss_pkix_Attribute_Clear(a);

 loser:
  if( (nssASN1Item **)NULL != new_asn1_items ) {
    nss_ZFreeIf(new_asn1_items[ newcount-1 ]);
    /* We could realloc back down, but we actually know it's a no-op */
    a->asn1values = new_asn1_items;
  }

  return PR_FAILURE;
}
