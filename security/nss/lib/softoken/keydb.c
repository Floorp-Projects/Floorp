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
 *   Dr Vipul Gupta <vipul.gupta@sun.com>, Sun Microsystems Laboratories
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
/* $Id: keydb.c,v 1.39 2004/06/05 00:50:32 jpierre%netscape.com Exp $ */

#include "lowkeyi.h"
#include "seccomon.h"
#include "sechash.h"
#include "secder.h"
#include "secasn1.h"
#include "secoid.h"
#include "blapi.h"
#include "secitem.h"
#include "pcert.h"
#include "mcom_db.h"
#include "lowpbe.h"
#include "secerr.h"
#include "cdbhdl.h"
#include "nsslocks.h"

#include "keydbi.h"

#ifdef NSS_ENABLE_ECC
extern SECStatus EC_FillParams(PRArenaPool *arena, 
			       const SECItem *encodedParams, 
			       ECParams *params);
#endif

/*
 * Record keys for keydb
 */
#define SALT_STRING "global-salt"
#define VERSION_STRING "Version"
#define KEYDB_PW_CHECK_STRING	"password-check"
#define KEYDB_PW_CHECK_LEN	14
#define KEYDB_FAKE_PW_CHECK_STRING	"fake-password-check"
#define KEYDB_FAKE_PW_CHECK_LEN	19

/* Size of the global salt for key database */
#define SALT_LENGTH     16

const SEC_ASN1Template nsslowkey_AttributeTemplate[] = {
    { SEC_ASN1_SEQUENCE, 
	0, NULL, sizeof(NSSLOWKEYAttribute) },
    { SEC_ASN1_OBJECT_ID, offsetof(NSSLOWKEYAttribute, attrType) },
    { SEC_ASN1_SET_OF, offsetof(NSSLOWKEYAttribute, attrValue), 
	SEC_AnyTemplate },
    { 0 }
};

const SEC_ASN1Template nsslowkey_SetOfAttributeTemplate[] = {
    { SEC_ASN1_SET_OF, 0, nsslowkey_AttributeTemplate },
};
/* ASN1 Templates for new decoder/encoder */
const SEC_ASN1Template nsslowkey_PrivateKeyInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	0, NULL, sizeof(NSSLOWKEYPrivateKeyInfo) },
    { SEC_ASN1_INTEGER,
	offsetof(NSSLOWKEYPrivateKeyInfo,version) },
    { SEC_ASN1_INLINE,
	offsetof(NSSLOWKEYPrivateKeyInfo,algorithm),
	SECOID_AlgorithmIDTemplate },
    { SEC_ASN1_OCTET_STRING,
	offsetof(NSSLOWKEYPrivateKeyInfo,privateKey) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	offsetof(NSSLOWKEYPrivateKeyInfo, attributes),
	nsslowkey_SetOfAttributeTemplate },
    { 0 }
};

const SEC_ASN1Template nsslowkey_PointerToPrivateKeyInfoTemplate[] = {
    { SEC_ASN1_POINTER, 0, nsslowkey_PrivateKeyInfoTemplate }
};

const SEC_ASN1Template nsslowkey_EncryptedPrivateKeyInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	0, NULL, sizeof(NSSLOWKEYEncryptedPrivateKeyInfo) },
    { SEC_ASN1_INLINE,
	offsetof(NSSLOWKEYEncryptedPrivateKeyInfo,algorithm),
	SECOID_AlgorithmIDTemplate },
    { SEC_ASN1_OCTET_STRING,
	offsetof(NSSLOWKEYEncryptedPrivateKeyInfo,encryptedData) },
    { 0 }
};

const SEC_ASN1Template nsslowkey_PointerToEncryptedPrivateKeyInfoTemplate[] = {
	{ SEC_ASN1_POINTER, 0, nsslowkey_EncryptedPrivateKeyInfoTemplate }
};


/* ====== Default key databse encryption algorithm ====== */

static SECOidTag defaultKeyDBAlg = SEC_OID_PKCS12_PBE_WITH_SHA1_AND_TRIPLE_DES_CBC;

/*
 * Default algorithm for encrypting data in the key database
 */
SECOidTag
nsslowkey_GetDefaultKeyDBAlg(void)
{
    return(defaultKeyDBAlg);
}

void
nsslowkey_SetDefaultKeyDBAlg(SECOidTag alg)
{
    defaultKeyDBAlg = alg;

    return;
}

static void
sec_destroy_dbkey(NSSLOWKEYDBKey *dbkey)
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

static int keydb_Get(DB *db, DBT *key, DBT *data, unsigned int flags);
static int keydb_Put(DB *db, DBT *key, DBT *data, unsigned int flags);
static int keydb_Sync(DB *db, unsigned int flags);
static int keydb_Del(DB *db, DBT *key, unsigned int flags);
static int keydb_Seq(DB *db, DBT *key, DBT *data, unsigned int flags);
static void keydb_Close(DB *db);

static PZLock *kdbLock = NULL;

static void
keydb_InitLocks(NSSLOWKEYDBHandle *handle) 
{
    if (kdbLock == NULL) {
	nss_InitLock(&kdbLock, nssILockKeyDB);
    }

    return;
}

