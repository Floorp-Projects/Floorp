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
 * Private Key Database code
 *
 * $Id: keydb.c,v 1.3 2000/10/01 17:13:34 wtc%netscape.com Exp $
 */

#include "keylow.h"
#include "keydbt.h"
#include "seccomon.h"
#include "sechash.h"
#include "secder.h"
#include "secasn1.h"
#include "secoid.h"
#include "blapi.h"
#include "secitem.h"
#include "cert.h"
#include "mcom_db.h"
#include "secpkcs5.h"
#include "secerr.h"

#include "private.h"

/*
 * Record keys for keydb
 */
#define SALT_STRING "global-salt"
#define VERSION_STRING "Version"
#define KEYDB_PW_CHECK_STRING	"password-check"
#define KEYDB_PW_CHECK_LEN	14

/* Size of the global salt for key database */
#define SALT_LENGTH     16

/* ASN1 Templates for new decoder/encoder */
/*
 * Attribute value for PKCS8 entries (static?)
 */
const SEC_ASN1Template SECKEY_AttributeTemplate[] = {
    { SEC_ASN1_SEQUENCE, 
	0, NULL, sizeof(SECKEYAttribute) },
    { SEC_ASN1_OBJECT_ID, offsetof(SECKEYAttribute, attrType) },
    { SEC_ASN1_SET_OF, offsetof(SECKEYAttribute, attrValue), 
	SEC_AnyTemplate },
    { 0 }
};

const SEC_ASN1Template SECKEY_SetOfAttributeTemplate[] = {
    { SEC_ASN1_SET_OF, 0, SECKEY_AttributeTemplate },
};

const SEC_ASN1Template SECKEY_PrivateKeyInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	0, NULL, sizeof(SECKEYPrivateKeyInfo) },
    { SEC_ASN1_INTEGER,
	offsetof(SECKEYPrivateKeyInfo,version) },
    { SEC_ASN1_INLINE,
	offsetof(SECKEYPrivateKeyInfo,algorithm),
	SECOID_AlgorithmIDTemplate },
    { SEC_ASN1_OCTET_STRING,
	offsetof(SECKEYPrivateKeyInfo,privateKey) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	offsetof(SECKEYPrivateKeyInfo,attributes),
	SECKEY_SetOfAttributeTemplate },
    { 0 }
};

const SEC_ASN1Template SECKEY_PointerToPrivateKeyInfoTemplate[] = {
    { SEC_ASN1_POINTER, 0, SECKEY_PrivateKeyInfoTemplate }
};

const SEC_ASN1Template SECKEY_EncryptedPrivateKeyInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	0, NULL, sizeof(SECKEYEncryptedPrivateKeyInfo) },
    { SEC_ASN1_INLINE,
	offsetof(SECKEYEncryptedPrivateKeyInfo,algorithm),
	SECOID_AlgorithmIDTemplate },
    { SEC_ASN1_OCTET_STRING,
	offsetof(SECKEYEncryptedPrivateKeyInfo,encryptedData) },
    { 0 }
};

const SEC_ASN1Template SECKEY_PointerToEncryptedPrivateKeyInfoTemplate[] = {
	{ SEC_ASN1_POINTER, 0, SECKEY_EncryptedPrivateKeyInfoTemplate }
};

/* ====== Default key databse encryption algorithm ====== */

static SECOidTag defaultKeyDBAlg = SEC_OID_PKCS12_PBE_WITH_SHA1_AND_TRIPLE_DES_CBC;

/*
 * Default algorithm for encrypting data in the key database
 */
SECOidTag
SECKEY_GetDefaultKeyDBAlg(void)
{
    return(defaultKeyDBAlg);
}

void
SECKEY_SetDefaultKeyDBAlg(SECOidTag alg)
{
    defaultKeyDBAlg = alg;

    return;
}

static void
sec_destroy_dbkey(SECKEYDBKey *dbkey)
{
    if ( dbkey && dbkey->arena ) {
	PORT_FreeArena(dbkey->arena, PR_FALSE);
    }
}

static void
free_dbt(DBT *dbt)
{
    if ( dbt ) {
	PORT_Free(dbt->data);
	PORT_Free(dbt);
    }
    
    return;
}

/*
 * format of key database entries for version 3 of database:
 *	byte offset	field
 *	-----------	-----
 *	0		version
 *	1		salt-len
 *	2		nn-len
 *	3..		salt-data
 *	...		nickname
 *	...		encrypted-key-data
 */
static DBT *
encode_dbkey(SECKEYDBKey *dbkey)
{
    DBT *bufitem = NULL;
    unsigned char *buf;
    int nnlen;
    char *nn;
    
    bufitem = (DBT *)PORT_ZAlloc(sizeof(DBT));
    if ( bufitem == NULL ) {
	goto loser;
    }
    
    if ( dbkey->nickname ) {
	nn = dbkey->nickname;
	nnlen = PORT_Strlen(nn) + 1;
    } else {
	nn = "";
	nnlen = 1;
    }
    
    /* compute the length of the record */
    /* 1 + 1 + 1 == version number header + salt length + nn len */
    bufitem->size = dbkey->salt.len + nnlen + dbkey->derPK.len + 1 + 1 + 1;
    
    bufitem->data = (void *)PORT_ZAlloc(bufitem->size);
    if ( bufitem->data == NULL ) {
	goto loser;
    }

    buf = (unsigned char *)bufitem->data;
    
    /* set version number */
    buf[0] = PRIVATE_KEY_DB_FILE_VERSION;

    /* set length of salt */
    PORT_Assert(dbkey->salt.len < 256);
    buf[1] = dbkey->salt.len;

    /* set length of nickname */
    PORT_Assert(nnlen < 256);
    buf[2] = nnlen;

    /* copy salt */
    PORT_Memcpy(&buf[3], dbkey->salt.data, dbkey->salt.len);

    /* copy nickname */
    PORT_Memcpy(&buf[3 + dbkey->salt.len], nn, nnlen);

    /* copy encrypted key */
    PORT_Memcpy(&buf[3 + dbkey->salt.len + nnlen], dbkey->derPK.data,
	      dbkey->derPK.len);
    
    return(bufitem);
    
loser:
    if ( bufitem ) {
	free_dbt(bufitem);
    }
    
    return(NULL);
}

static SECKEYDBKey *
decode_dbkey(DBT *bufitem, int expectedVersion)
{
    SECKEYDBKey *dbkey;
    PLArenaPool *arena = NULL;
    unsigned char *buf;
    int version;
    int keyoff;
    int nnlen;
    int saltoff;
    
    buf = (unsigned char *)bufitem->data;

    version = buf[0];
    
    if ( version != expectedVersion ) {
	goto loser;
    }
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	goto loser;
    }
    
    dbkey = (SECKEYDBKey *)PORT_ArenaZAlloc(arena, sizeof(SECKEYDBKey));
    if ( dbkey == NULL ) {
	goto loser;
    }

    dbkey->arena = arena;
    dbkey->salt.data = NULL;
    dbkey->derPK.data = NULL;
    
    dbkey->salt.len = buf[1];
    dbkey->salt.data = (unsigned char *)PORT_ArenaZAlloc(arena, dbkey->salt.len);
    if ( dbkey->salt.data == NULL ) {
	goto loser;
    }

    saltoff = 2;
    keyoff = 2 + dbkey->salt.len;
    
    if ( expectedVersion == PRIVATE_KEY_DB_FILE_VERSION ) {
	nnlen = buf[2];
	if ( nnlen ) {
	    dbkey->nickname = (char *)PORT_ArenaZAlloc(arena, nnlen + 1);
	    if ( dbkey->nickname ) {
		PORT_Memcpy(dbkey->nickname, &buf[keyoff+1], nnlen);
	    }
	}
	keyoff += ( nnlen + 1 );
	saltoff = 3;
    }

    PORT_Memcpy(dbkey->salt.data, &buf[saltoff], dbkey->salt.len);
    
    dbkey->derPK.len = bufitem->size - keyoff;
    dbkey->derPK.data = (unsigned char *)PORT_ArenaZAlloc(arena,dbkey->derPK.len);
    if ( dbkey->derPK.data == NULL ) {
	goto loser;
    }
    
    PORT_Memcpy(dbkey->derPK.data, &buf[keyoff], dbkey->derPK.len);
    
    return(dbkey);
    
loser:

    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(NULL);
}

