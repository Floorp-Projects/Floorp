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
 * Portions created by Red Hat, Inc, are Copyright (C) 2005
 *
 * Contributor(s):
 *   Bob Relyea (rrelyea@redhat.com)
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
static const char CVS_ID[] = "@(#) $RCSfile: cinst.c,v $ $Revision: 1.1 $ $Date: 2005/11/04 02:05:04 $";
#endif /* DEBUG */

#include "ckcapi.h"

/*
 * ckcapi/cinstance.c
 *
 * This file implements the NSSCKMDInstance object for the 
 * "capi" cryptoki module.
 */

/*
 * NSSCKMDInstance methods
 */

static CK_ULONG
ckcapi_mdInstance_GetNSlots
(
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (CK_ULONG)1;
}

static CK_VERSION
ckcapi_mdInstance_GetCryptokiVersion
(
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  return nss_ckcapi_CryptokiVersion;
}

static NSSUTF8 *
ckcapi_mdInstance_GetManufacturerID
(
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (NSSUTF8 *)nss_ckcapi_ManufacturerID;
}

static NSSUTF8 *
ckcapi_mdInstance_GetLibraryDescription
(
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (NSSUTF8 *)nss_ckcapi_LibraryDescription;
}

static CK_VERSION
ckcapi_mdInstance_GetLibraryVersion
(
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  return nss_ckcapi_LibraryVersion;
}

static CK_RV
ckcapi_mdInstance_GetSlots
(
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  NSSCKMDSlot *slots[]
)
{
  slots[0] = (NSSCKMDSlot *)&nss_ckcapi_mdSlot;
  return CKR_OK;
}

NSS_IMPLEMENT_DATA const NSSCKMDInstance
nss_ckcapi_mdInstance = {
  (void *)NULL, /* etc */
  NULL, /* Initialize */
  NULL, /* Finalize */
  ckcapi_mdInstance_GetNSlots,
  ckcapi_mdInstance_GetCryptokiVersion,
  ckcapi_mdInstance_GetManufacturerID,
  ckcapi_mdInstance_GetLibraryDescription,
  ckcapi_mdInstance_GetLibraryVersion,
  NULL, /* ModuleHandlesSessionObjects -- defaults to false */
  ckcapi_mdInstance_GetSlots,
  NULL, /* WaitForSlotEvent */
  (void *)NULL /* null terminator */
};