static void
keydb_DestroyLocks(NSSLOWKEYDBHandle *handle)
{
    if (kdbLock != NULL) {
	PZ_DestroyLock(kdbLock);
	kdbLock = NULL;
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
encode_dbkey(NSSLOWKEYDBKey *dbkey,unsigned char version)
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
    buf[0] = version;

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

static NSSLOWKEYDBKey *
decode_dbkey(DBT *bufitem, int expectedVersion)
{
    NSSLOWKEYDBKey *dbkey;
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
    
    dbkey = (NSSLOWKEYDBKey *)PORT_ArenaZAlloc(arena, sizeof(NSSLOWKEYDBKey));
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
    
    if ( expectedVersion >= 3 ) {
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

static NSSLOWKEYDBKey *
get_dbkey(NSSLOWKEYDBHandle *handle, DBT *index)
{
    NSSLOWKEYDBKey *dbkey;
    DBT entry;
    int ret;
    
    /* get it from the database */
    ret = keydb_Get(handle->db, index, &entry, 0);
    if ( ret ) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	return NULL;
    }

    /* set up dbkey struct */

    dbkey = decode_dbkey(&entry, handle->version);

    return(dbkey);
}

static SECStatus
put_dbkey(NSSLOWKEYDBHandle *handle, DBT *index, NSSLOWKEYDBKey *dbkey, PRBool update)
{
    DBT *keydata = NULL;
    int status;
    
    keydata = encode_dbkey(dbkey, handle->version);
    if ( keydata == NULL ) {
	goto loser;
    }
    
    /* put it in the database */
    if ( update ) {
	status = keydb_Put(handle->db, index, keydata, 0);
    } else {
	status = keydb_Put(handle->db, index, keydata, R_NOOVERWRITE);
    }
    
    if ( status ) {
	goto loser;
    }

    /* sync the database */
    status = keydb_Sync(handle->db, 0);
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
nsslowkey_TraverseKeys(NSSLOWKEYDBHandle *handle, 
		 SECStatus (* keyfunc)(DBT *k, DBT *d, void *pdata),
		 void *udata )
{
    DBT data;
    DBT key;
    SECStatus status;
    int ret;

    if (handle == NULL) {
	return(SECFailure);
    }

    ret = keydb_Seq(handle->db, &key, &data, R_FIRST);
    if ( ret ) {
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
    } while ( keydb_Seq(handle->db, &key, &data, R_NEXT) == 0 );

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
GetKeyDBGlobalSalt(NSSLOWKEYDBHandle *handle)
{
    DBT saltKey;
    DBT saltData;
    int ret;
    
    saltKey.data = SALT_STRING;
    saltKey.size = sizeof(SALT_STRING) - 1;

    ret = keydb_Get(handle->db, &saltKey, &saltData, 0);
    if ( ret ) {
	return(NULL);
    }

    return(decodeKeyDBGlobalSalt(&saltData));
}

static SECStatus
StoreKeyDBGlobalSalt(NSSLOWKEYDBHandle *handle)
{
    DBT saltKey;
    DBT saltData;
    int status;
    
    saltKey.data = SALT_STRING;
    saltKey.size = sizeof(SALT_STRING) - 1;

    saltData.data = (void *)handle->global_salt->data;
    saltData.size = handle->global_salt->len;

    /* put global salt into the database now */
    status = keydb_Put(handle->db, &saltKey, &saltData, 0);
    if ( status ) {
	return(SECFailure);
    }

    return(SECSuccess);
}

static SECStatus
makeGlobalVersion(NSSLOWKEYDBHandle *handle)
{
    unsigned char version;
    DBT versionData;
    DBT versionKey;
    int status;
    
    version = NSSLOWKEY_DB_FILE_VERSION;
    versionData.data = &version;
    versionData.size = 1;
    versionKey.data = VERSION_STRING;
    versionKey.size = sizeof(VERSION_STRING)-1;
		
    /* put version string into the database now */
    status = keydb_Put(handle->db, &versionKey, &versionData, 0);
    if ( status ) {
	return(SECFailure);
    }
    handle->version = version;

    return(SECSuccess);
}


static SECStatus
makeGlobalSalt(NSSLOWKEYDBHandle *handle)
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
    status = keydb_Put(handle->db, &saltKey, &saltData, 0);
    if ( status ) {
	return(SECFailure);
    }

    return(SECSuccess);
}

static SECStatus
ChangeKeyDBPasswordAlg(NSSLOWKEYDBHandle *handle,
		       SECItem *oldpwitem, SECItem *newpwitem,
		       SECOidTag new_algorithm);
/*
 * Second pass of updating the key db.  This time we have a password.
 */
static SECStatus
nsslowkey_UpdateKeyDBPass2(NSSLOWKEYDBHandle *handle, SECItem *pwitem)
{
    SECStatus rv;
    
    rv = ChangeKeyDBPasswordAlg(handle, pwitem, pwitem,
			     nsslowkey_GetDefaultKeyDBAlg());
    
    return(rv);
}

static SECStatus
encodePWCheckEntry(PLArenaPool *arena, SECItem *entry, SECOidTag alg,
		   SECItem *encCheck);

static unsigned char
nsslowkey_version(DB *db)
{
    DBT versionKey;
    DBT versionData;
    int ret;
    versionKey.data = VERSION_STRING;
    versionKey.size = sizeof(VERSION_STRING)-1;

    /* lookup version string in database */
    ret = keydb_Get( db, &versionKey, &versionData, 0 );

    /* error accessing the database */
    if ( ret < 0 ) {
	return 255;
    }

    if ( ret >= 1 ) {
	return 0;
    }
    return *( (unsigned char *)versionData.data);
}

static PRBool
seckey_HasAServerKey(DB *db)
{
    DBT key;
    DBT data;
    int ret;
    PRBool found = PR_FALSE;

    ret = keydb_Seq(db, &key, &data, R_FIRST);
    if ( ret ) {
	return PR_FALSE;
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
	    if ( key.size == KEYDB_PW_CHECK_LEN ) {
		if ( PORT_Memcmp(key.data, KEYDB_PW_CHECK_STRING, 
						KEYDB_PW_CHECK_LEN) == 0 ) {
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

	    if (PORT_Strcmp(key.data,"Server-Key") == 0) {
		found = PR_TRUE;
	        break;
	    }
	    
	}
    } while ( keydb_Seq(db, &key, &data, R_NEXT) == 0 );

    return found;
}
/*
 * currently updates key database from v2 to v3
 */
static SECStatus
nsslowkey_UpdateKeyDBPass1(NSSLOWKEYDBHandle *handle)
{
    SECStatus rv;
    DBT checkKey;
    DBT checkData;
    DBT saltKey;
    DBT saltData;
    DBT key;
    DBT data;
    unsigned char version;
    SECItem *rc4key = NULL;
    NSSLOWKEYDBKey *dbkey = NULL;
    SECItem *oldSalt = NULL;
    int ret;
    SECItem checkitem;

    if ( handle->updatedb == NULL ) {
	return(SECSuccess);
    }

    /*
     * check the version record
     */
    version = nsslowkey_version(handle->updatedb);
    if (version != 2) {
	goto done;
    }

    saltKey.data = SALT_STRING;
    saltKey.size = sizeof(SALT_STRING) - 1;

    ret = keydb_Get(handle->updatedb, &saltKey, &saltData, 0);
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
    
    ret = keydb_Get(handle->updatedb, &checkKey,
				   &checkData, 0 );
    if (ret) {
	/*
	 * if we have a key, but no KEYDB_PW_CHECK_STRING, then this must
	 * be an old server database, and it does have a password associated
	 * with it. Put a fake entry in so we can identify this db when we do
	 * get the password for it.
	 */
	if (seckey_HasAServerKey(handle->updatedb)) {
	    DBT fcheckKey;
	    DBT fcheckData;

	    /*
	     * include a fake string
	     */
	    fcheckKey.data = KEYDB_FAKE_PW_CHECK_STRING;
	    fcheckKey.size = KEYDB_FAKE_PW_CHECK_LEN;
	    fcheckData.data = "1";
	    fcheckData.size = 1;
	    /* put global salt into the new database now */
	    ret = keydb_Put( handle->db, &saltKey, &saltData, 0);
	    if ( ret ) {
		goto done;
	    }
	    ret = keydb_Put( handle->db, &fcheckKey, &fcheckData, 0);
	    if ( ret ) {
		goto done;
	    }
	} else {
	    goto done;
	}
    } else {
	/* put global salt into the new database now */
	ret = keydb_Put( handle->db, &saltKey, &saltData, 0);
	if ( ret ) {
	    goto done;
	}

	dbkey = decode_dbkey(&checkData, 2);
	if ( dbkey == NULL ) {
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
    }
    
    
    /* now traverse the database */
    ret = keydb_Seq(handle->updatedb, &key, &data, R_FIRST);
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
    } while ( keydb_Seq(handle->updatedb, &key, &data,
					R_NEXT) == 0 );

    dbkey = NULL;

done:
    /* sync the database */
    ret = keydb_Sync(handle->db, 0);

    keydb_Close(handle->updatedb);
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

static SECStatus
openNewDB(const char *appName, const char *prefix, const char *dbname, 
	NSSLOWKEYDBHandle *handle, NSSLOWKEYDBNameFunc namecb, void *cbarg)
{
    SECStatus rv = SECFailure;
    int status = RDB_FAIL;
    char *updname = NULL;
    DB *updatedb = NULL;
    PRBool updated = PR_FALSE;
    int ret;

    if (appName) {
	handle->db = rdbopen( appName, prefix, "key", NO_CREATE, &status);
    } else {
	handle->db = dbopen( dbname, NO_CREATE, 0600, DB_HASH, 0 );
    }
    /* if create fails then we lose */
    if ( handle->db == NULL ) {
	return (status == RDB_RETRY) ? SECWouldBlock: SECFailure;
    }

    rv = db_BeginTransaction(handle->db);
    if (rv != SECSuccess) {
	db_InitComplete(handle->db);
	return rv;
    }

    /* force a transactional read, which will verify that one and only one
     * process attempts the update. */
    if (nsslowkey_version(handle->db) == NSSLOWKEY_DB_FILE_VERSION) {
	/* someone else has already updated the database for us */
	db_FinishTransaction(handle->db, PR_FALSE);
	db_InitComplete(handle->db);
	return SECSuccess;
    }

    /*
     * if we are creating a multiaccess database, see if there is a
     * local database we can update from.
     */
    if (appName) {
	updatedb = dbopen( dbname, NO_RDONLY, 0600, DB_HASH, 0 );
	if (updatedb) {
	    handle->version = nsslowkey_version(updatedb);
	    if (handle->version != NSSLOWKEY_DB_FILE_VERSION) {
		keydb_Close(updatedb);
	    } else {
		db_Copy(handle->db, updatedb);
		keydb_Close(updatedb);
		db_FinishTransaction(handle->db,PR_FALSE);
		db_InitComplete(handle->db);
		return SECSuccess;
	    }
	}
    }

    /* update the version number */
    rv = makeGlobalVersion(handle);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    /*
     * try to update from v2 db
     */
    updname = (*namecb)(cbarg, 2);
    if ( updname != NULL ) {
	handle->updatedb = dbopen( updname, NO_RDONLY, 0600, DB_HASH, 0 );
        PORT_Free( updname );

	if ( handle->updatedb ) {
	    /*
	     * Try to update the db using a null password.  If the db
	     * doesn't have a password, then this will work.  If it does
	     * have a password, then this will fail and we will do the
	     * update later
	     */
	    rv = nsslowkey_UpdateKeyDBPass1(handle);
	    if ( rv == SECSuccess ) {
		updated = PR_TRUE;
	    }
	}
	    
    }

    /* we are using the old salt if we updated from an old db */
    if ( ! updated ) {
	rv = makeGlobalSalt(handle);
	if ( rv != SECSuccess ) {
	   goto loser;
	}
    }
	
    /* sync the database */
    ret = keydb_Sync(handle->db, 0);
    if ( ret ) {
	rv = SECFailure;
	goto loser;
    }
    rv = SECSuccess;

loser:
    db_FinishTransaction(handle->db, rv != SECSuccess);
    db_InitComplete(handle->db);
    return rv;
}


static DB *
openOldDB(const char *appName, const char *prefix, const char *dbname, 
					PRBool openflags, int *version) {
    DB *db = NULL;

    if (appName) {
	db = rdbopen( appName, prefix, "key", openflags, NULL);
    } else {
	db = dbopen( dbname, openflags, 0600, DB_HASH, 0 );
    }

    /* check for correct version number */
    if (db != NULL) {
	*version = nsslowkey_version(db);
	if (*version != NSSLOWKEY_DB_FILE_VERSION ) {
	    /* bogus version number record, reset the database */
	    keydb_Close( db );
	    db = NULL;
	}
    }
    return db;
}

NSSLOWKEYDBHandle *
nsslowkey_OpenKeyDB(PRBool readOnly, const char *appName, const char *prefix,
				NSSLOWKEYDBNameFunc namecb, void *cbarg)
{
    NSSLOWKEYDBHandle *handle;
    SECStatus rv;
    int openflags;
    char *dbname = NULL;
    
    handle = (NSSLOWKEYDBHandle *)PORT_ZAlloc (sizeof(NSSLOWKEYDBHandle));
    if (handle == NULL) {
	PORT_SetError (SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    openflags = readOnly ? NO_RDONLY : NO_RDWR;

    dbname = (*namecb)(cbarg, NSSLOWKEY_DB_FILE_VERSION);
    if ( dbname == NULL ) {
	goto loser;
    }

    handle->appname = appName ? PORT_Strdup(appName) : NULL ;
    handle->dbname = (appName == NULL) ? PORT_Strdup(dbname) : 
			(prefix ? PORT_Strdup(prefix) : NULL);
    handle->readOnly = readOnly;

    keydb_InitLocks(handle);


    handle->db = openOldDB(appName, prefix, dbname, openflags, 
							&handle->version);
    if (handle->version == 255) {
	goto loser;
    }
  

    /* if first open fails, try to create a new DB */
    if ( handle->db == NULL ) {
	if ( readOnly ) {
	    goto loser;
	}

	rv = openNewDB(appName, prefix, dbname, handle, namecb, cbarg);
	/* two processes started to initialize the database at the same time.
	 * The multiprocess code blocked the second one, then had it retry to
	 * see if it can just open the database normally */
	if (rv == SECWouldBlock) {
	    handle->db = openOldDB(appName,prefix,dbname, 
						openflags, &handle->version);
	    if (handle->db == NULL) {
		goto loser;
	    }
	} else if (rv != SECSuccess) {
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
	keydb_Close(handle->db);
    }
    if ( handle->updatedb ) {
	keydb_Close(handle->updatedb);
    }
    PORT_Free(handle);
    return NULL;
}

/*
 * Close the database
 */
void
nsslowkey_CloseKeyDB(NSSLOWKEYDBHandle *handle)
{
    if (handle != NULL) {
	if (handle->db != NULL) {
	    keydb_Close(handle->db);
	}
	if (handle->dbname) PORT_Free(handle->dbname);
	if (handle->appname) PORT_Free(handle->appname);
	if (handle->global_salt) {
	    SECITEM_FreeItem(handle->global_salt,PR_TRUE);
	}
	keydb_DestroyLocks(handle);
	    
	PORT_Free(handle);
    }
}

/* Get the key database version */
int
nsslowkey_GetKeyDBVersion(NSSLOWKEYDBHandle *handle)
{
    PORT_Assert(handle != NULL);

    return handle->version;
}

/*
 * Delete a private key that was stored in the database
 */
SECStatus
nsslowkey_DeleteKey(NSSLOWKEYDBHandle *handle, SECItem *pubkey)
{
    DBT namekey;
    int ret;

    if (handle == NULL) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	return(SECFailure);
    }

    /* set up db key and data */
    namekey.data = pubkey->data;
    namekey.size = pubkey->len;

    /* delete it from the database */
    ret = keydb_Del(handle->db, &namekey, 0);
    if ( ret ) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	return(SECFailure);
    }

    /* sync the database */
    ret = keydb_Sync(handle->db, 0);
    if ( ret ) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	return(SECFailure);
    }

    return(SECSuccess);
}

/*
 * Store a key in the database, indexed by its public key modulus.(value!)
 */
SECStatus
nsslowkey_StoreKeyByPublicKey(NSSLOWKEYDBHandle *handle, 
			   NSSLOWKEYPrivateKey *privkey,
			   SECItem *pubKeyData,
			   char *nickname,
			   SECItem *arg)
{
    return nsslowkey_StoreKeyByPublicKeyAlg(handle, privkey, pubKeyData, 
	     nickname, arg, nsslowkey_GetDefaultKeyDBAlg(),PR_FALSE);
}

SECStatus
nsslowkey_UpdateNickname(NSSLOWKEYDBHandle *handle, 
			   NSSLOWKEYPrivateKey *privkey,
			   SECItem *pubKeyData,
			   char *nickname,
			   SECItem *arg)
{
    return nsslowkey_StoreKeyByPublicKeyAlg(handle, privkey, pubKeyData, 
	     nickname, arg, nsslowkey_GetDefaultKeyDBAlg(),PR_TRUE);
}

/* see if the symetric CKA_ID already Exists.
 */
PRBool
nsslowkey_KeyForIDExists(NSSLOWKEYDBHandle *handle, SECItem *id)
{
    DBT namekey;
    DBT dummy;
    int status;

    namekey.data = (char *)id->data;
    namekey.size = id->len;
    status = keydb_Get(handle->db, &namekey, &dummy, 0);
    if ( status ) {
	return PR_FALSE;
    }
    
    return PR_TRUE;
}

/* see if the public key for this cert is in the database filed
 * by modulus
 */
PRBool
nsslowkey_KeyForCertExists(NSSLOWKEYDBHandle *handle, NSSLOWCERTCertificate *cert)
{
    NSSLOWKEYPublicKey *pubkey = NULL;
    DBT namekey;
    DBT dummy;
    int status;
    
    /* get cert's public key */
    pubkey = nsslowcert_ExtractPublicKey(cert);
    if ( pubkey == NULL ) {
	return PR_FALSE;
    }

    /* TNH - make key from NSSLOWKEYPublicKey */
    switch (pubkey->keyType) {
      case NSSLOWKEYRSAKey:
	namekey.data = pubkey->u.rsa.modulus.data;
	namekey.size = pubkey->u.rsa.modulus.len;
	break;
      case NSSLOWKEYDSAKey:
	namekey.data = pubkey->u.dsa.publicValue.data;
	namekey.size = pubkey->u.dsa.publicValue.len;
	break;
      case NSSLOWKEYDHKey:
	namekey.data = pubkey->u.dh.publicValue.data;
	namekey.size = pubkey->u.dh.publicValue.len;
	break;
#ifdef NSS_ENABLE_ECC
      case NSSLOWKEYECKey:
	namekey.data = pubkey->u.ec.publicValue.data;
	namekey.size = pubkey->u.ec.publicValue.len;
	break;
#endif /* NSS_ENABLE_ECC */
      default:
	/* XXX We don't do Fortezza or DH yet. */
	return PR_FALSE;
    }

    if (handle->version != 3) {
	unsigned char buf[SHA1_LENGTH];
	SHA1_HashBuf(buf,namekey.data,namekey.size);
	/* NOTE: don't use pubkey after this! it's now thrashed */
	PORT_Memcpy(namekey.data,buf,sizeof(buf));
	namekey.size = sizeof(buf);
    }

    status = keydb_Get(handle->db, &namekey, &dummy, 0);
    /* some databases have the key stored as a signed value */
    if (status) {
	unsigned char *buf = (unsigned char *)PORT_Alloc(namekey.size+1);
	if (buf) {
	    PORT_Memcpy(&buf[1], namekey.data, namekey.size);
	    buf[0] = 0;
	    namekey.data = buf;
	    namekey.size ++;
    	    status = keydb_Get(handle->db, &namekey, &dummy, 0);
	    PORT_Free(buf);
	}
    }
    nsslowkey_DestroyPublicKey(pubkey);
    if ( status ) {
	return PR_FALSE;
    }
    
    return PR_TRUE;
}

/*
 * check to see if the user has a password
 */
SECStatus
nsslowkey_HasKeyDBPassword(NSSLOWKEYDBHandle *handle)
{
    DBT checkkey, checkdata;
    int ret;

    if (handle == NULL) {
	return(SECFailure);
    }

    checkkey.data = KEYDB_PW_CHECK_STRING;
    checkkey.size = KEYDB_PW_CHECK_LEN;
    
    ret = keydb_Get(handle->db, &checkkey, &checkdata, 0 );
    if ( ret ) {
	/* see if this was an updated DB first */
	checkkey.data = KEYDB_FAKE_PW_CHECK_STRING;
	checkkey.size = KEYDB_FAKE_PW_CHECK_LEN;
	ret = keydb_Get(handle->db, &checkkey, &checkdata, 0 );
    	if ( ret ) {
	    return(SECFailure);
	}
    }

    return(SECSuccess);
}

/*
 * Set up the password checker in the key database.
 * This is done by encrypting a known plaintext with the user's key.
 */
SECStatus
nsslowkey_SetKeyDBPassword(NSSLOWKEYDBHandle *handle, SECItem *pwitem)
{
    return nsslowkey_SetKeyDBPasswordAlg(handle, pwitem,
				      nsslowkey_GetDefaultKeyDBAlg());
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
nsslowkey_HashPassword(char *pw, SECItem *salt)
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
nsslowkey_DeriveKeyDBPassword(NSSLOWKEYDBHandle *keydb, char *pw)
{
    PORT_Assert(keydb != NULL);
    PORT_Assert(pw != NULL);
    if (keydb == NULL || pw == NULL) return(NULL);

    return nsslowkey_HashPassword(pw, keydb->global_salt);
}

#if 0
/* Appears obsolete - TNH */
/* get the algorithm with which a private key
 * is encrypted.
 */
SECOidTag 
seckey_get_private_key_algorithm(NSSLOWKEYDBHandle *keydb, DBT *index)   
{
    NSSLOWKEYDBKey *dbkey = NULL;
    SECOidTag algorithm = SEC_OID_UNKNOWN;
    NSSLOWKEYEncryptedPrivateKeyInfo *epki = NULL;
    PLArenaPool *poolp = NULL;
    SECStatus rv;

    poolp = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if(poolp == NULL)
	return (SECOidTag)SECFailure;  /* TNH - this is bad */

    dbkey = get_dbkey(keydb, index);
    if(dbkey == NULL)
	return (SECOidTag)SECFailure;

    epki = (NSSLOWKEYEncryptedPrivateKeyInfo *)PORT_ArenaZAlloc(poolp, 
	sizeof(NSSLOWKEYEncryptedPrivateKeyInfo));
    if(epki == NULL)
	goto loser;
    rv = SEC_ASN1DecodeItem(poolp, epki, 
	nsslowkey_EncryptedPrivateKeyInfoTemplate, &dbkey->derPK);
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
seckey_rc4_decode(SECItem *key, SECItem *src)
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


#ifdef EC_DEBUG
#define SEC_PRINT(str1, str2, num, sitem) \
    printf("pkcs11c.c:%s:%s (keytype=%d) [len=%d]\n", \
            str1, str2, num, sitem->len); \
    for (i = 0; i < sitem->len; i++) { \
	    printf("%02x:", sitem->data[i]); \
    } \
    printf("\n") 
#else
#define SEC_PRINT(a, b, c, d) 
#endif /* EC_DEBUG */

/* TNH - keydb is unused */
/* TNH - the pwitem should be the derived key for RC4 */
NSSLOWKEYEncryptedPrivateKeyInfo *
seckey_encrypt_private_key(
        NSSLOWKEYPrivateKey *pk, SECItem *pwitem, NSSLOWKEYDBHandle *keydb,
	SECOidTag algorithm, SECItem **salt)
{
    NSSLOWKEYEncryptedPrivateKeyInfo *epki = NULL;
    NSSLOWKEYPrivateKeyInfo *pki = NULL;
    SECStatus rv = SECFailure;
    PLArenaPool *temparena = NULL, *permarena = NULL;
    SECItem *der_item = NULL;
    NSSPKCS5PBEParameter *param = NULL;
    SECItem *dummy = NULL, *dest = NULL;
    SECAlgorithmID *algid;
#ifdef NSS_ENABLE_ECC
    SECItem *fordebug = NULL;
    int savelen;
    int i;
#endif

    *salt = NULL;
    permarena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if(permarena == NULL)
	return NULL;

    temparena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if(temparena == NULL)
	goto loser;

    /* allocate structures */
    epki = (NSSLOWKEYEncryptedPrivateKeyInfo *)PORT_ArenaZAlloc(permarena,
	sizeof(NSSLOWKEYEncryptedPrivateKeyInfo));
    pki = (NSSLOWKEYPrivateKeyInfo *)PORT_ArenaZAlloc(temparena, 
	sizeof(NSSLOWKEYPrivateKeyInfo));
    der_item = (SECItem *)PORT_ArenaZAlloc(temparena, sizeof(SECItem));
    if((epki == NULL) || (pki == NULL) || (der_item == NULL))
	goto loser;

    epki->arena = permarena;

    /* setup private key info */
    dummy = SEC_ASN1EncodeInteger(temparena, &(pki->version), 
	NSSLOWKEY_PRIVATE_KEY_INFO_VERSION);
    if(dummy == NULL)
	goto loser;

    /* Encode the key, and set the algorithm (with params) */
    switch (pk->keyType) {
      case NSSLOWKEYRSAKey:
        prepare_low_rsa_priv_key_for_asn1(pk);
	dummy = SEC_ASN1EncodeItem(temparena, &(pki->privateKey), pk, 
				   nsslowkey_RSAPrivateKeyTemplate);
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
      case NSSLOWKEYDSAKey:
        prepare_low_dsa_priv_key_for_asn1(pk);
	dummy = SEC_ASN1EncodeItem(temparena, &(pki->privateKey), pk,
				   nsslowkey_DSAPrivateKeyTemplate);
	if (dummy == NULL) {
	    rv = SECFailure;
	    goto loser;
	}
	
        prepare_low_pqg_params_for_asn1(&pk->u.dsa.params);
	dummy = SEC_ASN1EncodeItem(temparena, NULL, &pk->u.dsa.params,
				   nsslowkey_PQGParamsTemplate);
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
      case NSSLOWKEYDHKey:
        prepare_low_dh_priv_key_for_asn1(pk);
	dummy = SEC_ASN1EncodeItem(temparena, &(pki->privateKey), pk,
				   nsslowkey_DHPrivateKeyTemplate);
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
#ifdef NSS_ENABLE_ECC
      case NSSLOWKEYECKey:
	prepare_low_ec_priv_key_for_asn1(pk);
	/* Public value is encoded as a bit string so adjust length
	 * to be in bits before ASN encoding and readjust 
	 * immediately after.
	 *
	 * Since the SECG specification recommends not including the
	 * parameters as part of ECPrivateKey, we zero out the curveOID
	 * length before encoding and restore it later.
	 */
	pk->u.ec.publicValue.len <<= 3;
	savelen = pk->u.ec.ecParams.curveOID.len;
	pk->u.ec.ecParams.curveOID.len = 0;
	dummy = SEC_ASN1EncodeItem(temparena, &(pki->privateKey), pk,
				   nsslowkey_ECPrivateKeyTemplate);
	pk->u.ec.ecParams.curveOID.len = savelen;
	pk->u.ec.publicValue.len >>= 3;

	if (dummy == NULL) {
	    rv = SECFailure;
	    goto loser;
	}

	dummy = &pk->u.ec.ecParams.DEREncoding;

	/* At this point dummy should contain the encoded params */
	rv = SECOID_SetAlgorithmID(temparena, &(pki->algorithm),
				   SEC_OID_ANSIX962_EC_PUBLIC_KEY, dummy);

	if (rv == SECFailure) {
	    goto loser;
	}
	
	fordebug = &(pki->privateKey);
	SEC_PRINT("seckey_encrypt_private_key()", "PrivateKey", 
		  pk->keyType, fordebug);

	break;
#endif /* NSS_ENABLE_ECC */
      default:
	/* We don't support DH or Fortezza private keys yet */
	PORT_Assert(PR_FALSE);
	break;
    }

    /* setup encrypted private key info */
    dummy = SEC_ASN1EncodeItem(temparena, der_item, pki, 
	nsslowkey_PrivateKeyInfoTemplate);

    SEC_PRINT("seckey_encrypt_private_key()", "PrivateKeyInfo", 
	      pk->keyType, der_item);

    if(dummy == NULL) {
	rv = SECFailure;
	goto loser;
    }

    rv = SECFailure; /* assume failure */
    *salt = seckey_create_rc4_salt();
    if (*salt == NULL) {
	goto loser;
    }

    param = nsspkcs5_NewParam(algorithm,*salt,1);
    if (param == NULL) {
	goto loser;
    }

    dest = nsspkcs5_CipherData(param, pwitem, der_item, PR_TRUE, NULL);
    if (dest == NULL) {
	goto loser;
    }

    rv = SECITEM_CopyItem(permarena, &epki->encryptedData, dest);
    if (rv != SECSuccess) {
	goto loser;
    }

    algid = nsspkcs5_CreateAlgorithmID(permarena, algorithm, param);
    if (algid == NULL) {
	rv = SECFailure;
	goto loser;
    }

    rv = SECOID_CopyAlgorithmID(permarena, &epki->algorithm, algid);
    SECOID_DestroyAlgorithmID(algid, PR_TRUE);

loser:
    if(dest != NULL)
	SECITEM_FreeItem(dest, PR_TRUE);

    if(param != NULL)
       nsspkcs5_DestroyPBEParameter(param);

    /* let success fall through */

    if(rv == SECFailure)
    {
	PORT_FreeArena(permarena, PR_TRUE);
	epki = NULL;
	if(*salt != NULL)
	    SECITEM_FreeItem(*salt, PR_TRUE);
    }

    if(temparena != NULL)
	PORT_FreeArena(temparena, PR_TRUE);

    return epki;
}

static SECStatus 
seckey_put_private_key(NSSLOWKEYDBHandle *keydb, DBT *index, SECItem *pwitem,
		       NSSLOWKEYPrivateKey *pk, char *nickname, PRBool update,
		       SECOidTag algorithm)
{
    NSSLOWKEYDBKey *dbkey = NULL;
    NSSLOWKEYEncryptedPrivateKeyInfo *epki = NULL;
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

    dbkey = (NSSLOWKEYDBKey *)PORT_ArenaZAlloc(permarena, sizeof(NSSLOWKEYDBKey));
    if(dbkey == NULL)
	goto loser;
    dbkey->arena = permarena;
    dbkey->nickname = nickname;

    /* TNH - for RC4, the salt should be created here */
	
    epki = seckey_encrypt_private_key(pk, pwitem, keydb, algorithm, &salt);
    if(epki == NULL)
	goto loser;
    temparena = epki->arena;

    if(salt != NULL)
    {
	rv = SECITEM_CopyItem(permarena, &(dbkey->salt), salt);
	SECITEM_ZfreeItem(salt, PR_TRUE);
    }

    dummy = SEC_ASN1EncodeItem(permarena, &(dbkey->derPK), epki, 
	nsslowkey_EncryptedPrivateKeyInfoTemplate);
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
nsslowkey_StoreKeyByPublicKeyAlg(NSSLOWKEYDBHandle *handle, 
			      NSSLOWKEYPrivateKey *privkey,
			      SECItem *pubKeyData,
			      char *nickname,
			      SECItem *pwitem,
			      SECOidTag algorithm,
                              PRBool update)
{
    DBT namekey;
    SECStatus rv;

    if (handle == NULL) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	return(SECFailure);
    }

    /* set up db key and data */
    namekey.data = pubKeyData->data;
    namekey.size = pubKeyData->len;

    /* encrypt the private key */
    rv = seckey_put_private_key(handle, &namekey, pwitem, privkey, nickname,
				update, algorithm);
    
    return(rv);
}

NSSLOWKEYPrivateKey *
seckey_decrypt_private_key(NSSLOWKEYEncryptedPrivateKeyInfo *epki,
			   SECItem *pwitem)
{
    NSSLOWKEYPrivateKey *pk = NULL;
    NSSLOWKEYPrivateKeyInfo *pki = NULL;
    SECStatus rv = SECFailure;
    SECOidTag algorithm;
    PLArenaPool *temparena = NULL, *permarena = NULL;
    SECItem *salt = NULL, *dest = NULL, *key = NULL;
    NSSPKCS5PBEParameter *param;
#ifdef NSS_ENABLE_ECC
    ECPrivateKey *ecpriv;
    SECItem *fordebug = NULL;
    int i;
#endif

    if((epki == NULL) || (pwitem == NULL))
	goto loser;

    temparena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    permarena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if((temparena == NULL) || (permarena == NULL))
	goto loser;

    /* allocate temporary items */
    pki = (NSSLOWKEYPrivateKeyInfo *)PORT_ArenaZAlloc(temparena, 
	sizeof(NSSLOWKEYPrivateKeyInfo));

    /* allocate permanent arena items */
    pk = (NSSLOWKEYPrivateKey *)PORT_ArenaZAlloc(permarena,
	sizeof(NSSLOWKEYPrivateKey));

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
		    dest = seckey_rc4_decode(key, &epki->encryptedData);
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
	    param = nsspkcs5_AlgidToParam(&epki->algorithm);
	    if (param == NULL) {
		break;
	    }
	    dest = nsspkcs5_CipherData(param, pwitem, &epki->encryptedData, 
							PR_FALSE, NULL);
	    nsspkcs5_DestroyPBEParameter(param);
	    break;
    }

    if(dest != NULL)
    {
        SECItem newPrivateKey;
        SECItem newAlgParms;

        SEC_PRINT("seckey_decrypt_private_key()", "PrivateKeyInfo", -1,
		  dest);

	rv = SEC_QuickDERDecodeItem(temparena, pki, 
	    nsslowkey_PrivateKeyInfoTemplate, dest);
	if(rv == SECSuccess)
	{
	    switch(SECOID_GetAlgorithmTag(&pki->algorithm)) {
	      case SEC_OID_X500_RSA_ENCRYPTION:
	      case SEC_OID_PKCS1_RSA_ENCRYPTION:
		pk->keyType = NSSLOWKEYRSAKey;
		prepare_low_rsa_priv_key_for_asn1(pk);
                if (SECSuccess != SECITEM_CopyItem(permarena, &newPrivateKey,
                    &pki->privateKey) ) break;
		rv = SEC_QuickDERDecodeItem(permarena, pk,
					nsslowkey_RSAPrivateKeyTemplate,
					&newPrivateKey);
		break;
	      case SEC_OID_ANSIX9_DSA_SIGNATURE:
		pk->keyType = NSSLOWKEYDSAKey;
		prepare_low_dsa_priv_key_for_asn1(pk);
                if (SECSuccess != SECITEM_CopyItem(permarena, &newPrivateKey,
                    &pki->privateKey) ) break;
		rv = SEC_QuickDERDecodeItem(permarena, pk,
					nsslowkey_DSAPrivateKeyTemplate,
					&newPrivateKey);
		if (rv != SECSuccess)
		    goto loser;
		prepare_low_pqg_params_for_asn1(&pk->u.dsa.params);
                if (SECSuccess != SECITEM_CopyItem(permarena, &newAlgParms,
                    &pki->algorithm.parameters) ) break;
		rv = SEC_QuickDERDecodeItem(permarena, &pk->u.dsa.params,
					nsslowkey_PQGParamsTemplate,
					&newAlgParms);
		break;
	      case SEC_OID_X942_DIFFIE_HELMAN_KEY:
		pk->keyType = NSSLOWKEYDHKey;
		prepare_low_dh_priv_key_for_asn1(pk);
                if (SECSuccess != SECITEM_CopyItem(permarena, &newPrivateKey,
                    &pki->privateKey) ) break;
		rv = SEC_QuickDERDecodeItem(permarena, pk,
					nsslowkey_DHPrivateKeyTemplate,
					&newPrivateKey);
		break;
#ifdef NSS_ENABLE_ECC
	      case SEC_OID_ANSIX962_EC_PUBLIC_KEY:
		pk->keyType = NSSLOWKEYECKey;
		prepare_low_ec_priv_key_for_asn1(pk);

		fordebug = &pki->privateKey;
		SEC_PRINT("seckey_decrypt_private_key()", "PrivateKey", 
			  pk->keyType, fordebug);
                if (SECSuccess != SECITEM_CopyItem(permarena, &newPrivateKey,
                    &pki->privateKey) ) break;
		rv = SEC_QuickDERDecodeItem(permarena, pk,
					nsslowkey_ECPrivateKeyTemplate,
					&newPrivateKey);
		if (rv != SECSuccess)
		    goto loser;

		prepare_low_ecparams_for_asn1(&pk->u.ec.ecParams);

		rv = SECITEM_CopyItem(permarena, 
		    &pk->u.ec.ecParams.DEREncoding, 
		    &pki->algorithm.parameters);

		if (rv != SECSuccess)
		    goto loser;

		/* Fill out the rest of EC params */
		rv = EC_FillParams(permarena, &pk->u.ec.ecParams.DEREncoding,
				   &pk->u.ec.ecParams);

		/* 
		 * NOTE: Encoding of the publicValue is optional
		 * so we need to be able to regenerate the publicValue
		 * from the base point and the private key.
		 *
		 * XXX This part of the code needs more testing.
		 */
		if (pk->u.ec.publicValue.len == 0) {
		    rv = EC_NewKeyFromSeed(&pk->u.ec.ecParams, 
				      &ecpriv, pk->u.ec.privateValue.data,
				      pk->u.ec.privateValue.len);
		    if (rv == SECSuccess) {
			SECITEM_CopyItem(permarena, &pk->u.ec.publicValue,
					 &(ecpriv->publicValue));
			PORT_FreeArena(ecpriv->ecParams.arena, PR_TRUE);
		    }
		} else {
		    /* If publicValue was filled as part of DER decoding,
		     * change length in bits to length in bytes.
		     */
		    pk->u.ec.publicValue.len >>= 3;
		}

		break;
#endif /* NSS_ENABLE_ECC */
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

static NSSLOWKEYPrivateKey *
seckey_decode_encrypted_private_key(NSSLOWKEYDBKey *dbkey, SECItem *pwitem)
{
    NSSLOWKEYPrivateKey *pk = NULL;
    NSSLOWKEYEncryptedPrivateKeyInfo *epki;
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

    epki = (NSSLOWKEYEncryptedPrivateKeyInfo *)
	PORT_ArenaZAlloc(temparena, sizeof(NSSLOWKEYEncryptedPrivateKeyInfo));

    if(epki == NULL) {
	goto loser;
    }

    rv = SEC_QuickDERDecodeItem(temparena, epki,
			    nsslowkey_EncryptedPrivateKeyInfoTemplate,
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

NSSLOWKEYPrivateKey *
seckey_get_private_key(NSSLOWKEYDBHandle *keydb, DBT *index, char **nickname,
		       SECItem *pwitem)
{
    NSSLOWKEYDBKey *dbkey = NULL;
    NSSLOWKEYPrivateKey *pk = NULL;

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
NSSLOWKEYPrivateKey *
nsslowkey_DecryptKey(DBT *key, SECItem *pwitem,
					 NSSLOWKEYDBHandle *handle) {
    return seckey_get_private_key(handle,key,NULL,pwitem);
}

/*
 * Find a key in the database, indexed by its public key modulus
 * This is used to find keys that have been stored before their
 * certificate arrives.  Once the certificate arrives the key
 * is looked up by the public modulus in the certificate, and the
 * re-stored by its nickname.
 */
NSSLOWKEYPrivateKey *
nsslowkey_FindKeyByPublicKey(NSSLOWKEYDBHandle *handle, SECItem *modulus,
			  				 SECItem *pwitem)
{
    DBT namekey;
    NSSLOWKEYPrivateKey *pk = NULL;

    if (handle == NULL) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	return NULL;
    }

    /* set up db key */
    namekey.data = modulus->data;
    namekey.size = modulus->len;

    pk = seckey_get_private_key(handle, &namekey, NULL, pwitem);
    
    /* no need to free dbkey, since its on the stack, and the data it
     * points to is owned by the database
     */
    return(pk);
}

char *
nsslowkey_FindKeyNicknameByPublicKey(NSSLOWKEYDBHandle *handle, 
					SECItem *modulus, SECItem *pwitem)
{
    DBT namekey;
    NSSLOWKEYPrivateKey *pk = NULL;
    char *nickname = NULL;

    if (handle == NULL) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	return NULL;
    }

    /* set up db key */
    namekey.data = modulus->data;
    namekey.size = modulus->len;

    pk = seckey_get_private_key(handle, &namekey, &nickname, pwitem);
    if (pk) {
	nsslowkey_DestroyPrivateKey(pk);
    }
    
    /* no need to free dbkey, since its on the stack, and the data it
     * points to is owned by the database
     */
    return(nickname);
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
nsslowkey_SetKeyDBPasswordAlg(NSSLOWKEYDBHandle *handle, 
			   SECItem *pwitem, SECOidTag algorithm)
{
    DBT checkkey;
    NSSPKCS5PBEParameter *param = NULL;
    SECStatus rv = SECFailure;
    NSSLOWKEYDBKey *dbkey = NULL;
    PLArenaPool *arena;
    SECItem *salt = NULL;
    SECItem *dest = NULL, test_key;
    
    if (handle == NULL) {
	return(SECFailure);
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	rv = SECFailure;
	goto loser;
    }
    
    dbkey = (NSSLOWKEYDBKey *)PORT_ArenaZAlloc(arena, sizeof(NSSLOWKEYDBKey));
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

    param = nsspkcs5_NewParam(algorithm, salt, 1);
    if (param == NULL) {
	rv = SECFailure;
	goto loser;
    }

    dest = nsspkcs5_CipherData(param, pwitem, &test_key, PR_TRUE, NULL);
    if (dest == NULL)
    {
	rv = SECFailure;
	goto loser;
    }

    rv = SECITEM_CopyItem(arena, &dbkey->salt, salt);
    if (rv == SECFailure) {
	goto loser;
    }
   
    rv = encodePWCheckEntry(arena, &dbkey->derPK, algorithm, dest);
	
    if ( rv != SECSuccess ) {
	goto loser;
    }
	
    rv = put_dbkey(handle, &checkkey, dbkey, PR_TRUE);
    
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

    if (param != NULL) {
	nsspkcs5_DestroyPBEParameter(param);
    }
	
    return(rv);
}

static SECStatus
seckey_CheckKeyDB1Password(NSSLOWKEYDBHandle *handle, SECItem *pwitem)
{
    SECStatus rv = SECFailure;
    keyList keylist;
    keyNode *node = NULL;
    NSSLOWKEYPrivateKey *privkey = NULL;


    /*
     * first find a key
     */

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
    rv = nsslowkey_TraverseKeys(handle, sec_add_key_to_list, (void *)&keylist);
    if ( rv != SECSuccess ) 
	goto done;

    /* just get the first key from the list */
    node = keylist.head;

    /* no private keys, accept any password */
    if (node == NULL) {
	rv = SECSuccess;
	goto done;
    }
    privkey = seckey_get_private_key(handle, &node->key, NULL, pwitem);
    if (privkey == NULL) {
	 rv = SECFailure;
	 goto done;
    }

    /* if we can decrypt the private key, then we had the correct password */
    rv = SECSuccess;
    nsslowkey_DestroyPrivateKey(privkey);

done:

    /* free the arena */
    if ( keylist.arena ) {
	PORT_FreeArena(keylist.arena, PR_FALSE);
    }
    
    return(rv);
}

/*
 * check to see if the user has typed the right password
 */
SECStatus
nsslowkey_CheckKeyDBPassword(NSSLOWKEYDBHandle *handle, SECItem *pwitem)
{
    DBT checkkey;
    DBT checkdata;
    NSSPKCS5PBEParameter *param = NULL;
    SECStatus rv = SECFailure;
    NSSLOWKEYDBKey *dbkey = NULL;
    SECItem *key = NULL;
    SECItem *dest = NULL;
    SECOidTag algorithm;
    SECItem oid;
    SECItem encstring;
    PRBool update = PR_FALSE;
    int ret;
    
    if (handle == NULL) {
	goto loser;
    }

    checkkey.data = KEYDB_PW_CHECK_STRING;
    checkkey.size = KEYDB_PW_CHECK_LEN;
    
    dbkey = get_dbkey(handle, &checkkey);
    
    if ( dbkey == NULL ) {
	checkkey.data = KEYDB_FAKE_PW_CHECK_STRING;
	checkkey.size = KEYDB_FAKE_PW_CHECK_LEN;
	ret = keydb_Get(handle->db, &checkkey, &checkdata, 0 );
	if (ret) {
	    goto loser;
	}
	/* if we have the fake PW_CHECK, then try to decode the key
	 * rather than the pwcheck item.
	 */
	rv = seckey_CheckKeyDB1Password(handle,pwitem);
	if (rv == SECSuccess) {
	    /* OK we have enough to complete our conversion */
	    nsslowkey_UpdateKeyDBPass2(handle,pwitem);
	}
	return rv;
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
    encstring.type = 0;
    
    switch(algorithm)
    {
	case SEC_OID_RC4:
	    key = seckey_create_rc4_key(pwitem, &dbkey->salt);
	    if(key != NULL) {
		dest = seckey_rc4_decode(key, &encstring);
		SECITEM_FreeItem(key, PR_TRUE);
	    }
	    break;
	default:
	    param = nsspkcs5_NewParam(algorithm, &dbkey->salt, 1);
	    if (param != NULL) {
                /* Decrypt - this function implements a workaround for
                 * a previous coding error.  It will decrypt values using
                 * DES rather than 3DES, if the initial try at 3DES
                 * decryption fails.  In this case, the update flag is
                 * set to TRUE.  This indication is used later to force
                 * an update of the database to "real" 3DES encryption.
                 */
		dest = nsspkcs5_CipherData(param, pwitem, 
					   &encstring, PR_FALSE, &update);
		nsspkcs5_DestroyPBEParameter(param);
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
	    nsslowkey_UpdateKeyDBPass2(handle, pwitem);
	}
        /* Force an update of the password to remove the incorrect DES
         * encryption (see the note above)
         */
	if (update && 
	     (algorithm == SEC_OID_PKCS12_PBE_WITH_SHA1_AND_TRIPLE_DES_CBC)) {
	    /* data base was encoded with DES not triple des, fix it */
	    nsslowkey_UpdateKeyDBPass2(handle,pwitem);
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
ChangeKeyDBPasswordAlg(NSSLOWKEYDBHandle *handle,
		       SECItem *oldpwitem, SECItem *newpwitem,
		       SECOidTag new_algorithm)
{
    SECStatus rv;
    keyList keylist;
    keyNode *node = NULL;
    NSSLOWKEYPrivateKey *privkey = NULL;
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

    rv = db_BeginTransaction(handle->db);
    if (rv != SECSuccess) {
	goto loser;
    }
    
    /* TNH - TraverseKeys should not be public, since it exposes
       the underlying DBT data type. */
    rv = nsslowkey_TraverseKeys(handle, sec_add_key_to_list, (void *)&keylist);
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
	ret = keydb_Del(handle->db, &node->key, 0);
	if ( ret ) {
	    PORT_SetError(SEC_ERROR_BAD_DATABASE);
	    rv = SECFailure;
	    goto loser;
	}
	
	/* get the public key, which we use as the database index */

	switch (privkey->keyType) {
	  case NSSLOWKEYRSAKey:
	    newkey.data = privkey->u.rsa.modulus.data;
	    newkey.size = privkey->u.rsa.modulus.len;
	    break;
	  case NSSLOWKEYDSAKey:
	    newkey.data = privkey->u.dsa.publicValue.data;
	    newkey.size = privkey->u.dsa.publicValue.len;
	    break;
	  case NSSLOWKEYDHKey:
	    newkey.data = privkey->u.dh.publicValue.data;
	    newkey.size = privkey->u.dh.publicValue.len;
	    break;
#ifdef NSS_ENABLE_ECC
	  case NSSLOWKEYECKey:
	    newkey.data = privkey->u.ec.publicValue.data;
	    newkey.size = privkey->u.ec.publicValue.len;
	    break;
#endif /* NSS_ENABLE_ECC */
	  default:
	    /* should we continue here and loose the key? */
	    PORT_SetError(SEC_ERROR_BAD_DATABASE);
	    rv = SECFailure;
	    goto loser;
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

    rv = nsslowkey_SetKeyDBPasswordAlg(handle, newpwitem, new_algorithm);

loser:

    db_FinishTransaction(handle->db,rv != SECSuccess);

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
nsslowkey_ChangeKeyDBPassword(NSSLOWKEYDBHandle *handle,
			      SECItem *oldpwitem, SECItem *newpwitem)
{
    SECStatus rv;
    
    if (handle == NULL) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	rv = SECFailure;
	goto loser;
    }

    rv = nsslowkey_CheckKeyDBPassword(handle, oldpwitem);
    if ( rv != SECSuccess ) {
	return(SECFailure);  /* return rv? */
    }

    rv = ChangeKeyDBPasswordAlg(handle, oldpwitem, newpwitem, 
				nsslowkey_GetDefaultKeyDBAlg());

loser:
    return(rv);
}
    

#define MAX_DB_SIZE 0xffff 
/*
 * Clear out all the keys in the existing database
 */
SECStatus
nsslowkey_ResetKeyDB(NSSLOWKEYDBHandle *handle)
{
    SECStatus rv;
    int ret;
    int errors = 0;

    if ( handle->db == NULL ) {
	return(SECSuccess);
    }

    if (handle->readOnly) {
	/* set an error code */
	return SECFailure;
     }

    if (handle->appname == NULL && handle->dbname == NULL) {
	return SECFailure;
    }

    keydb_Close(handle->db);
    if (handle->appname) {
	handle->db= 
	    rdbopen(handle->appname, handle->dbname, "key", NO_CREATE, NULL);
    } else {
	handle->db = dbopen( handle->dbname, NO_CREATE, 0600, DB_HASH, 0 );
    }
    if (handle->db == NULL) {
	/* set an error code */
	return SECFailure;
    }
    
    rv = makeGlobalVersion(handle);
    if ( rv != SECSuccess ) {
	errors++;
	goto done;
    }

    if (handle->global_salt) {
	rv = StoreKeyDBGlobalSalt(handle);
    } else {
	rv = makeGlobalSalt(handle);
	if ( rv == SECSuccess ) {
	    handle->global_salt = GetKeyDBGlobalSalt(handle);
	}
    }
    if ( rv != SECSuccess ) {
	errors++;
    }

done:
    /* sync the database */
    ret = keydb_Sync(handle->db, 0);
    db_InitComplete(handle->db);

    return (errors == 0 ? SECSuccess : SECFailure);
}

static int
keydb_Get(DB *db, DBT *key, DBT *data, unsigned int flags)
{
    PRStatus prstat;
    int ret;
    
    PORT_Assert(kdbLock != NULL);
    PZ_Lock(kdbLock);

    ret = (* db->get)(db, key, data, flags);

    prstat = PZ_Unlock(kdbLock);

    return(ret);
}

static int
keydb_Put(DB *db, DBT *key, DBT *data, unsigned int flags)
{
    PRStatus prstat;
    int ret = 0;

    PORT_Assert(kdbLock != NULL);
    PZ_Lock(kdbLock);

    ret = (* db->put)(db, key, data, flags);
    
    prstat = PZ_Unlock(kdbLock);

    return(ret);
}

static int
keydb_Sync(DB *db, unsigned int flags)
{
    PRStatus prstat;
    int ret;

    PORT_Assert(kdbLock != NULL);
    PZ_Lock(kdbLock);

    ret = (* db->sync)(db, flags);
    
    prstat = PZ_Unlock(kdbLock);

    return(ret);
}

static int
keydb_Del(DB *db, DBT *key, unsigned int flags)
{
    PRStatus prstat;
    int ret;

    PORT_Assert(kdbLock != NULL);
    PZ_Lock(kdbLock);

    ret = (* db->del)(db, key, flags);
    
    prstat = PZ_Unlock(kdbLock);

    return(ret);
}

static int
keydb_Seq(DB *db, DBT *key, DBT *data, unsigned int flags)
{
    PRStatus prstat;
    int ret;
    
    PORT_Assert(kdbLock != NULL);
    PZ_Lock(kdbLock);
    
    ret = (* db->seq)(db, key, data, flags);

    prstat = PZ_Unlock(kdbLock);

    return(ret);
}

static void
keydb_Close(DB *db)
{
    PRStatus prstat;

    PORT_Assert(kdbLock != NULL);
    PZ_Lock(kdbLock);

    (* db->close)(db);
    
    prstat = PZ_Unlock(kdbLock);

    return;
}

