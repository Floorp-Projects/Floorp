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
static const char CVS_ID[] = "@(#) $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/security/nss/lib/pkix/src/Attribute/Attic/PFindValue.c,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:11:09 $ $Name:  $";
#endif /* DEBUG */

#ifndef PKIX_H
#include "pkix.h"
#endif /* PKIX_H */

/*
 * nssPKIXAttribute_FindValue
 *
 * This routine searches the set of attribute values in the specified
 * NSSPKIXAttribute for the provided data.  If an exact match is found,
 * then that value's index is returned.  If an exact match is not 
 * found, -1 is returned.  If there is more than one exact match, one
 * index will be returned.  {notes about unorderdness of SET, etc}
 * If the index may not be represented as an integer, an error is
 * indicated.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 *  NSS_ERROR_INVALID_ITEM
 *  NSS_ERROR_NOT_FOUND
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value
 *  The index of the specified attribute value upon success
 *  -1 upon failure.
 */

NSS_IMPLEMENT PRInt32
nssPKIXAttribute_FindValue
(
  NSSPKIXAttribute *a,
  NSSPKIXAttributeValue *attributeValue
)
{
  PRUint32 i;
  nssASN1Item **a;

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
    nss_SetError(NSS_ERROR_ASSERTION_FAILED);
    return -1;
  }

  for( i = 0, a = &a->asn1values[0]; *a; a++, (i > 0x7fffffff) || i++ ) {
    NSSItem tmp;

    tmp.size = (*a)->size;
    tmp.data = (*a)->data;

    if( PR_TRUE == nssItem_Equal(attributeValue, &tmp, (PRStatus *)NULL) ) {
      if( i > 0x7fffffff ) {
        nss_SetError(NSS_ERROR_VALUE_OUT_OF_RANGE);
        return -1;
      }
      return (PRInt32)i;
    }
  }

  nss_SetError(NSS_ERROR_NOT_FOUND);
  return -1;
}
