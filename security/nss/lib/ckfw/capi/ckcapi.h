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

#ifndef CKCAPI_H
#define CKCAPI_H 1

#ifdef DEBUG
static const char CKCAPI_CVS_ID[] = "@(#) $RCSfile: ckcapi.h,v $ $Revision: 1.1 $ $Date: 2005/11/04 23:44:19 $";
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

#include "WTypes.h"
#include "WinCrypt.h"

struct ckcapiRawObjectStr {
  CK_ULONG n;
  const CK_ATTRIBUTE_TYPE *types;
  const NSSItem *items;
};
typedef struct ckcapiRawObjectStr ckcapiRawObject;

struct ckcapiCertObjectStr {
  CK_OBJECT_CLASS objClass;
  PCCERT_CONTEXT  certContext;
  PRBool          hasID;
  NSSItem	  hashKey;
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
  void		  *privateKey;
  /* static data: to do, make these dynamic like privateKey */
  unsigned char	  idData[256];
  unsigned char   hashKeyData[128];
  unsigned char   derSerial[128];
  unsigned char   labelData[256];
};
typedef struct ckcapiCertObjectStr ckcapiCertObject;

typedef enum {
  ckcapiRaw,
  ckcapiCert
} ckcapiObjectType;

struct ckcapiInternalObjectStr {
  ckcapiObjectType type;
  union {
    ckcapiRawObject raw;
    ckcapiCertObject cert;
  } u;
  NSSCKMDObject mdObject;
};
typedef struct ckcapiInternalObjectStr ckcapiInternalObject;

NSS_EXTERN_DATA ckcapiInternalObject nss_ckcapi_data[];
NSS_EXTERN_DATA const PRUint32               nss_ckcapi_nObjects;

NSS_EXTERN_DATA const CK_VERSION   nss_ckcapi_CryptokiVersion;
NSS_EXTERN_DATA const NSSUTF8 *    nss_ckcapi_ManufacturerID;
NSS_EXTERN_DATA const NSSUTF8 *    nss_ckcapi_LibraryDescription;
NSS_EXTERN_DATA const CK_VERSION   nss_ckcapi_LibraryVersion;
NSS_EXTERN_DATA const NSSUTF8 *    nss_ckcapi_SlotDescription;
NSS_EXTERN_DATA const CK_VERSION   nss_ckcapi_HardwareVersion;
NSS_EXTERN_DATA const CK_VERSION   nss_ckcapi_FirmwareVersion;
NSS_EXTERN_DATA const NSSUTF8 *    nss_ckcapi_TokenLabel;
NSS_EXTERN_DATA const NSSUTF8 *    nss_ckcapi_TokenModel;
NSS_EXTERN_DATA const NSSUTF8 *    nss_ckcapi_TokenSerialNumber;

NSS_EXTERN_DATA const NSSCKMDInstance  nss_ckcapi_mdInstance;
NSS_EXTERN_DATA const NSSCKMDSlot      nss_ckcapi_mdSlot;
NSS_EXTERN_DATA const NSSCKMDToken     nss_ckcapi_mdToken;
NSS_EXTERN_DATA const NSSCKMDMechanism nss_ckcapi_mdMechanismRSA;

NSS_EXTERN NSSCKMDSession *
nss_ckcapi_CreateSession
(
  NSSCKFWSession *fwSession,
  CK_RV *pError
);

NSS_EXTERN NSSCKMDFindObjects *
nss_ckcapi_FindObjectsInit
(
  NSSCKFWSession *fwSession,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulAttributeCount,
  CK_RV *pError
);

NSS_EXTERN NSSCKMDObject *
nss_ckcapi_CreateMDObject
(
  NSSArena *arena,
  ckcapiInternalObject *io,
  CK_RV *pError
);

/*
 * unwrap a single DER value
 */
char *
nss_ckcapi_DERUnwrap
(
  char *src, 
  int size, 
  int *outSize, 
  char **next
);

const NSSItem *
nss_ckcapi_FetchAttribute(
  ckcapiInternalObject *io, 
  CK_ATTRIBUTE_TYPE type
);

/*
 * So everyone else in the worlds stores their bignum data MSB first, but not
 * Microsoft, we need to byte swap everything coming into and out of CAPI.
 */
void
ckcapi_ReverseData(NSSItem *item);

void
nss_ckcapi_DestroyInternalObject(ckcapiInternalObject *io);

#define NSS_CKCAPI_ARRAY_SIZE(x) ((sizeof (x))/(sizeof ((x)[0])))
 
#endif