static SECKEYDBKey *
get_dbkey(SECKEYKeyDBHandle *handle, DBT *index)
{
    SECKEYDBKey *dbkey;
    DBT entry;
    SECStatus rv;
    
    /* get it from the database */
    rv = (SECStatus)(* handle->db->get)(handle->db, index, &entry, 0);
    if ( rv ) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	return NULL;
    }

    /* set up dbkey struct */

    dbkey = decode_dbkey(&entry, PRIVATE_KEY_DB_FILE_VERSION);

    return(dbkey);
}

static SECStatus
put_dbkey(SECKEYKeyDBHandle *handle, DBT *index, SECKEYDBKey *dbkey, PRBool update)
{
    DBT *keydata = NULL;
    int status;
    
    keydata = encode_dbkey(dbkey);
    if ( keydata == NULL ) {
	goto loser;
    }
    
    /* put it in the database */
    if ( update ) {
	status = (* handle->db->put)(handle->db, index, keydata, 0);
    } else {
	status = (* handle->db->put)(handle->db, index, keydata,
				     R_NOOVERWRITE);
    }
    
    if ( status ) {
	goto loser;
    }

    /* sync the database */
    status = (* handle->db->sync)(handle->db, 0);
    if ( status ) {
	goto loser;
    }

    free_dbt(keydata);
    return(SECSuccess);

loser:
    if ( keydata ) {
	free_dbt(keydata);
    }
    
    return(SECFailure);
}

SECStatus
SECKEY_TraverseKeys(SECKEYKeyDBHandle *handle, 
		 SECStatus (* keyfunc)(DBT *k, DBT *d, void *pdata),
		 void *udata )
{
    DBT data;
    DBT key;
    SECStatus status;
    int rv;

    if (handle == NULL) {
	return(SECFailure);
    }

    rv = (* handle->db->seq)(handle->db, &key, &data, R_FIRST);
    if ( rv ) {
	return(SECFailure);
    }
    
    do {
	/* skip version record */
	if ( data.size > 1 ) {
	    if ( key.size == ( sizeof(SALT_STRING) - 1 ) ) {
		if ( PORT_Memcmp(key.data, SALT_STRING, key.size) == 0 ) {
		    continue;
		}
	    }

	    /* skip password check */
	    if ( key.size == KEYDB_PW_CHECK_LEN ) {
		if ( PORT_Memcmp(key.data, KEYDB_PW_CHECK_STRING,
				 KEYDB_PW_CHECK_LEN) == 0 ) {
		    continue;
		}
	    }
	    
	    status = (* keyfunc)(&key, &data, udata);
	    if (status != SECSuccess) {
		return(status);
	    }
	}
    } while ( (* handle->db->seq)(handle->db, &key, &data, R_NEXT) == 0 );

    return(SECSuccess);
}

typedef struct keyNode {
    struct keyNode *next;
    DBT key;
} keyNode;

typedef struct {
    PLArenaPool *arena;
    keyNode *head;
} keyList;

static SECStatus
sec_add_key_to_list(DBT *key, DBT *data, void *arg)
{
    keyList *keylist;
    keyNode *node;
    void *keydata;
    
    keylist = (keyList *)arg;

    /* allocate the node struct */
    node = (keyNode*)PORT_ArenaZAlloc(keylist->arena, sizeof(keyNode));
    if ( node == NULL ) {
	return(SECFailure);
    }
    
    /* allocate room for key data */
    keydata = PORT_ArenaZAlloc(keylist->arena, key->size);
    if ( keydata == NULL ) {
	return(SECFailure);
    }

    /* link node into list */
    node->next = keylist->head;
    keylist->head = node;

    /* copy key into node */
    PORT_Memcpy(keydata, key->data, key->size);
    node->key.size = key->size;
    node->key.data = keydata;
    
    return(SECSuccess);
}

static SECItem *
decodeKeyDBGlobalSalt(DBT *saltData)
{
    SECItem *saltitem;
    
    saltitem = (SECItem *)PORT_ZAlloc(sizeof(SECItem));
    if ( saltitem == NULL ) {
	return(NULL);
    }
    
    saltitem->data = (unsigned char *)PORT_ZAlloc(saltData->size);
    if ( saltitem->data == NULL ) {
	PORT_Free(saltitem);
	return(NULL);
    }
    
    saltitem->len = saltData->size;
    PORT_Memcpy(saltitem->data, saltData->data, saltitem->len);
    
    return(saltitem);
}

static SECItem *
GetKeyDBGlobalSalt(SECKEYKeyDBHandle *handle)
{
    DBT saltKey;
    DBT saltData;
    int ret;
    
    saltKey.data = SALT_STRING;
    saltKey.size = sizeof(SALT_STRING) - 1;

    ret = (* handle->db->get)(handle->db, &saltKey, &saltData, 0);
    if ( ret ) {
	return(NULL);
    }

    return(decodeKeyDBGlobalSalt(&saltData));
}

static SECStatus
makeGlobalVersion(SECKEYKeyDBHandle *handle)
{
    unsigned char version;
    DBT versionData;
    DBT versionKey;
    int status;
    
    version = PRIVATE_KEY_DB_FILE_VERSION;
    versionData.data = &version;
    versionData.size = 1;
    versionKey.data = VERSION_STRING;
    versionKey.size = sizeof(VERSION_STRING)-1;
		
    /* put version string into the database now */
    status = (* handle->db->put)(handle->db, &versionKey, &versionData, 0);
    if ( status ) {
	return(SECFailure);
    }

    return(SECSuccess);
}


static SECStatus
makeGlobalSalt(SECKEYKeyDBHandle *handle)
{
    DBT saltKey;
    DBT saltData;
    unsigned char saltbuf[16];
    int status;
    
    saltKey.data = SALT_STRING;
    saltKey.size = sizeof(SALT_STRING) - 1;

    saltData.data = (void *)saltbuf;
    saltData.size = sizeof(saltbuf);
    RNG_GenerateGlobalRandomBytes(saltbuf, sizeof(saltbuf));

    /* put global salt into the database now */
    status = (* handle->db->put)( handle->db, &saltKey, &saltData, 0);
    if ( status ) {
	return(SECFailure);
    }

    return(SECSuccess);
}

static char *
keyDBFilenameCallback(void *arg, int dbVersion)
{
    return(PORT_Strdup((char *)arg));
}

SECKEYKeyDBHandle *
SECKEY_OpenKeyDBFilename(char *dbname, PRBool readOnly)
{
    return(SECKEY_OpenKeyDB(readOnly, keyDBFilenameCallback,
			   (void *)dbname));
}

