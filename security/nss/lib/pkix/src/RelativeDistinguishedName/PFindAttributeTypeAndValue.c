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
static const char CVS_ID[] = "@(#) $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/security/nss/lib/pkix/src/RelativeDistinguishedName/Attic/PFindAttributeTypeAndValue.c,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:14:25 $ $Name:  $";
#endif /* DEBUG */

#ifndef PKIX_H
#include "pkix.h"
#endif /* PKIX_H */

/*
 * nssPKIXRelativeDistinguishedName_FindAttributeTypeAndValue
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN
 *  NSS_ERROR_INVALID_PKIX_ATAV
 *  NSS_ERROR_NOT_FOUND
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value:
 *  The index of the specified attribute value upon success
 *  -1 upon failure.
 */

NSS_IMPLEMENT PRInt32
nssPKIXRelativeDistinguishedName_FindAttributeTypeAndValue
(
  NSSPKIXRelativeDistinguishedName *rdn,
  NSSPKIXAttributeTypeAndValue *atav
)
{
  PRUint32 i;
  NSSPKIXAttributeTypeAndValue **a;

#ifdef NSSDEBUG
  if( PR_SUCCESS != nssPKIXRelativeDistinguishedName_verifyPointer(rdn) ) {
    return -1;
  }

  if( PR_SUCCESS != nssPKIXAttributeTypeAndValue_verifyPointer(atav) ) {
    return -1;
  }
#endif /* NSSDEBUG */

  PR_ASSERT((NSSPKIXAttributeTypeAndValue **)NULL != rdn->atavs);
  if( (NSSPKIXAttributeTypeAndValue **)NULL == rdn->atavs ) {
    nss_SetError(NSS_ERROR_INTERNAL_ERROR);
    return -1;
  }

  for( i = 0, a = rdn->atavs; *a; a++, (i > 0x7fffffff) || i++ ) {
    if( PR_TRUE == nssPKIXAttributeTypeAndValue_Equal(*a, atav) ) {
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
