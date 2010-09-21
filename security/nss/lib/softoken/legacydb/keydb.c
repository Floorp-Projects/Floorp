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
/* $Id: keydb.c,v 1.11.22.1 2010/08/07 05:49:16 wtc%google.com Exp $ */

#include "lowkeyi.h"
#include "secasn1.h"
#include "secder.h"
#include "secoid.h"
#include "blapi.h"
#include "secitem.h"
#include "pcert.h"
#include "mcom_db.h"
#include "secerr.h"

#include "keydbi.h"
#include "lgdb.h"

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

SEC_ASN1_MKSUB(SECOID_AlgorithmIDTemplate)

const SEC_ASN1Template nsslowkey_EncryptedPrivateKeyInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	0, NULL, sizeof(NSSLOWKEYEncryptedPrivateKeyInfo) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	offsetof(NSSLOWKEYEncryptedPrivateKeyInfo,algorithm),
	SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OCTET_STRING,
	offsetof(NSSLOWKEYEncryptedPrivateKeyInfo,encryptedData) },
    { 0 }
};

const SEC_ASN1Template nsslowkey_PointerToEncryptedPrivateKeyInfoTemplate[] = {
	{ SEC_ASN1_POINTER, 0, nsslowkey_EncryptedPrivateKeyInfoTemplate }
};


/* ====== Default key databse encryption algorithm ====== */
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

static int keydb_Get(NSSLOWKEYDBHandle *db, DBT *key, DBT *data, 
		     unsigned int flags);
static int keydb_Put(NSSLOWKEYDBHandle *db, DBT *key, DBT *data, 
		     unsigned int flags);
static int keydb_Sync(NSSLOWKEYDBHandle *db, unsigned int flags);
static int keydb_Del(NSSLOWKEYDBHandle *db, DBT *key, unsigned int flags);
static int keydb_Seq(NSSLOWKEYDBHandle *db, DBT *key, DBT *data, 
		     unsigned int flags);
static void keydb_Close(NSSLOWKEYDBHandle *db);

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
    ret = keydb_Get(handle, index, &entry, 0);
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
	status = keydb_Put(handle, index, keydata, 0);
    } else {
	status = keydb_Put(handle, index, keydata, R_NOOVERWRITE);
    }
    
    if ( status ) {
	goto loser;
    }

    /* sync the database */
    status = keydb_Sync(handle, 0);
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

    ret = keydb_Seq(handle, &key, &data, R_FIRST);
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
    } while ( keydb_Seq(handle, &key, &data, R_NEXT) == 0 );

    return(SECSuccess);
}

#ifdef notdef
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
#endif

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

    ret = keydb_Get(handle, &saltKey, &saltData, 0);
    if ( ret ) {
	return(NULL);
    }

    return(decodeKeyDBGlobalSalt(&saltData));
}

static SECStatus
StoreKeyDBGlobalSalt(NSSLOWKEYDBHandle *handle, SECItem *salt)
{
    DBT saltKey;
    DBT saltData;
    int status;
    
    saltKey.data = SALT_STRING;
    saltKey.size = sizeof(SALT_STRING) - 1;

    saltData.data = (void *)salt->data;
    saltData.size = salt->len;

    /* put global salt into the database now */
    status = keydb_Put(handle, &saltKey, &saltData, 0);
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
    status = keydb_Put(handle, &versionKey, &versionData, 0);
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
    status = keydb_Put(handle, &saltKey, &saltData, 0);
    if ( status ) {
	return(SECFailure);
    }

    return(SECSuccess);
}

static SECStatus
encodePWCheckEntry(PLArenaPool *arena, SECItem *entry, SECOidTag alg,
		   SECItem *encCheck);

