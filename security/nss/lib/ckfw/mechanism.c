/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: mechanism.c,v $ $Revision: 1.3 $ $Date: 2005/01/20 02:25:45 $";
#endif /* DEBUG */

/*
 * mechanism.c
 *
 * This file implements the NSSCKFWMechanism type and methods.
 * These functions are currently stubs.
 */

#ifndef CK_T
#include "ck.h"
#endif /* CK_T */

/*
 * NSSCKFWMechanism
 *
 *  -- create/destroy --
 *  nssCKFWMechanism_Create
 *  nssCKFWMechanism_Destroy
 *
 *  -- implement public accessors --
 *  nssCKFWMechanism_GetMDMechanism
 *  nssCKFWMechanism_GetParameter
 *
 *  -- private accessors --
 *
 *  -- module fronts --
 *  nssCKFWMechanism_GetMinKeySize
 *  nssCKFWMechanism_GetMaxKeySize
 *  nssCKFWMechanism_GetInHardware
 */


struct NSSCKFWMechanismStr {
   void * dummy;
};

/*
 * nssCKFWMechanism_Create
 *
 */
NSS_IMPLEMENT NSSCKFWMechanism *
nssCKFWMechanism_Create
(
  void /* XXX fgmr */
)
{
  return (NSSCKFWMechanism *)NULL;
}

/*
 * nssCKFWMechanism_Destroy
 *
 */
NSS_IMPLEMENT CK_RV
nssCKFWMechanism_Destroy
(
  NSSCKFWMechanism *fwMechanism
)
{
   return CKR_OK;
}

/*
 * nssCKFWMechanism_GetMDMechanism
 *
 */

NSS_IMPLEMENT NSSCKMDMechanism *
nssCKFWMechanism_GetMDMechanism
(
  NSSCKFWMechanism *fwMechanism
)
{
    return NULL;
}

/*
 * nssCKFWMechanism_GetParameter
 *
 * XXX fgmr-- or as an additional parameter to the crypto ops?
 */
NSS_IMPLEMENT NSSItem *
nssCKFWMechanism_GetParameter
(
  NSSCKFWMechanism *fwMechanism
)
{
	return NULL;
}

/*
 * nssCKFWMechanism_GetMinKeySize
 *
 */
NSS_IMPLEMENT CK_ULONG
nssCKFWMechanism_GetMinKeySize
(
  NSSCKFWMechanism *fwMechanism
)
{
    return 0;
}

/*
 * nssCKFWMechanism_GetMaxKeySize
 *
 */
NSS_IMPLEMENT CK_ULONG
nssCKFWMechanism_GetMaxKeySize
(
  NSSCKFWMechanism *fwMechanism
)
{
   return 0;
}

/*
 * nssCKFWMechanism_GetInHardware
 *
 */
NSS_IMPLEMENT CK_BBOOL
nssCKFWMechanism_GetInHardware
(
  NSSCKFWMechanism *fwMechanism
)
{
    return PR_FALSE;
}

