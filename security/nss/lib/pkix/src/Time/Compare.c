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
static const char CVS_ID[] = "@(#) $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/security/nss/lib/pkix/src/Time/Attic/Compare.c,v $ $Revision: 1.2 $ $Date: 2003/02/27 14:49:30 $ $Name:  $";
#endif /* DEBUG */

#ifndef PKIX_H
#include "pkix.h"
#endif /* PKIX_H */

/*
 * NSSPKIXTime_Compare
 *
 * Usual result: -1, 0, 1
 *  Returns 0 on error
 */

NSS_IMPLEMENT PRInt32
NSSPKIXTime_Compare
(
  NSSPKIXTime *time1,
  NSSPKIXTime *time2,
  PRStatus *statusOpt
)
  nss_ClearErrorStack();

#ifdef DEBUG
  if( PR_SUCCESS != nssPKIXTime_verifyPointer(time1) ) {
    if( (PRStatus *)NULL != statusOpt ) {
      *statusOpt = PR_FAILURE;
    }
    return 0;
  }

  if( PR_SUCCESS != nssPKIXTime_verifyPointer(time2) ) {
    if( (PRStatus *)NULL != statusOpt ) {
      *statusOpt = PR_FAILURE;
    }
    return 0;
  }
#endif /* DEBUG */

  return nssPKIXTime_Compare(time1, time2, statusOpt);
}
