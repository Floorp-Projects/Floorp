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
 * Portions created by Sun Microsystems, Inc. are Copyright (C) 2003
 * Sun Microsystems, Inc. All Rights Reserved. 
 *
 * Contributor(s): 
 *	Dr Vipul Gupta <vipul.gupta@sun.com>, Sun Microsystems Laboratories
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
#ifndef _KEYTHI_H_
#define _KEYTHI_H_ 1

#include "plarena.h"
#include "pkcs11t.h"
#include "secmodt.h"
#include "prclist.h"

typedef enum { 
    nullKey = 0, 
    rsaKey = 1, 
    dsaKey = 2, 
    fortezzaKey = 3,
    dhKey = 4, 
    keaKey = 5,
    ecKey = 6
} KeyType;

/*
** Template Definitions
**/

SEC_BEGIN_PROTOS
extern const SEC_ASN1Template SECKEY_RSAPublicKeyTemplate[];
extern const SEC_ASN1Template SECKEY_DSAPublicKeyTemplate[];
extern const SEC_ASN1Template SECKEY_DHPublicKeyTemplate[];
extern const SEC_ASN1Template SECKEY_DHParamKeyTemplate[];
extern const SEC_ASN1Template SECKEY_PQGParamsTemplate[];
extern const SEC_ASN1Template SECKEY_DSAPrivateKeyExportTemplate[];

/* Windows DLL accessor functions */
extern SEC_ASN1TemplateChooser NSS_Get_SECKEY_DSAPublicKeyTemplate;
extern SEC_ASN1TemplateChooser NSS_Get_SECKEY_RSAPublicKeyTemplate;
SEC_END_PROTOS


/*
** RSA Public Key structures
** member names from PKCS#1, section 7.1 
*/

struct SECKEYRSAPublicKeyStr {
    PRArenaPool * arena;
    SECItem modulus;
    SECItem publicExponent;
};
typedef struct SECKEYRSAPublicKeyStr SECKEYRSAPublicKey;


/*
** DSA Public Key and related structures
*/

struct SECKEYPQGParamsStr {
    PRArenaPool *arena;
    SECItem prime;    /* p */
    SECItem subPrime; /* q */
    SECItem base;     /* g */
    /* XXX chrisk: this needs to be expanded to hold j and validationParms (RFC2459 7.3.2) */
};
typedef struct SECKEYPQGParamsStr SECKEYPQGParams;

struct SECKEYDSAPublicKeyStr {
    SECKEYPQGParams params;
    SECItem publicValue;
};
typedef struct SECKEYDSAPublicKeyStr SECKEYDSAPublicKey;


/*
** Diffie-Hellman Public Key structure
** Structure member names suggested by PKCS#3.
*/
struct SECKEYDHParamsStr {
    PRArenaPool * arena;
    SECItem prime; /* p */
    SECItem base; /* g */
};
typedef struct SECKEYDHParamsStr SECKEYDHParams;

struct SECKEYDHPublicKeyStr {
    PRArenaPool * arena;
    SECItem prime;
    SECItem base;
    SECItem publicValue;
};
typedef struct SECKEYDHPublicKeyStr SECKEYDHPublicKey;

/*
** Elliptic curve Public Key structure
** The PKCS#11 layer needs DER encoding of ANSI X9.62
** parameters value
*/
typedef SECItem SECKEYECParams;

struct SECKEYECPublicKeyStr {
    SECKEYECParams DEREncodedParams;
    int     size;             /* size in bits */
    SECItem publicValue;      /* encoded point */
    /* XXX Even though the PKCS#11 interface takes encoded parameters,
     * we may still wish to decode them above PKCS#11 for things like
     * printing key information. For named curves, which is what
     * we initially support, we ought to have the curve name at the
     * very least.
     */
};
typedef struct SECKEYECPublicKeyStr SECKEYECPublicKey;

/*
** FORTEZZA Public Key structures
*/
struct SECKEYFortezzaPublicKeyStr {
    int      KEAversion;
    int      DSSversion;
    unsigned char    KMID[8];
    SECItem clearance;
    SECItem KEApriviledge;
    SECItem DSSpriviledge;
    SECItem KEAKey;
    SECItem DSSKey;
    SECKEYPQGParams params;
    SECKEYPQGParams keaParams;
};
typedef struct SECKEYFortezzaPublicKeyStr SECKEYFortezzaPublicKey;

struct SECKEYDiffPQGParamsStr {
    SECKEYPQGParams DiffKEAParams;
    SECKEYPQGParams DiffDSAParams;
};
typedef struct SECKEYDiffPQGParamsStr SECKEYDiffPQGParams;

struct SECKEYPQGDualParamsStr {
    SECKEYPQGParams CommParams;
    SECKEYDiffPQGParams DiffParams;
};
typedef struct SECKEYPQGDualParamsStr SECKEYPQGDualParams;

struct SECKEYKEAParamsStr {
    PLArenaPool *arena;
    SECItem hash;
};
typedef struct SECKEYKEAParamsStr SECKEYKEAParams;
 
struct SECKEYKEAPublicKeyStr {
    SECKEYKEAParams params;
    SECItem publicValue;
};
typedef struct SECKEYKEAPublicKeyStr SECKEYKEAPublicKey;

/*
** A Generic  public key object.
*/
struct SECKEYPublicKeyStr {
    PLArenaPool *arena;
    KeyType keyType;
    PK11SlotInfo *pkcs11Slot;
    CK_OBJECT_HANDLE pkcs11ID;
    union {
        SECKEYRSAPublicKey rsa;
	SECKEYDSAPublicKey dsa;
	SECKEYDHPublicKey  dh;
        SECKEYKEAPublicKey kea;
        SECKEYFortezzaPublicKey fortezza;
	SECKEYECPublicKey  ec;
    } u;
};
typedef struct SECKEYPublicKeyStr SECKEYPublicKey;

/*
** A generic key structure
*/ 
struct SECKEYPrivateKeyStr {
    PLArenaPool *arena;
    KeyType keyType;
    PK11SlotInfo *pkcs11Slot;	/* pkcs11 slot this key lives in */
    CK_OBJECT_HANDLE pkcs11ID;  /* ID of pkcs11 object */
    PRBool pkcs11IsTemp;	/* temp pkcs11 object, delete it when done */
    void *wincx;		/* context for errors and pw prompts */
};
typedef struct SECKEYPrivateKeyStr SECKEYPrivateKey;

/* Despite the name, this struct isn't used by any pkcs5 code.
** It's used by pkcs7 and pkcs12 code.
*/
typedef struct {
    SECItem *pwitem;
    PK11SymKey *key;
    PK11SlotInfo *slot;
    void *wincx;
} SEC_PKCS5KeyAndPassword;

typedef struct {
    PRCList links;
    SECKEYPrivateKey *key;
} SECKEYPrivateKeyListNode;

typedef struct {
    PRCList list;
    PRArenaPool *arena;
} SECKEYPrivateKeyList;

typedef struct {
    PRCList links;
    SECKEYPublicKey *key;
} SECKEYPublicKeyListNode;

typedef struct {
    PRCList list;
    PRArenaPool *arena;
} SECKEYPublicKeyList;
#endif /* _KEYTHI_H_ */