static unsigned char
nsslowkey_version(NSSLOWKEYDBHandle *handle)
{
    DBT versionKey;
    DBT versionData;
    int ret;
    versionKey.data = VERSION_STRING;
    versionKey.size = sizeof(VERSION_STRING)-1;

    if (handle->db == NULL) {
	return 255;
    }

    /* lookup version string in database */
    ret = keydb_Get( handle, &versionKey, &versionData, 0 );

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
seckey_HasAServerKey(NSSLOWKEYDBHandle *handle)
{
    DBT key;
    DBT data;
    int ret;
    PRBool found = PR_FALSE;

    ret = keydb_Seq(handle, &key, &data, R_FIRST);
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
    } while ( keydb_Seq(handle, &key, &data, R_NEXT) == 0 );

    return found;
}

/* forward declare local create function */
static NSSLOWKEYDBHandle * nsslowkey_NewHandle(DB *dbHandle);

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
    NSSLOWKEYDBKey *dbkey = NULL;
    NSSLOWKEYDBHandle *update = NULL;
    SECItem *oldSalt = NULL;
    int ret;
    SECItem checkitem;

    if ( handle->updatedb == NULL ) {
	return SECSuccess;
    }

    /* create a full DB Handle for our update so we 
     * can use the correct locks for the db primatives */
    update = nsslowkey_NewHandle(handle->updatedb);
    if ( update == NULL) {
	return SECSuccess;
    }

    /* update has now inherited the database handle */
    handle->updatedb = NULL;

    /*
     * check the version record
     */
    version = nsslowkey_version(update);
    if (version != 2) {
	goto done;
    }

    saltKey.data = SALT_STRING;
    saltKey.size = sizeof(SALT_STRING) - 1;

    ret = keydb_Get(update, &saltKey, &saltData, 0);
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
    
    ret = keydb_Get(update, &checkKey, &checkData, 0 );
    if (ret) {
	/*
	 * if we have a key, but no KEYDB_PW_CHECK_STRING, then this must
	 * be an old server database, and it does have a password associated
	 * with it. Put a fake entry in so we can identify this db when we do
	 * get the password for it.
	 */
	if (seckey_HasAServerKey(update)) {
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
	    ret = keydb_Put( handle, &saltKey, &saltData, 0);
	    if ( ret ) {
		goto done;
	    }
	    ret = keydb_Put( handle, &fcheckKey, &fcheckData, 0);
	    if ( ret ) {
		goto done;
	    }
	} else {
	    goto done;
	}
    } else {
	/* put global salt into the new database now */
	ret = keydb_Put( handle, &saltKey, &saltData, 0);
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
    ret = keydb_Seq(update, &key, &data, R_FIRST);
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
    } while ( keydb_Seq(update, &key, &data, R_NEXT) == 0 );

    dbkey = NULL;

done:
    /* sync the database */
    ret = keydb_Sync(handle, 0);

    nsslowkey_CloseKeyDB(update);
    
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

    /* force a transactional read, which will verify that one and only one
     * process attempts the update. */
    if (nsslowkey_version(handle) == NSSLOWKEY_DB_FILE_VERSION) {
	/* someone else has already updated the database for us */
	db_InitComplete(handle->db);
	return SECSuccess;
    }

    /*
     * if we are creating a multiaccess database, see if there is a
     * local database we can update from.
     */
    if (appName) {
        NSSLOWKEYDBHandle *updateHandle;
	updatedb = dbopen( dbname, NO_RDONLY, 0600, DB_HASH, 0 );
	if (!updatedb) {
	    goto noupdate;
	}

	/* nsslowkey_version needs a full handle because it calls
         * the kdb_Get() function, which needs to lock.
         */
        updateHandle = nsslowkey_NewHandle(updatedb);
	if (!updateHandle) {
	    updatedb->close(updatedb);
	    goto noupdate;
	}

	handle->version = nsslowkey_version(updateHandle);
	if (handle->version != NSSLOWKEY_DB_FILE_VERSION) {
	    nsslowkey_CloseKeyDB(updateHandle);
	    goto noupdate;
	}

	/* copy the new DB from the old one */
	db_Copy(handle->db, updatedb);
	nsslowkey_CloseKeyDB(updateHandle);
	db_InitComplete(handle->db);
	return SECSuccess;
    }
