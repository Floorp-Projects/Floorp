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
/*
 * Internal data structures and functions used by pkcs11.c
 */
#ifndef _PKCS11I_H_
#define _PKCS11I_H_ 1

#include "nssilock.h"
#include "seccomon.h"
#include "secoidt.h"
#include "keytlow.h"
#include "pkcs11t.h"

#define PKCS11_USE_THREADS

#define NO_ARENA
#define MAX_OBJS_ATTRS 45
#define ATTR_SPACE 50  /* hold up to a SSL premaster secret */


#ifdef PKCS11_USE_THREADS
#define PK11_USE_THREADS(x) x
#else
#define PK11_USE_THREADS(x) 
#endif

/* define typedefs, double as forward declarations as well */
typedef struct PK11AttributeStr PK11Attribute;
typedef struct PK11ObjectListStr PK11ObjectList;
typedef struct PK11ObjectListElementStr PK11ObjectListElement;
typedef struct PK11ObjectStr PK11Object;
typedef struct PK11SessionStr PK11Session;
typedef struct PK11SlotStr PK11Slot;
typedef struct PK11SessionContextStr PK11SessionContext;
typedef struct PK11SearchResultsStr PK11SearchResults;
typedef struct PK11HashVerifyInfoStr PK11HashVerifyInfo;
typedef struct PK11HashSignInfoStr PK11HashSignInfo;
typedef struct PK11SSLMACInfoStr PK11SSLMACInfo;

/* define function pointer typdefs for pointer tables */
typedef void (*PK11Destroy)(void *, PRBool);
typedef void (*PK11Begin)(void *);
typedef SECStatus (*PK11Cipher)(void *,void *,unsigned int *,unsigned int,
					void *, unsigned int);
typedef SECStatus (*PK11Verify)(void *,void *,unsigned int,void *,unsigned int);
typedef void (*PK11Hash)(void *,void *,unsigned int);
typedef void (*PK11End)(void *,void *,unsigned int *,unsigned int);
typedef void (*PK11Free)(void *);

/*
 * these are data base storage hashes, not cryptographic hashes.. The define
 * the effective size of the various object hash tables
 */
#define ATTRIBUTE_HASH_SIZE 32
#define SESSION_OBJECT_HASH_SIZE 32
#define TOKEN_OBJECT_HASH_SIZE 1024
#define SESSION_HASH_SIZE 512
#define MAX_KEY_LEN 256
#define MAX_OBJECT_LIST_SIZE 800

/* Value to tell if an attribute is modifiable or not.
 *    NEVER: attribute is only set on creation.
 *    ONCOPY: attribute is set on creation and can only be changed on copy.
 *    SENSITIVE: attribute can only be changed to TRUE.
 *    ALWAYS: attribute can always be changed.
 */
typedef enum {
	PK11_NEVER = 0,
	PK11_ONCOPY = 1,
	PK11_SENSITIVE = 2,
	PK11_ALWAYS = 3
} PK11ModifyType;

/*
 * Free Status Enum... tell us more information when we think we're
 * deleting an object.
 */
typedef enum {
	PK11_DestroyFailure,
	PK11_Destroyed,
	PK11_Busy
} PK11FreeStatus;

/*
 * attribute values of an object.
 */
struct PK11AttributeStr {
    PK11Attribute  	*next;
    PK11Attribute  	*prev;
#ifdef REF_COUNT_ATTRIBUTE
    int 		refCount;
    PZLock 		*refLock;
#endif
    /*must be called handle to make pk11queue_find work */
    CK_ATTRIBUTE_TYPE	handle;
    CK_ATTRIBUTE 	attrib;
#ifdef NO_ARENA
    unsigned char space[ATTR_SPACE];
#endif
};


/*
 * doubly link list of objects
 */
struct PK11ObjectListStr {
    PK11ObjectList *next;
    PK11ObjectList *prev;
    PK11Object	   *parent;
};

/*
 * PKCS 11 crypto object structure
 */
struct PK11ObjectStr {
    PK11Object *next;
    PK11Object *prev;
    PK11ObjectList sessionList;
    CK_OBJECT_HANDLE handle;
#ifdef NO_ARENA
    int nextAttr;
#else
    PLArenaPool	*arena;
#endif
    int refCount;
    PZLock 		*refLock;
    PZLock		*attributeLock;
    PK11Session   	*session;
    PK11Slot	   	*slot;
    CK_OBJECT_CLASS 	objclass;
    void 		*objectInfo;
    PK11Free 		infoFree;
    char		*label;
    PRBool		inDB;
    PRBool		wasDerived;
    PK11Attribute 	*head[ATTRIBUTE_HASH_SIZE];
#ifdef NO_ARENA
    PK11Attribute	attrList[MAX_OBJS_ATTRS];
#endif
};

