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
/*
 * Internal data structures and functions used by pkcs11.c
 */
#ifndef _PKCS11I_H_
#define _PKCS11I_H_ 1

#include "nssilock.h"
#include "seccomon.h"
#include "secoidt.h"
#include "lowkeyti.h"
#include "pkcs11t.h"
#include "pcertt.h"


/* 
 * Configuration Defines 
 *
 * The following defines affect the space verse speed trade offs of
 * the PKCS #11 module. For the most part the current settings are optimized
 * for web servers, where we want faster speed and lower lock contention at
 * the expense of space.
 */

/* 
 * The attribute allocation strategy is static allocation:
 *   Attributes are pre-allocated as part of the session object and used from
 *   the object array.
 */
#define MAX_OBJS_ATTRS 45	/* number of attributes to preallocate in
				 * the object (must me the absolute max) */
#define ATTR_SPACE 50  		/* Maximum size of attribute data before extra
				 * data needs to be allocated. This is set to
				 * enough space to hold an SSL MASTER secret */

#define NSC_STRICT      PR_FALSE  /* forces the code to do strict template
				   * matching when doing C_FindObject on token
				   * objects. This will slow down search in
				   * NSS. */
/* default search block allocations and increments */
#define NSC_CERT_BLOCK_SIZE     50
#define NSC_SEARCH_BLOCK_SIZE   5 
#define NSC_SLOT_LIST_BLOCK_SIZE 10

#define NSC_FIPS_MODULE 1
#define NSC_NON_FIPS_MODULE 0

/* these are data base storage hashes, not cryptographic hashes.. The define
 * the effective size of the various object hash tables */
/* clients care more about memory usage than lookup performance on
 * cyrptographic objects. Clients also have less objects around to play with 
 *
 * we eventually should make this configurable at runtime! Especially now that
 * NSS is a shared library.
 */
#define SPACE_ATTRIBUTE_HASH_SIZE 32 
#define SPACE_TOKEN_OBJECT_HASH_SIZE 32
#define SPACE_SESSION_HASH_SIZE 32
#define TIME_ATTRIBUTE_HASH_SIZE 32
#define TIME_TOKEN_OBJECT_HASH_SIZE 1024
#define TIME_SESSION_HASH_SIZE 1024
#define MAX_OBJECT_LIST_SIZE 800  
				  /* how many objects to keep on the free list
				   * before we start freeing them */
#define MAX_KEY_LEN 256

#define MULTIACCESS "multiaccess:"

/*
 * LOG2_BUCKETS_PER_SESSION_LOCK must be a prime number.
 * With SESSION_HASH_SIZE=1024, LOG2 can be 9, 5, 1, or 0.
 * With SESSION_HASH_SIZE=4096, LOG2 can be 11, 9, 5, 1, or 0.
 *
 * HASH_SIZE   LOG2_BUCKETS_PER   BUCKETS_PER_LOCK  NUMBER_OF_BUCKETS
 * 1024        9                  512               2
 * 1024        5                  32                32
 * 1024        1                  2                 512
 * 1024        0                  1                 1024
 * 4096        11                 2048              2
 * 4096        9                  512               8
 * 4096        5                  32                128
 * 4096        1                  2                 2048
 * 4096        0                  1                 4096
 */
#define LOG2_BUCKETS_PER_SESSION_LOCK 1
#define BUCKETS_PER_SESSION_LOCK (1 << (LOG2_BUCKETS_PER_SESSION_LOCK))
/* NOSPREAD sessionID to hash table index macro has been slower. */

