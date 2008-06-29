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
static const char BUILTINS_CVS_ID[] = "@(#) $RCSfile: builtins.h,v $ $Revision: 1.6 $ $Date: 2008/01/23 07:34:49 $";
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
  const CK_ATTRIBUTE_TYPE *types;
  const NSSItem *items;
  NSSCKMDObject mdObject;
};
typedef struct builtinsInternalObjectStr builtinsInternalObject;

extern       builtinsInternalObject nss_builtins_data[];
extern const PRUint32               nss_builtins_nObjects;

extern const CK_VERSION   nss_builtins_CryptokiVersion;
extern const CK_VERSION   nss_builtins_LibraryVersion;
extern const CK_VERSION   nss_builtins_HardwareVersion;
extern const CK_VERSION   nss_builtins_FirmwareVersion;

extern const NSSUTF8      nss_builtins_ManufacturerID[];
extern const NSSUTF8      nss_builtins_LibraryDescription[];
extern const NSSUTF8      nss_builtins_SlotDescription[];
extern const NSSUTF8      nss_builtins_TokenLabel[];
extern const NSSUTF8      nss_builtins_TokenModel[];
extern const NSSUTF8      nss_builtins_TokenSerialNumber[];

extern const NSSCKMDInstance nss_builtins_mdInstance;
extern const NSSCKMDSlot     nss_builtins_mdSlot;
extern const NSSCKMDToken    nss_builtins_mdToken;

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