SECKEYKeyDBHandle *
SECKEY_OpenKeyDB(PRBool readOnly, SECKEYDBNameFunc namecb, void *cbarg)
{
    SECKEYKeyDBHandle *handle;
    DBT versionKey;
    DBT versionData;
    int rv;
    int openflags;
    char *dbname = NULL;
    PRBool updated = PR_FALSE;
    
    handle = (SECKEYKeyDBHandle *)PORT_ZAlloc (sizeof(SECKEYKeyDBHandle));
    if (handle == NULL) {
	PORT_SetError (SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    versionKey.data = VERSION_STRING;
    versionKey.size = sizeof(VERSION_STRING)-1;
    
    if ( readOnly ) {
	openflags = O_RDONLY;
    } else {
	openflags = O_RDWR;
    }

    dbname = (*namecb)(cbarg, PRIVATE_KEY_DB_FILE_VERSION);
    if ( dbname == NULL ) {
	goto loser;
    }
    
    handle->db = dbopen( dbname, openflags, 0600, DB_HASH, 0 );

    /* check for correct version number */
    if (handle->db != NULL) {
	/* lookup version string in database */
	rv = (* handle->db->get)( handle->db, &versionKey, &versionData, 0 );

	/* error accessing the database */
	if ( rv < 0 ) {
	    goto loser;
	}

	if ( rv == 1 ) {
	    /* no version number record, reset the database */
	    (* handle->db->close)( handle->db );
	    handle->db = NULL;

	    goto newdb;
	    
	}
	
	handle->version = *( (unsigned char *)versionData.data);
	    
	if (handle->version != PRIVATE_KEY_DB_FILE_VERSION ) {
	    /* bogus version number record, reset the database */
	    (* handle->db->close)( handle->db );
	    handle->db = NULL;

	    goto newdb;
	}

    }

newdb:
	
    /* if first open fails, try to create a new DB */
    if ( handle->db == NULL ) {
	if ( readOnly ) {
	    goto loser;
	}
	
	handle->db = dbopen( dbname,
			     O_RDWR | O_CREAT | O_TRUNC, 0600, DB_HASH, 0 );

        PORT_Free( dbname );
        dbname = NULL;

	/* if create fails then we lose */
	if ( handle->db == NULL ) {
	    goto loser;
	}

	rv = makeGlobalVersion(handle);
	if ( rv != SECSuccess ) {
	    goto loser;
	}

	/*
	 * try to update from v2 db
	 */
	dbname = (*namecb)(cbarg, 2);
	if ( dbname != NULL ) {
	    handle->updatedb = dbopen( dbname, O_RDONLY, 0600, DB_HASH, 0 );

	    if ( handle->updatedb ) {


		/*
		 * Try to update the db using a null password.  If the db
		 * doesn't have a password, then this will work.  If it does
		 * have a password, then this will fail and we will do the
		 * update later
		 */
		rv = SECKEY_UpdateKeyDBPass1(handle);
		if ( rv == SECSuccess ) {
		    updated = PR_TRUE;
		}
	    }
	    
            PORT_Free( dbname );
            dbname = NULL;
	}

	/* we are using the old salt if we updated from an old db */
	if ( ! updated ) {
	    rv = makeGlobalSalt(handle);
	    if ( rv != SECSuccess ) {
		goto loser;
	    }
	}
	
	/* sync the database */
	rv = (* handle->db->sync)(handle->db, 0);
	if ( rv ) {
	    goto loser;
	}
    }

    handle->global_salt = GetKeyDBGlobalSalt(handle);
    if ( dbname )
        PORT_Free( dbname );
    return handle;

loser:

    if ( dbname )
        PORT_Free( dbname );
    PORT_SetError(SEC_ERROR_BAD_DATABASE);

    if ( handle->db ) {
	(* handle->db->close)(handle->db);
    }
    if ( handle->updatedb ) {
	(* handle->updatedb->close)(handle->updatedb);
    }
    PORT_Free(handle);
    return NULL;
}

/*
 * Close the database
 */
void
SECKEY_CloseKeyDB(SECKEYKeyDBHandle *handle)
{
    if (handle != NULL) {
	if (handle == SECKEY_GetDefaultKeyDB()) {
	    SECKEY_SetDefaultKeyDB(NULL);
	}
	if (handle->db != NULL) {
	    (* handle->db->close)(handle->db);
	}
	PORT_Free(handle);
    }
}

/* Get the key database version */
int
SECKEY_GetKeyDBVersion(SECKEYKeyDBHandle *handle)
{
    PORT_Assert(handle != NULL);

    return handle->version;
}

/*
 * Allow use of default key database, so that apps (such as mozilla) do
 * not have to pass the handle all over the place.
 */

static SECKEYKeyDBHandle *sec_default_key_db = NULL;

void
SECKEY_SetDefaultKeyDB(SECKEYKeyDBHandle *handle)
{
    sec_default_key_db = handle;
}

SECKEYKeyDBHandle *
SECKEY_GetDefaultKeyDB(void)
{
    return sec_default_key_db;
}

/*
 * Delete a private key that was stored in the database
 */
SECStatus
SECKEY_DeleteKey(SECKEYKeyDBHandle *handle, SECItem *pubkey)
{
    DBT namekey;
    int rv;

    if (handle == NULL) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	return(SECFailure);
    }

    /* set up db key and data */
    namekey.data = pubkey->data;
    namekey.size = pubkey->len;

    /* delete it from the database */
    rv = (* handle->db->del)(handle->db, &namekey, 0);
    if ( rv ) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	return(SECFailure);
    }

    /* sync the database */
    rv = (* handle->db->sync)(handle->db, 0);
    if ( rv ) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	return(SECFailure);
    }

    return(SECSuccess);
}

/*
 * Store a key in the database, indexed by its public key modulus.(value!)
 */
SECStatus
SECKEY_StoreKeyByPublicKey(SECKEYKeyDBHandle *handle, 
			   SECKEYLowPrivateKey *privkey,
			   SECItem *pubKeyData,
			   char *nickname,
			   SECKEYGetPasswordKey f, void *arg)
{
    return SECKEY_StoreKeyByPublicKeyAlg(handle, privkey, pubKeyData, nickname,
					 f, arg, SECKEY_GetDefaultKeyDBAlg());
}

/* see if the public key for this cert is in the database filed
 * by modulus
 */
SECStatus
SECKEY_KeyForCertExists(SECKEYKeyDBHandle *handle, CERTCertificate *cert)
{
    SECKEYPublicKey *pubkey = NULL;
    DBT namekey;
    DBT dummy;
    int status;
    
    /* get cert's public key */
    pubkey = CERT_ExtractPublicKey(cert);
    if ( pubkey == NULL ) {
	return SECFailure;
    }

    /* TNH - make key from SECKEYPublicKey */
    switch (pubkey->keyType) {
      case rsaKey:
	namekey.data = pubkey->u.rsa.modulus.data;
	namekey.size = pubkey->u.rsa.modulus.len;
	break;
      case dsaKey:
	namekey.data = pubkey->u.dsa.publicValue.data;
	namekey.size = pubkey->u.dsa.publicValue.len;
	break;
      case dhKey:
	namekey.data = pubkey->u.dh.publicValue.data;
	namekey.size = pubkey->u.dh.publicValue.len;
	break;
      default:
	/* XXX We don't do Fortezza or DH yet. */
	return SECFailure;
    }

    status = (* handle->db->get)(handle->db, &namekey, &dummy, 0);
    SECKEY_DestroyPublicKey(pubkey);
    if ( status ) {
	/* TNH - should this really set an error? */
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	return SECFailure;
    }
    
    return SECSuccess;
}

/*
 * find the private key for a cert
 */
SECKEYLowPrivateKey *
SECKEY_FindKeyByCert(SECKEYKeyDBHandle *handle, CERTCertificate *cert,
		     SECKEYGetPasswordKey f, void *arg)
{
    SECKEYPublicKey *pubkey = NULL;
    SECItem *keyItem;
    SECKEYLowPrivateKey *privKey = NULL;
    
    /* get cert's public key */
    pubkey = CERT_ExtractPublicKey(cert);
    if ( !pubkey ) {
	goto loser;
    }

    /* TNH - make record key from SECKEYPublicKey (again) */
    switch (pubkey->keyType) {
      case rsaKey:
	keyItem = &pubkey->u.rsa.modulus;
	break;
      case dsaKey:
	keyItem = &pubkey->u.dsa.publicValue;
	break;
      case dhKey:
	keyItem = &pubkey->u.dh.publicValue;
	break;
	/* fortezza an NULL keys are not stored in the data base */
      case fortezzaKey:
      case nullKey:
	goto loser;
    }

    privKey = SECKEY_FindKeyByPublicKey(handle, keyItem, f, arg);

    /* success falls through */
loser:
    SECKEY_DestroyPublicKey(pubkey);
    return(privKey);
}

/*
 * check to see if the user has a password
 */
SECStatus
SECKEY_HasKeyDBPassword(SECKEYKeyDBHandle *handle)
{
    DBT checkkey, checkdata;
    int rv;

    if (handle == NULL) {
	return(SECFailure);
    }

    checkkey.data = KEYDB_PW_CHECK_STRING;
    checkkey.size = KEYDB_PW_CHECK_LEN;
    
    rv = (* handle->db->get)(handle->db, &checkkey, &checkdata, 0 );
    if ( rv ) {
	return(SECFailure);
    }

    return(SECSuccess);
}

/*
 * Set up the password checker in the key database.
 * This is done by encrypting a known plaintext with the user's key.
 */
SECStatus
SECKEY_SetKeyDBPassword(SECKEYKeyDBHandle *handle, SECItem *pwitem)
{
    return SECKEY_SetKeyDBPasswordAlg(handle, pwitem,
				      SECKEY_GetDefaultKeyDBAlg());
}

/*
 * Re-encrypt the entire key database with a new password.
 * NOTE: This really should create a new database rather than doing it
 * in place in the original
 */
SECStatus
SECKEY_ChangeKeyDBPassword(SECKEYKeyDBHandle *handle,
			   SECItem *oldpwitem, SECItem *newpwitem)
{
    return SECKEY_ChangeKeyDBPasswordAlg(handle, oldpwitem, newpwitem,
					 SECKEY_GetDefaultKeyDBAlg());
}