/* define typedefs, double as forward declarations as well */
typedef struct SFTKAttributeStr SFTKAttribute;
typedef struct SFTKObjectListStr SFTKObjectList;
typedef struct SFTKObjectFreeListStr SFTKObjectFreeList;
typedef struct SFTKObjectListElementStr SFTKObjectListElement;
typedef struct SFTKObjectStr SFTKObject;
typedef struct SFTKSessionObjectStr SFTKSessionObject;
typedef struct SFTKTokenObjectStr SFTKTokenObject;
typedef struct SFTKSessionStr SFTKSession;
typedef struct SFTKSlotStr SFTKSlot;
typedef struct SFTKSessionContextStr SFTKSessionContext;
typedef struct SFTKSearchResultsStr SFTKSearchResults;
typedef struct SFTKHashVerifyInfoStr SFTKHashVerifyInfo;
typedef struct SFTKHashSignInfoStr SFTKHashSignInfo;
typedef struct SFTKSSLMACInfoStr SFTKSSLMACInfo;

/* define function pointer typdefs for pointer tables */
typedef void (*SFTKDestroy)(void *, PRBool);
typedef void (*SFTKBegin)(void *);
typedef SECStatus (*SFTKCipher)(void *,void *,unsigned int *,unsigned int,
					void *, unsigned int);
typedef SECStatus (*SFTKVerify)(void *,void *,unsigned int,void *,unsigned int);
typedef void (*SFTKHash)(void *,void *,unsigned int);
typedef void (*SFTKEnd)(void *,void *,unsigned int *,unsigned int);
typedef void (*SFTKFree)(void *);

/* Value to tell if an attribute is modifiable or not.
 *    NEVER: attribute is only set on creation.
 *    ONCOPY: attribute is set on creation and can only be changed on copy.
 *    SENSITIVE: attribute can only be changed to TRUE.
 *    ALWAYS: attribute can always be changed.
 */
typedef enum {
	SFTK_NEVER = 0,
	SFTK_ONCOPY = 1,
	SFTK_SENSITIVE = 2,
	SFTK_ALWAYS = 3
} SFTKModifyType;

/*
 * Free Status Enum... tell us more information when we think we're
 * deleting an object.
 */
typedef enum {
	SFTK_DestroyFailure,
	SFTK_Destroyed,
	SFTK_Busy
} SFTKFreeStatus;

/*
 * attribute values of an object.
 */
struct SFTKAttributeStr {
    SFTKAttribute  	*next;
    SFTKAttribute  	*prev;
    PRBool		freeAttr;
    PRBool		freeData;
    /*must be called handle to make sftkqueue_find work */
    CK_ATTRIBUTE_TYPE	handle;
    CK_ATTRIBUTE 	attrib;
    unsigned char space[ATTR_SPACE];
};


/*
 * doubly link list of objects
 */
struct SFTKObjectListStr {
    SFTKObjectList *next;
    SFTKObjectList *prev;
    SFTKObject	   *parent;
};

struct SFTKObjectFreeListStr {
    SFTKObject	*head;
    PZLock	*lock;
    int		count;
};

/*
 * PKCS 11 crypto object structure
 */
struct SFTKObjectStr {
    SFTKObject *next;
    SFTKObject	*prev;
    CK_OBJECT_CLASS 	objclass;
    CK_OBJECT_HANDLE	handle;
    int 		refCount;
    PZLock 		*refLock;
    SFTKSlot	   	*slot;
    void 		*objectInfo;
    SFTKFree 		infoFree;
};

struct SFTKTokenObjectStr {
    SFTKObject  obj;
    SECItem	dbKey;
};

struct SFTKSessionObjectStr {
    SFTKObject	   obj;
    SFTKObjectList sessionList;
    PZLock		*attributeLock;
    SFTKSession   	*session;
    PRBool		wasDerived;
    int nextAttr;
    SFTKAttribute	attrList[MAX_OBJS_ATTRS];
    PRBool		optimizeSpace;
    unsigned int	hashSize;
    SFTKAttribute 	*head[1];
};

/*
 * struct to deal with a temparary list of objects
 */
struct SFTKObjectListElementStr {
    SFTKObjectListElement	*next;
    SFTKObject 			*object;
};

/*
 * Area to hold Search results
 */
