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
 *
 * Definition of Security Module Data Structure. There is a separate data
 * structure for each loaded PKCS #11 module.
 */
#ifndef _SECMODT_H_
#define _SECMODT_H_ 1

#include "secoid.h"
#include "secasn1.h"

/* find a better home for these... */
extern const SEC_ASN1Template SECKEY_PointerToEncryptedPrivateKeyInfoTemplate[];
extern SEC_ASN1TemplateChooser NSS_Get_SECKEY_PointerToEncryptedPrivateKeyInfoTemplate;
extern const SEC_ASN1Template SECKEY_PrivateKeyInfoTemplate[];
extern SEC_ASN1TemplateChooser NSS_Get_SECKEY_PrivateKeyInfoTemplate;
extern const SEC_ASN1Template SECKEY_PointerToPrivateKeyInfoTemplate[];
extern SEC_ASN1TemplateChooser NSS_Get_SECKEY_PointerToPrivateKeyInfoTemplate;

/* PKCS11 needs to be included */
typedef struct SECMODModuleStr SECMODModule;
typedef struct SECMODModuleListStr SECMODModuleList;
typedef struct SECMODListLockStr SECMODListLock; /* defined in secmodi.h */
typedef struct PK11SlotInfoStr PK11SlotInfo; /* defined in secmodti.h */
typedef struct PK11PreSlotInfoStr PK11PreSlotInfo; /* defined in secmodti.h */
typedef struct PK11SymKeyStr PK11SymKey; /* defined in secmodti.h */
typedef struct PK11ContextStr PK11Context; /* defined in secmodti.h */
typedef struct PK11SlotListStr PK11SlotList;
typedef struct PK11SlotListElementStr PK11SlotListElement;
typedef struct PK11RSAGenParamsStr PK11RSAGenParams;
typedef unsigned long SECMODModuleID;
typedef struct PK11DefaultArrayEntryStr PK11DefaultArrayEntry;

struct SECMODModuleStr {
    PRArenaPool	*arena;
    PRBool	internal;	/* true of internally linked modules, false
				 * for the loaded modules */
    PRBool	loaded;		/* Set to true if module has been loaded */
    PRBool	isFIPS;		/* Set to true if module is finst internal */
    char	*dllName;	/* name of the shared library which implements
				 * this module */
    char	*commonName;	/* name of the module to display to the user */
    void	*library;	/* pointer to the library. opaque. used only by
				 * pk11load.c */
    void	*functionList; /* The PKCS #11 function table */
    void	*refLock;	/* only used pk11db.c */
    int		refCount;	/* Module reference count */
    PK11SlotInfo **slots;	/* array of slot points attatched to this mod*/
    int		slotCount;	/* count of slot in above array */
    PK11PreSlotInfo *slotInfo;	/* special info about slots default settings */
    int		slotInfoCount;  /* count */
    SECMODModuleID moduleID;	/* ID so we can find this module again */
    PRBool	isThreadSafe;
    unsigned long ssl[2];	/* SSL cipher enable flags */
    char	*libraryParams;  /* Module specific parameters */
    void *moduleDBFunc; /* function to return module configuration data*/
    SECMODModule *parent;	/* module that loaded us */
    PRBool	isCritical;	/* This module must load successfully */
    PRBool	isModuleDB;	/* this module has lists of PKCS #11 modules */
    PRBool	moduleDBOnly;	/* this module only has lists of PKCS #11 modules */
    int		trustOrder;	/* order for this module's certificate trust rollup */
    int		cipherOrder;	/* order for cipher operations */
};

struct SECMODModuleListStr {
    SECMODModuleList	*next;
    SECMODModule	*module;
};

struct PK11SlotListStr {
    PK11SlotListElement *head;
    PK11SlotListElement *tail;
    void *lock;
};

struct PK11SlotListElementStr {
    PK11SlotListElement *next;
    PK11SlotListElement *prev;
    PK11SlotInfo *slot;
    int refCount;
};

struct PK11RSAGenParamsStr {
    int keySizeInBits;
    unsigned long pe;
};

typedef enum {
     PK11CertListUnique = 0,
     PK11CertListUser = 1,
     PK11CertListRootUnique = 2,
     PK11CertListCA = 3
} PK11CertListType;

/*
 * Entry into the Array which lists all the legal bits for the default flags
 * in the slot, their definition, and the PKCS #11 mechanism the represent
 * Always Statically allocated. 
 */