static SECStatus
HashPassword(unsigned char *hashresult, char *pw, SECItem *salt)
{
    SHA1Context *cx;
    unsigned int outlen;
    cx = SHA1_NewContext();
    if ( cx == NULL ) {
	return(SECFailure);
    }
    
    SHA1_Begin(cx);
    if ( ( salt != NULL ) && ( salt->data != NULL ) ) {
	SHA1_Update(cx, salt->data, salt->len);
    }
    
    SHA1_Update(cx, (unsigned char *)pw, PORT_Strlen(pw));
    SHA1_End(cx, hashresult, &outlen, SHA1_LENGTH);
    
    SHA1_DestroyContext(cx, PR_TRUE);
    
    return(SECSuccess);
}

SECItem *
SECKEY_HashPassword(char *pw, SECItem *salt)
{
    SECItem *pwitem;
    SECStatus rv;
    
    pwitem = (SECItem *)PORT_ZAlloc(sizeof(SECItem));
    if ( pwitem == NULL ) {
	return(NULL);
    }
    pwitem->len = SHA1_LENGTH;
    pwitem->data = (unsigned char *)PORT_ZAlloc(SHA1_LENGTH);
    if ( pwitem->data == NULL ) {
	PORT_Free(pwitem);
	return(NULL);
    }
    if ( pw ) {
	rv = HashPassword(pwitem->data, pw, salt);
	if ( rv != SECSuccess ) {
	    SECITEM_ZfreeItem(pwitem, PR_TRUE);
	    return(NULL);
	}
    }
    
    return(pwitem);
}

/* Derive the actual password value for the database from a pw string */
SECItem *
SECKEY_DeriveKeyDBPassword(SECKEYKeyDBHandle *keydb, char *pw)
{
    PORT_Assert(keydb != NULL);
    PORT_Assert(pw != NULL);
    if (keydb == NULL || pw == NULL) return(NULL);

    return SECKEY_HashPassword(pw, keydb->global_salt);
}

#if 0
/* Appears obsolete - TNH */
/* get the algorithm with which a private key
 * is encrypted.
 */
SECOidTag 
seckey_get_private_key_algorithm(SECKEYKeyDBHandle *keydb, DBT *index)   
{
    SECKEYDBKey *dbkey = NULL;
    SECOidTag algorithm = SEC_OID_UNKNOWN;
    SECKEYEncryptedPrivateKeyInfo *epki = NULL;
    PLArenaPool *poolp = NULL;
    SECStatus rv;

    poolp = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if(poolp == NULL)
	return (SECOidTag)SECFailure;  /* TNH - this is bad */

    dbkey = get_dbkey(keydb, index);
    if(dbkey == NULL)
	return (SECOidTag)SECFailure;

    epki = (SECKEYEncryptedPrivateKeyInfo *)PORT_ArenaZAlloc(poolp, 
	sizeof(SECKEYEncryptedPrivateKeyInfo));
    if(epki == NULL)
	goto loser;
    rv = SEC_ASN1DecodeItem(poolp, epki, 
	SECKEY_EncryptedPrivateKeyInfoTemplate, &dbkey->derPK);
    if(rv == SECFailure)
	goto loser;

    algorithm = SECOID_GetAlgorithmTag(&epki->algorithm);

    /* let success fall through */
loser:
    if(poolp != NULL)
	PORT_FreeArena(poolp, PR_TRUE);\
    if(dbkey != NULL)
	sec_destroy_dbkey(dbkey);

    return algorithm;
}
#endif
	
/*
 * Derive an RC4 key from a password key and a salt.  This
 * was the method to used to encrypt keys in the version 2?
 * database
 */
SECItem *
seckey_create_rc4_key(SECItem *pwitem, SECItem *salt)
{
    MD5Context *md5 = NULL;
    unsigned int part;
    SECStatus rv = SECFailure;
    SECItem *key = NULL;

    key = (SECItem *)PORT_ZAlloc(sizeof(SECItem));
    if(key != NULL)
    {
	key->data = (unsigned char *)PORT_ZAlloc(sizeof(unsigned char) *
		MD5_LENGTH);
	key->len = MD5_LENGTH;
	if(key->data != NULL)
	{
	    md5 = MD5_NewContext();
	    if ( md5 != NULL ) 
	    {
		MD5_Begin(md5);
		MD5_Update(md5, salt->data, salt->len);
		MD5_Update(md5, pwitem->data, pwitem->len);
		MD5_End(md5, key->data, &part, MD5_LENGTH);
		MD5_DestroyContext(md5, PR_TRUE);
		rv = SECSuccess;
	    }
	}
	
	if(rv != SECSuccess)
	{
	    SECITEM_FreeItem(key, PR_TRUE);
	    key = NULL;
	}
    }

    return key;
}

SECItem *
seckey_create_rc4_salt(void)
{
    SECItem *salt = NULL;
    SECStatus rv = SECFailure;

    salt = (SECItem *)PORT_ZAlloc(sizeof(SECItem));
    if(salt == NULL)
	return NULL;

    salt->data = (unsigned char *)PORT_ZAlloc(sizeof(unsigned char) *
	SALT_LENGTH);
    if(salt->data != NULL)
    {
	salt->len = SALT_LENGTH;
	RNG_GenerateGlobalRandomBytes(salt->data, salt->len);
	rv = SECSuccess;
    }
	
    if(rv == SECFailure)
    {
	SECITEM_FreeItem(salt, PR_TRUE);
	salt = NULL;
    }

    return salt;
}

SECItem *
seckey_rc4_cipher(SECItem *key, SECItem *src, PRBool encrypt)
{
    SECItem *dest = NULL;
    RC4Context *ctxt = NULL;
    SECStatus rv = SECFailure;

    if((key == NULL) || (src == NULL))
	return NULL;

    dest = (SECItem *)PORT_ZAlloc(sizeof(SECItem));
    if(dest == NULL)
	return NULL;

    dest->data = (unsigned char *)PORT_ZAlloc(sizeof(unsigned char) *
	(src->len + 64));  /* TNH - padding? */
    if(dest->data != NULL)
    {
	ctxt = RC4_CreateContext(key->data, key->len);
	if(ctxt != NULL)
	{
	    if(encrypt == PR_TRUE)
	    rv = RC4_Encrypt(ctxt, dest->data, &dest->len,
		src->len + 64, src->data, src->len);
	    else
		rv = RC4_Decrypt(ctxt, dest->data, &dest->len,
		    src->len + 64, src->data, src->len);
	    RC4_DestroyContext(ctxt, PR_TRUE);
	}
    }

    if(rv == SECFailure)
	if(dest != NULL)
	{
	    SECITEM_FreeItem(dest, PR_TRUE);
	    dest = NULL;
	}

    return dest;
}