struct SFTKSearchResultsStr {
    CK_OBJECT_HANDLE	*handles;
    int			size;
    int			index;
    int			array_size;
};


/* 
 * the universal crypto/hash/sign/verify context structure
 */
typedef enum {
    SFTK_ENCRYPT,
    SFTK_DECRYPT,
    SFTK_HASH,
    SFTK_SIGN,
    SFTK_SIGN_RECOVER,
    SFTK_VERIFY,
    SFTK_VERIFY_RECOVER
} SFTKContextType;


#define SFTK_MAX_BLOCK_SIZE 16
/* currently SHA512 is the biggest hash length */
#define SFTK_MAX_MAC_LENGTH 64
#define SFTK_INVALID_MAC_SIZE 0xffffffff

struct SFTKSessionContextStr {
    SFTKContextType	type;
    PRBool		multi; 		/* is multipart */
    PRBool		doPad; 		/* use PKCS padding for block ciphers */
    unsigned int	blockSize; 	/* blocksize for padding */
    unsigned int	padDataLength; 	/* length of the valid data in padbuf */
    unsigned char	padBuf[SFTK_MAX_BLOCK_SIZE];
    unsigned char	macBuf[SFTK_MAX_BLOCK_SIZE];
    CK_ULONG		macSize;	/* size of a general block cipher mac*/
    void		*cipherInfo;
    void		*hashInfo;
    unsigned int	cipherInfoLen;
    CK_MECHANISM_TYPE	currentMech;
    SFTKCipher		update;
    SFTKHash		hashUpdate;
    SFTKEnd		end;
    SFTKDestroy		destroy;
    SFTKDestroy		hashdestroy;
    SFTKVerify		verify;
    unsigned int	maxLen;
    SFTKObject		*key;
};

/*
 * Sessions (have objects)
 */
struct SFTKSessionStr {
    SFTKSession        *next;
    SFTKSession        *prev;
    CK_SESSION_HANDLE	handle;
    int			refCount;
    PZLock		*objectLock;
    int			objectIDCount;
    CK_SESSION_INFO	info;
    CK_NOTIFY		notify;
    CK_VOID_PTR		appData;
    SFTKSlot		*slot;
    SFTKSearchResults	*search;
    SFTKSessionContext	*enc_context;
    SFTKSessionContext	*hash_context;
    SFTKSessionContext	*sign_context;
    SFTKObjectList	*objects[1];
};

/*
 * slots (have sessions and objects)
 *
 * The array of sessionLock's protect the session hash table (head[])
 * as well as the reference count of session objects in that bucket
 * (head[]->refCount),  objectLock protects all elements of the token
 * object hash table (tokObjects[], tokenIDCount, and tokenHashTable),
 * slotLock protects the remaining protected elements:
 * password, isLoggedIn, ssoLoggedIn, and sessionCount,
 * and pwCheckLock serializes the key database password checks in
 * NSC_SetPIN and NSC_Login.
 */
struct SFTKSlotStr {
    CK_SLOT_ID		slotID;
    PZLock		*slotLock;
    PZLock		**sessionLock;
    unsigned int	numSessionLocks;
    unsigned long	sessionLockMask;
    PZLock		*objectLock;
    PRLock		*pwCheckLock;
    SECItem		*password;
    PRBool		hasTokens;
    PRBool		isLoggedIn;
    PRBool		ssoLoggedIn;
    PRBool		needLogin;
    PRBool		DB_loaded;
    PRBool		readOnly;
    PRBool		optimizeSpace;
    NSSLOWCERTCertDBHandle *certDB;
    NSSLOWKEYDBHandle	*keyDB;
    int			minimumPinLen;
    PRInt32		sessionIDCount;  /* atomically incremented */
    int			sessionIDConflict;  /* not protected by a lock */
    int			sessionCount;
    PRInt32             rwSessionCount; /* set by atomic operations */
    int			tokenIDCount;
    int			index;
    PLHashTable		*tokenHashTable;
    SFTKObject		**tokObjects;
    unsigned int	tokObjHashSize;
    SFTKSession		**head;
    unsigned int	sessHashSize;
    char		tokDescription[33];
    char		slotDescription[64];
};