noupdate:

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
    ret = keydb_Sync(handle, 0);
    if ( ret ) {
	rv = SECFailure;
	goto loser;
    }
    rv = SECSuccess;

loser:
    db_InitComplete(handle->db);
    return rv;
}


static DB *
openOldDB(const char *appName, const char *prefix, const char *dbname, 
					PRBool openflags) {
    DB *db = NULL;

    if (appName) {
	db = rdbopen( appName, prefix, "key", openflags, NULL);
    } else {
	db = dbopen( dbname, openflags, 0600, DB_HASH, 0 );
    }

    return db;
}

/* check for correct version number */
static PRBool
verifyVersion(NSSLOWKEYDBHandle *handle)
{
    int version = nsslowkey_version(handle);

    handle->version = version;
    if (version != NSSLOWKEY_DB_FILE_VERSION ) {
	if (handle->db) {
	    keydb_Close(handle);
	    handle->db = NULL;
	}
    }
    return handle->db != NULL;
}

static NSSLOWKEYDBHandle *
nsslowkey_NewHandle(DB *dbHandle)
{
    NSSLOWKEYDBHandle *handle;
    handle = (NSSLOWKEYDBHandle *)PORT_ZAlloc (sizeof(NSSLOWKEYDBHandle));
    if (handle == NULL) {
	PORT_SetError (SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    handle->appname = NULL;
    handle->dbname = NULL;
    handle->global_salt = NULL;
    handle->updatedb = NULL;
    handle->db = dbHandle;
    handle->ref = 1;
    handle->lock = PZ_NewLock(nssILockKeyDB);

    return handle;
}

NSSLOWKEYDBHandle *
nsslowkey_OpenKeyDB(PRBool readOnly, const char *appName, const char *prefix,
				NSSLOWKEYDBNameFunc namecb, void *cbarg)
{
    NSSLOWKEYDBHandle *handle = NULL;
    SECStatus rv;
    int openflags;
    char *dbname = NULL;


    handle = nsslowkey_NewHandle(NULL);

    openflags = readOnly ? NO_RDONLY : NO_RDWR;


    dbname = (*namecb)(cbarg, NSSLOWKEY_DB_FILE_VERSION);
    if ( dbname == NULL ) {
	goto loser;
    }
    handle->appname = appName ? PORT_Strdup(appName) : NULL ;
    handle->dbname = (appName == NULL) ? PORT_Strdup(dbname) : 
			(prefix ? PORT_Strdup(prefix) : NULL);
    handle->readOnly = readOnly;



    handle->db = openOldDB(appName, prefix, dbname, openflags);
    if (handle->db) {
	verifyVersion(handle);
	if (handle->version == 255) {
	    goto loser;
	}
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
	    handle->db = openOldDB(appName,prefix,dbname, openflags);
	    verifyVersion(handle);
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
    nsslowkey_CloseKeyDB(handle);
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
	    keydb_Close(handle);
	}
	if (handle->updatedb) {
	    handle->updatedb->close(handle->updatedb);
        }
	if (handle->dbname) PORT_Free(handle->dbname);
	if (handle->appname) PORT_Free(handle->appname);
	if (handle->global_salt) {
	    SECITEM_FreeItem(handle->global_salt,PR_TRUE);
	}
	if (handle->lock != NULL) {
	    SKIP_AFTER_FORK(PZ_DestroyLock(handle->lock));
	}
	    
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
nsslowkey_DeleteKey(NSSLOWKEYDBHandle *handle, const SECItem *pubkey)
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
    ret = keydb_Del(handle, &namekey, 0);
    if ( ret ) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	return(SECFailure);
    }

    /* sync the database */
    ret = keydb_Sync(handle, 0);
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
			   SDB *sdb)
{
    return nsslowkey_StoreKeyByPublicKeyAlg(handle, privkey, pubKeyData, 
	     nickname, sdb, PR_FALSE);
}

SECStatus
nsslowkey_UpdateNickname(NSSLOWKEYDBHandle *handle, 
			   NSSLOWKEYPrivateKey *privkey,
			   SECItem *pubKeyData,
			   char *nickname,
			   SDB *sdb)
{
    return nsslowkey_StoreKeyByPublicKeyAlg(handle, privkey, pubKeyData, 
	     nickname, sdb, PR_TRUE);
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
    status = keydb_Get(handle, &namekey, &dummy, 0);
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

    status = keydb_Get(handle, &namekey, &dummy, 0);
    /* some databases have the key stored as a signed value */
    if (status) {
	unsigned char *buf = (unsigned char *)PORT_Alloc(namekey.size+1);
	if (buf) {
	    PORT_Memcpy(&buf[1], namekey.data, namekey.size);
	    buf[0] = 0;
	    namekey.data = buf;
	    namekey.size ++;
    	    status = keydb_Get(handle, &namekey, &dummy, 0);
	    PORT_Free(buf);
	}
    }
    nsslowkey_DestroyPublicKey(pubkey);
    if ( status ) {
	return PR_FALSE;
    }
    
    return PR_TRUE;
}

typedef struct NSSLowPasswordDataParamStr {
    SECItem salt;
    SECItem iter;
} NSSLowPasswordDataParam;

static const SEC_ASN1Template NSSLOWPasswordParamTemplate[] =
{
    {SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSSLowPasswordDataParam) },
    {SEC_ASN1_OCTET_STRING, offsetof(NSSLowPasswordDataParam, salt) },
    {SEC_ASN1_INTEGER, offsetof(NSSLowPasswordDataParam, iter) },
    {0}
};
struct LGEncryptedDataInfoStr {
    SECAlgorithmID algorithm;
    SECItem encryptedData;
};
typedef struct LGEncryptedDataInfoStr LGEncryptedDataInfo;

