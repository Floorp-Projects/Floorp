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
static const char BUILTINS_CVS_ID[] = "@(#) $RCSfile: builtins.h,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:43:47 $ $Name:  $";
#endif /* DEBUG */

#include "nssckmdt.h"
#include "nssckfw.h"

/*
 * I'm including this for access to the arena functions.
 * Looks like we should publish that API.
 */
#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

/*
 * This is where the Netscape extensions live, at least for now.
 */
#ifndef CKT_H
#include "ckt.h"
#endif /* CKT_H */

struct builtinsInternalObjectStr {
  CK_ULONG n;
  CK_ATTRIBUTE_TYPE *types;
  NSSItem *items;
};
typedef struct builtinsInternalObjectStr builtinsInternalObject;

NSS_EXTERN_DATA const builtinsInternalObject nss_builtins_data[];
NSS_EXTERN_DATA const PRUint32               nss_builtins_nObjects;

NSS_EXTERN_DATA const CK_VERSION   nss_builtins_CryptokiVersion;
NSS_EXTERN_DATA const NSSUTF8 *    nss_builtins_ManufacturerID;
NSS_EXTERN_DATA const NSSUTF8 *    nss_builtins_LibraryDescription;
NSS_EXTERN_DATA const CK_VERSION   nss_builtins_LibraryVersion;
NSS_EXTERN_DATA const NSSUTF8 *    nss_builtins_SlotDescription;
NSS_EXTERN_DATA const CK_VERSION   nss_builtins_HardwareVersion;
NSS_EXTERN_DATA const CK_VERSION   nss_builtins_FirmwareVersion;
NSS_EXTERN_DATA const NSSUTF8 *    nss_builtins_TokenLabel;
NSS_EXTERN_DATA const NSSUTF8 *    nss_builtins_TokenModel;
NSS_EXTERN_DATA const NSSUTF8 *    nss_builtins_TokenSerialNumber;

NSS_EXTERN_DATA const NSSCKMDInstance nss_builtins_mdInstance;
NSS_EXTERN_DATA const NSSCKMDSlot     nss_builtins_mdSlot;
NSS_EXTERN_DATA const NSSCKMDToken    nss_builtins_mdToken;

NSS_EXTERN NSSCKMDSession *
nss_builtins_CreateSession
(
  NSSCKFWSession *fwSession,
  CK_RV *pError
);

NSS_EXTERN NSSCKMDFindObjects *
nss_builtins_FindObjectsInit
(
  NSSCKFWSession *fwSession,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulAttributeCount,
  CK_RV *pError
);

NSS_EXTERN NSSCKMDObject *
nss_builtins_CreateMDObject
(
  NSSArena *arena,
  builtinsInternalObject *io,
  CK_RV *pError
);