/*
 * special joint operations Contexts
 */
struct SFTKHashVerifyInfoStr {
    SECOidTag   	hashOid;
    NSSLOWKEYPublicKey	*key;
};

struct SFTKHashSignInfoStr {
    SECOidTag   	hashOid;
    NSSLOWKEYPrivateKey	*key;
};

/* context for the Final SSLMAC message */
struct SFTKSSLMACInfoStr {
    void 		*hashContext;
    SFTKBegin		begin;
    SFTKHash		update;
    SFTKEnd		end;
    CK_ULONG		macSize;
    int			padSize;
    unsigned char	key[MAX_KEY_LEN];
    unsigned int	keySize;
};

/*
 * session handle modifiers
 */
#define SFTK_SESSION_SLOT_MASK	0xff000000L

/*
 * object handle modifiers
 */
#define SFTK_TOKEN_MASK		0x80000000L
#define SFTK_TOKEN_MAGIC	0x80000000L
#define SFTK_TOKEN_TYPE_MASK	0x70000000L
/* keydb (high bit == 0) */
#define SFTK_TOKEN_TYPE_PRIV	0x10000000L
#define SFTK_TOKEN_TYPE_PUB	0x20000000L
#define SFTK_TOKEN_TYPE_KEY	0x30000000L
/* certdb (high bit == 1) */
#define SFTK_TOKEN_TYPE_TRUST	0x40000000L
#define SFTK_TOKEN_TYPE_CRL	0x50000000L
#define SFTK_TOKEN_TYPE_SMIME	0x60000000L
#define SFTK_TOKEN_TYPE_CERT	0x70000000L

#define SFTK_TOKEN_KRL_HANDLE	(SFTK_TOKEN_MAGIC|SFTK_TOKEN_TYPE_CRL|1)
/* how big (in bytes) a password/pin we can deal with */
#define SFTK_MAX_PIN	255
/* minimum password/pin length (in Unicode characters) in FIPS mode */
#define FIPS_MIN_PIN	7

/* slot ID's */
#define NETSCAPE_SLOT_ID 1
#define PRIVATE_KEY_SLOT_ID 2
#define FIPS_SLOT_ID 3

/* slot helper macros */
#define sftk_SlotFromSession(sp) ((sp)->slot)
#define sftk_isToken(id) (((id) & SFTK_TOKEN_MASK) == SFTK_TOKEN_MAGIC)

/* the session hash multiplier (see bug 201081) */
#define SHMULTIPLIER 1791398085

/* queueing helper macros */
#define sftk_hash(value,size) \
	((PRUint32)((value) * SHMULTIPLIER) & (size-1))
#define sftkqueue_add(element,id,head,hash_size) \
	{ int tmp = sftk_hash(id,hash_size); \
	(element)->next = (head)[tmp]; \
	(element)->prev = NULL; \
	if ((head)[tmp]) (head)[tmp]->prev = (element); \
	(head)[tmp] = (element); }
#define sftkqueue_find(element,id,head,hash_size) \
	for( (element) = (head)[sftk_hash(id,hash_size)]; (element) != NULL; \
					 (element) = (element)->next) { \
	    if ((element)->handle == (id)) { break; } }
#define sftkqueue_is_queued(element,id,head,hash_size) \
	( ((element)->next) || ((element)->prev) || \
	 ((head)[sftk_hash(id,hash_size)] == (element)) )
#define sftkqueue_delete(element,id,head,hash_size) \
	if ((element)->next) (element)->next->prev = (element)->prev; \
	if ((element)->prev) (element)->prev->next = (element)->next; \
	   else (head)[sftk_hash(id,hash_size)] = ((element)->next); \
	(element)->next = NULL; \
	(element)->prev = NULL; \