const SEC_ASN1Template lg_EncryptedDataInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
        0, NULL, sizeof(LGEncryptedDataInfo) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
        offsetof(LGEncryptedDataInfo,algorithm),
        SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OCTET_STRING,
        offsetof(LGEncryptedDataInfo,encryptedData) },
    { 0 }
};

static SECItem *
nsslowkey_EncodePW(SECOidTag alg, const SECItem *salt, SECItem *data)
{
    NSSLowPasswordDataParam param;
    LGEncryptedDataInfo edi;
    PLArenaPool *arena;
    unsigned char one = 1;
    SECItem *epw = NULL;
    SECItem *encParam;
    SECStatus rv;

    param.salt = *salt;
    param.iter.type = siBuffer;  /* encode as signed integer */
    param.iter.data = &one;
    param.iter.len = 1;
    edi.encryptedData = *data;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	return NULL;
    }

    encParam = SEC_ASN1EncodeItem(arena, NULL, &param,
				  NSSLOWPasswordParamTemplate);
    if (encParam == NULL) {
	goto loser;
    }
    rv = SECOID_SetAlgorithmID(arena, &edi.algorithm, alg, encParam);
    if (rv != SECSuccess) {
	goto loser;
    }
    epw = SEC_ASN1EncodeItem(NULL, NULL, &edi, lg_EncryptedDataInfoTemplate);

loser:
    PORT_FreeArena(arena, PR_FALSE);
    return epw;
}

static SECItem *
nsslowkey_DecodePW(const SECItem *derData, SECOidTag *alg, SECItem *salt)
{
    NSSLowPasswordDataParam param;
    LGEncryptedDataInfo edi;
    PLArenaPool *arena;
    SECItem *pwe = NULL;
    SECStatus rv;

    salt->data = NULL;
    param.iter.type = siBuffer;  /* decode as signed integer */

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	return NULL;
    }

    rv = SEC_QuickDERDecodeItem(arena, &edi, lg_EncryptedDataInfoTemplate, 
						derData);
    if (rv != SECSuccess) {
	goto loser;
    }
    *alg = SECOID_GetAlgorithmTag(&edi.algorithm);
    rv = SEC_QuickDERDecodeItem(arena, &param, NSSLOWPasswordParamTemplate,
						&edi.algorithm.parameters);
    if (rv != SECSuccess) {
	goto loser;
    }
    rv = SECITEM_CopyItem(NULL, salt, &param.salt);
    if (rv != SECSuccess) {
	goto loser;
    }
    pwe = SECITEM_DupItem(&edi.encryptedData);

