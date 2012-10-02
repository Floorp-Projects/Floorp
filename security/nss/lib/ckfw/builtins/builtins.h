/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef DEBUG
static const char BUILTINS_CVS_ID[] = "@(#) $RCSfile: builtins.h,v $ $Revision: 1.7 $ $Date: 2012/04/25 14:49:29 $";
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