#define sftkqueue_init_element(element) \
    (element)->prev = NULL;

#define sftkqueue_add2(element, id, index, head) \
    {                                            \
	(element)->next = (head)[index];         \
	if ((head)[index])                       \
	    (head)[index]->prev = (element);     \
	(head)[index] = (element);               \
    }

#define sftkqueue_find2(element, id, index, head) \
    for ( (element) = (head)[index];              \
          (element) != NULL;                      \
          (element) = (element)->next) {          \
	if ((element)->handle == (id)) { break; } \
    }

#define sftkqueue_delete2(element, id, index, head) \
	if ((element)->next) (element)->next->prev = (element)->prev; \
	if ((element)->prev) (element)->prev->next = (element)->next; \
	   else (head)[index] = ((element)->next);

#define sftkqueue_clear_deleted_element(element) \
	(element)->next = NULL; \
	(element)->prev = NULL; \


/* sessionID (handle) is used to determine session lock bucket */
#ifdef NOSPREAD
/* NOSPREAD:	(ID>>L2LPB) & (perbucket-1) */
#define SFTK_SESSION_LOCK(slot,handle) \
    ((slot)->sessionLock[((handle) >> LOG2_BUCKETS_PER_SESSION_LOCK) \
        & (slot)->sessionLockMask])
#else
/* SPREAD:	ID & (perbucket-1) */
#define SFTK_SESSION_LOCK(slot,handle) \
    ((slot)->sessionLock[(handle) & (slot)->sessionLockMask])
#endif

/* expand an attribute & secitem structures out */
#define sftk_attr_expand(ap) (ap)->type,(ap)->pValue,(ap)->ulValueLen
#define sftk_item_expand(ip) (ip)->data,(ip)->len

typedef struct sftk_token_parametersStr {
    CK_SLOT_ID slotID;
    char *configdir;
    char *certPrefix;
    char *keyPrefix;
    char *tokdes;
    char *slotdes;
    int minPW; 
    PRBool readOnly;
    PRBool noCertDB;
    PRBool noKeyDB;
    PRBool forceOpen;
    PRBool pwRequired;
    PRBool optimizeSpace;
} sftk_token_parameters;

typedef struct sftk_parametersStr {
    char *configdir;
    char *secmodName;
    char *man;
    char *libdes; 
    PRBool readOnly;
    PRBool noModDB;
    PRBool noCertDB;
    PRBool forceOpen;
    PRBool pwRequired;
    PRBool optimizeSpace;
    sftk_token_parameters *tokens;
    int token_count;
} sftk_parameters;


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

extern int nsf_init;
extern CK_RV nsc_CommonInitialize(CK_VOID_PTR pReserved, PRBool isFIPS);
extern CK_RV nsc_CommonFinalize(CK_VOID_PTR pReserved, PRBool isFIPS);
extern CK_RV nsc_CommonGetSlotList(CK_BBOOL tokPresent, 
	CK_SLOT_ID_PTR pSlotList, CK_ULONG_PTR pulCount, int moduleIndex);
/* shared functions between PKCS11.c and SFTKFIPS.c */
extern CK_RV SFTK_SlotInit(char *configdir,sftk_token_parameters *params, 
							int moduleIndex);

/* internal utility functions used by pkcs11.c */
extern SFTKAttribute *sftk_FindAttribute(SFTKObject *object,
					 CK_ATTRIBUTE_TYPE type);
extern void sftk_FreeAttribute(SFTKAttribute *attribute);
extern CK_RV sftk_AddAttributeType(SFTKObject *object, CK_ATTRIBUTE_TYPE type,
				   void *valPtr,
				  CK_ULONG length);
extern CK_RV sftk_Attribute2SecItem(PLArenaPool *arena, SECItem *item,
				    SFTKObject *object, CK_ATTRIBUTE_TYPE type);