/* TNH - keydb is unused */
/* TNH - the pwitem should be the derived key for RC4 */
SECKEYEncryptedPrivateKeyInfo *
seckey_encrypt_private_key(
	SECKEYLowPrivateKey *pk, SECItem *pwitem, SECKEYKeyDBHandle *keydb,
	SECOidTag algorithm)
{
    SECKEYEncryptedPrivateKeyInfo *epki = NULL;
    SECKEYPrivateKeyInfo *pki = NULL;
    SECStatus rv = SECFailure;
    SECAlgorithmID *algid = NULL;
    PLArenaPool *temparena = NULL, *permarena = NULL;
    SECItem *key = NULL, *salt = NULL, *der_item = NULL;
    SECItem *dummy = NULL, *dest = NULL;

    permarena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if(permarena == NULL)
	return NULL;

    temparena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if(temparena == NULL)
	goto loser;

    /* allocate structures */
    epki = (SECKEYEncryptedPrivateKeyInfo *)PORT_ArenaZAlloc(permarena,
	sizeof(SECKEYEncryptedPrivateKeyInfo));
    pki = (SECKEYPrivateKeyInfo *)PORT_ArenaZAlloc(temparena, 
	sizeof(SECKEYPrivateKeyInfo));
    der_item = (SECItem *)PORT_ArenaZAlloc(temparena, sizeof(SECItem));
    if((epki == NULL) || (pki == NULL) || (der_item == NULL))
	goto loser;

    epki->arena = permarena;

    /* setup private key info */
    dummy = SEC_ASN1EncodeInteger(temparena, &(pki->version), 
	SEC_PRIVATE_KEY_INFO_VERSION);
    if(dummy == NULL)
	goto loser;

    /* Encode the key, and set the algorithm (with params) */
    switch (pk->keyType) {
      case rsaKey:
	dummy = SEC_ASN1EncodeItem(temparena, &(pki->privateKey), pk, 
				   SECKEY_RSAPrivateKeyTemplate);
	if (dummy == NULL) {
	    rv = SECFailure;
	    goto loser;
	}
	
	rv = SECOID_SetAlgorithmID(temparena, &(pki->algorithm), 
				   SEC_OID_PKCS1_RSA_ENCRYPTION, 0);
	if (rv == SECFailure) {
	    goto loser;
	}
	
	break;
      case dsaKey:
	dummy = SEC_ASN1EncodeItem(temparena, &(pki->privateKey), pk,
				   SECKEY_DSAPrivateKeyTemplate);
	if (dummy == NULL) {
	    rv = SECFailure;
	    goto loser;
	}
	
	dummy = SEC_ASN1EncodeItem(temparena, NULL, &pk->u.dsa.params,
				   SECKEY_PQGParamsTemplate);
	if (dummy == NULL) {
	    rv = SECFailure;
	    goto loser;
	}
	
	rv = SECOID_SetAlgorithmID(temparena, &(pki->algorithm),
				   SEC_OID_ANSIX9_DSA_SIGNATURE, dummy);
	if (rv == SECFailure) {
	    goto loser;
	}
	
	break;
      case dhKey:
	dummy = SEC_ASN1EncodeItem(temparena, &(pki->privateKey), pk,
				   SECKEY_DHPrivateKeyTemplate);
	if (dummy == NULL) {
	    rv = SECFailure;
	    goto loser;
	}

	rv = SECOID_SetAlgorithmID(temparena, &(pki->algorithm),
				   SEC_OID_X942_DIFFIE_HELMAN_KEY, dummy);
	if (rv == SECFailure) {
	    goto loser;
	}
	break;
      default:
	/* We don't support DH or Fortezza private keys yet */
	PORT_Assert(PR_FALSE);
	break;
    }

    /* setup encrypted private key info */
    dummy = SEC_ASN1EncodeItem(temparena, der_item, pki, 
	SECKEY_PrivateKeyInfoTemplate);
    if(dummy == NULL) {
	rv = SECFailure;
	goto loser;
    }

    rv = SECFailure; /* assume failure */
    switch(algorithm)
    {
	case SEC_OID_RC4:
	    salt = seckey_create_rc4_salt();
	    if(salt != NULL)
	    {
		key = seckey_create_rc4_key(pwitem, salt);
		if(key != NULL)
		{
		    dest = seckey_rc4_cipher(key, der_item, PR_TRUE);
		    if(dest != NULL)
		    {
			rv = SECITEM_CopyItem(permarena, &epki->encryptedData,
			    dest);
			if(rv == SECSuccess)
			rv = SECOID_SetAlgorithmID(permarena, 
			&epki->algorithm, SEC_OID_RC4, salt);
		    }
		}
	    }
	    if(dest != NULL)
		SECITEM_FreeItem(dest, PR_TRUE);
	    if(key != NULL)
		SECITEM_ZfreeItem(key, PR_TRUE);
	    break;
	default:
	    algid = SEC_PKCS5CreateAlgorithmID(algorithm, NULL, 1);
	    if(algid != NULL)
	    {
		dest = SEC_PKCS5CipherData(algid, pwitem, 
		der_item, PR_TRUE, NULL);
		if(dest != NULL)
		{
		    rv = SECITEM_CopyItem(permarena, &epki->encryptedData,
			    dest);
		    if(rv == SECSuccess)
			rv = SECOID_CopyAlgorithmID(permarena, 
			    &epki->algorithm, algid);
		}
	    }
	    if(dest != NULL)
		SECITEM_FreeItem(dest, PR_TRUE);
	    if(algid != NULL)
		SECOID_DestroyAlgorithmID(algid, PR_TRUE);
	    break;
    }

    /* let success fall through */
loser:

    if(rv == SECFailure)
    {
	PORT_FreeArena(permarena, PR_TRUE);
	    epki = NULL;
    }

    if(temparena != NULL)
	PORT_FreeArena(temparena, PR_TRUE);
	
    if(salt != NULL)
	SECITEM_FreeItem(salt, PR_TRUE);

    return epki;
}

SECStatus 
seckey_put_private_key(SECKEYKeyDBHandle *keydb, DBT *index, SECItem *pwitem,
		       SECKEYLowPrivateKey *pk, char *nickname, PRBool update,
		       SECOidTag algorithm)
{
    SECKEYDBKey *dbkey = NULL;
    SECKEYEncryptedPrivateKeyInfo *epki = NULL;
    PLArenaPool *temparena = NULL, *permarena = NULL;
    SECItem *dummy = NULL;
    SECItem *salt = NULL;
    SECStatus rv = SECFailure;

    if((keydb == NULL) || (index == NULL) || (pwitem == NULL) ||
	(pk == NULL))
	return SECFailure;
	
    permarena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if(permarena == NULL)
	return SECFailure;

    dbkey = (SECKEYDBKey *)PORT_ArenaZAlloc(permarena, sizeof(SECKEYDBKey));
    if(dbkey == NULL)
	goto loser;
    dbkey->arena = permarena;
    dbkey->nickname = nickname;

    /* TNH - for RC4, the salt should be created here */
	
    epki = seckey_encrypt_private_key(pk, pwitem, keydb, algorithm);
    if(epki == NULL)
	goto loser;
    temparena = epki->arena;

    /* extract salt for db key */
    switch(algorithm)
    {
	case SEC_OID_RC4:
	    rv = SECITEM_CopyItem(permarena, &(dbkey->salt), 
		&(epki->algorithm.parameters));
	    epki->algorithm.parameters.len = 0;
		epki->algorithm.parameters.data = NULL;
	    break;
	default:
            /* TNH - this should not be necessary */
	    salt = SEC_PKCS5GetSalt(&epki->algorithm);
	    if(salt != NULL)
	    {
		rv = SECITEM_CopyItem(permarena, &(dbkey->salt), salt);
		SECITEM_ZfreeItem(salt, PR_TRUE);
	    }
	    break;
    }

    dummy = SEC_ASN1EncodeItem(permarena, &(dbkey->derPK), epki, 
	SECKEY_EncryptedPrivateKeyInfoTemplate);
    if(dummy == NULL)
	rv = SECFailure;
    else
	rv = put_dbkey(keydb, index, dbkey, update);

    /* let success fall through */
loser:
    if(rv != SECSuccess)
	if(permarena != NULL)
	    PORT_FreeArena(permarena, PR_TRUE);
    if(temparena != NULL)
	PORT_FreeArena(temparena, PR_TRUE);

    return rv;
}

/*
 * Store a key in the database, indexed by its public key modulus.
 * Note that the nickname is optional.  It was only used by keyutil.
 */
SECStatus
SECKEY_StoreKeyByPublicKeyAlg(SECKEYKeyDBHandle *handle, 
			      SECKEYLowPrivateKey *privkey,
			      SECItem *pubKeyData,
			      char *nickname,
			      SECKEYGetPasswordKey f, void *arg,
			      SECOidTag algorithm)
{
    DBT namekey;
    SECItem *pwitem = NULL;
    SECStatus rv;

    if (handle == NULL) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	return(SECFailure);
    }

    /* set up db key and data */
    namekey.data = pubKeyData->data;
    namekey.size = pubKeyData->len;

    pwitem = (*f )(arg, handle);
    if ( pwitem == NULL ) {
	return(SECFailure);
    }
    
    /* encrypt the private key */
    rv = seckey_put_private_key(handle, &namekey, pwitem, privkey, nickname,
				PR_FALSE, algorithm);
    SECITEM_ZfreeItem(pwitem, PR_TRUE);
    
    return(rv);
}