/*
 * struct to deal with a temparary list of objects
 */
struct PK11ObjectListElementStr {
    PK11ObjectListElement	*next;
    PK11Object 			*object;
};

/*
 * Area to hold Search results
 */
struct PK11SearchResultsStr {
    CK_OBJECT_HANDLE	*handles;
    int			size;
    int			index;
};


/* 
 * the universal crypto/hash/sign/verify context structure
 */
typedef enum {
    PK11_ENCRYPT,
    PK11_DECRYPT,
    PK11_HASH,
    PK11_SIGN,
    PK11_SIGN_RECOVER,
    PK11_VERIFY,
    PK11_VERIFY_RECOVER
} PK11ContextType;


#define PK11_MAX_BLOCK_SIZE 16
/* currently SHA1 is the biggest hash length */
#define PK11_MAX_MAC_LENGTH 20
#define PK11_INVALID_MAC_SIZE 0xffffffff

struct PK11SessionContextStr {
    PK11ContextType	type;
    PRBool		multi; 		/* is multipart */
    PRBool		doPad; 		/* use PKCS padding for block ciphers */
    unsigned int	blockSize; 	/* blocksize for padding */
    unsigned int	padDataLength; 	/* length of the valid data in padbuf */
    unsigned char	padBuf[PK11_MAX_BLOCK_SIZE];
    unsigned char	macBuf[PK11_MAX_BLOCK_SIZE];
    CK_ULONG		macSize;	/* size of a general block cipher mac*/
    void		*cipherInfo;
    void		*hashInfo;
    unsigned int	cipherInfoLen;
    CK_MECHANISM_TYPE	currentMech;
    PK11Cipher		update;
    PK11Hash		hashUpdate;
    PK11End		end;
    PK11Destroy		destroy;
    PK11Destroy		hashdestroy;
    PK11Verify		verify;
    unsigned int	maxLen;
};

/*
 * Sessions (have objects)
 */
struct PK11SessionStr {
    PK11Session        *next;
    PK11Session        *prev;
    CK_SESSION_HANDLE	handle;
    int			refCount;
    PZLock 		*refLock;
    PZLock		*objectLock;
    int			objectIDCount;
    CK_SESSION_INFO	info;
    CK_NOTIFY		notify;
    CK_VOID_PTR		appData;
    PK11Slot		*slot;
    PK11SearchResults	*search;
    PK11SessionContext	*enc_context;
    PK11SessionContext	*hash_context;
    PK11SessionContext	*sign_context;
    PK11ObjectList	*objects[1];
};

/*
 * slots (have sessions and objects)
 */
struct PK11SlotStr {
    CK_SLOT_ID		slotID;
    PZLock		*sessionLock;
    PZLock		*objectLock;
    SECItem		*password;
    PRBool		hasTokens;
    PRBool		isLoggedIn;
    PRBool		ssoLoggedIn;
    PRBool		needLogin;
    PRBool		DB_loaded;
    int			sessionIDCount;
    int			sessionCount;
    int			rwSessionCount;
    int			tokenIDCount;
    PK11Object		*tokObjects[TOKEN_OBJECT_HASH_SIZE];
    PK11Session		*head[SESSION_HASH_SIZE];
};

/*
 * special joint operations Contexts
 */
struct PK11HashVerifyInfoStr {
    SECOidTag   	hashOid;
    SECKEYLowPublicKey	*key;
};

struct PK11HashSignInfoStr {
    SECOidTag   	hashOid;
    SECKEYLowPrivateKey	*key;
};

/* context for the Final SSLMAC message */
struct PK11SSLMACInfoStr {
    void 		*hashContext;
    PK11Begin		begin;
    PK11Hash		update;
    PK11End		end;
    CK_ULONG		macSize;
    int			padSize;
    unsigned char	key[MAX_KEY_LEN];
    unsigned int	keySize;
};

/*
 * session handle modifiers
 */
#define PK11_PRIVATE_KEY_FLAG	0x80000000L
#define PK11_FIPS_FLAG		0x40000000L

/*
 * object handle modifiers
 */
#define PK11_TOKEN_MASK		0x80000000L
#define PK11_TOKEN_MAGIC	0x80000000L
#define PK11_TOKEN_TYPE_MASK	0x70000000L
#define PK11_TOKEN_TYPE_CERT	0x00000000L
#define PK11_TOKEN_TYPE_PRIV	0x10000000L
#define PK11_TOKEN_TYPE_PUB	0x20000000L

/* how big a password/pin we can deal with */
#define PK11_MAX_PIN	255

