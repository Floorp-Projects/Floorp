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
static const char CVS_ID[] = "@(#) $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/security/nss/lib/pkix/src/X520Name/Attic/MDoUTF8.c,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:14:38 $ $Name:  $";
#endif /* DEBUG */

#ifndef PKIXM_H
#include "pkixm.h"
#endif /* PKIXM_H */

/*
 * nss_pkix_X520Name_DoUTF8
 *
 */

NSS_IMPLEMENT PR_STATUS
nss_pkix_X520Name_DoUTF8
(
  NSSPKIXX520Name *name
)
{
#ifdef NSSDEBUG
  if( PR_SUCCESS != nssPKIXX520Name_verifyPointer(name) ) {
    return PR_FAILURE;
  }
#endif /* NSSDEBUG */

  if( (NSSUTF8 *)NULL == name->utf8 ) {
    PRUint8 tag = (*(PRUint8 *)name->string.data) & nssASN1_TAG_MASK;
    nssStringType type;
    void *data = (void *)&((PRUint8 *)name->string.data)[1];
    PRUint32 size = name->string.size-1;

    switch( tag ) {
    case nssASN1_TELETEX_STRING:
      type = nssStringType_TeletexString;
      break;
    case nssASN1_PRINTABLE_STRING:
      type = nssStringType_PrintableString;
      break;
    case nssASN1_UNIVERSAL_STRING:
      type = nssStringType_UniversalString;
      break;
    case nssASN1_BMP_STRING:
      type = nssStringType_BMPString;
      break;
    case nssASN1_UTF8_STRING:
      type = nssStringType_UTF8String;
      break;
    default:
      nss_SetError(NSS_ERROR_INVALID_BER);
      return PR_FAILURE;
    }

    name->utf8 = nssUTF8_Create(arenaOpt, type, data, size);
    if( (NSSUTF8 *)NULL == name->utf8 ) {
      return PR_FAILURE;
    }

    if( nssASN1_PRINTABLE_STRING == tag ) {
      name->wasPrintable = PR_TRUE;
    }
  }

  return PR_SUCCESS;
}
