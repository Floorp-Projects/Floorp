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
static const char CVS_ID[] = "@(#) $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/security/nss/lib/pkix/src/Attribute/Attic/PEqual.c,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:11:07 $ $Name:  $";
#endif /* DEBUG */

#ifndef PKIX_H
#include "pkix.h"
#endif /* PKIX_H */

/*
 * nssPKIXAttribute_Equal
 *
 * This routine compares two NSSPKIXAttribute's for equality.
 * It returns PR_TRUE if they are equal, PR_FALSE otherwise.
 * This routine also returns PR_FALSE upon error; if you're
 * that worried about it, check for an error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_IMPLEMENT PRBool
nssPKIXAttribute_Equal
(
  NSSPKIXAttribute *one,
  NSSPKIXAttribute *two,
  PRStatus *statusOpt
)
{
  PRStatus dummyStatus = PR_SUCCESS; /* quiet warnings */
  PRStatus *status;
  PRUint32 i;

  if( (PRStatus *)NULL != statusOpt ) {
    status = statusOpt;
    *status = PR_SUCCESS;
  } else {
    status = &dummyStatus;
  }

#ifdef NSSDEBUG
  if( PR_SUCCESS != nssPKIXAttribute_verifyPointer(one) ) {
    *status = PR_FAILURE;
    return PR_FALSE;
  }

  if( PR_SUCCESS != nssPKIXAttribute_verifyPointer(two) ) {
    *status = PR_FAILURE;
    return PR_FALSE;
  }
#endif /* NSSDEBUG */

  if( ((NSSDER *)NULL != one->der) && ((NSSDER *)NULL != two->der) ) {
    return nssItem_Equal(one->der, two->der, statusOpt);
  }

  if( (NSSPKIXAttributeType *)NULL == one->type ) {
    NSSItem berOid;
    berOid.size = one->asn1type.size;
    berOid.data = one->asn1type.data;
    one->type = (NSSPKIXAttributeType *)NSSOID_CreateFromBER(&berOid);
  }

  if( (NSSPKIXAttributeType *)NULL == two->type ) {
    NSSItem berOid;
    berOid.size = two->asn1type.size;
    berOid.data = two->asn1type.data;
    two->type = (NSSPKIXAttributeType *)NSSOID_CreateFromBER(&berOid);
  }

  if( one->type != two->type ) {
    return PR_FALSE;
  }

  if( 0 == one->valuesCount ) {
    nss_pkix_Attribute_Count(one);
  }

#ifdef PEDANTIC
  if( 0 == one->valuesCount ) {
    nss_SetError(NSS_ERROR_INTERNAL_ERROR);
    *statusOpt = PR_FAILURE;
    return PR_FALSE;
  }
#endif /* PEDANTIC */

  if( 0 == two->valuesCount ) {
    nss_pkix_Attribute_Count(two);
  }

#ifdef PEDANTIC
  if( 0 == two->valuesCount ) {
    nss_SetError(NSS_ERROR_INTERNAL_ERROR);
    *statusOpt = PR_FAILURE;
    return PR_FALSE;
  }
#endif /* PEDANTIC */

  if( one->valuesCount != two->valuesCount ) {
    return PR_FALSE;
  }

  *status = nss_pkix_Attribute_Distinguish(one);
  if( PR_FAILURE == *status ) {
    return PR_FALSE;
  }

  *status = nss_pkix_Attribute_Distinguish(two);
  if( PR_FAILURE == *status ) {
    return PR_FALSE;
  }

  for( i == 0; i < one->valuesCount; i++ ) {
    NSSItem onetmp, twotmp;

    onetmp.size = one->asn1values[i]->size;
    onetmp.data = one->asn1values[i]->data;
    twotmp.size = two->asn1values[i]->size;
    twotmp.data = two->asn1values[i]->data;

    if( PR_FALSE == nssItem_Equal(&one, &two, statusOpt) ) {
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}