loser:
    if (!pwe && salt->data) {
	PORT_Free(salt->data);
	salt->data = NULL;
    }
    PORT_FreeArena(arena, PR_FALSE);
    return pwe;
}


/*
 * check to see if the user has a password
 */
static SECStatus
nsslowkey_GetPWCheckEntry(NSSLOWKEYDBHandle *handle,NSSLOWKEYPasswordEntry *entry)
{
    DBT checkkey; /*, checkdata; */
    NSSLOWKEYDBKey *dbkey = NULL;
    SECItem   *global_salt = NULL; 
    SECItem   *item = NULL; 
    SECItem   entryData, oid;
    SECItem   none = { siBuffer, NULL, 0 };
    SECStatus rv = SECFailure;
    SECOidTag algorithm;

    if (handle == NULL) {
	/* PORT_SetError */
	return(SECFailure);
    }

    global_salt = GetKeyDBGlobalSalt(handle);
    if (!global_salt) {
	global_salt = &none;
    }
    if (global_salt->len > sizeof(entry->data)) {
	/* PORT_SetError */
	goto loser;
    }
	
    PORT_Memcpy(entry->data, global_salt->data, global_salt->len);
    entry->salt.data = entry->data;
    entry->salt.len = global_salt->len;
    entry->value.data = &entry->data[entry->salt.len];

    checkkey.data = KEYDB_PW_CHECK_STRING;
    checkkey.size = KEYDB_PW_CHECK_LEN;
    dbkey = get_dbkey(handle, &checkkey);
    if (dbkey == NULL) {
	/* handle 'FAKE' check here */
	goto loser;
    }

    oid.len = dbkey->derPK.data[0];
    oid.data = &dbkey->derPK.data[1];

    if (dbkey->derPK.len < (KEYDB_PW_CHECK_LEN + 1 +oid.len)) {
	goto loser;
    }
    algorithm = SECOID_FindOIDTag(&oid);
    entryData.type = siBuffer;
    entryData.len = dbkey->derPK.len - (oid.len+1);
    entryData.data = &dbkey->derPK.data[oid.len+1];

    item = nsslowkey_EncodePW(algorithm, &dbkey->salt, &entryData);
    if (!item || (item->len + entry->salt.len) > sizeof(entry->data)) {
	goto loser;
    }
    PORT_Memcpy(entry->value.data, item->data, item->len);
    entry->value.len = item->len;
    rv = SECSuccess;

loser:
    if (item) {
	SECITEM_FreeItem(item, PR_TRUE);
    }
    if (dbkey) {
 	sec_destroy_dbkey(dbkey);
    }
    if (global_salt && global_salt != &none) {
	SECITEM_FreeItem(global_salt,PR_TRUE);
    }
    return rv;
}

/*
 * check to see if the user has a password
 */
