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
static const char CVS_ID[] = "@(#) $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/security/nss/lib/pkix/src/X520Name/Attic/PGetUTF8Encoding.c,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:14:40 $ $Name:  $";
#endif /* DEBUG */

#ifndef PKIX_H
#include "pkix.h"
#endif /* PKIX_H */

/*
 * nssPKIXX520Name_GetUTF8Encoding
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_X520_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSDER pointer upon success
 *  NULL upon failure
 */

NSS_IMPLEMENT NSSUTF8 *
nssPKIXX520Name_GetUTF8Encoding
(
  NSSPKIXX520Name *name,
  NSSArena *arenaOpt
)
{
#ifdef NSSDEBUG
  if( PR_SUCCESS != nssPKIXX520Name_verifyPointer(name) ) {
    return (NSSUTF8 *)NULL;
  }

  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (NSSUTF8 *)NULL;
    }
  }
#endif /* NSSDEBUG */

  if( (NSSUTF8 *)NULL == name->utf8 ) {
    if( PR_SUCCESS != nss_pkix_X520Name_DoUTF8(name) ) {
      return (NSSUTF8 *)NULL;
    }
  }
  
  return nssUTF8_Duplicate(rv->utf8, arenaOpt);
}
