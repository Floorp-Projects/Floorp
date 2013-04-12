/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CKCAPI_H
#define CKCAPI_H 1

#ifdef DEBUG
static const char CKCAPI_CVS_ID[] = "@(#) $RCSfile$ $Revision$ $Date$";
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

#include "wtypes.h"
#include "wincrypt.h"

/*
 * statically defined raw objects. Allows us to data description objects
 * to this PKCS #11 module.
 */
struct ckcapiRawObjectStr {
  CK_ULONG n;
  const CK_ATTRIBUTE_TYPE *types;
  const NSSItem *items;
};
typedef struct ckcapiRawObjectStr ckcapiRawObject;


/*
 * common values needed for both bare keys and cert referenced keys.
 */
struct ckcapiKeyParamsStr {
  NSSItem	  modulus;
  NSSItem	  exponent;
  NSSItem	  privateExponent;
  NSSItem	  prime1;
  NSSItem	  prime2;
  NSSItem	  exponent1;
  NSSItem	  exponent2;
  NSSItem	  coefficient;
  unsigned char   publicExponentData[sizeof(CK_ULONG)];
  void		  *privateKey;
  void		  *pubKey;
};
typedef struct ckcapiKeyParamsStr ckcapiKeyParams;

/*
 * Key objects. Handles bare keys which do not yet have certs associated
 * with them. These are usually short lived, but may exist for several days
 * while the CA is issuing the certificate.
 */
struct ckcapiKeyObjectStr {
  CRYPT_KEY_PROV_INFO provInfo;
  char            *provName;
  char            *containerName;
  HCRYPTPROV      hProv;
  ckcapiKeyParams key;
};
typedef struct ckcapiKeyObjectStr ckcapiKeyObject;

/*
 * Certificate and certificate referenced keys.
 */
struct ckcapiCertObjectStr {
  PCCERT_CONTEXT  certContext;
  PRBool          hasID;
  const char	  *certStore;
  NSSItem	  label;
  NSSItem	  subject;
  NSSItem	  issuer;
  NSSItem	  serial;
  NSSItem	  derCert;
  ckcapiKeyParams key;
  unsigned char   *labelData;
  /* static data: to do, make this dynamic like labelData */
  unsigned char   derSerial[128];
};
typedef struct ckcapiCertObjectStr ckcapiCertObject;

typedef enum {
  ckcapiRaw,
  ckcapiCert,
  ckcapiBareKey
} ckcapiObjectType;

/*
 * all the various types of objects are abstracted away in cobject and
 * cfind as ckcapiInternalObjects.
 */
struct ckcapiInternalObjectStr {
  ckcapiObjectType type;
  union {
    ckcapiRawObject  raw;
    ckcapiCertObject cert;
    ckcapiKeyObject  key;
  } u;
  CK_OBJECT_CLASS objClass;
  NSSItem	  hashKey;
  NSSItem	  id;
  void		  *idData;
  unsigned char   hashKeyData[128];
  NSSCKMDObject mdObject;
};
typedef struct ckcapiInternalObjectStr ckcapiInternalObject;

/* our raw object data array */
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

/*
 * Object Utilities
 */
NSS_EXTERN NSSCKMDObject *
nss_ckcapi_CreateMDObject
(
  NSSArena *arena,
  ckcapiInternalObject *io,
  CK_RV *pError
);

NSS_EXTERN NSSCKMDObject *
nss_ckcapi_CreateObject
(
  NSSCKFWSession *fwSession,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulAttributeCount,
  CK_RV *pError
);

NSS_EXTERN const NSSItem *
nss_ckcapi_FetchAttribute
(
  ckcapiInternalObject *io, 
  CK_ATTRIBUTE_TYPE type
);

NSS_EXTERN void
nss_ckcapi_DestroyInternalObject
(
  ckcapiInternalObject *io
);

NSS_EXTERN CK_RV
nss_ckcapi_FetchKeyContainer
(
  ckcapiInternalObject *iKey,
  HCRYPTPROV  *hProv,
  DWORD       *keySpec,
  HCRYPTKEY   *hKey
);

/*
 * generic utilities
 */

/*
 * So everyone else in the worlds stores their bignum data MSB first, but not
 * Microsoft, we need to byte swap everything coming into and out of CAPI.
 */
void
ckcapi_ReverseData
(
  NSSItem *item
);

/*
 * unwrap a single DER value
 */
unsigned char *
nss_ckcapi_DERUnwrap
(
  unsigned char *src, 
  unsigned int size, 
  unsigned int *outSize, 
  unsigned char **next
);

/*
 * Return the size in bytes of a wide string
 */
int 
nss_ckcapi_WideSize
(
  LPCWSTR wide
);

/*
 * Covert a Unicode wide character string to a UTF8 string
 */
char *
nss_ckcapi_WideToUTF8
(
  LPCWSTR wide 
);

/*
 * Return a Wide String duplicated with nss allocated memory.
 */
LPWSTR
nss_ckcapi_WideDup
(
  LPCWSTR wide
);

/*
 * Covert a UTF8 string to Unicode wide character
 */
LPWSTR
nss_ckcapi_UTF8ToWide
(
  char *buf
);


NSS_EXTERN PRUint32
nss_ckcapi_collect_all_certs(
  CK_ATTRIBUTE_PTR pTemplate, 
  CK_ULONG ulAttributeCount, 
  ckcapiInternalObject ***listp,
  PRUint32 *sizep,
  PRUint32 count,
  CK_RV *pError
);

#define NSS_CKCAPI_ARRAY_SIZE(x) ((sizeof (x))/(sizeof ((x)[0])))
 
#endif