struct PK11DefaultArrayEntryStr {
    char *name;
    unsigned long flag;
    unsigned long mechanism; /* this is a long so we don't include the 
			      * whole pkcs 11 world to use this header */
};


#define SECMOD_RSA_FLAG 	0x00000001L
#define SECMOD_DSA_FLAG 	0x00000002L
#define SECMOD_RC2_FLAG 	0x00000004L
#define SECMOD_RC4_FLAG 	0x00000008L
#define SECMOD_DES_FLAG 	0x00000010L
#define SECMOD_DH_FLAG	 	0x00000020L
#define SECMOD_FORTEZZA_FLAG	0x00000040L
#define SECMOD_RC5_FLAG		0x00000080L
#define SECMOD_SHA1_FLAG	0x00000100L
#define SECMOD_MD5_FLAG		0x00000200L
#define SECMOD_MD2_FLAG		0x00000400L
#define SECMOD_SSL_FLAG		0x00000800L
#define SECMOD_TLS_FLAG		0x00001000L
#define SECMOD_AES_FLAG 	0x00002000L
/* reserved bit for future, do not use */
#define SECMOD_RESERVED_FLAG    0X08000000L
#define SECMOD_FRIENDLY_FLAG	0x10000000L
#define SECMOD_RANDOM_FLAG	0x80000000L

/* need to make SECMOD and PK11 prefixes consistant. */
#define PK11_OWN_PW_DEFAULTS 0x20000000L
#define PK11_DISABLE_FLAG    0x40000000L

/* FAKE PKCS #11 defines */
#define CKM_FAKE_RANDOM       0x80000efeL
#define CKM_INVALID_MECHANISM 0xffffffffL
#define CKA_DIGEST            0x81000000L

/* Cryptographic module types */
#define SECMOD_EXTERNAL	0	/* external module */
#define SECMOD_INTERNAL 1	/* internal default module */
#define SECMOD_FIPS	2	/* internal fips module */

/*
 * What is the origin of a given Key. Normally this doesn't matter, but
 * the fortezza code needs to know if it needs to invoke the SSL3 fortezza
 * hack.
 */
typedef enum {
    PK11_OriginNULL = 0,	/* There is not key, it's a null SymKey */
    PK11_OriginDerive = 1,	/* Key was derived from some other key */
    PK11_OriginGenerated = 2,	/* Key was generated (also PBE keys) */
    PK11_OriginFortezzaHack = 3,/* Key was marked for fortezza hack */
    PK11_OriginUnwrap = 4	/* Key was unwrapped or decrypted */
} PK11Origin;

/* PKCS #11 disable reasons */
typedef enum {
    PK11_DIS_NONE = 0,
    PK11_DIS_USER_SELECTED = 1,
    PK11_DIS_COULD_NOT_INIT_TOKEN = 2,
    PK11_DIS_TOKEN_VERIFY_FAILED = 3,
    PK11_DIS_TOKEN_NOT_PRESENT = 4
} PK11DisableReasons;

/* function pointer type for password callback function.
 * This type is passed in to PK11_SetPasswordFunc() 
 */
typedef char *(*PK11PasswordFunc)(PK11SlotInfo *slot, PRBool retry, void *arg);
typedef PRBool (*PK11VerifyPasswordFunc)(PK11SlotInfo *slot, void *arg);
typedef PRBool (*PK11IsLoggedInFunc)(PK11SlotInfo *slot, void *arg);

/*
 * PKCS #11 key structures
 */

/*
** Attributes
*/
struct SECKEYAttributeStr {
    SECItem attrType;
    SECItem **attrValue;
};
typedef struct SECKEYAttributeStr SECKEYAttribute;

/*
** A PKCS#8 private key info object
*/
struct SECKEYPrivateKeyInfoStr {
    PLArenaPool *arena;
    SECItem version;
    SECAlgorithmID algorithm;
    SECItem privateKey;
    SECKEYAttribute **attributes;
};
typedef struct SECKEYPrivateKeyInfoStr SECKEYPrivateKeyInfo;

/*
** A PKCS#8 private key info object
*/
struct SECKEYEncryptedPrivateKeyInfoStr {
    PLArenaPool *arena;
    SECAlgorithmID algorithm;
    SECItem encryptedData;
};
typedef struct SECKEYEncryptedPrivateKeyInfoStr SECKEYEncryptedPrivateKeyInfo;

#endif /*_SECMODT_H_ */