/* slot ID's */
#define NETSCAPE_SLOT_ID 1
#define PRIVATE_KEY_SLOT_ID 2
#define FIPS_SLOT_ID 3

/* slot helper macros */
#define pk11_SlotFromSession(sp) ((sp)->slot)
#define pk11_isToken(id) (((id) & PK11_TOKEN_MASK) == PK11_TOKEN_MAGIC)

/* queueing helper macros */
#define pk11_hash(value,size) ((value) & (size-1))/*size must be a power of 2*/
#define pk11queue_add(element,id,head,hash_size) \
	{ int tmp = pk11_hash(id,hash_size); \
	(element)->next = (head)[tmp]; \
	(element)->prev = NULL; \
	if ((head)[tmp]) (head)[tmp]->prev = (element); \
	(head)[tmp] = (element); }
#define pk11queue_find(element,id,head,hash_size) \
	for( (element) = (head)[pk11_hash(id,hash_size)]; (element) != NULL; \
					 (element) = (element)->next) { \
	    if ((element)->handle == (id)) { break; } }
#define pk11queue_is_queued(element,id,head,hash_size) \
	( ((element)->next) || ((element)->prev) || \
	 ((head)[pk11_hash(id,hash_size)] == (element)) )
#define pk11queue_delete(element,id,head,hash_size) \
	if ((element)->next) (element)->next->prev = (element)->prev; \
	if ((element)->prev) (element)->prev->next = (element)->next; \
	   else (head)[pk11_hash(id,hash_size)] = ((element)->next); \
	(element)->next = NULL; \
	(element)->prev = NULL; \

/* expand an attribute & secitem structures out */
#define pk11_attr_expand(ap) (ap)->type,(ap)->pValue,(ap)->ulValueLen
#define pk11_item_expand(ip) (ip)->data,(ip)->len

typedef struct pk11_parametersStr {
    char *configdir;
    char *certPrefix;
    char *keyPrefix;
    char *secmodName;
    char *man;
    char *libdes; 
    char *tokdes;
    char *ptokdes;
    char *slotdes;
    char *pslotdes;
    char *fslotdes;
    char *fpslotdes;
    int minPW; 
    PRBool readOnly;
    PRBool noCertDB;
    PRBool noModDB;
    PRBool forceOpen;
    PRBool pwRequired;
} pk11_parameters;

/* machine dependent path stuff used by dbinit.c and pk11db.c */
#ifdef macintosh
#define PATH_SEPARATOR ":"
#define SECMOD_DB "Security Modules"
#define CERT_DB_FMT "%sCertificates%s"
#define KEY_DB_FMT "%sKey Database%s"
#else
#define PATH_SEPARATOR "/"
#define SECMOD_DB "secmod.db"
#define CERT_DB_FMT "%scert%s.db"
#define KEY_DB_FMT "%skey%s.db"
#endif

SEC_BEGIN_PROTOS

/* shared functions between PKCS11.c and PK11FIPS.c */
extern CK_RV PK11_LowInitialize(CK_VOID_PTR pReserved);
extern CK_RV PK11_SlotInit(CK_SLOT_ID slotID, PRBool needLogin);

/* internal utility functions used by pkcs11.c */
extern PK11Attribute *pk11_FindAttribute(PK11Object *object,
					 CK_ATTRIBUTE_TYPE type);
extern void pk11_FreeAttribute(PK11Attribute *attribute);
extern CK_RV pk11_AddAttributeType(PK11Object *object, CK_ATTRIBUTE_TYPE type,
				   void *valPtr,
				  CK_ULONG length);
extern CK_RV pk11_Attribute2SecItem(PLArenaPool *arena, SECItem *item,
				    PK11Object *object, CK_ATTRIBUTE_TYPE type);
extern PRBool pk11_hasAttribute(PK11Object *object, CK_ATTRIBUTE_TYPE type);
extern PRBool pk11_isTrue(PK11Object *object, CK_ATTRIBUTE_TYPE type);
extern void pk11_DeleteAttributeType(PK11Object *object,
				     CK_ATTRIBUTE_TYPE type);
extern CK_RV pk11_Attribute2SecItem(PLArenaPool *arena, SECItem *item,
				    PK11Object *object, CK_ATTRIBUTE_TYPE type);
extern CK_RV pk11_Attribute2SSecItem(PLArenaPool *arena, SECItem *item,
				     PK11Object *object,
				     CK_ATTRIBUTE_TYPE type);
extern PK11ModifyType pk11_modifyType(CK_ATTRIBUTE_TYPE type,
				      CK_OBJECT_CLASS inClass);