extern unsigned int sftk_GetLengthInBits(unsigned char *buf,
							 unsigned int bufLen);
extern CK_RV sftk_ConstrainAttribute(SFTKObject *object, 
	CK_ATTRIBUTE_TYPE type, int minLength, int maxLength, int minMultiple);
extern PRBool sftk_hasAttribute(SFTKObject *object, CK_ATTRIBUTE_TYPE type);
extern PRBool sftk_isTrue(SFTKObject *object, CK_ATTRIBUTE_TYPE type);
extern void sftk_DeleteAttributeType(SFTKObject *object,
				     CK_ATTRIBUTE_TYPE type);
extern CK_RV sftk_Attribute2SecItem(PLArenaPool *arena, SECItem *item,
				    SFTKObject *object, CK_ATTRIBUTE_TYPE type);
extern CK_RV sftk_Attribute2SSecItem(PLArenaPool *arena, SECItem *item,
				     SFTKObject *object,
				     CK_ATTRIBUTE_TYPE type);
extern SFTKModifyType sftk_modifyType(CK_ATTRIBUTE_TYPE type,
				      CK_OBJECT_CLASS inClass);
extern PRBool sftk_isSensitive(CK_ATTRIBUTE_TYPE type, CK_OBJECT_CLASS inClass);
extern char *sftk_getString(SFTKObject *object, CK_ATTRIBUTE_TYPE type);
extern void sftk_nullAttribute(SFTKObject *object,CK_ATTRIBUTE_TYPE type);
extern CK_RV sftk_GetULongAttribute(SFTKObject *object, CK_ATTRIBUTE_TYPE type,
                                                         CK_ULONG *longData);
extern CK_RV sftk_forceAttribute(SFTKObject *object, CK_ATTRIBUTE_TYPE type,
				 void *value, unsigned int len);
extern CK_RV sftk_defaultAttribute(SFTKObject *object, CK_ATTRIBUTE_TYPE type,
				   void *value, unsigned int len);
extern unsigned int sftk_MapTrust(CK_TRUST trust, PRBool clientAuth);

extern SFTKObject *sftk_NewObject(SFTKSlot *slot);
extern CK_RV sftk_CopyObject(SFTKObject *destObject, SFTKObject *srcObject);
extern SFTKFreeStatus sftk_FreeObject(SFTKObject *object);
extern CK_RV sftk_DeleteObject(SFTKSession *session, SFTKObject *object);
extern void sftk_ReferenceObject(SFTKObject *object);
extern SFTKObject *sftk_ObjectFromHandle(CK_OBJECT_HANDLE handle,
					 SFTKSession *session);
extern void sftk_AddSlotObject(SFTKSlot *slot, SFTKObject *object);
extern void sftk_AddObject(SFTKSession *session, SFTKObject *object);

extern CK_RV sftk_searchObjectList(SFTKSearchResults *search,
				   SFTKObject **head, unsigned int size,
				   PZLock *lock, CK_ATTRIBUTE_PTR inTemplate,
				   int count, PRBool isLoggedIn);
extern SFTKObjectListElement *sftk_FreeObjectListElement(
					     SFTKObjectListElement *objectList);
extern void sftk_FreeObjectList(SFTKObjectListElement *objectList);
extern void sftk_FreeSearch(SFTKSearchResults *search);
extern CK_RV sftk_handleObject(SFTKObject *object, SFTKSession *session);

extern SFTKSlot *sftk_SlotFromID(CK_SLOT_ID slotID);
extern SFTKSlot *sftk_SlotFromSessionHandle(CK_SESSION_HANDLE handle);
extern SFTKSession *sftk_SessionFromHandle(CK_SESSION_HANDLE handle);
extern void sftk_FreeSession(SFTKSession *session);
extern SFTKSession *sftk_NewSession(CK_SLOT_ID slotID, CK_NOTIFY notify,
				    CK_VOID_PTR pApplication, CK_FLAGS flags);
