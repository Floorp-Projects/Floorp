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
static const char CVS_ID[] = "@(#) $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/security/nss/lib/pkix/src/AttributeTypeAndValue/Attic/PEncode.c,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:13:28 $ $Name:  $";
#endif /* DEBUG */

#ifndef PKIX_H
#include "pkix.h"
#endif /* PKIX_H */

/*
 * nssPKIXAttributeTypeAndValue_Encode
 *
 * 
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATAV
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXAttributeTypeAndValue_Encode
(
  NSSPKIXAttributeTypeAndValue *atav,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
)
{
#ifdef NSSDEBUG
  if( PR_SUCCESS != nssPKIXAttributeTypeAndValue_verifyPointer(atav) ) {
    return PR_FAILURE;
  }

  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (NSSBER *)NULL;
    }
  }
#endif /* NSSDEBUG */

  switch( encoding ) {
  case NSSASN1BER:
  case NSSASN1DER:
    break;
  case NSSASN1CER:
  case NSSASN1LWER:
  case NSSASN1PER:
  default:
    nss_SetError(NSS_ERROR_UNSUPPORTED_ENCODING);
    return (NSSBER *)NULL;
  }

  /* For this item its DER is BER */

  if( (NSSDER *)NULL == atav->der ) {
    atav->der = nssASN1_EncodeDER(atav->arena, (NSSItem *)NULL, a,
                  nssPKIXAtributeTypeAndValue_template, NSSASN1DER);
    if( (NSSDER *)NULL == atav->der ) {
      return (NSSBER *)NULL;
    }
  }

  return nssItem_Duplicate(a->der, arenaOpt, rvOpt);
}