static SECStatus
nsslowkey_PutPWCheckEntry(NSSLOWKEYDBHandle *handle,NSSLOWKEYPasswordEntry *entry)
{
    DBT checkkey;
    NSSLOWKEYDBKey *dbkey = NULL;
    SECItem   *item = NULL; 
    SECItem   salt; 
    SECOidTag algid;
    SECStatus rv = SECFailure;
    PLArenaPool *arena;
    int ret;

    if (handle == NULL) {
	/* PORT_SetError */
	return(SECFailure);
    }

    checkkey.data = KEYDB_PW_CHECK_STRING;
    checkkey.size = KEYDB_PW_CHECK_LEN;

    salt.data = NULL;
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	return SECFailure;
    }

    item = nsslowkey_DecodePW(&entry->value, &algid, &salt);
    if (item == NULL) {
	goto loser;
    }

    dbkey = PORT_ArenaZNew(arena, NSSLOWKEYDBKey);
    if (dbkey == NULL) {
	goto loser;
    }

    dbkey->arena = arena;

    rv = SECITEM_CopyItem(arena, &dbkey->salt, &salt);
    if (rv != SECSuccess) {
	goto loser;
    }

    rv = encodePWCheckEntry(arena, &dbkey->derPK, algid, item);
    if (rv != SECSuccess) {
	goto loser;
    }

    rv = put_dbkey(handle, &checkkey, dbkey, PR_TRUE);
    if (rv != SECSuccess) {
	goto loser;
    }

    if (handle->global_salt) {
	SECITEM_FreeItem(handle->global_salt, PR_TRUE);
	handle->global_salt = NULL;
    }
    rv = StoreKeyDBGlobalSalt(handle, &entry->salt);
    if (rv != SECSuccess) {
	goto loser;
    }
    ret = keydb_Sync(handle, 0);
    if ( ret ) {
	rv = SECFailure;
	goto loser;
    }
    handle->global_salt = GetKeyDBGlobalSalt(handle);

loser:
    if (item) {
	SECITEM_FreeItem(item, PR_TRUE);
    }
    if (arena) {
	PORT_FreeArena(arena, PR_TRUE);
    }
    if (salt.data) {
	PORT_Free(salt.data);
    }
    return rv;
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


SECStatus 
seckey_encrypt_private_key( PLArenaPool *permarena, NSSLOWKEYPrivateKey *pk, 
			    SDB *sdbpw, SECItem *result)
{
    NSSLOWKEYPrivateKeyInfo *pki = NULL;
    SECStatus rv = SECFailure;
    PLArenaPool *temparena = NULL;
    SECItem *der_item = NULL;
    SECItem *cipherText = NULL;
    SECItem *dummy = NULL;
#ifdef NSS_ENABLE_ECC
    SECItem *fordebug = NULL;
    int savelen;
#endif

    temparena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if(temparena == NULL)
	goto loser;

    /* allocate structures */
    pki = (NSSLOWKEYPrivateKeyInfo *)PORT_ArenaZAlloc(temparena, 
	sizeof(NSSLOWKEYPrivateKeyInfo));
    der_item = (SECItem *)PORT_ArenaZAlloc(temparena, sizeof(SECItem));
    if((pki == NULL) || (der_item == NULL))
	goto loser;


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

    rv = lg_util_encrypt(temparena, sdbpw, dummy, &cipherText);
    if (rv != SECSuccess) {
	goto loser;
    }

    rv = SECITEM_CopyItem ( permarena, result, cipherText);

loser:

    if(temparena != NULL)
	PORT_FreeArena(temparena, PR_TRUE);

    return rv;
}

static SECStatus 
seckey_put_private_key(NSSLOWKEYDBHandle *keydb, DBT *index, SDB *sdbpw,
		       NSSLOWKEYPrivateKey *pk, char *nickname, PRBool update)
{
    NSSLOWKEYDBKey *dbkey = NULL;
    PLArenaPool *arena = NULL;
    SECStatus rv = SECFailure;

    if((keydb == NULL) || (index == NULL) || (sdbpw == NULL) ||
	(pk == NULL))
	return SECFailure;
	
    arena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if(arena == NULL)
	return SECFailure;

    dbkey = (NSSLOWKEYDBKey *)PORT_ArenaZAlloc(arena, sizeof(NSSLOWKEYDBKey));
    if(dbkey == NULL)
	goto loser;
    dbkey->arena = arena;
    dbkey->nickname = nickname;

    rv = seckey_encrypt_private_key(arena, pk, sdbpw, &dbkey->derPK);
    if(rv != SECSuccess)
	goto loser;

    rv = put_dbkey(keydb, index, dbkey, update);

    /* let success fall through */
loser:
    if(arena != NULL)
	PORT_FreeArena(arena, PR_TRUE);

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
			      SDB *sdbpw,
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
    rv = seckey_put_private_key(handle, &namekey, sdbpw, privkey, nickname,
				update);
    
    return(rv);
}