extern void sftk_update_state(SFTKSlot *slot,SFTKSession *session);
extern void sftk_update_all_states(SFTKSlot *slot);
extern void sftk_FreeContext(SFTKSessionContext *context);
extern void sftk_InitFreeLists(void);
extern void sftk_CleanupFreeLists(void);

extern NSSLOWKEYPublicKey *sftk_GetPubKey(SFTKObject *object,
					  CK_KEY_TYPE key_type, CK_RV *crvp);
extern NSSLOWKEYPrivateKey *sftk_GetPrivKey(SFTKObject *object,
					    CK_KEY_TYPE key_type, CK_RV *crvp);
extern void sftk_FormatDESKey(unsigned char *key, int length);
extern PRBool sftk_CheckDESKey(unsigned char *key);
extern PRBool sftk_IsWeakKey(unsigned char *key,CK_KEY_TYPE key_type);

extern CK_RV secmod_parseParameters(char *param, sftk_parameters *parsed,
								PRBool isFIPS);
extern void secmod_freeParams(sftk_parameters *params);
extern char *secmod_getSecmodName(char *params, char **domain, 
						char **filename, PRBool *rw);
extern char ** secmod_ReadPermDB(const char *domain, const char *filename, 
			const char *dbname, char *params, PRBool rw);
extern SECStatus secmod_DeletePermDB(const char *domain, const char *filename,
			const char *dbname, char *args, PRBool rw);
extern SECStatus secmod_AddPermDB(const char *domain, const char *filename,
			const char *dbname, char *module, PRBool rw);
extern SECStatus secmod_ReleasePermDBData(const char *domain, 
	const char *filename, const char *dbname, char **specList, PRBool rw);
/* mechanism allows this operation */
extern CK_RV sftk_MechAllowsOperation(CK_MECHANISM_TYPE type, CK_ATTRIBUTE_TYPE op);
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
CK_RV sftk_DBInit(const char *configdir, const char *certPrefix,
	 	const char *keyPrefix, PRBool readOnly, PRBool noCertDB, 
		PRBool noKeyDB, PRBool forceOpen, 
		NSSLOWCERTCertDBHandle **certDB, NSSLOWKEYDBHandle **keyDB);

void sftk_DBShutdown(NSSLOWCERTCertDBHandle *certHandle, 
		     NSSLOWKEYDBHandle *keyHandle);

const char *sftk_EvaluateConfigDir(const char *configdir, char **domain);

/*
 * narrow objects
 */
SFTKSessionObject * sftk_narrowToSessionObject(SFTKObject *);
SFTKTokenObject * sftk_narrowToTokenObject(SFTKObject *);

/*
 * token object utilities
 */
void sftk_addHandle(SFTKSearchResults *search, CK_OBJECT_HANDLE handle);
PRBool sftk_poisonHandle(SFTKSlot *slot, SECItem *dbkey, 
						CK_OBJECT_HANDLE handle);
PRBool sftk_tokenMatch(SFTKSlot *slot, SECItem *dbKey, CK_OBJECT_HANDLE class,
                                        CK_ATTRIBUTE_PTR theTemplate,int count);
CK_OBJECT_HANDLE sftk_mkHandle(SFTKSlot *slot, 
					SECItem *dbKey, CK_OBJECT_HANDLE class);
SFTKObject * sftk_NewTokenObject(SFTKSlot *slot, SECItem *dbKey, 
						CK_OBJECT_HANDLE handle);
SFTKTokenObject *sftk_convertSessionToToken(SFTKObject *so);

/****************************************
 * implement TLS Pseudo Random Function (PRF)
 */

extern CK_RV
sftk_TLSPRFInit(SFTKSessionContext *context, 
		  SFTKObject *        key, 
		  CK_KEY_TYPE         key_type);

SEC_END_PROTOS

#endif /* _PKCS11I_H_ */
