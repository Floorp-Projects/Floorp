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

#ifndef DEVM_H
#define DEVM_H

#ifdef DEBUG
static const char DEVM_CVS_ID[] = "@(#) $RCSfile: devm.h,v $ $Revision: 1.12 $ $Date: 2010/01/08 02:00:58 $";
#endif /* DEBUG */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

#ifndef DEV_H
#include "dev.h"
#endif /* DEV_H */

#ifndef DEVTM_H
#include "devtm.h"
#endif /* DEVTM_H */

PR_BEGIN_EXTERN_C

/* Shortcut to cryptoki API functions. */
#define CKAPI(epv) \
    ((CK_FUNCTION_LIST_PTR)(epv))

NSS_EXTERN void
nssDevice_AddRef
(
 struct nssDeviceBaseStr *device
);

NSS_EXTERN PRBool
nssDevice_Destroy
(
 struct nssDeviceBaseStr *device
);

NSS_EXTERN PRBool
nssModule_IsThreadSafe
(
  NSSModule *module
);

NSS_EXTERN PRBool
nssModule_IsInternal
(
  NSSModule *mod
);

NSS_EXTERN PRBool
nssModule_IsModuleDBOnly
(
  NSSModule *mod
);

NSS_EXTERN void *
nssModule_GetCryptokiEPV
(
  NSSModule *mod
);

NSS_EXTERN NSSSlot *
nssSlot_Create
(
  CK_SLOT_ID slotId,
  NSSModule *parent
);

NSS_EXTERN void *
nssSlot_GetCryptokiEPV
(
  NSSSlot *slot
);

NSS_EXTERN NSSToken *
nssToken_Create
(
  CK_SLOT_ID slotID,
  NSSSlot *peer
);

NSS_EXTERN void *
nssToken_GetCryptokiEPV
(
  NSSToken *token
);

NSS_EXTERN nssSession *
nssToken_GetDefaultSession
(
  NSSToken *token
);

NSS_EXTERN PRBool
nssToken_IsLoginRequired
(
  NSSToken *token
);

NSS_EXTERN void
nssToken_Remove
(
  NSSToken *token
);

NSS_EXTERN nssCryptokiObject *
nssCryptokiObject_Create
(
  NSSToken *t, 
  nssSession *session,
  CK_OBJECT_HANDLE h
);

NSS_EXTERN nssTokenObjectCache *
nssTokenObjectCache_Create
(
  NSSToken *token,
  PRBool cacheCerts,
  PRBool cacheTrust,
  PRBool cacheCRLs
);

NSS_EXTERN void
nssTokenObjectCache_Destroy
(
  nssTokenObjectCache *cache
);

NSS_EXTERN void
nssTokenObjectCache_Clear
(
  nssTokenObjectCache *cache
);

NSS_EXTERN PRBool
nssTokenObjectCache_HaveObjectClass
(
  nssTokenObjectCache *cache,
  CK_OBJECT_CLASS objclass
);

NSS_EXTERN nssCryptokiObject **
nssTokenObjectCache_FindObjectsByTemplate
(
  nssTokenObjectCache *cache,
  CK_OBJECT_CLASS objclass,
  CK_ATTRIBUTE_PTR otemplate,
  CK_ULONG otlen,
  PRUint32 maximumOpt,
  PRStatus *statusOpt
);

NSS_EXTERN PRStatus
nssTokenObjectCache_GetObjectAttributes
(
  nssTokenObjectCache *cache,
  NSSArena *arenaOpt,
  nssCryptokiObject *object,
  CK_OBJECT_CLASS objclass,
  CK_ATTRIBUTE_PTR atemplate,
  CK_ULONG atlen
);

NSS_EXTERN PRStatus
nssTokenObjectCache_ImportObject
(
  nssTokenObjectCache *cache,
  nssCryptokiObject *object,
  CK_OBJECT_CLASS objclass,
  CK_ATTRIBUTE_PTR ot,
  CK_ULONG otlen
);

NSS_EXTERN void
nssTokenObjectCache_RemoveObject
(
  nssTokenObjectCache *cache,
  nssCryptokiObject *object
);

/* XXX allows peek back into token */
NSS_EXTERN PRStatus
nssToken_GetCachedObjectAttributes
(
  NSSToken *token,
  NSSArena *arenaOpt,
  nssCryptokiObject *object,
  CK_OBJECT_CLASS objclass,
  CK_ATTRIBUTE_PTR atemplate,
  CK_ULONG atlen
);

/* PKCS#11 stores strings in a fixed-length buffer padded with spaces.  This
 * function gets the length of the actual string.
 */
NSS_EXTERN PRUint32
nssPKCS11String_Length
(
  CK_CHAR *pkcs11str, 
  PRUint32 bufLen
);

PR_END_EXTERN_C

#endif /* DEV_H */