extern PRBool pk11_isSensitive(CK_ATTRIBUTE_TYPE type, CK_OBJECT_CLASS inClass);
extern char *pk11_getString(PK11Object *object, CK_ATTRIBUTE_TYPE type);
extern void pk11_nullAttribute(PK11Object *object,CK_ATTRIBUTE_TYPE type);
extern CK_RV pk11_forceAttribute(PK11Object *object, CK_ATTRIBUTE_TYPE type,
				 void *value, unsigned int len);
extern CK_RV pk11_defaultAttribute(PK11Object *object, CK_ATTRIBUTE_TYPE type,
				   void *value, unsigned int len);

extern PK11Object *pk11_NewObject(PK11Slot *slot);
extern CK_RV pk11_CopyObject(PK11Object *destObject, PK11Object *srcObject);
extern PK11FreeStatus pk11_FreeObject(PK11Object *object);
extern void pk11_DeleteObject(PK11Session *session, PK11Object *object);
extern void pk11_ReferenceObject(PK11Object *object);
extern PK11Object *pk11_ObjectFromHandle(CK_OBJECT_HANDLE handle,
					 PK11Session *session);
extern void pk11_AddSlotObject(PK11Slot *slot, PK11Object *object);
extern void pk11_AddObject(PK11Session *session, PK11Object *object);

extern CK_RV pk11_searchObjectList(PK11ObjectListElement **objectList,
				   PK11Object **head, PZLock *lock,
				   CK_ATTRIBUTE_PTR inTemplate, int count,
				   PRBool isLoggedIn);
extern PK11ObjectListElement *pk11_FreeObjectListElement(
					     PK11ObjectListElement *objectList);
extern void pk11_FreeObjectList(PK11ObjectListElement *objectList);
extern void pk11_FreeSearch(PK11SearchResults *search);
extern CK_RV pk11_handleObject(PK11Object *object, PK11Session *session);

extern PK11Slot *pk11_SlotFromID(CK_SLOT_ID slotID);
extern PK11Slot *pk11_SlotFromSessionHandle(CK_SESSION_HANDLE handle);
extern PK11Session *pk11_SessionFromHandle(CK_SESSION_HANDLE handle);
extern void pk11_FreeSession(PK11Session *session);
extern PK11Session *pk11_NewSession(CK_SLOT_ID slotID, CK_NOTIFY notify,
				    CK_VOID_PTR pApplication, CK_FLAGS flags);
extern void pk11_update_state(PK11Slot *slot,PK11Session *session);
extern void pk11_update_all_states(PK11Slot *slot);
extern void pk11_FreeContext(PK11SessionContext *context);

extern SECKEYLowPublicKey *pk11_GetPubKey(PK11Object *object,
					  CK_KEY_TYPE key_type);
extern SECKEYLowPrivateKey *pk11_GetPrivKey(PK11Object *object,
					    CK_KEY_TYPE key_type);
extern void pk11_FormatDESKey(unsigned char *key, int length);
extern PRBool pk11_CheckDESKey(unsigned char *key);
extern PRBool pk11_IsWeakKey(unsigned char *key,CK_KEY_TYPE key_type);

extern CK_RV secmod_parseParameters(char *param, pk11_parameters *parsed);
extern void secmod_freeParams(pk11_parameters *params);
extern char *secmod_getSecmodName(char *params, PRBool *rw);
extern char ** secmod_ReadPermDB(char *dbname, char *params, PRBool rw);
extern SECStatus secmod_DeletePermDB(char *dbname,char *args, PRBool rw);
extern SECStatus secmod_AddPermDB(char *dbname, char *module, PRBool rw);
/*
 * OK there are now lots of options here, lets go through them all:
 *
 * configdir - base directory where all the cert, key, and module datbases live.
 * certPrefix - prefix added to the beginning of the cert database example: "
 *                      "https-server1-"
 * keyPrefix - prefix added to the beginning of the key database example: "
 *                      "https-server1-"
 * secmodName - name of the security module database (usually "secmod.db").
 * readOnly - Boolean: true if the databases are to be openned read only.
 * nocertdb - Don't open the cert DB and key DB's, just initialize the
 *                      Volatile certdb.
 * nomoddb - Don't open the security module DB, just initialize the
 *                      PKCS #11 module.
 * forceOpen - Continue to force initializations even if the databases cannot
 *                      be opened.
 */
CK_RV pk11_DBInit(const char *configdir, const char *certPrefix,
	 const char *keyPrefix, const char *secmodName, PRBool readOnly, 
			PRBool noCertDB, PRBool noModDB, PRBool forceOpen);


SEC_END_PROTOS

#endif /* _PKCS11I_H_ */
