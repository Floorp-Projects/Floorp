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
static const char CVS_ID[] = "@(#) $RCSfile: instance.c,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:43:49 $ $Name:  $";
#endif /* DEBUG */

#include "builtins.h"

/*
 * builtins/instance.c
 *
 * This file implements the NSSCKMDInstance object for the 
 * "builtin objects" cryptoki module.
 */

/*
 * NSSCKMDInstance methods
 */

static CK_ULONG
builtins_mdInstance_GetNSlots
(
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (CK_ULONG)1;
}

static CK_VERSION
builtins_mdInstance_GetCryptokiVersion
(
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  return nss_builtins_CryptokiVersion;
}

static NSSUTF8 *
builtins_mdInstance_GetManufacturerID
(
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (NSSUTF8 *)nss_builtins_ManufacturerID;
}

static NSSUTF8 *
builtins_mdInstance_GetLibraryDescription
(
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (NSSUTF8 *)nss_builtins_LibraryDescription;
}

static CK_VERSION
builtins_mdInstance_GetLibraryVersion
(
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  return nss_builtins_LibraryVersion;
}

static CK_RV
builtins_mdInstance_GetSlots
(
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  NSSCKMDSlot *slots[]
)
{
  slots[0] = (NSSCKMDSlot *)&nss_builtins_mdSlot;
  return CKR_OK;
}

NSS_IMPLEMENT_DATA const NSSCKMDInstance
nss_builtins_mdInstance = {
  (void *)NULL, /* etc */
  NULL, /* Initialize */
  NULL, /* Finalize */
  builtins_mdInstance_GetNSlots,
  builtins_mdInstance_GetCryptokiVersion,
  builtins_mdInstance_GetManufacturerID,
  builtins_mdInstance_GetLibraryDescription,
  builtins_mdInstance_GetLibraryVersion,
  NULL, /* ModuleHandlesSessionObjects -- defaults to false */
  builtins_mdInstance_GetSlots,
  NULL, /* WaitForSlotEvent */
  (void *)NULL /* null terminator */
};