SECKEYLowPrivateKey *
seckey_decrypt_private_key(SECKEYEncryptedPrivateKeyInfo *epki,
			   SECItem *pwitem)
{
    SECKEYLowPrivateKey *pk = NULL;
    SECKEYPrivateKeyInfo *pki = NULL;
    SECStatus rv = SECFailure;
    SECOidTag algorithm;
    PLArenaPool *temparena = NULL, *permarena = NULL;
    SECItem *salt = NULL, *dest = NULL, *key = NULL;

    if((epki == NULL) || (pwitem == NULL))
	goto loser;

    temparena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    permarena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if((temparena == NULL) || (permarena == NULL))
	goto loser;

    /* allocate temporary items */
    pki = (SECKEYPrivateKeyInfo *)PORT_ArenaZAlloc(temparena, 
	sizeof(SECKEYPrivateKeyInfo));

    /* allocate permanent arena items */
    pk = (SECKEYLowPrivateKey *)PORT_ArenaZAlloc(permarena,
	sizeof(SECKEYLowPrivateKey));

    if((pk == NULL) || (pki == NULL))
	goto loser;

    pk->arena = permarena;
	
    algorithm = SECOID_GetAlgorithmTag(&(epki->algorithm));
    switch(algorithm)
    {
	case SEC_OID_RC4:
	    salt = SECITEM_DupItem(&epki->algorithm.parameters);
	    if(salt != NULL)
	    {
		key = seckey_create_rc4_key(pwitem, salt);
		if(key != NULL)
		{
		    dest = seckey_rc4_cipher(key, &epki->encryptedData, 
			PR_FALSE);
		}
	    }	
	    if(salt != NULL)
		SECITEM_ZfreeItem(salt, PR_TRUE);
	    if(key != NULL)
		SECITEM_ZfreeItem(key, PR_TRUE);
	    break;
	default:
	    /* we depend on the fact that if this key was encoded with
	     * DES, that the pw was also encoded with DES, so we don't have
	     * to do the update here, the password code will handle it. */
	    dest = SEC_PKCS5CipherData(&epki->algorithm, pwitem, 
		&epki->encryptedData, PR_FALSE, NULL);
	    break;
    }

    if(dest != NULL)
    {
	rv = SEC_ASN1DecodeItem(temparena, pki, 
	    SECKEY_PrivateKeyInfoTemplate, dest);
	if(rv == SECSuccess)
	{
	    switch(SECOID_GetAlgorithmTag(&pki->algorithm)) {
	      case SEC_OID_X500_RSA_ENCRYPTION:
	      case SEC_OID_PKCS1_RSA_ENCRYPTION:
		pk->keyType = rsaKey;
		rv = SEC_ASN1DecodeItem(permarena, pk,
					SECKEY_RSAPrivateKeyTemplate,
					&pki->privateKey);
		break;
	      case SEC_OID_ANSIX9_DSA_SIGNATURE:
		pk->keyType = dsaKey;
		rv = SEC_ASN1DecodeItem(permarena, pk,
					SECKEY_DSAPrivateKeyTemplate,
					&pki->privateKey);
		if (rv != SECSuccess)
		    goto loser;
		rv = SEC_ASN1DecodeItem(permarena, &pk->u.dsa.params,
					SECKEY_PQGParamsTemplate,
					&pki->algorithm.parameters);
		break;
	      case SEC_OID_X942_DIFFIE_HELMAN_KEY:
		pk->keyType = dhKey;
		rv = SEC_ASN1DecodeItem(permarena, pk,
					SECKEY_DHPrivateKeyTemplate,
					&pki->privateKey);
		break;
	      default:
		rv = SECFailure;
		break;
	    }
	}
	else if(PORT_GetError() == SEC_ERROR_BAD_DER)
	{
	    PORT_SetError(SEC_ERROR_BAD_PASSWORD);
	    goto loser;
	}
    }

    /* let success fall through */
loser:
    if(temparena != NULL)
	PORT_FreeArena(temparena, PR_TRUE);
    if(dest != NULL)
	SECITEM_ZfreeItem(dest, PR_TRUE);

    if(rv != SECSuccess)
    {
	if(permarena != NULL)
	    PORT_FreeArena(permarena, PR_TRUE);
	pk = NULL;
    }

    return pk;
}

static SECKEYLowPrivateKey *
seckey_decode_encrypted_private_key(SECKEYDBKey *dbkey, SECItem *pwitem)
{
    SECKEYLowPrivateKey *pk = NULL;
    SECKEYEncryptedPrivateKeyInfo *epki;
    PLArenaPool *temparena = NULL;
    SECStatus rv;
    SECOidTag algorithm;

    if( ( dbkey == NULL ) || ( pwitem == NULL ) ) {
	return NULL;
    }

    temparena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if(temparena == NULL) {
	return NULL;
    }

    epki = (SECKEYEncryptedPrivateKeyInfo *)
	PORT_ArenaZAlloc(temparena, sizeof(SECKEYEncryptedPrivateKeyInfo));

    if(epki == NULL) {
	goto loser;
    }

    rv = SEC_ASN1DecodeItem(temparena, epki,
			    SECKEY_EncryptedPrivateKeyInfoTemplate,
			    &(dbkey->derPK)); 
    if(rv != SECSuccess) {
	goto loser;
    }

    algorithm = SECOID_GetAlgorithmTag(&(epki->algorithm));
    switch(algorithm)
    {
      case SEC_OID_RC4:
	/* TNH - this code should derive the actual RC4 key from salt and
           pwitem */
	rv = SECITEM_CopyItem(temparena, &(epki->algorithm.parameters),
			      &(dbkey->salt));
	break;
      default:
	break;
    }

    pk = seckey_decrypt_private_key(epki, pwitem);

    /* let success fall through */
loser:

    PORT_FreeArena(temparena, PR_TRUE);
    return pk;
}

SECKEYLowPrivateKey *
seckey_get_private_key(SECKEYKeyDBHandle *keydb, DBT *index, char **nickname,
		       SECItem *pwitem)
{
    SECKEYDBKey *dbkey = NULL;
    SECKEYLowPrivateKey *pk = NULL;

    if( ( keydb == NULL ) || ( index == NULL ) || ( pwitem == NULL ) ) {
	return NULL;
    }

    dbkey = get_dbkey(keydb, index);
    if(dbkey == NULL) {
	goto loser;
    }
    
    if ( nickname ) {
	if ( dbkey->nickname && ( dbkey->nickname[0] != 0 ) ) {
	    *nickname = PORT_Strdup(dbkey->nickname);
	} else {
	    *nickname = NULL;
	}
    }
    
    pk = seckey_decode_encrypted_private_key(dbkey, pwitem);
    
    /* let success fall through */
loser:

    if ( dbkey != NULL ) {
	sec_destroy_dbkey(dbkey);
    }

    return pk;
}

/*
 * used by pkcs11 to import keys into it's object format... In the future
 * we really need a better way to tie in...
 */
SECKEYLowPrivateKey *
SECKEY_DecryptKey(DBT *key, SECItem *pwitem,
					 SECKEYKeyDBHandle *handle) {
    return seckey_get_private_key(handle,key,NULL,pwitem);
}

/*
 * Find a key in the database, indexed by its public key modulus
 * This is used to find keys that have been stored before their
 * certificate arrives.  Once the certificate arrives the key
 * is looked up by the public modulus in the certificate, and the
 * re-stored by its nickname.
 */
SECKEYLowPrivateKey *
SECKEY_FindKeyByPublicKey(SECKEYKeyDBHandle *handle, SECItem *modulus,
			  SECKEYGetPasswordKey f, void *arg)
{
    DBT namekey;
    SECKEYLowPrivateKey *pk = NULL;
    SECItem *pwitem = NULL;

    if (handle == NULL) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	return NULL;
    }

    /* set up db key */
    namekey.data = modulus->data;
    namekey.size = modulus->len;

    pwitem = (*f )(arg, handle);
    if ( pwitem == NULL ) {
	return(NULL);
    }

    pk = seckey_get_private_key(handle, &namekey, NULL, pwitem);
    SECITEM_ZfreeItem(pwitem, PR_TRUE);
    
    /* no need to free dbkey, since its on the stack, and the data it
     * points to is owned by the database
     */
    return(pk);
}

/* ===== ENCODING ROUTINES ===== */

static SECStatus
encodePWCheckEntry(PLArenaPool *arena, SECItem *entry, SECOidTag alg,
		   SECItem *encCheck)
{
    SECOidData *oidData;
    SECStatus rv;
    
    oidData = SECOID_FindOIDByTag(alg);
    if ( oidData == NULL ) {
	rv = SECFailure;
	goto loser;
    }

    entry->len = 1 + oidData->oid.len + encCheck->len;
    if ( arena ) {
	entry->data = (unsigned char *)PORT_ArenaAlloc(arena, entry->len);
    } else {
	entry->data = (unsigned char *)PORT_Alloc(entry->len);
    }
    
    if ( entry->data == NULL ) {
	goto loser;
    }
	
    /* first length of oid */
    entry->data[0] = (unsigned char)oidData->oid.len;
    /* next oid itself */
    PORT_Memcpy(&entry->data[1], oidData->oid.data, oidData->oid.len);
    /* finally the encrypted check string */
    PORT_Memcpy(&entry->data[1+oidData->oid.len], encCheck->data,
		encCheck->len);

    return(SECSuccess);

loser:
    return(SECFailure);
}

