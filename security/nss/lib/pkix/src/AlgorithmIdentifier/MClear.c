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
static const char CVS_ID[] = "@(#) $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/security/nss/lib/pkix/src/AlgorithmIdentifier/Attic/MClear.c,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:06:07 $ $Name:  $";
#endif /* DEBUG */

#ifndef PKIXM_H
#include "pkixm.h"
#endif /* PKIXM_H */

/*
 * nss_pkix_AlgorithmIdentifier_Clear
 *
 * Wipes out cached data.
 */

NSS_IMPLEMENT PRStatus
nss_pkix_AlgorithmIdentifier_Clear
(
  NSSPKIXAlgorithmIdentifier *algid
)
{
#ifdef NSSDEBUG
  if( PR_SUCCESS != nssPKIXAlgorithmIdentifier_verifyPointer(algid) ) {
    return PR_FAILURE;
  }
#endif /* NSSDEBUG */

  if( (NSSBER *)NULL != algid->ber ) {
    nss_ZFreeIf(algid->ber->data);
    nss_ZFreeIf(algid->ber);
  }

  if( (NSSDER *)NULL != algid->der ) {
    nss_ZFreeIf(algid->der->data);
    nss_ZFreeIf(algid->der);
  }

  return PR_SUCCESS;
}
