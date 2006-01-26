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

#ifndef CKMK_H
#define CKMK_H 1

#ifdef DEBUG
static const char CKMK_CVS_ID[] = "@(#) $RCSfile: ckmk.h,v $ $Revision: 1.2 $ $Date: 2006/01/26 23:21:39 $";
#endif /* DEBUG */

#include <Security/SecKeychainSearch.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecKeychain.h>
#include <Security/cssmtype.h>
#include <Security/cssmapi.h>
#include <Security/SecKey.h>
#include <Security/SecCertificate.h>

#define NTO

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

/*
 * statically defined raw objects. Allows us to data description objects
 * to this PKCS #11 module.
 */
struct ckmkRawObjectStr {
  CK_ULONG n;
  const CK_ATTRIBUTE_TYPE *types;
  const NSSItem *items;
};
typedef struct ckmkRawObjectStr ckmkRawObject;

/*
 * Key/Cert Items
 */
struct ckmkItemObjectStr {
  SecKeychainItemRef itemRef;
  SecItemClass    itemClass;
  PRBool          hasID;
  NSSItem	  modify;
  NSSItem	  private;
  NSSItem	  encrypt;
  NSSItem	  decrypt;
  NSSItem	  derive;
  NSSItem	  sign;
  NSSItem	  signRecover;
  NSSItem	  verify;
  NSSItem	  verifyRecover;
  NSSItem	  wrap;
  NSSItem	  unwrap;
  NSSItem	  label;
  NSSItem	  subject;
  NSSItem	  issuer;
  NSSItem	  serial;
  NSSItem	  derCert;
  NSSItem	  id;
  NSSItem	  modulus;
  NSSItem	  exponent;
  NSSItem	  privateExponent;
  NSSItem	  prime1;
  NSSItem	  prime2;
  NSSItem	  exponent1;
  NSSItem	  exponent2;
  NSSItem	  coefficient;
};
typedef struct ckmkItemObjectStr ckmkItemObject;

typedef enum {
  ckmkRaw,
  ckmkItem,
} ckmkObjectType;

/*
 * all the various types of objects are abstracted away in cobject and
 * cfind as ckmkInternalObjects.
 */
struct ckmkInternalObjectStr {
  ckmkObjectType type;
  union {
    ckmkRawObject  raw;
    ckmkItemObject item;
  } u;
  CK_OBJECT_CLASS objClass;
  NSSItem	  hashKey;
  unsigned char   hashKeyData[128];
  NSSCKMDObject mdObject;
};
typedef struct ckmkInternalObjectStr ckmkInternalObject;

/* our raw object data array */
NSS_EXTERN_DATA ckmkInternalObject nss_ckmk_data[];
NSS_EXTERN_DATA const PRUint32               nss_ckmk_nObjects;

NSS_EXTERN_DATA const CK_VERSION   nss_ckmk_CryptokiVersion;
NSS_EXTERN_DATA const NSSUTF8 *    nss_ckmk_ManufacturerID;
NSS_EXTERN_DATA const NSSUTF8 *    nss_ckmk_LibraryDescription;
NSS_EXTERN_DATA const CK_VERSION   nss_ckmk_LibraryVersion;
NSS_EXTERN_DATA const NSSUTF8 *    nss_ckmk_SlotDescription;
NSS_EXTERN_DATA const CK_VERSION   nss_ckmk_HardwareVersion;
NSS_EXTERN_DATA const CK_VERSION   nss_ckmk_FirmwareVersion;
NSS_EXTERN_DATA const NSSUTF8 *    nss_ckmk_TokenLabel;
NSS_EXTERN_DATA const NSSUTF8 *    nss_ckmk_TokenModel;
NSS_EXTERN_DATA const NSSUTF8 *    nss_ckmk_TokenSerialNumber;

NSS_EXTERN_DATA const NSSCKMDInstance  nss_ckmk_mdInstance;
NSS_EXTERN_DATA const NSSCKMDSlot      nss_ckmk_mdSlot;
NSS_EXTERN_DATA const NSSCKMDToken     nss_ckmk_mdToken;
NSS_EXTERN_DATA const NSSCKMDMechanism nss_ckmk_mdMechanismRSA;

NSS_EXTERN NSSCKMDSession *
nss_ckmk_CreateSession
(
  NSSCKFWSession *fwSession,
  CK_RV *pError
);

NSS_EXTERN NSSCKMDFindObjects *
nss_ckmk_FindObjectsInit
(
  NSSCKFWSession *fwSession,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulAttributeCount,
  CK_RV *pError
);

/*
 * Object Utilities
 */
NSS_EXTERN NSSCKMDObject *
nss_ckmk_CreateMDObject
(
  NSSArena *arena,
  ckmkInternalObject *io,
  CK_RV *pError
);

NSS_EXTERN NSSCKMDObject *
nss_ckmk_CreateObject
(
  NSSCKFWSession *fwSession,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulAttributeCount,
  CK_RV *pError
);

NSS_EXTERN const NSSItem *
nss_ckmk_FetchAttribute
(
  ckmkInternalObject *io, 
  CK_ATTRIBUTE_TYPE type,
  CK_RV *pError
);

NSS_EXTERN void
nss_ckmk_DestroyInternalObject
(
  ckmkInternalObject *io
);

unsigned char *
nss_ckmk_DERUnwrap
(
  unsigned char *src,
  int size,
  int *outSize,
  unsigned char **next
);

CK_ULONG
nss_ckmk_GetULongAttribute
(
  CK_ATTRIBUTE_TYPE type,
  CK_ATTRIBUTE *template,
  CK_ULONG templateSize,
  CK_RV *pError
);

#define NSS_CKMK_ARRAY_SIZE(x) ((sizeof (x))/(sizeof ((x)[0])))

#ifdef DEBUG
#define CKMK_MACERR(str,err) cssmPerror(str,err)
#else
#define CKMK_MACERR(str,err) 
#endif
 
#endif