/*
 * Set up the password checker in the key database.
 * This is done by encrypting a known plaintext with the user's key.
 */
SECStatus
SECKEY_SetKeyDBPasswordAlg(SECKEYKeyDBHandle *handle, 
			   SECItem *pwitem, SECOidTag algorithm)
{
    DBT checkkey;
    SECAlgorithmID *algid = NULL;
    SECStatus rv = SECFailure;
    SECKEYDBKey *dbkey = NULL;
    PLArenaPool *arena;
    SECItem *key = NULL, *salt = NULL;
    SECItem *dest = NULL, test_key;
    
    if (handle == NULL) {
	return(SECFailure);
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	rv = SECFailure;
	goto loser;
    }
    
    dbkey = (SECKEYDBKey *)PORT_ArenaZAlloc(arena, sizeof(SECKEYDBKey));
    if ( dbkey == NULL ) {
	rv = SECFailure;
	goto loser;
    }
    
    dbkey->arena = arena;

    /* encrypt key */
    checkkey.data = test_key.data = (unsigned char *)KEYDB_PW_CHECK_STRING;
    checkkey.size = test_key.len = KEYDB_PW_CHECK_LEN;

    salt = seckey_create_rc4_salt();
    if(salt == NULL) {
	rv = SECFailure;
	goto loser;
    }

    switch(algorithm)
    {
	case SEC_OID_RC4:
	    key = seckey_create_rc4_key(pwitem, salt);
	    if(key != NULL)
	    {
		dest = seckey_rc4_cipher(key, &test_key, PR_TRUE);
		SECITEM_FreeItem(key, PR_TRUE);
	    }
	    break;
	default:
	    algid = SEC_PKCS5CreateAlgorithmID(algorithm, salt, 1);
	    if(algid != NULL)
		dest = SEC_PKCS5CipherData(algid, pwitem, &test_key,
							 PR_TRUE, NULL);
	    break;
    }

    if(dest != NULL)
    {
	rv = SECITEM_CopyItem(arena, &dbkey->salt, salt);
	if(rv == SECFailure)
	    goto loser;
   
	rv = encodePWCheckEntry(arena, &dbkey->derPK, algorithm, dest);
	
	if ( rv != SECSuccess ) {
	    goto loser;
	}
	
	rv = put_dbkey(handle, &checkkey, dbkey, PR_TRUE);
    } else {
	rv = SECFailure;
    }
    
    /* let success fall through */
loser: 
    if ( arena != NULL ) {
	PORT_FreeArena(arena, PR_TRUE);
    }
    
    if ( dest != NULL ) {
	SECITEM_ZfreeItem(dest, PR_TRUE);
    }
    
    if ( salt != NULL ) {
	SECITEM_ZfreeItem(salt, PR_TRUE);
    }
	
    return(rv);
}

/*
 * check to see if the user has typed the right password
 */
SECStatus
SECKEY_CheckKeyDBPassword(SECKEYKeyDBHandle *handle, SECItem *pwitem)
{
    DBT checkkey;
    SECAlgorithmID *algid = NULL;
    SECStatus rv = SECFailure;
    SECKEYDBKey *dbkey = NULL;
    SECItem *key = NULL;
    SECItem *dest = NULL;
    SECOidTag algorithm;
    SECItem oid;
    SECItem encstring;
    PRBool update = PR_FALSE;
    
    if (handle == NULL) {
	goto loser;
    }

    checkkey.data = KEYDB_PW_CHECK_STRING;
    checkkey.size = KEYDB_PW_CHECK_LEN;
    
    dbkey = get_dbkey(handle, &checkkey);
    
    if ( dbkey == NULL ) {
	goto loser;
    }

    /* build the oid item */
    oid.len = dbkey->derPK.data[0];
    oid.data = &dbkey->derPK.data[1];
    
    /* make sure entry is the correct length
     * since we are probably using a block cipher, the block will be
     * padded, so we may get a bit more than the exact size we need.
     */
    if ( dbkey->derPK.len < (KEYDB_PW_CHECK_LEN + 1 + oid.len ) ) {
	goto loser;
    }

    /* find the algorithm tag */
    algorithm = SECOID_FindOIDTag(&oid);

    /* make a secitem of the encrypted check string */
    encstring.len = dbkey->derPK.len - ( oid.len + 1 );
    encstring.data = &dbkey->derPK.data[oid.len+1];
    
    switch(algorithm)
    {
	case SEC_OID_RC4:
	    key = seckey_create_rc4_key(pwitem, &dbkey->salt);
	    if(key != NULL) {
		dest = seckey_rc4_cipher(key, &encstring, PR_FALSE);
		SECITEM_FreeItem(key, PR_TRUE);
	    }
	    break;
	default:
	    algid = SEC_PKCS5CreateAlgorithmID(algorithm, 
					       &dbkey->salt, 1);
	    if(algid != NULL) {
                /* Decrypt - this function implements a workaround for
                 * a previous coding error.  It will decrypt values using
                 * DES rather than 3DES, if the initial try at 3DES
                 * decryption fails.  In this case, the update flag is
                 * set to TRUE.  This indication is used later to force
                 * an update of the database to "real" 3DES encryption.
                 */
		dest = SEC_PKCS5CipherData(algid, pwitem, 
					   &encstring, PR_FALSE, &update);
		SECOID_DestroyAlgorithmID(algid, PR_TRUE);
	    }	
	    break;
    }

    if(dest == NULL) {
	goto loser;
    }

    if ((dest->len == KEYDB_PW_CHECK_LEN) &&
	(PORT_Memcmp(dest->data, 
		     KEYDB_PW_CHECK_STRING, KEYDB_PW_CHECK_LEN) == 0)) {
	rv = SECSuccess;
	/* we succeeded */
	if ( algorithm == SEC_OID_RC4 ) {
	    /* partially updated database */
	    SECKEY_UpdateKeyDBPass2(handle, pwitem);
	}
        /* Force an update of the password to remove the incorrect DES
         * encryption (see the note above)
         */
	if (update && 
	     (algorithm == SEC_OID_PKCS12_PBE_WITH_SHA1_AND_TRIPLE_DES_CBC)) {
	    /* data base was encoded with DES not triple des, fix it */
	    SECKEY_UpdateKeyDBPass2(handle,pwitem);
	}
    }
			
loser:
    sec_destroy_dbkey(dbkey);
    if(dest != NULL) {
    	SECITEM_ZfreeItem(dest, PR_TRUE);
    }

    return(rv);
}

/*
 *  Change the database password and/or algorithm.  This internal
 *  routine does not check the old password.  That must be done by 
 *  the caller.
 */
static SECStatus
ChangeKeyDBPasswordAlg(SECKEYKeyDBHandle *handle,
		       SECItem *oldpwitem, SECItem *newpwitem,
		       SECOidTag new_algorithm)
{
    SECStatus rv;
    keyList keylist;
    keyNode *node = NULL;
    SECKEYLowPrivateKey *privkey = NULL;
    char *nickname;
    DBT newkey;
    int ret;

    /* traverse the database, collecting the keys of all records */
    keylist.arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( keylist.arena == NULL ) 
    {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return(SECFailure);
    }
    keylist.head = NULL;
    
    /* TNH - TraverseKeys should not be public, since it exposes
       the underlying DBT data type. */
    rv = SECKEY_TraverseKeys(handle, sec_add_key_to_list, (void *)&keylist);
    if ( rv != SECSuccess ) 
	goto loser;

    /* walk the list, re-encrypting each entry */
    node = keylist.head;
    while ( node != NULL ) 
    {
	privkey = seckey_get_private_key(handle, &node->key, &nickname,
					 oldpwitem);
	
	if (privkey == NULL) {
	    PORT_SetError(SEC_ERROR_BAD_DATABASE);
	    rv = SECFailure;
	    goto loser;
	}

	/* delete the old record */
	ret = (* handle->db->del)(handle->db, &node->key, 0);
	if ( ret ) {
	    PORT_SetError(SEC_ERROR_BAD_DATABASE);
	    rv = SECFailure;
	    goto loser;
	}
	
	/* get the public key, which we use as the database index */

	switch (privkey->keyType) {
	  case rsaKey:
	    newkey.data = privkey->u.rsa.modulus.data;
	    newkey.size = privkey->u.rsa.modulus.len;
	    break;
	  case dsaKey:
	    newkey.data = privkey->u.dsa.publicValue.data;
	    newkey.size = privkey->u.dsa.publicValue.len;
	    break;
	  case dhKey:
	    newkey.data = privkey->u.dh.publicValue.data;
	    newkey.size = privkey->u.dh.publicValue.len;
	    break;
	  default:
	    /* XXX We don't do Fortezza. */
	    return SECFailure;
	}

	rv = seckey_put_private_key(handle, &newkey, newpwitem, privkey,
				    nickname, PR_TRUE, new_algorithm);
	
	if ( rv != SECSuccess ) 
	{
	    PORT_SetError(SEC_ERROR_BAD_DATABASE);
	    rv = SECFailure;
	    goto loser;
	}

	/* next node */
	node = node->next;
    }

    rv = SECKEY_SetKeyDBPasswordAlg(handle, newpwitem, new_algorithm);

loser:

    /* free the arena */
    if ( keylist.arena ) {
	PORT_FreeArena(keylist.arena, PR_FALSE);
    }
    
    return(rv);
}

