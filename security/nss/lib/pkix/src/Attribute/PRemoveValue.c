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
static const char CVS_ID[] = "@(#) $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/security/nss/lib/pkix/src/Attribute/Attic/PRemoveValue.c,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:12:17 $ $Name:  $";
#endif /* DEBUG */

#ifndef PKIX_H
#include "pkix.h"
#endif /* PKIX_H */

/*
 * nssPKIXAttribute_RemoveValue
 *
 * This routine removes the i'th attribute value of the set in the
 * specified NSSPKIXAttribute.  An attempt to remove the last value
 * will fail.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_AT_MINIMUM
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

PR_IMPLEMENET(PRStatus)
nssPKIXAttribute_RemoveValue
(
  NSSPKIXAttribute *a,
  PRInt32 i
)
{
  nssASN1Item **ip;

#ifdef NSSDEBUG
  if( PR_SUCCESS != nssPKIXAttribute_verifyPointer(a) ) {
    return PR_FAILURE;
  }
#endif /* NSSDEBUG */

  if( 0 == a->valuesCount ) {
    nss_pkix_Attribute_Count(a);
  }

  if( i < 0 ) {
    nss_SetError(NSS_ERROR_VALUE_OUT_OF_RANGE);
    return PR_FAILURE;
  }

  if( 1 == a->valuesCount ) {
    nss_SetError(NSS_ERROR_AT_MINIMUM);
    return PR_FAILURE;
  }

#ifdef PEDANTIC
  if( 0 == a->valuesCount ) {
    /* Too big.. but we can still remove one */
    nss_ZFreeIf(a->asn1values[i].data);
    for( ip = &a->asn1values[i]; *ip; ip++ ) {
      ip[0] = ip[1];
    }
  } else
#endif /* PEDANTIC */

  {
    nssASN1Item *si;
    PRUint32 end;

    if( i >= a->valueCount ) {
      nss_SetError(NSS_ERROR_VALUE_OUT_OF_RANGE);
      return PR_FAILURE;
    }

    end = a->valuesCount - 1;

    si = a->asn1values[i];
    a->asn1values[i] = a->asn1values[ end ];
    a->asn1values[ end ] = (nssASN1Item *)NULL;

    nss_ZFreeIf(si->data);
    nss_ZFreeIf(si);

    /* We could realloc down, but we know it's a no-op */
    a->valuesCount = end;
  }

  return nss_pkix_Attribute_Clear(a);
}