static NSSLOWKEYPrivateKey *
seckey_decrypt_private_key(SECItem*epki,
			   SDB *sdbpw)
{
    NSSLOWKEYPrivateKey *pk = NULL;
    NSSLOWKEYPrivateKeyInfo *pki = NULL;
    SECStatus rv = SECFailure;
    PLArenaPool *temparena = NULL, *permarena = NULL;
    SECItem *dest = NULL;
#ifdef NSS_ENABLE_ECC
    SECItem *fordebug = NULL;
#endif

    if((epki == NULL) || (sdbpw == NULL))
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

    rv = lg_util_decrypt(sdbpw, epki, &dest);
    if (rv != SECSuccess) {
	goto loser;
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
		rv = LGEC_FillParams(permarena, &pk->u.ec.ecParams.DEREncoding,
				   &pk->u.ec.ecParams);

		if (rv != SECSuccess)
		    goto loser;

		if (pk->u.ec.publicValue.len != 0) {
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
seckey_decode_encrypted_private_key(NSSLOWKEYDBKey *dbkey, SDB *sdbpw)
{
    if( ( dbkey == NULL ) || ( sdbpw == NULL ) ) {
	return NULL;
    }

    return seckey_decrypt_private_key(&(dbkey->derPK), sdbpw);
}

static NSSLOWKEYPrivateKey *
seckey_get_private_key(NSSLOWKEYDBHandle *keydb, DBT *index, char **nickname,
		       SDB *sdbpw)
{
    NSSLOWKEYDBKey *dbkey = NULL;
    NSSLOWKEYPrivateKey *pk = NULL;

    if( ( keydb == NULL ) || ( index == NULL ) || ( sdbpw == NULL ) ) {
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
    
    pk = seckey_decode_encrypted_private_key(dbkey, sdbpw);
    
    /* let success fall through */
loser:

    if ( dbkey != NULL ) {
	sec_destroy_dbkey(dbkey);
    }

    return pk;
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
			  				 SDB *sdbpw)
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

    pk = seckey_get_private_key(handle, &namekey, NULL, sdbpw);
    
    /* no need to free dbkey, since its on the stack, and the data it
     * points to is owned by the database
     */
    return(pk);
}

char *
nsslowkey_FindKeyNicknameByPublicKey(NSSLOWKEYDBHandle *handle, 
					SECItem *modulus, SDB *sdbpw)
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

    pk = seckey_get_private_key(handle, &namekey, &nickname, sdbpw);
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
    

#define MAX_DB_SIZE 0xffff 
/*
 * Clear out all the keys in the existing database
 */
static SECStatus
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

    keydb_Close(handle);
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
	rv = StoreKeyDBGlobalSalt(handle, handle->global_salt);
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
    ret = keydb_Sync(handle, 0);
    db_InitComplete(handle->db);

    return (errors == 0 ? SECSuccess : SECFailure);
}

static int
keydb_Get(NSSLOWKEYDBHandle *kdb, DBT *key, DBT *data, unsigned int flags)
{
    PRStatus prstat;
    int ret;
    PRLock *kdbLock = kdb->lock;
    DB *db = kdb->db;
    
    PORT_Assert(kdbLock != NULL);
    PZ_Lock(kdbLock);

    ret = (* db->get)(db, key, data, flags);

    prstat = PZ_Unlock(kdbLock);

    return(ret);
}

static int
keydb_Put(NSSLOWKEYDBHandle *kdb, DBT *key, DBT *data, unsigned int flags)
{
    PRStatus prstat;
    int ret = 0;
    PRLock *kdbLock = kdb->lock;
    DB *db = kdb->db;

    PORT_Assert(kdbLock != NULL);
    PZ_Lock(kdbLock);

    ret = (* db->put)(db, key, data, flags);
    
    prstat = PZ_Unlock(kdbLock);

    return(ret);
}

static int
keydb_Sync(NSSLOWKEYDBHandle *kdb, unsigned int flags)
{
    PRStatus prstat;
    int ret;
    PRLock *kdbLock = kdb->lock;
    DB *db = kdb->db;

    PORT_Assert(kdbLock != NULL);
    PZ_Lock(kdbLock);

    ret = (* db->sync)(db, flags);
    
    prstat = PZ_Unlock(kdbLock);

    return(ret);
}

static int
keydb_Del(NSSLOWKEYDBHandle *kdb, DBT *key, unsigned int flags)
{
    PRStatus prstat;
    int ret;
    PRLock *kdbLock = kdb->lock;
    DB *db = kdb->db;

    PORT_Assert(kdbLock != NULL);
    PZ_Lock(kdbLock);

    ret = (* db->del)(db, key, flags);
    
    prstat = PZ_Unlock(kdbLock);

    return(ret);
}

static int
keydb_Seq(NSSLOWKEYDBHandle *kdb, DBT *key, DBT *data, unsigned int flags)
{
    PRStatus prstat;
    int ret;
    PRLock *kdbLock = kdb->lock;
    DB *db = kdb->db;
    
    PORT_Assert(kdbLock != NULL);
    PZ_Lock(kdbLock);
    
    ret = (* db->seq)(db, key, data, flags);

    prstat = PZ_Unlock(kdbLock);

    return(ret);
}

static void
keydb_Close(NSSLOWKEYDBHandle *kdb)
{
    PRStatus prstat;
    PRLock *kdbLock = kdb->lock;
    DB *db = kdb->db;

    PORT_Assert(kdbLock != NULL);
    SKIP_AFTER_FORK(PZ_Lock(kdbLock));

    (* db->close)(db);
    
    SKIP_AFTER_FORK(prstat = PZ_Unlock(kdbLock));

    return;
}

/*
 * SDB Entry Points for the Key DB 
 */

CK_RV
lg_GetMetaData(SDB *sdb, const char *id, SECItem *item1, SECItem *item2)
{
    NSSLOWKEYDBHandle *keydb;
    NSSLOWKEYPasswordEntry entry;
    SECStatus rv;

    keydb = lg_getKeyDB(sdb);
    if (keydb == NULL) {
        return CKR_TOKEN_WRITE_PROTECTED;
    }
    if (PORT_Strcmp(id,"password") != 0) {
	/* shouldn't happen */
	return CKR_GENERAL_ERROR; /* no extra data stored */
    }
    rv = nsslowkey_GetPWCheckEntry(keydb, &entry);
    if (rv != SECSuccess) {
        return CKR_GENERAL_ERROR;
    }
    item1->len = entry.salt.len;
    PORT_Memcpy(item1->data, entry.salt.data, item1->len);
    item2->len = entry.value.len;
    PORT_Memcpy(item2->data, entry.value.data, item2->len);
    return CKR_OK;
}

CK_RV
lg_PutMetaData(SDB *sdb, const char *id, 
	       const SECItem *item1, const SECItem *item2)
{
    NSSLOWKEYDBHandle *keydb;
    NSSLOWKEYPasswordEntry entry;
    SECStatus rv;

    keydb = lg_getKeyDB(sdb);
    if (keydb == NULL) {
        return CKR_TOKEN_WRITE_PROTECTED;
    }
    if (PORT_Strcmp(id,"password") != 0) {
	/* shouldn't happen */
	return CKR_GENERAL_ERROR; /* no extra data stored */
    }
    entry.salt = *item1;
    entry.value = *item2;
    rv = nsslowkey_PutPWCheckEntry(keydb, &entry);
    if (rv != SECSuccess) {
        return CKR_GENERAL_ERROR;
    }
    return CKR_OK;
}

CK_RV
lg_Reset(SDB *sdb)
{
    NSSLOWKEYDBHandle *keydb;
    SECStatus rv;

    keydb = lg_getKeyDB(sdb);
    if (keydb == NULL) {
        return CKR_TOKEN_WRITE_PROTECTED;
    }
    rv = nsslowkey_ResetKeyDB(keydb);
    if (rv != SECSuccess) {
        return CKR_GENERAL_ERROR;
    }
    return CKR_OK;
}