/*
 * Re-encrypt the entire key database with a new password.
 * NOTE: The really  should create a new database rather than doing it
 * in place in the original
 */
SECStatus
SECKEY_ChangeKeyDBPasswordAlg(SECKEYKeyDBHandle *handle,
			      SECItem *oldpwitem, SECItem *newpwitem,
			      SECOidTag new_algorithm)
{
    SECStatus rv;
    
    if (handle == NULL) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	rv = SECFailure;
	goto loser;
    }

    rv = SECKEY_CheckKeyDBPassword(handle, oldpwitem);
    if ( rv != SECSuccess ) {
	return(SECFailure);  /* return rv? */
    }

    rv = ChangeKeyDBPasswordAlg(handle, oldpwitem, newpwitem, new_algorithm);

loser:
    return(rv);
}
    
/*
 * Second pass of updating the key db.  This time we have a password.
 */
SECStatus
SECKEY_UpdateKeyDBPass2(SECKEYKeyDBHandle *handle, SECItem *pwitem)
{
    SECStatus rv;
    
    rv = ChangeKeyDBPasswordAlg(handle, pwitem, pwitem,
			     SECKEY_GetDefaultKeyDBAlg());
    
    return(rv);
}

/*
 * currently updates key database from v2 to v3
 */
SECStatus
SECKEY_UpdateKeyDBPass1(SECKEYKeyDBHandle *handle)
{
    SECStatus rv;
    DBT versionKey;
    DBT versionData;
    DBT checkKey;
    DBT checkData;
    DBT saltKey;
    DBT saltData;
    DBT key;
    DBT data;
    SECItem *rc4key = NULL;
    SECKEYDBKey *dbkey = NULL;
    SECItem *oldSalt = NULL;
    int ret;
    SECItem checkitem;

    if ( handle->updatedb == NULL ) {
	return(SECSuccess);
    }

    /*
     * check the version record
     */
    versionKey.data = VERSION_STRING;
    versionKey.size = sizeof(VERSION_STRING)-1;

    ret = (* handle->updatedb->get)(handle->updatedb, &versionKey,
				    &versionData, 0 );

    if (ret) {
	/* no version record, so old db never used */
	goto done;
    }

    if ( ( versionData.size != 1 ) ||
	( *((unsigned char *)versionData.data) != 2 ) ) {
	/* corrupt or wrong version number so don't update */
	goto done;
    }

    saltKey.data = SALT_STRING;
    saltKey.size = sizeof(SALT_STRING) - 1;

    ret = (* handle->updatedb->get)(handle->updatedb, &saltKey, &saltData, 0);
    if ( ret ) {
	/* no salt in old db, so it is corrupted */
	goto done;
    }

    oldSalt = decodeKeyDBGlobalSalt(&saltData);
    if ( oldSalt == NULL ) {
	/* bad salt in old db, so it is corrupted */
	goto done;
    }

    /*
     * look for a pw check entry
     */
    checkKey.data = KEYDB_PW_CHECK_STRING;
    checkKey.size = KEYDB_PW_CHECK_LEN;
    
    rv = (SECStatus)(* handle->updatedb->get)(handle->updatedb, &checkKey,
				   &checkData, 0 );
    if (rv) {
	/* no pw check, so old db never used */
	goto done;
    }

    dbkey = decode_dbkey(&checkData, 2);
    if ( dbkey == NULL ) {
	goto done;
    }
    
    /* put global salt into the new database now */
    ret = (* handle->db->put)( handle->db, &saltKey, &saltData, 0);
    if ( ret ) {
	goto done;
    }

    checkitem = dbkey->derPK;
    dbkey->derPK.data = NULL;
    
    /* format the new pw check entry */
    rv = encodePWCheckEntry(NULL, &dbkey->derPK, SEC_OID_RC4, &checkitem);
    if ( rv != SECSuccess ) {
	goto done;
    }

    rv = put_dbkey(handle, &checkKey, dbkey, PR_TRUE);
    if ( rv != SECSuccess ) {
	goto done;
    }

    /* free the dbkey */
    sec_destroy_dbkey(dbkey);
    dbkey = NULL;
    
    /* now traverse the database */
    ret = (* handle->updatedb->seq)(handle->updatedb, &key, &data, R_FIRST);
    if ( ret ) {
	goto done;
    }
    
    do {
	/* skip version record */
	if ( data.size > 1 ) {
	    /* skip salt */
	    if ( key.size == ( sizeof(SALT_STRING) - 1 ) ) {
		if ( PORT_Memcmp(key.data, SALT_STRING, key.size) == 0 ) {
		    continue;
		}
	    }
	    /* skip pw check entry */
	    if ( key.size == checkKey.size ) {
		if ( PORT_Memcmp(key.data, checkKey.data, key.size) == 0 ) {
		    continue;
		}
	    }

	    /* keys stored by nickname will have 0 as the last byte of the
	     * db key.  Other keys must be stored by modulus.  We will not
	     * update those because they are left over from a keygen that
	     * never resulted in a cert.
	     */
	    if ( ((unsigned char *)key.data)[key.size-1] != 0 ) {
		continue;
	    }
	    
	    dbkey = decode_dbkey(&data, 2);
	    if ( dbkey == NULL ) {
		continue;
	    }

	    /* This puts the key into the new database with the same
	     * index (nickname) that it had before.  The second pass
	     * of the update will have the password.  It will decrypt
	     * and re-encrypt the entries using a new algorithm.
	     */
	    dbkey->nickname = (char *)key.data;
	    rv = put_dbkey(handle, &key, dbkey, PR_FALSE);
	    dbkey->nickname = NULL;

	    sec_destroy_dbkey(dbkey);
	}
    } while ( (* handle->updatedb->seq)(handle->updatedb, &key, &data,
					R_NEXT) == 0 );

    dbkey = NULL;

done:
    /* sync the database */
    ret = (* handle->db->sync)(handle->db, 0);

    (* handle->updatedb->close)(handle->updatedb);
    handle->updatedb = NULL;
    
    if ( rc4key ) {
	SECITEM_FreeItem(rc4key, PR_TRUE);
    }
    
    if ( oldSalt ) {
	SECITEM_FreeItem(oldSalt, PR_TRUE);
    }
    
    if ( dbkey ) {
	sec_destroy_dbkey(dbkey);
    }

    return(SECSuccess);
}

/*
 * Clear out all the keys in the existing database
 */
SECStatus
SECKEY_ResetKeyDB(SECKEYKeyDBHandle *handle)
{
    SECStatus rv;
    DBT key;
    DBT data;
    int ret;
    int errors = 0;

    if ( handle->db == NULL ) {
	return(SECSuccess);
    }

    
    /* now traverse the database */
    ret = (* handle->db->seq)(handle->db, &key, &data, R_FIRST);
    if ( ret ) {
	goto done;
    }
    
    do {
        /* delete each entry */
	ret = (* handle->db->del)(handle->db, &key, 0);
	if ( ret ) errors++;

    } while ( (* handle->db->seq)(handle->db, &key, &data,
					R_NEXT) == 0 );
    rv = makeGlobalVersion(handle);
    if ( rv != SECSuccess ) {
	errors++;
	goto done;
    }

    rv = makeGlobalSalt(handle);
    if ( rv != SECSuccess ) {
	errors++;
	goto done;
    }

    if (handle->global_salt) {
	SECITEM_FreeItem(handle->global_salt,PR_TRUE);
    }
    handle->global_salt = GetKeyDBGlobalSalt(handle);

done:
    /* sync the database */
    ret = (* handle->db->sync)(handle->db, 0);

    return (errors == 0 ? SECSuccess : SECFailure);
}
