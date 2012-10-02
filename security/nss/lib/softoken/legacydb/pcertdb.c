/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Permanent Certificate database handling code 
 *
 * $Id: pcertdb.c,v 1.13 2012/04/25 14:50:11 gerv%gerv.net Exp $
 */
#include "lowkeyti.h"
#include "pcert.h"
#include "mcom_db.h"
#include "pcert.h"
#include "secitem.h"
#include "secder.h"

#include "secerr.h"
#include "lgdb.h"

/* forward declaration */
NSSLOWCERTCertificate *
nsslowcert_FindCertByDERCertNoLocking(NSSLOWCERTCertDBHandle *handle, SECItem *derCert);
static SECStatus
nsslowcert_UpdateSMimeProfile(NSSLOWCERTCertDBHandle *dbhandle, 
	char *emailAddr, SECItem *derSubject, SECItem *emailProfile, 
							SECItem *profileTime);
static SECStatus
nsslowcert_UpdatePermCert(NSSLOWCERTCertDBHandle *dbhandle,
    NSSLOWCERTCertificate *cert, char *nickname, NSSLOWCERTCertTrust *trust);
static SECStatus
nsslowcert_UpdateCrl(NSSLOWCERTCertDBHandle *handle, SECItem *derCrl, 
			SECItem *crlKey, char *url, PRBool isKRL);

static NSSLOWCERTCertificate *certListHead = NULL;
static NSSLOWCERTTrust *trustListHead = NULL;
static certDBEntryCert *entryListHead = NULL;
static int certListCount = 0;
static int trustListCount = 0;
static int entryListCount = 0;
#define MAX_CERT_LIST_COUNT 10
#define MAX_TRUST_LIST_COUNT 10
#define MAX_ENTRY_LIST_COUNT 10

/*
 * the following functions are wrappers for the db library that implement
 * a global lock to make the database thread safe.
 */
static PZLock *dbLock = NULL;
static PZLock *certRefCountLock = NULL;
static PZLock *certTrustLock = NULL;
static PZLock *freeListLock = NULL;

void
certdb_InitDBLock(NSSLOWCERTCertDBHandle *handle)
{
    if (dbLock == NULL) {
	dbLock = PZ_NewLock(nssILockCertDB);
	PORT_Assert(dbLock != NULL);
    }
}

SECStatus
nsslowcert_InitLocks(void)
{
    if (freeListLock == NULL) {
	freeListLock = PZ_NewLock(nssILockRefLock);
	if (freeListLock == NULL) {
	    return SECFailure;
	}
    }
    if (certRefCountLock == NULL) {
	certRefCountLock = PZ_NewLock(nssILockRefLock);
	if (certRefCountLock == NULL) {
	    return SECFailure;
	}
    }
    if (certTrustLock == NULL ) {
	certTrustLock = PZ_NewLock(nssILockCertDB);
	if (certTrustLock == NULL) {
	    return SECFailure;
	}
    }
    
    return SECSuccess;
}

/*
 * Acquire the global lock on the cert database.
 * This lock is currently used for the following operations:
 *	adding or deleting a cert to either the temp or perm databases
 *	converting a temp to perm or perm to temp
 *	changing (maybe just adding!?) the trust of a cert
 *      chaning the DB status checking Configuration
 */
static void
nsslowcert_LockDB(NSSLOWCERTCertDBHandle *handle)
{
    PZ_EnterMonitor(handle->dbMon);
    return;
}

/*
 * Free the global cert database lock.
 */
static void
nsslowcert_UnlockDB(NSSLOWCERTCertDBHandle *handle)
{
    PRStatus prstat;
    
    prstat = PZ_ExitMonitor(handle->dbMon);
    
    PORT_Assert(prstat == PR_SUCCESS);
    
    return;
}


/*
 * Acquire the cert reference count lock
 * There is currently one global lock for all certs, but I'm putting a cert
 * arg here so that it will be easy to make it per-cert in the future if
 * that turns out to be necessary.
 */
static void
nsslowcert_LockCertRefCount(NSSLOWCERTCertificate *cert)
{
    PORT_Assert(certRefCountLock != NULL);
    
    PZ_Lock(certRefCountLock);
    return;
}

/*
 * Free the cert reference count lock
 */
static void
nsslowcert_UnlockCertRefCount(NSSLOWCERTCertificate *cert)
{
    PRStatus prstat;

    PORT_Assert(certRefCountLock != NULL);
    
    prstat = PZ_Unlock(certRefCountLock);
    
    PORT_Assert(prstat == PR_SUCCESS);

    return;
}

/*
 * Acquire the cert trust lock
 * There is currently one global lock for all certs, but I'm putting a cert
 * arg here so that it will be easy to make it per-cert in the future if
 * that turns out to be necessary.
 */
static void
nsslowcert_LockCertTrust(NSSLOWCERTCertificate *cert)
{
    PORT_Assert(certTrustLock != NULL);

    PZ_Lock(certTrustLock);
    return;
}

/*
 * Free the cert trust lock
 */
static void
nsslowcert_UnlockCertTrust(NSSLOWCERTCertificate *cert)
{
    PRStatus prstat;

    PORT_Assert(certTrustLock != NULL);
    
    prstat = PZ_Unlock(certTrustLock);
    
    PORT_Assert(prstat == PR_SUCCESS);

    return;
}


/*
 * Acquire the cert reference count lock
 * There is currently one global lock for all certs, but I'm putting a cert
 * arg here so that it will be easy to make it per-cert in the future if
 * that turns out to be necessary.
 */
static void
nsslowcert_LockFreeList(void)
{
    PORT_Assert(freeListLock != NULL);
    
    SKIP_AFTER_FORK(PZ_Lock(freeListLock));
    return;
}

/*
 * Free the cert reference count lock
 */
static void
nsslowcert_UnlockFreeList(void)
{
    PRStatus prstat = PR_SUCCESS;

    PORT_Assert(freeListLock != NULL);
    
    SKIP_AFTER_FORK(prstat = PZ_Unlock(freeListLock));
    
    PORT_Assert(prstat == PR_SUCCESS);

    return;
}

NSSLOWCERTCertificate *
nsslowcert_DupCertificate(NSSLOWCERTCertificate *c)
{
    if (c) {
	nsslowcert_LockCertRefCount(c);
	++c->referenceCount;
	nsslowcert_UnlockCertRefCount(c);
    }
    return c;
}

static int
certdb_Get(DB *db, DBT *key, DBT *data, unsigned int flags)
{
    PRStatus prstat;
    int ret;
    
    PORT_Assert(dbLock != NULL);
    PZ_Lock(dbLock);

    ret = (* db->get)(db, key, data, flags);

    prstat = PZ_Unlock(dbLock);

    return(ret);
}

static int
certdb_Put(DB *db, DBT *key, DBT *data, unsigned int flags)
{
    PRStatus prstat;
    int ret = 0;

    PORT_Assert(dbLock != NULL);
    PZ_Lock(dbLock);

    ret = (* db->put)(db, key, data, flags);
    
    prstat = PZ_Unlock(dbLock);

    return(ret);
}

static int
certdb_Sync(DB *db, unsigned int flags)
{
    PRStatus prstat;
    int ret;

    PORT_Assert(dbLock != NULL);
    PZ_Lock(dbLock);

    ret = (* db->sync)(db, flags);
    
    prstat = PZ_Unlock(dbLock);

    return(ret);
}

#define DB_NOT_FOUND -30991  /* from DBM 3.2 */
static int
certdb_Del(DB *db, DBT *key, unsigned int flags)
{
    PRStatus prstat;
    int ret;

    PORT_Assert(dbLock != NULL);
    PZ_Lock(dbLock);

    ret = (* db->del)(db, key, flags);
    
    prstat = PZ_Unlock(dbLock);

    /* don't fail if the record is already deleted */
    if (ret == DB_NOT_FOUND) {
	ret = 0;
    }

    return(ret);
}

static int
certdb_Seq(DB *db, DBT *key, DBT *data, unsigned int flags)
{
    PRStatus prstat;
    int ret;
    
    PORT_Assert(dbLock != NULL);
    PZ_Lock(dbLock);
    
    ret = (* db->seq)(db, key, data, flags);

    prstat = PZ_Unlock(dbLock);

    return(ret);
}

static void
certdb_Close(DB *db)
{
    PRStatus prstat = PR_SUCCESS;

    PORT_Assert(dbLock != NULL);
    SKIP_AFTER_FORK(PZ_Lock(dbLock));

    (* db->close)(db);
    
    SKIP_AFTER_FORK(prstat = PZ_Unlock(dbLock));

    return;
}

void
pkcs11_freeNickname(char *nickname, char *space)
{
    if (nickname && nickname != space) {
	PORT_Free(nickname);
    }
}

char *
pkcs11_copyNickname(char *nickname,char *space, int spaceLen)
{
    int len;
    char *copy = NULL;

    len = PORT_Strlen(nickname)+1;
    if (len <= spaceLen) {
	copy = space;
	PORT_Memcpy(copy,nickname,len);
    } else {
	copy = PORT_Strdup(nickname);
    }

    return copy;
}

void
pkcs11_freeStaticData (unsigned char *data, unsigned char *space)
{
    if (data && data != space) {
	PORT_Free(data);
    }
}

unsigned char *
pkcs11_allocStaticData(int len, unsigned char *space, int spaceLen)
{
    unsigned char *data = NULL;

    if (len <= spaceLen) {
	data = space;
    } else {
	data = (unsigned char *) PORT_Alloc(len);
    }

    return data;
}

unsigned char *
pkcs11_copyStaticData(unsigned char *data, int len, 
					unsigned char *space, int spaceLen)
{
    unsigned char *copy = pkcs11_allocStaticData(len, space, spaceLen);
    if (copy) {
	PORT_Memcpy(copy,data,len);
    }

    return copy;
}

/*
 * destroy a database entry
 */
static void
DestroyDBEntry(certDBEntry *entry)
{
    PRArenaPool *arena = entry->common.arena;

    /* must be one of our certDBEntry from the free list */
    if (arena == NULL) {
	certDBEntryCert *certEntry;
	if ( entry->common.type != certDBEntryTypeCert) {
	    return;
	}
	certEntry = (certDBEntryCert *)entry;

	pkcs11_freeStaticData(certEntry->derCert.data, certEntry->derCertSpace);
	pkcs11_freeNickname(certEntry->nickname, certEntry->nicknameSpace);

	nsslowcert_LockFreeList();
	if (entryListCount > MAX_ENTRY_LIST_COUNT) {
	    PORT_Free(certEntry);
	} else {
	    entryListCount++;
	    PORT_Memset(certEntry, 0, sizeof( *certEntry));
	    certEntry->next = entryListHead;
	    entryListHead = certEntry;
	}
	nsslowcert_UnlockFreeList();
	return;
    }


    /* Zero out the entry struct, so that any further attempts to use it
     * will cause an exception (e.g. null pointer reference). */
    PORT_Memset(&entry->common, 0, sizeof entry->common);
    PORT_FreeArena(arena, PR_FALSE);

    return;
}

/* forward references */
static void nsslowcert_DestroyCertificateNoLocking(NSSLOWCERTCertificate *cert);

static SECStatus
DeleteDBEntry(NSSLOWCERTCertDBHandle *handle, certDBEntryType type, SECItem *dbkey)
{
    DBT key;
    int ret;

    /* init the database key */
    key.data = dbkey->data;
    key.size = dbkey->len;
    
    dbkey->data[0] = (unsigned char)type;

    /* delete entry from database */
    ret = certdb_Del(handle->permCertDB, &key, 0 );
    if ( ret != 0 ) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	goto loser;
    }

    ret = certdb_Sync(handle->permCertDB, 0);
    if ( ret ) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	goto loser;
    }

    return(SECSuccess);
    
loser:
    return(SECFailure);
}

static SECStatus
ReadDBEntry(NSSLOWCERTCertDBHandle *handle, certDBEntryCommon *entry,
	    SECItem *dbkey, SECItem *dbentry, PRArenaPool *arena)
{
    DBT data, key;
    int ret;
    unsigned char *buf;
    
    /* init the database key */
    key.data = dbkey->data;
    key.size = dbkey->len;
    
    dbkey->data[0] = (unsigned char)entry->type;

    /* read entry from database */
    ret = certdb_Get(handle->permCertDB, &key, &data, 0 );
    if ( ret != 0 ) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	goto loser;
    }
    
    /* validate the entry */
    if ( data.size < SEC_DB_ENTRY_HEADER_LEN ) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	goto loser;
    }
    buf = (unsigned char *)data.data;
    /* version 7 has the same schema, we may be using a v7 db if we openned
     * the databases readonly. */
    if (!((buf[0] == (unsigned char)CERT_DB_FILE_VERSION) 
		|| (buf[0] == (unsigned char) CERT_DB_V7_FILE_VERSION))) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	goto loser;
    }
    if ( buf[1] != (unsigned char)entry->type ) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	goto loser;
    }

    /* copy out header information */
    entry->version = (unsigned int)buf[0];
    entry->type = (certDBEntryType)buf[1];
    entry->flags = (unsigned int)buf[2];
    
    /* format body of entry for return to caller */
    dbentry->len = data.size - SEC_DB_ENTRY_HEADER_LEN;
    if ( dbentry->len ) {
	if (arena) {
	    dbentry->data = (unsigned char *)
				PORT_ArenaAlloc(arena, dbentry->len);
	    if ( dbentry->data == NULL ) {
		PORT_SetError(SEC_ERROR_NO_MEMORY);
		goto loser;
	    }
    
	    PORT_Memcpy(dbentry->data, &buf[SEC_DB_ENTRY_HEADER_LEN],
		  dbentry->len);
	} else {
	    dbentry->data = &buf[SEC_DB_ENTRY_HEADER_LEN];
	}
    } else {
	dbentry->data = NULL;
    }
    
    return(SECSuccess);

loser:
    return(SECFailure);
}

/**
 ** Implement low level database access
 **/
static SECStatus
WriteDBEntry(NSSLOWCERTCertDBHandle *handle, certDBEntryCommon *entry,
	     SECItem *dbkey, SECItem *dbentry)
{
    int ret;
    DBT data, key;
    unsigned char *buf;
    
    data.data = dbentry->data;
    data.size = dbentry->len;
    
    buf = (unsigned char*)data.data;
    
    buf[0] = (unsigned char)entry->version;
    buf[1] = (unsigned char)entry->type;
    buf[2] = (unsigned char)entry->flags;
    
    key.data = dbkey->data;
    key.size = dbkey->len;
    
    dbkey->data[0] = (unsigned char)entry->type;

    /* put the record into the database now */
    ret = certdb_Put(handle->permCertDB, &key, &data, 0);

    if ( ret != 0 ) {
	goto loser;
    }

    ret = certdb_Sync( handle->permCertDB, 0 );
    
    if ( ret ) {
	goto loser;
    }

    return(SECSuccess);

loser:
    return(SECFailure);
}

/*
 * encode a database cert record
 */
static SECStatus
EncodeDBCertEntry(certDBEntryCert *entry, PRArenaPool *arena, SECItem *dbitem)
{
    unsigned int nnlen;
    unsigned char *buf;
    char *nn;
    char zbuf = 0;
    
    if ( entry->nickname ) {
	nn = entry->nickname;
    } else {
	nn = &zbuf;
    }
    nnlen = PORT_Strlen(nn) + 1;
    
    /* allocate space for encoded database record, including space
     * for low level header
     */
    dbitem->len = entry->derCert.len + nnlen + DB_CERT_ENTRY_HEADER_LEN +
	SEC_DB_ENTRY_HEADER_LEN;
    
    dbitem->data = (unsigned char *)PORT_ArenaAlloc(arena, dbitem->len);
    if ( dbitem->data == NULL) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    
    /* fill in database record */
    buf = &dbitem->data[SEC_DB_ENTRY_HEADER_LEN];
    
    buf[0] = (PRUint8)( entry->trust.sslFlags >> 8 );
    buf[1] = (PRUint8)( entry->trust.sslFlags      );
    buf[2] = (PRUint8)( entry->trust.emailFlags >> 8 );
    buf[3] = (PRUint8)( entry->trust.emailFlags      );
    buf[4] = (PRUint8)( entry->trust.objectSigningFlags >> 8 );
    buf[5] = (PRUint8)( entry->trust.objectSigningFlags      );
    buf[6] = (PRUint8)( entry->derCert.len >> 8 );
    buf[7] = (PRUint8)( entry->derCert.len      );
    buf[8] = (PRUint8)( nnlen >> 8 );
    buf[9] = (PRUint8)( nnlen      );
    
    PORT_Memcpy(&buf[DB_CERT_ENTRY_HEADER_LEN], entry->derCert.data,
	      entry->derCert.len);

    PORT_Memcpy(&buf[DB_CERT_ENTRY_HEADER_LEN + entry->derCert.len],
	      nn, nnlen);

    return(SECSuccess);

loser:
    return(SECFailure);
}

/*
 * encode a database key for a cert record
 */
static SECStatus
EncodeDBCertKey(const SECItem *certKey, PRArenaPool *arena, SECItem *dbkey)
{
    unsigned int len = certKey->len + SEC_DB_KEY_HEADER_LEN;
    if (len > NSS_MAX_LEGACY_DB_KEY_SIZE)
	goto loser;
    if (arena) {
	dbkey->data = (unsigned char *)PORT_ArenaAlloc(arena, len);
    } else {
	if (dbkey->len < len) {
	    dbkey->data = (unsigned char *)PORT_Alloc(len);
	}
    }
    dbkey->len = len;
    if ( dbkey->data == NULL ) {
	goto loser;
    }
    PORT_Memcpy(&dbkey->data[SEC_DB_KEY_HEADER_LEN],
	      certKey->data, certKey->len);
    dbkey->data[0] = certDBEntryTypeCert;

    return(SECSuccess);
loser:
    return(SECFailure);
}

static SECStatus
EncodeDBGenericKey(const SECItem *certKey, PRArenaPool *arena, SECItem *dbkey, 
				certDBEntryType entryType)
{
    /*
     * we only allow _one_ KRL key!
     */
    if (entryType == certDBEntryTypeKeyRevocation) {
	dbkey->len = SEC_DB_KEY_HEADER_LEN;
 	dbkey->data = (unsigned char *)PORT_ArenaAlloc(arena, dbkey->len);
	if ( dbkey->data == NULL ) {
	    goto loser;
	}
        dbkey->data[0] = (unsigned char) entryType;
        return(SECSuccess);
    }
    

    dbkey->len = certKey->len + SEC_DB_KEY_HEADER_LEN;
    if (dbkey->len > NSS_MAX_LEGACY_DB_KEY_SIZE)
	goto loser;
    dbkey->data = (unsigned char *)PORT_ArenaAlloc(arena, dbkey->len);
    if ( dbkey->data == NULL ) {
	goto loser;
    }
    PORT_Memcpy(&dbkey->data[SEC_DB_KEY_HEADER_LEN],
	       certKey->data, certKey->len);
    dbkey->data[0] = (unsigned char) entryType;

    return(SECSuccess);
loser:
    return(SECFailure);
}

static SECStatus
DecodeDBCertEntry(certDBEntryCert *entry, SECItem *dbentry)
{
    unsigned int nnlen;
    unsigned int headerlen;
    int lenoff;

    /* allow updates of old versions of the database */
    switch ( entry->common.version ) {
      case 5:
	headerlen = DB_CERT_V5_ENTRY_HEADER_LEN;
	lenoff = 3;
	break;
      case 6:
	/* should not get here */
	PORT_Assert(0);
	headerlen = DB_CERT_V6_ENTRY_HEADER_LEN;
	lenoff = 3;
	break;
      case 7:
      case 8:
	headerlen = DB_CERT_ENTRY_HEADER_LEN;
	lenoff = 6;
	break;
      default:
	/* better not get here */
	PORT_Assert(0);
	headerlen = DB_CERT_V5_ENTRY_HEADER_LEN;
	lenoff = 3;
	break;
    }
    
    /* is record long enough for header? */
    if ( dbentry->len < headerlen ) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	goto loser;
    }
    
    /* is database entry correct length? */
    entry->derCert.len = ( ( dbentry->data[lenoff] << 8 ) |
			  dbentry->data[lenoff+1] );
    nnlen = ( ( dbentry->data[lenoff+2] << 8 ) | dbentry->data[lenoff+3] );
    lenoff = dbentry->len - ( entry->derCert.len + nnlen + headerlen );
    if ( lenoff ) {
	if ( lenoff < 0 || (lenoff & 0xffff) != 0 ) {
	    PORT_SetError(SEC_ERROR_BAD_DATABASE);
	    goto loser;
	}
	/* The cert size exceeded 64KB.  Reconstruct the correct length. */
	entry->derCert.len += lenoff;
    }
    
    /* copy the dercert */
    entry->derCert.data = pkcs11_copyStaticData(&dbentry->data[headerlen],
	entry->derCert.len,entry->derCertSpace,sizeof(entry->derCertSpace));
    if ( entry->derCert.data == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    /* copy the nickname */
    if ( nnlen > 1 ) {
	entry->nickname = (char *)pkcs11_copyStaticData(
			&dbentry->data[headerlen+entry->derCert.len], nnlen,
			(unsigned char *)entry->nicknameSpace, 
			sizeof(entry->nicknameSpace));
	if ( entry->nickname == NULL ) {
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    goto loser;
	}
    } else {
	entry->nickname = NULL;
    }
    
    if ( entry->common.version < 7 ) {
	/* allow updates of v5 db */
	entry->trust.sslFlags = dbentry->data[0];
	entry->trust.emailFlags = dbentry->data[1];
	entry->trust.objectSigningFlags = dbentry->data[2];
    } else {
	entry->trust.sslFlags = ( dbentry->data[0] << 8 ) | dbentry->data[1];
	entry->trust.emailFlags = ( dbentry->data[2] << 8 ) | dbentry->data[3];
	entry->trust.objectSigningFlags =
	    ( dbentry->data[4] << 8 ) | dbentry->data[5];
    }
    
    return(SECSuccess);
loser:
    return(SECFailure);
}


/*
 * Create a new certDBEntryCert from existing data
 */
static certDBEntryCert *
NewDBCertEntry(SECItem *derCert, char *nickname,
	       NSSLOWCERTCertTrust *trust, int flags)
{
    certDBEntryCert *entry;
    PRArenaPool *arena = NULL;
    int nnlen;
    
    arena = PORT_NewArena( DER_DEFAULT_CHUNKSIZE );

    if ( !arena ) {
	goto loser;
    }
	
    entry = PORT_ArenaZNew(arena, certDBEntryCert);
    if ( entry == NULL ) {
	goto loser;
    }
    
    /* fill in the dbCert */
    entry->common.arena = arena;
    entry->common.type = certDBEntryTypeCert;
    entry->common.version = CERT_DB_FILE_VERSION;
    entry->common.flags = flags;
    
    if ( trust ) {
	entry->trust = *trust;
    }

    entry->derCert.data = (unsigned char *)PORT_ArenaAlloc(arena, derCert->len);
    if ( !entry->derCert.data ) {
	goto loser;
    }
    entry->derCert.len = derCert->len;
    PORT_Memcpy(entry->derCert.data, derCert->data, derCert->len);
    
    nnlen = ( nickname ? strlen(nickname) + 1 : 0 );
    
    if ( nnlen ) {
	entry->nickname = (char *)PORT_ArenaAlloc(arena, nnlen);
	if ( !entry->nickname ) {
	    goto loser;
	}
	PORT_Memcpy(entry->nickname, nickname, nnlen);
	
    } else {
	entry->nickname = 0;
    }

    return(entry);

loser:
    
    /* allocation error, free arena and return */
    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    PORT_SetError(SEC_ERROR_NO_MEMORY);
    return(0);
}

/*
 * Decode a version 4 DBCert from the byte stream database format
 * and construct a current database entry struct
 */
static certDBEntryCert *
DecodeV4DBCertEntry(unsigned char *buf, int len)
{
    certDBEntryCert *entry;
    int certlen;
    int nnlen;
    PRArenaPool *arena;
    
    /* make sure length is at least long enough for the header */
    if ( len < DBCERT_V4_HEADER_LEN ) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	return(0);
    }

    /* get other lengths */
    certlen = buf[3] << 8 | buf[4];
    nnlen = buf[5] << 8 | buf[6];
    
    /* make sure DB entry is the right size */
    if ( ( certlen + nnlen + DBCERT_V4_HEADER_LEN ) != len ) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	return(0);
    }

    /* allocate arena */
    arena = PORT_NewArena( DER_DEFAULT_CHUNKSIZE );

    if ( !arena ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return(0);
    }
	
    /* allocate structure and members */
    entry = (certDBEntryCert *)  PORT_ArenaAlloc(arena, sizeof(certDBEntryCert));

    if ( !entry ) {
	goto loser;
    }

    entry->common.arena = arena;
    entry->common.version = CERT_DB_FILE_VERSION;
    entry->common.type = certDBEntryTypeCert;
    entry->common.flags = 0;
    entry->trust.sslFlags = buf[0];
    entry->trust.emailFlags = buf[1];
    entry->trust.objectSigningFlags = buf[2];

    entry->derCert.data = (unsigned char *)PORT_ArenaAlloc(arena, certlen);
    if ( !entry->derCert.data ) {
	goto loser;
    }
    entry->derCert.len = certlen;
    PORT_Memcpy(entry->derCert.data, &buf[DBCERT_V4_HEADER_LEN], certlen);

    if ( nnlen ) {
        entry->nickname = (char *) PORT_ArenaAlloc(arena, nnlen);
        if ( !entry->nickname ) {
            goto loser;
        }
        PORT_Memcpy(entry->nickname, &buf[DBCERT_V4_HEADER_LEN + certlen], nnlen);
        
        if (PORT_Strcmp(entry->nickname, "Server-Cert") == 0) {
            entry->trust.sslFlags |= CERTDB_USER;
        }
    } else {
        entry->nickname = 0;
    }

    return(entry);
    
loser:
    PORT_FreeArena(arena, PR_FALSE);
    PORT_SetError(SEC_ERROR_NO_MEMORY);
    return(0);
}

/*
 * Encode a Certificate database entry into byte stream suitable for
 * the database
 */
static SECStatus
WriteDBCertEntry(NSSLOWCERTCertDBHandle *handle, certDBEntryCert *entry)
{
    SECItem dbitem, dbkey;
    PRArenaPool *tmparena = NULL;
    SECItem tmpitem;
    SECStatus rv;
    
    tmparena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( tmparena == NULL ) {
	goto loser;
    }
    
    rv = EncodeDBCertEntry(entry, tmparena, &dbitem);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* get the database key and format it */
    rv = nsslowcert_KeyFromDERCert(tmparena, &entry->derCert, &tmpitem);
    if ( rv == SECFailure ) {
	goto loser;
    }

    rv = EncodeDBCertKey(&tmpitem, tmparena, &dbkey);
    if ( rv == SECFailure ) {
	goto loser;
    }
    
    /* now write it to the database */
    rv = WriteDBEntry(handle, &entry->common, &dbkey, &dbitem);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    
    PORT_FreeArena(tmparena, PR_FALSE);
    return(SECSuccess);

loser:
    if ( tmparena ) {
	PORT_FreeArena(tmparena, PR_FALSE);
    }
    return(SECFailure);
}


/*
 * delete a certificate entry
 */
static SECStatus
DeleteDBCertEntry(NSSLOWCERTCertDBHandle *handle, SECItem *certKey)
{
    SECItem dbkey;
    SECStatus rv;

    dbkey.data= NULL;
    dbkey.len = 0;

    rv = EncodeDBCertKey(certKey, NULL, &dbkey);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    
    rv = DeleteDBEntry(handle, certDBEntryTypeCert, &dbkey);
    if ( rv == SECFailure ) {
	goto loser;
    }

    PORT_Free(dbkey.data);

    return(SECSuccess);

loser:
    if (dbkey.data) {
	PORT_Free(dbkey.data);
    }
    return(SECFailure);
}

static certDBEntryCert *
CreateCertEntry(void)
{
    certDBEntryCert *entry;

    nsslowcert_LockFreeList();
    entry = entryListHead;
    if (entry) {
	entryListCount--;
	entryListHead = entry->next;
    }
    PORT_Assert(entryListCount >= 0);
    nsslowcert_UnlockFreeList();
    if (entry) {
	return entry;
    }

    return PORT_ZNew(certDBEntryCert);
}

static void
DestroyCertEntryFreeList(void)
{
    certDBEntryCert *entry;

    nsslowcert_LockFreeList();
    while (NULL != (entry = entryListHead)) {
	entryListCount--;
	entryListHead = entry->next;
	PORT_Free(entry);
    }
    PORT_Assert(!entryListCount);
    entryListCount = 0;
    nsslowcert_UnlockFreeList();
}

/*
 * Read a certificate entry
 */
static certDBEntryCert *
ReadDBCertEntry(NSSLOWCERTCertDBHandle *handle, const SECItem *certKey)
{
    certDBEntryCert *entry;
    SECItem dbkey;
    SECItem dbentry;
    SECStatus rv;
    unsigned char buf[512];

    dbkey.data = buf;
    dbkey.len = sizeof(buf);

    entry = CreateCertEntry();
    if ( entry == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    entry->common.arena = NULL;
    entry->common.type = certDBEntryTypeCert;

    rv = EncodeDBCertKey(certKey, NULL, &dbkey);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    
    rv = ReadDBEntry(handle, &entry->common, &dbkey, &dbentry, NULL);
    if ( rv == SECFailure ) {
	goto loser;
    }

    rv = DecodeDBCertEntry(entry, &dbentry);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    pkcs11_freeStaticData(dbkey.data,buf);    
    dbkey.data = NULL;
    return(entry);
    
loser:
    pkcs11_freeStaticData(dbkey.data,buf);    
    dbkey.data = NULL;
    if ( entry ) {
        DestroyDBEntry((certDBEntry *)entry);
    }
    
    return(NULL);
}

/*
 * encode a database cert record
 */
static SECStatus
EncodeDBCrlEntry(certDBEntryRevocation *entry, PRArenaPool *arena, SECItem *dbitem)
{
    unsigned int nnlen = 0;
    unsigned char *buf;
  
    if (entry->url) {  
	nnlen = PORT_Strlen(entry->url) + 1;
    }
    
    /* allocate space for encoded database record, including space
     * for low level header
     */
    dbitem->len = entry->derCrl.len + nnlen 
		+ SEC_DB_ENTRY_HEADER_LEN + DB_CRL_ENTRY_HEADER_LEN;
    
    dbitem->data = (unsigned char *)PORT_ArenaAlloc(arena, dbitem->len);
    if ( dbitem->data == NULL) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    
    /* fill in database record */
    buf = &dbitem->data[SEC_DB_ENTRY_HEADER_LEN];
    
    buf[0] = (PRUint8)( entry->derCrl.len >> 8 );
    buf[1] = (PRUint8)( entry->derCrl.len      );
    buf[2] = (PRUint8)( nnlen >> 8 );
    buf[3] = (PRUint8)( nnlen      );
    
    PORT_Memcpy(&buf[DB_CRL_ENTRY_HEADER_LEN], entry->derCrl.data,
	      entry->derCrl.len);

    if (nnlen != 0) {
	PORT_Memcpy(&buf[DB_CRL_ENTRY_HEADER_LEN + entry->derCrl.len],
	      entry->url, nnlen);
    }

    return(SECSuccess);

loser:
    return(SECFailure);
}

static SECStatus
DecodeDBCrlEntry(certDBEntryRevocation *entry, SECItem *dbentry)
{
    unsigned int urlLen;
    int lenDiff;

    /* is record long enough for header? */
    if ( dbentry->len < DB_CRL_ENTRY_HEADER_LEN ) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	goto loser;
    }
    
    /* is database entry correct length? */
    entry->derCrl.len = ( ( dbentry->data[0] << 8 ) | dbentry->data[1] );
    urlLen =            ( ( dbentry->data[2] << 8 ) | dbentry->data[3] );
    lenDiff = dbentry->len - 
			(entry->derCrl.len + urlLen + DB_CRL_ENTRY_HEADER_LEN);
    if (lenDiff) {
    	if (lenDiff < 0 || (lenDiff & 0xffff) != 0) {
	    PORT_SetError(SEC_ERROR_BAD_DATABASE);
	    goto loser;
	}    
	/* CRL entry is greater than 64 K. Hack to make this continue to work */
	entry->derCrl.len += lenDiff;
    }
    
    /* copy the der CRL */
    entry->derCrl.data = (unsigned char *)PORT_ArenaAlloc(entry->common.arena,
							 entry->derCrl.len);
    if ( entry->derCrl.data == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    PORT_Memcpy(entry->derCrl.data, &dbentry->data[DB_CRL_ENTRY_HEADER_LEN],
	      entry->derCrl.len);

    /* copy the url */
    entry->url = NULL;
    if (urlLen != 0) {
	entry->url = (char *)PORT_ArenaAlloc(entry->common.arena, urlLen);
	if ( entry->url == NULL ) {
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    goto loser;
	}
	PORT_Memcpy(entry->url,
	      &dbentry->data[DB_CRL_ENTRY_HEADER_LEN + entry->derCrl.len],
	      urlLen);
    }
    
    return(SECSuccess);
loser:
    return(SECFailure);
}

/*
 * Create a new certDBEntryRevocation from existing data
 */
static certDBEntryRevocation *
NewDBCrlEntry(SECItem *derCrl, char * url, certDBEntryType crlType, int flags)
{
    certDBEntryRevocation *entry;
    PRArenaPool *arena = NULL;
    int nnlen;
    
    arena = PORT_NewArena( DER_DEFAULT_CHUNKSIZE );

    if ( !arena ) {
	goto loser;
    }
	
    entry = PORT_ArenaZNew(arena, certDBEntryRevocation);
    if ( entry == NULL ) {
	goto loser;
    }
    
    /* fill in the dbRevolcation */
    entry->common.arena = arena;
    entry->common.type = crlType;
    entry->common.version = CERT_DB_FILE_VERSION;
    entry->common.flags = flags;
    

    entry->derCrl.data = (unsigned char *)PORT_ArenaAlloc(arena, derCrl->len);
    if ( !entry->derCrl.data ) {
	goto loser;
    }

    if (url) {
	nnlen = PORT_Strlen(url) + 1;
	entry->url  = (char *)PORT_ArenaAlloc(arena, nnlen);
	if ( !entry->url ) {
	    goto loser;
	}
	PORT_Memcpy(entry->url, url, nnlen);
    } else {
	entry->url = NULL;
    }

	
    entry->derCrl.len = derCrl->len;
    PORT_Memcpy(entry->derCrl.data, derCrl->data, derCrl->len);

    return(entry);

loser:
    
    /* allocation error, free arena and return */
    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    PORT_SetError(SEC_ERROR_NO_MEMORY);
    return(0);
}


static SECStatus
WriteDBCrlEntry(NSSLOWCERTCertDBHandle *handle, certDBEntryRevocation *entry,
				SECItem *crlKey )
{
    SECItem dbkey;
    PRArenaPool *tmparena = NULL;
    SECItem encodedEntry;
    SECStatus rv;
    
    tmparena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( tmparena == NULL ) {
	goto loser;
    }

    rv = EncodeDBCrlEntry(entry, tmparena, &encodedEntry);
    if ( rv == SECFailure ) {
	goto loser;
    }

    rv = EncodeDBGenericKey(crlKey, tmparena, &dbkey, entry->common.type);
    if ( rv == SECFailure ) {
	goto loser;
    }
    
    /* now write it to the database */
    rv = WriteDBEntry(handle, &entry->common, &dbkey, &encodedEntry);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    
    PORT_FreeArena(tmparena, PR_FALSE);
    return(SECSuccess);

loser:
    if ( tmparena ) {
	PORT_FreeArena(tmparena, PR_FALSE);
    }
    return(SECFailure);
}
/*
 * delete a crl entry
 */
static SECStatus
DeleteDBCrlEntry(NSSLOWCERTCertDBHandle *handle, const SECItem *crlKey, 
						certDBEntryType crlType)
{
    SECItem dbkey;
    PRArenaPool *arena = NULL;
    SECStatus rv;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	goto loser;
    }

    rv = EncodeDBGenericKey(crlKey, arena, &dbkey, crlType);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    
    rv = DeleteDBEntry(handle, crlType, &dbkey);
    if ( rv == SECFailure ) {
	goto loser;
    }

    PORT_FreeArena(arena, PR_FALSE);
    return(SECSuccess);

loser:
    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(SECFailure);
}

/*
 * Read a certificate entry
 */
static certDBEntryRevocation *
ReadDBCrlEntry(NSSLOWCERTCertDBHandle *handle, SECItem *certKey,
						certDBEntryType crlType)
{
    PRArenaPool *arena = NULL;
    PRArenaPool *tmparena = NULL;
    certDBEntryRevocation *entry;
    SECItem dbkey;
    SECItem dbentry;
    SECStatus rv;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    tmparena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( tmparena == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    
    entry = (certDBEntryRevocation *)
			PORT_ArenaAlloc(arena, sizeof(certDBEntryRevocation));
    if ( entry == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    entry->common.arena = arena;
    entry->common.type = crlType;

    rv = EncodeDBGenericKey(certKey, tmparena, &dbkey, crlType);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    
    rv = ReadDBEntry(handle, &entry->common, &dbkey, &dbentry, NULL);
    if ( rv == SECFailure ) {
	goto loser;
    }

    rv = DecodeDBCrlEntry(entry, &dbentry);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    
    PORT_FreeArena(tmparena, PR_FALSE);
    return(entry);
    
loser:
    if ( tmparena ) {
	PORT_FreeArena(tmparena, PR_FALSE);
    }
    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(NULL);
}

void
nsslowcert_DestroyDBEntry(certDBEntry *entry)
{
    DestroyDBEntry(entry);
    return;
}

/*
 * Encode a database nickname record
 */
static SECStatus
EncodeDBNicknameEntry(certDBEntryNickname *entry, PRArenaPool *arena,
		      SECItem *dbitem)
{
    unsigned char *buf;
    
    /* allocate space for encoded database record, including space
     * for low level header
     */
    dbitem->len = entry->subjectName.len + DB_NICKNAME_ENTRY_HEADER_LEN +
	SEC_DB_ENTRY_HEADER_LEN;
    dbitem->data = (unsigned char *)PORT_ArenaAlloc(arena, dbitem->len);
    if ( dbitem->data == NULL) {
	goto loser;
    }
    
    /* fill in database record */
    buf = &dbitem->data[SEC_DB_ENTRY_HEADER_LEN];
    buf[0] = (PRUint8)( entry->subjectName.len >> 8 );
    buf[1] = (PRUint8)( entry->subjectName.len      );
    PORT_Memcpy(&buf[DB_NICKNAME_ENTRY_HEADER_LEN], entry->subjectName.data,
	      entry->subjectName.len);

    return(SECSuccess);

loser:
    return(SECFailure);
}

/*
 * Encode a database key for a nickname record
 */
static SECStatus
EncodeDBNicknameKey(char *nickname, PRArenaPool *arena,
		    SECItem *dbkey)
{
    unsigned int nnlen;
    
    nnlen = PORT_Strlen(nickname) + 1; /* includes null */

    /* now get the database key and format it */
    dbkey->len = nnlen + SEC_DB_KEY_HEADER_LEN;
    if (dbkey->len > NSS_MAX_LEGACY_DB_KEY_SIZE)
	goto loser;
    dbkey->data = (unsigned char *)PORT_ArenaAlloc(arena, dbkey->len);
    if ( dbkey->data == NULL ) {
	goto loser;
    }
    PORT_Memcpy(&dbkey->data[SEC_DB_KEY_HEADER_LEN], nickname, nnlen);
    dbkey->data[0] = certDBEntryTypeNickname;

    return(SECSuccess);

loser:
    return(SECFailure);
}

static SECStatus
DecodeDBNicknameEntry(certDBEntryNickname *entry, SECItem *dbentry,
                      char *nickname)
{
    int lenDiff;

    /* is record long enough for header? */
    if ( dbentry->len < DB_NICKNAME_ENTRY_HEADER_LEN ) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	goto loser;
    }
    
    /* is database entry correct length? */
    entry->subjectName.len = ( ( dbentry->data[0] << 8 ) | dbentry->data[1] );
    lenDiff = dbentry->len - 
	      (entry->subjectName.len + DB_NICKNAME_ENTRY_HEADER_LEN);
    if (lenDiff) {
	if (lenDiff < 0 || (lenDiff & 0xffff) != 0 ) { 
	    PORT_SetError(SEC_ERROR_BAD_DATABASE);
	    goto loser;
	}
	/* The entry size exceeded 64KB.  Reconstruct the correct length. */
	entry->subjectName.len += lenDiff;
    }

    /* copy the certkey */
    entry->subjectName.data =
	(unsigned char *)PORT_ArenaAlloc(entry->common.arena,
					 entry->subjectName.len);
    if ( entry->subjectName.data == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    PORT_Memcpy(entry->subjectName.data,
	      &dbentry->data[DB_NICKNAME_ENTRY_HEADER_LEN],
	      entry->subjectName.len);
    entry->subjectName.type = siBuffer;
    
    entry->nickname = (char *)PORT_ArenaAlloc(entry->common.arena, 
                                              PORT_Strlen(nickname)+1);
    if ( entry->nickname ) {
	PORT_Strcpy(entry->nickname, nickname);
    }
    
    return(SECSuccess);

loser:
    return(SECFailure);
}

/*
 * create a new nickname entry
 */
static certDBEntryNickname *
NewDBNicknameEntry(char *nickname, SECItem *subjectName, unsigned int flags)
{
    PRArenaPool *arena = NULL;
    certDBEntryNickname *entry;
    int nnlen;
    SECStatus rv;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    entry = (certDBEntryNickname *)PORT_ArenaAlloc(arena,
						 sizeof(certDBEntryNickname));
    if ( entry == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    /* init common fields */
    entry->common.arena = arena;
    entry->common.type = certDBEntryTypeNickname;
    entry->common.version = CERT_DB_FILE_VERSION;
    entry->common.flags = flags;

    /* copy the nickname */
    nnlen = PORT_Strlen(nickname) + 1;
    
    entry->nickname = (char*)PORT_ArenaAlloc(arena, nnlen);
    if ( entry->nickname == NULL ) {
	goto loser;
    }
    
    PORT_Memcpy(entry->nickname, nickname, nnlen);
    
    rv = SECITEM_CopyItem(arena, &entry->subjectName, subjectName);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    
    return(entry);
loser:
    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(NULL);
}

/*
 * delete a nickname entry
 */
static SECStatus
DeleteDBNicknameEntry(NSSLOWCERTCertDBHandle *handle, char *nickname)
{
    PRArenaPool *arena = NULL;
    SECStatus rv;
    SECItem dbkey;
    
    if ( nickname == NULL ) {
	return(SECSuccess);
    }
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	goto loser;
    }

    rv = EncodeDBNicknameKey(nickname, arena, &dbkey);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    rv = DeleteDBEntry(handle, certDBEntryTypeNickname, &dbkey);
    if ( rv == SECFailure ) {
	goto loser;
    }

    PORT_FreeArena(arena, PR_FALSE);
    return(SECSuccess);

loser:
    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(SECFailure);
}

/*
 * Read a nickname entry
 */
static certDBEntryNickname *
ReadDBNicknameEntry(NSSLOWCERTCertDBHandle *handle, char *nickname)
{
    PRArenaPool *arena = NULL;
    PRArenaPool *tmparena = NULL;
    certDBEntryNickname *entry;
    SECItem dbkey;
    SECItem dbentry;
    SECStatus rv;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    tmparena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( tmparena == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    
    entry = (certDBEntryNickname *)PORT_ArenaAlloc(arena,
						 sizeof(certDBEntryNickname));
    if ( entry == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    entry->common.arena = arena;
    entry->common.type = certDBEntryTypeNickname;

    rv = EncodeDBNicknameKey(nickname, tmparena, &dbkey);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    
    rv = ReadDBEntry(handle, &entry->common, &dbkey, &dbentry, tmparena);
    if ( rv == SECFailure ) {
	goto loser;
    }

    /* is record long enough for header? */
    if ( dbentry.len < DB_NICKNAME_ENTRY_HEADER_LEN ) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	goto loser;
    }

    rv = DecodeDBNicknameEntry(entry, &dbentry, nickname);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    
    PORT_FreeArena(tmparena, PR_FALSE);
    return(entry);
    
loser:
    if ( tmparena ) {
	PORT_FreeArena(tmparena, PR_FALSE);
    }
    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(NULL);
}

/*
 * Encode a nickname entry into byte stream suitable for
 * the database
 */
static SECStatus
WriteDBNicknameEntry(NSSLOWCERTCertDBHandle *handle, certDBEntryNickname *entry)
{
    SECItem dbitem, dbkey;
    PRArenaPool *tmparena = NULL;
    SECStatus rv;
    
    tmparena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( tmparena == NULL ) {
	goto loser;
    }
    
    rv = EncodeDBNicknameEntry(entry, tmparena, &dbitem);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    rv = EncodeDBNicknameKey(entry->nickname, tmparena, &dbkey);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* now write it to the database */
    rv = WriteDBEntry(handle, &entry->common, &dbkey, &dbitem);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    
    PORT_FreeArena(tmparena, PR_FALSE);
    return(SECSuccess);

loser:
    if ( tmparena ) {
	PORT_FreeArena(tmparena, PR_FALSE);
    }
    return(SECFailure);
    
}

static SECStatus
EncodeDBSMimeEntry(certDBEntrySMime *entry, PRArenaPool *arena,
		   SECItem *dbitem)
{
    unsigned char *buf;
    
    /* allocate space for encoded database record, including space
     * for low level header
     */
    dbitem->len = entry->subjectName.len + entry->smimeOptions.len +
	entry->optionsDate.len +
	DB_SMIME_ENTRY_HEADER_LEN + SEC_DB_ENTRY_HEADER_LEN;
    
    dbitem->data = (unsigned char *)PORT_ArenaAlloc(arena, dbitem->len);
    if ( dbitem->data == NULL) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    
    /* fill in database record */
    buf = &dbitem->data[SEC_DB_ENTRY_HEADER_LEN];
    
    buf[0] = (PRUint8)( entry->subjectName.len >> 8 );
    buf[1] = (PRUint8)( entry->subjectName.len      );
    buf[2] = (PRUint8)( entry->smimeOptions.len >> 8 );
    buf[3] = (PRUint8)( entry->smimeOptions.len      );
    buf[4] = (PRUint8)( entry->optionsDate.len >> 8 );
    buf[5] = (PRUint8)( entry->optionsDate.len      );

    /* if no smime options, then there should not be an options date either */
    PORT_Assert( ! ( ( entry->smimeOptions.len == 0 ) &&
		    ( entry->optionsDate.len != 0 ) ) );
    
    PORT_Memcpy(&buf[DB_SMIME_ENTRY_HEADER_LEN], entry->subjectName.data,
	      entry->subjectName.len);
    if ( entry->smimeOptions.len ) {
	PORT_Memcpy(&buf[DB_SMIME_ENTRY_HEADER_LEN+entry->subjectName.len],
		    entry->smimeOptions.data,
		    entry->smimeOptions.len);
	PORT_Memcpy(&buf[DB_SMIME_ENTRY_HEADER_LEN + entry->subjectName.len +
			 entry->smimeOptions.len],
		    entry->optionsDate.data,
		    entry->optionsDate.len);
    }

    return(SECSuccess);

loser:
    return(SECFailure);
}

/*
 * Encode a database key for a SMIME record
 */
static SECStatus
EncodeDBSMimeKey(char *emailAddr, PRArenaPool *arena,
		 SECItem *dbkey)
{
    unsigned int addrlen;
    
    addrlen = PORT_Strlen(emailAddr) + 1; /* includes null */

    /* now get the database key and format it */
    dbkey->len = addrlen + SEC_DB_KEY_HEADER_LEN;
    if (dbkey->len > NSS_MAX_LEGACY_DB_KEY_SIZE)
	goto loser;
    dbkey->data = (unsigned char *)PORT_ArenaAlloc(arena, dbkey->len);
    if ( dbkey->data == NULL ) {
	goto loser;
    }
    PORT_Memcpy(&dbkey->data[SEC_DB_KEY_HEADER_LEN], emailAddr, addrlen);
    dbkey->data[0] = certDBEntryTypeSMimeProfile;

    return(SECSuccess);

loser:
    return(SECFailure);
}

/*
 * Decode a database SMIME record
 */
static SECStatus
DecodeDBSMimeEntry(certDBEntrySMime *entry, SECItem *dbentry, char *emailAddr)
{
    int lenDiff;

    /* is record long enough for header? */
    if ( dbentry->len < DB_SMIME_ENTRY_HEADER_LEN ) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	goto loser;
    }
    
    /* is database entry correct length? */
    entry->subjectName.len  = (( dbentry->data[0] << 8 ) | dbentry->data[1] );
    entry->smimeOptions.len = (( dbentry->data[2] << 8 ) | dbentry->data[3] );
    entry->optionsDate.len  = (( dbentry->data[4] << 8 ) | dbentry->data[5] );
    lenDiff = dbentry->len - (entry->subjectName.len + 
                              entry->smimeOptions.len + 
			      entry->optionsDate.len + 
			      DB_SMIME_ENTRY_HEADER_LEN);
    if (lenDiff) {
	if (lenDiff < 0 || (lenDiff & 0xffff) != 0 ) { 
	    PORT_SetError(SEC_ERROR_BAD_DATABASE);
	    goto loser;
	}
	/* The entry size exceeded 64KB.  Reconstruct the correct length. */
	entry->subjectName.len += lenDiff;
    }

    /* copy the subject name */
    entry->subjectName.data =
	(unsigned char *)PORT_ArenaAlloc(entry->common.arena,
					 entry->subjectName.len);
    if ( entry->subjectName.data == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    PORT_Memcpy(entry->subjectName.data,
	      &dbentry->data[DB_SMIME_ENTRY_HEADER_LEN],
	      entry->subjectName.len);

    /* copy the smime options */
    if ( entry->smimeOptions.len ) {
	entry->smimeOptions.data =
	    (unsigned char *)PORT_ArenaAlloc(entry->common.arena,
					     entry->smimeOptions.len);
	if ( entry->smimeOptions.data == NULL ) {
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    goto loser;
	}
	PORT_Memcpy(entry->smimeOptions.data,
		    &dbentry->data[DB_SMIME_ENTRY_HEADER_LEN +
				   entry->subjectName.len],
		    entry->smimeOptions.len);
    }
    if ( entry->optionsDate.len ) {
	entry->optionsDate.data =
	    (unsigned char *)PORT_ArenaAlloc(entry->common.arena,
					     entry->optionsDate.len);
	if ( entry->optionsDate.data == NULL ) {
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    goto loser;
	}
	PORT_Memcpy(entry->optionsDate.data,
		    &dbentry->data[DB_SMIME_ENTRY_HEADER_LEN +
				   entry->subjectName.len +
				   entry->smimeOptions.len],
		    entry->optionsDate.len);
    }

    /* both options and options date must either exist or not exist */
    if ( ( ( entry->optionsDate.len == 0 ) ||
	  ( entry->smimeOptions.len == 0 ) ) &&
	entry->smimeOptions.len != entry->optionsDate.len ) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	goto loser;
    }

    entry->emailAddr = (char *)PORT_ArenaAlloc(entry->common.arena,
						PORT_Strlen(emailAddr)+1);
    if ( entry->emailAddr ) {
	PORT_Strcpy(entry->emailAddr, emailAddr);
    }
    
    return(SECSuccess);

loser:
    return(SECFailure);
}

/*
 * create a new SMIME entry
 */
static certDBEntrySMime *
NewDBSMimeEntry(char *emailAddr, SECItem *subjectName, SECItem *smimeOptions,
		SECItem *optionsDate, unsigned int flags)
{
    PRArenaPool *arena = NULL;
    certDBEntrySMime *entry;
    int addrlen;
    SECStatus rv;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    entry = (certDBEntrySMime *)PORT_ArenaAlloc(arena,
						sizeof(certDBEntrySMime));
    if ( entry == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    /* init common fields */
    entry->common.arena = arena;
    entry->common.type = certDBEntryTypeSMimeProfile;
    entry->common.version = CERT_DB_FILE_VERSION;
    entry->common.flags = flags;

    /* copy the email addr */
    addrlen = PORT_Strlen(emailAddr) + 1;
    
    entry->emailAddr = (char*)PORT_ArenaAlloc(arena, addrlen);
    if ( entry->emailAddr == NULL ) {
	goto loser;
    }
    
    PORT_Memcpy(entry->emailAddr, emailAddr, addrlen);
    
    /* copy the subject name */
    rv = SECITEM_CopyItem(arena, &entry->subjectName, subjectName);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* copy the smime options */
    if ( smimeOptions ) {
	rv = SECITEM_CopyItem(arena, &entry->smimeOptions, smimeOptions);
	if ( rv != SECSuccess ) {
	    goto loser;
	}
    } else {
	PORT_Assert(optionsDate == NULL);
	entry->smimeOptions.data = NULL;
	entry->smimeOptions.len = 0;
    }

    /* copy the options date */
    if ( optionsDate ) {
	rv = SECITEM_CopyItem(arena, &entry->optionsDate, optionsDate);
	if ( rv != SECSuccess ) {
	    goto loser;
	}
    } else {
	PORT_Assert(smimeOptions == NULL);
	entry->optionsDate.data = NULL;
	entry->optionsDate.len = 0;
    }
    
    return(entry);
loser:
    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(NULL);
}

/*
 * delete a SMIME entry
 */
static SECStatus
DeleteDBSMimeEntry(NSSLOWCERTCertDBHandle *handle, char *emailAddr)
{
    PRArenaPool *arena = NULL;
    SECStatus rv;
    SECItem dbkey;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	goto loser;
    }

    rv = EncodeDBSMimeKey(emailAddr, arena, &dbkey);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    rv = DeleteDBEntry(handle, certDBEntryTypeSMimeProfile, &dbkey);
    if ( rv == SECFailure ) {
	goto loser;
    }

    PORT_FreeArena(arena, PR_FALSE);
    return(SECSuccess);

loser:
    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(SECFailure);
}

/*
 * Read a SMIME entry
 */
certDBEntrySMime *
nsslowcert_ReadDBSMimeEntry(NSSLOWCERTCertDBHandle *handle, char *emailAddr)
{
    PRArenaPool *arena = NULL;
    PRArenaPool *tmparena = NULL;
    certDBEntrySMime *entry;
    SECItem dbkey;
    SECItem dbentry;
    SECStatus rv;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    tmparena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( tmparena == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    
    entry = (certDBEntrySMime *)PORT_ArenaAlloc(arena,
						sizeof(certDBEntrySMime));
    if ( entry == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    entry->common.arena = arena;
    entry->common.type = certDBEntryTypeSMimeProfile;

    rv = EncodeDBSMimeKey(emailAddr, tmparena, &dbkey);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    
    rv = ReadDBEntry(handle, &entry->common, &dbkey, &dbentry, tmparena);
    if ( rv == SECFailure ) {
	goto loser;
    }

    /* is record long enough for header? */
    if ( dbentry.len < DB_SMIME_ENTRY_HEADER_LEN ) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	goto loser;
    }

    rv = DecodeDBSMimeEntry(entry, &dbentry, emailAddr);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    
    PORT_FreeArena(tmparena, PR_FALSE);
    return(entry);
    
loser:
    if ( tmparena ) {
	PORT_FreeArena(tmparena, PR_FALSE);
    }
    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(NULL);
}

/*
 * Encode a SMIME entry into byte stream suitable for
 * the database
 */
static SECStatus
WriteDBSMimeEntry(NSSLOWCERTCertDBHandle *handle, certDBEntrySMime *entry)
{
    SECItem dbitem, dbkey;
    PRArenaPool *tmparena = NULL;
    SECStatus rv;
    
    tmparena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( tmparena == NULL ) {
	goto loser;
    }
    
    rv = EncodeDBSMimeEntry(entry, tmparena, &dbitem);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    rv = EncodeDBSMimeKey(entry->emailAddr, tmparena, &dbkey);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* now write it to the database */
    rv = WriteDBEntry(handle, &entry->common, &dbkey, &dbitem);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    
    PORT_FreeArena(tmparena, PR_FALSE);
    return(SECSuccess);

loser:
    if ( tmparena ) {
	PORT_FreeArena(tmparena, PR_FALSE);
    }
    return(SECFailure);
    
}

/*
 * Encode a database subject record
 */
static SECStatus
EncodeDBSubjectEntry(certDBEntrySubject *entry, PRArenaPool *arena,
		     SECItem *dbitem)
{
    unsigned char *buf;
    int len;
    unsigned int ncerts;
    unsigned int i;
    unsigned char *tmpbuf;
    unsigned int nnlen = 0;
    unsigned int eaddrslen = 0;
    int keyidoff;
    SECItem *certKeys = entry->certKeys;
    SECItem *keyIDs   = entry->keyIDs;;
    
    if ( entry->nickname ) {
	nnlen = PORT_Strlen(entry->nickname) + 1;
    }
    if ( entry->emailAddrs ) {
	eaddrslen = 2;
	for (i=0; i < entry->nemailAddrs; i++) {
	    eaddrslen += PORT_Strlen(entry->emailAddrs[i]) + 1 + 2;
	}
    }

    ncerts = entry->ncerts;
    
    /* compute the length of the entry */
    keyidoff = DB_SUBJECT_ENTRY_HEADER_LEN + nnlen ;
    len = keyidoff + (4 * ncerts) + eaddrslen;
    for ( i = 0; i < ncerts; i++ ) {
	if (keyIDs[i].len   > 0xffff ||
	   (certKeys[i].len > 0xffff)) {
    	    PORT_SetError(SEC_ERROR_INPUT_LEN);
	    goto loser;
	}
	len += certKeys[i].len;
	len += keyIDs[i].len;
    }
    
    /* allocate space for encoded database record, including space
     * for low level header
     */
    dbitem->len = len + SEC_DB_ENTRY_HEADER_LEN;
    
    dbitem->data = (unsigned char *)PORT_ArenaAlloc(arena, dbitem->len);
    if ( dbitem->data == NULL) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    
    /* fill in database record */
    buf = &dbitem->data[SEC_DB_ENTRY_HEADER_LEN];
    
    buf[0] = (PRUint8)( ncerts >> 8 );
    buf[1] = (PRUint8)( ncerts      );
    buf[2] = (PRUint8)( nnlen >> 8 );
    buf[3] = (PRUint8)( nnlen      );
    /* v7 email field is NULL in v8 */
    buf[4] = 0;
    buf[5] = 0;

    PORT_Memcpy(&buf[DB_SUBJECT_ENTRY_HEADER_LEN], entry->nickname, nnlen);
    tmpbuf = &buf[keyidoff];   
    for ( i = 0; i < ncerts; i++ ) {
	tmpbuf[0] = (PRUint8)( certKeys[i].len >> 8 );
	tmpbuf[1] = (PRUint8)( certKeys[i].len      );
	tmpbuf += 2;
    }
    for ( i = 0; i < ncerts; i++ ) {
	tmpbuf[0] = (PRUint8)( keyIDs[i].len >> 8 );
	tmpbuf[1] = (PRUint8)( keyIDs[i].len      );
	tmpbuf += 2;
    }
    
    for ( i = 0; i < ncerts; i++ ) {
	PORT_Memcpy(tmpbuf, certKeys[i].data, certKeys[i].len);
	tmpbuf += certKeys[i].len;
    }
    for ( i = 0; i < ncerts; i++ ) {
	PORT_Memcpy(tmpbuf, keyIDs[i].data, keyIDs[i].len);
	tmpbuf += keyIDs[i].len;
    }

    if (entry->emailAddrs) {
	tmpbuf[0] = (PRUint8)( entry->nemailAddrs >> 8 );
	tmpbuf[1] = (PRUint8)( entry->nemailAddrs      );
	tmpbuf += 2;
	for (i=0; i < entry->nemailAddrs; i++) {
	    int nameLen = PORT_Strlen(entry->emailAddrs[i]) + 1;
	    tmpbuf[0] = (PRUint8)( nameLen >> 8 );
	    tmpbuf[1] = (PRUint8)( nameLen      );
	    tmpbuf += 2;
	    PORT_Memcpy(tmpbuf,entry->emailAddrs[i],nameLen);
	    tmpbuf +=nameLen;
	}
    }

    PORT_Assert(tmpbuf == &buf[len]);
    
    return(SECSuccess);

loser:
    return(SECFailure);
}

/*
 * Encode a database key for a subject record
 */
static SECStatus
EncodeDBSubjectKey(SECItem *derSubject, PRArenaPool *arena,
		   SECItem *dbkey)
{
    dbkey->len = derSubject->len + SEC_DB_KEY_HEADER_LEN;
    if (dbkey->len > NSS_MAX_LEGACY_DB_KEY_SIZE)
	goto loser;
    dbkey->data = (unsigned char *)PORT_ArenaAlloc(arena, dbkey->len);
    if ( dbkey->data == NULL ) {
	goto loser;
    }
    PORT_Memcpy(&dbkey->data[SEC_DB_KEY_HEADER_LEN], derSubject->data,
	      derSubject->len);
    dbkey->data[0] = certDBEntryTypeSubject;

    return(SECSuccess);

loser:
    return(SECFailure);
}

static SECStatus
DecodeDBSubjectEntry(certDBEntrySubject *entry, SECItem *dbentry,
		     const SECItem *derSubject)
{
    PRArenaPool *arena     = entry->common.arena;
    unsigned char *tmpbuf;
    unsigned char *end;
    void        *mark      = PORT_ArenaMark(arena);
    unsigned int eaddrlen;
    unsigned int i;
    unsigned int keyidoff;
    unsigned int len;
    unsigned int ncerts    = 0;
    unsigned int nnlen;
    SECStatus rv;

    rv = SECITEM_CopyItem(arena, &entry->derSubject, derSubject);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* is record long enough for header? */
    if ( dbentry->len < DB_SUBJECT_ENTRY_HEADER_LEN ) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	goto loser;
    }
    
    entry->ncerts = ncerts = (( dbentry->data[0] << 8 ) | dbentry->data[1] );
    nnlen =                  (( dbentry->data[2] << 8 ) | dbentry->data[3] );
    eaddrlen =               (( dbentry->data[4] << 8 ) | dbentry->data[5] );
    keyidoff = DB_SUBJECT_ENTRY_HEADER_LEN + nnlen + eaddrlen;
    len = keyidoff + (4 * ncerts);
    if ( dbentry->len < len) {
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	goto loser;
    }
    
    entry->certKeys = PORT_ArenaNewArray(arena, SECItem, ncerts);
    entry->keyIDs   = PORT_ArenaNewArray(arena, SECItem, ncerts);
    if ( ( entry->certKeys == NULL ) || ( entry->keyIDs == NULL ) ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    if ( nnlen > 1 ) { /* null terminator is stored */
	entry->nickname = (char *)PORT_ArenaAlloc(arena, nnlen);
	if ( entry->nickname == NULL ) {
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    goto loser;
	}
	PORT_Memcpy(entry->nickname,
		    &dbentry->data[DB_SUBJECT_ENTRY_HEADER_LEN],
		    nnlen);
    } else {
	entry->nickname = NULL;
    }

    /* if we have an old style email entry, there is only one */    
    entry->nemailAddrs = 0;
    if ( eaddrlen > 1 ) { /* null terminator is stored */
	entry->emailAddrs = PORT_ArenaNewArray(arena, char *, 2);
	if ( entry->emailAddrs == NULL ) {
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    goto loser;
	}
	entry->emailAddrs[0] = (char *)PORT_ArenaAlloc(arena, eaddrlen);
	if ( entry->emailAddrs[0] == NULL ) {
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    goto loser;
	}
	PORT_Memcpy(entry->emailAddrs[0],
		    &dbentry->data[DB_SUBJECT_ENTRY_HEADER_LEN+nnlen],
		    eaddrlen);
	 entry->nemailAddrs = 1;
    } else {
	entry->emailAddrs = NULL;
    }
    
    /* collect the lengths of the certKeys and keyIDs, and total the
     * overall length.
     */
    tmpbuf = &dbentry->data[keyidoff];
    for ( i = 0; i < ncerts; i++ ) {
        unsigned int itemlen = ( tmpbuf[0] << 8 ) | tmpbuf[1];
        entry->certKeys[i].len = itemlen;
        len += itemlen;
        tmpbuf += 2;
    }
    for ( i = 0; i < ncerts; i++ ) {
        unsigned int itemlen = ( tmpbuf[0] << 8 ) | tmpbuf[1] ;
        entry->keyIDs[i].len = itemlen;
        len += itemlen;
        tmpbuf += 2;
    }

    /* is encoded entry large enough ? */
    if ( len > dbentry->len ){
	PORT_SetError(SEC_ERROR_BAD_DATABASE);
	goto loser;
    }

    for ( i = 0; i < ncerts; i++ ) {
	unsigned int kLen = entry->certKeys[i].len;
	entry->certKeys[i].data = (unsigned char *)PORT_ArenaAlloc(arena, kLen);
	if ( entry->certKeys[i].data == NULL ) {
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    goto loser;
	}
	PORT_Memcpy(entry->certKeys[i].data, tmpbuf, kLen);
	tmpbuf += kLen;
    }
    for ( i = 0; i < ncerts; i++ ) {
	unsigned int iLen = entry->keyIDs[i].len;
	entry->keyIDs[i].data = (unsigned char *)PORT_ArenaAlloc(arena, iLen);
	if ( entry->keyIDs[i].data == NULL ) {
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    goto loser;
	}
	PORT_Memcpy(entry->keyIDs[i].data, tmpbuf, iLen);
	tmpbuf += iLen;
    }

    end = dbentry->data + dbentry->len;
    if ((eaddrlen == 0) && (end - tmpbuf > 1)) {
	/* read in the additional email addresses */
	entry->nemailAddrs = (((unsigned int)tmpbuf[0]) << 8) | tmpbuf[1];
	tmpbuf += 2;
	if (end - tmpbuf < 2 * (int)entry->nemailAddrs)
	    goto loser;
	entry->emailAddrs = PORT_ArenaNewArray(arena, char *, entry->nemailAddrs);
	if (entry->emailAddrs == NULL) {
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    goto loser;
	}
	for (i=0; i < entry->nemailAddrs; i++) {
	    int nameLen;
	    if (end - tmpbuf < 2) {
		goto loser;
	    }
	    nameLen = (((int)tmpbuf[0]) << 8) | tmpbuf[1];
	    tmpbuf += 2;
	    if (end - tmpbuf < nameLen) {
		goto loser;
	    }
	    entry->emailAddrs[i] = PORT_ArenaAlloc(arena,nameLen);
	    if (entry->emailAddrs == NULL) {
	        PORT_SetError(SEC_ERROR_NO_MEMORY);
	        goto loser;
	    }
	    PORT_Memcpy(entry->emailAddrs[i], tmpbuf, nameLen);
	    tmpbuf += nameLen;
	}
	if (tmpbuf != end) 
	    goto loser;
    }
    PORT_ArenaUnmark(arena, mark);
    return(SECSuccess);

loser:
    PORT_ArenaRelease(arena, mark); /* discard above allocations */
    return(SECFailure);
}

/*
 * create a new subject entry with a single cert
 */
static certDBEntrySubject *
NewDBSubjectEntry(SECItem *derSubject, SECItem *certKey,
		  SECItem *keyID, char *nickname, char *emailAddr,
		  unsigned int flags)
{
    PRArenaPool *arena = NULL;
    certDBEntrySubject *entry;
    SECStatus rv;
    unsigned int nnlen;
    unsigned int eaddrlen;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    entry = (certDBEntrySubject *)PORT_ArenaAlloc(arena,
						  sizeof(certDBEntrySubject));
    if ( entry == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    /* init common fields */
    entry->common.arena = arena;
    entry->common.type = certDBEntryTypeSubject;
    entry->common.version = CERT_DB_FILE_VERSION;
    entry->common.flags = flags;

    /* copy the subject */
    rv = SECITEM_CopyItem(arena, &entry->derSubject, derSubject);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    
    entry->ncerts = 1;
    entry->nemailAddrs = 0;
    /* copy nickname */
    if ( nickname && ( *nickname != '\0' ) ) {
	nnlen = PORT_Strlen(nickname) + 1;
	entry->nickname = (char *)PORT_ArenaAlloc(arena, nnlen);
	if ( entry->nickname == NULL ) {
	    goto loser;
	}
						  
	PORT_Memcpy(entry->nickname, nickname, nnlen);
    } else {
	entry->nickname = NULL;
    }
    
    /* copy email addr */
    if ( emailAddr && ( *emailAddr != '\0' ) ) {
	emailAddr = nsslowcert_FixupEmailAddr(emailAddr);
	if ( emailAddr == NULL ) {
	    entry->emailAddrs = NULL;
	    goto loser;
	}
	
	eaddrlen = PORT_Strlen(emailAddr) + 1;
	entry->emailAddrs = (char **)PORT_ArenaAlloc(arena, sizeof(char *));
	if ( entry->emailAddrs == NULL ) {
	    PORT_Free(emailAddr);
	    goto loser;
	}
	entry->emailAddrs[0] = PORT_ArenaStrdup(arena,emailAddr);
	if (entry->emailAddrs[0]) {
	    entry->nemailAddrs = 1;
	} 
	
	PORT_Free(emailAddr);
    } else {
	entry->emailAddrs = NULL;
    }
    
    /* allocate space for certKeys and keyIDs */
    entry->certKeys = (SECItem *)PORT_ArenaAlloc(arena, sizeof(SECItem));
    entry->keyIDs = (SECItem *)PORT_ArenaAlloc(arena, sizeof(SECItem));
    if ( ( entry->certKeys == NULL ) || ( entry->keyIDs == NULL ) ) {
	goto loser;
    }

    /* copy the certKey and keyID */
    rv = SECITEM_CopyItem(arena, &entry->certKeys[0], certKey);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    rv = SECITEM_CopyItem(arena, &entry->keyIDs[0], keyID);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    
    return(entry);
loser:
    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(NULL);
}

/*
 * delete a subject entry
 */
static SECStatus
DeleteDBSubjectEntry(NSSLOWCERTCertDBHandle *handle, SECItem *derSubject)
{
    SECItem dbkey;
    PRArenaPool *arena = NULL;
    SECStatus rv;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	goto loser;
    }
    
    rv = EncodeDBSubjectKey(derSubject, arena, &dbkey);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    
    rv = DeleteDBEntry(handle, certDBEntryTypeSubject, &dbkey);
    if ( rv == SECFailure ) {
	goto loser;
    }

    PORT_FreeArena(arena, PR_FALSE);
    return(SECSuccess);

loser:
    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(SECFailure);
}

/*
 * Read the subject entry
 */
static certDBEntrySubject *
ReadDBSubjectEntry(NSSLOWCERTCertDBHandle *handle, SECItem *derSubject)
{
    PRArenaPool *arena = NULL;
    PRArenaPool *tmparena = NULL;
    certDBEntrySubject *entry;
    SECItem dbkey;
    SECItem dbentry;
    SECStatus rv;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    tmparena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( tmparena == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    
    entry = (certDBEntrySubject *)PORT_ArenaAlloc(arena,
						sizeof(certDBEntrySubject));
    if ( entry == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    entry->common.arena = arena;
    entry->common.type = certDBEntryTypeSubject;

    rv = EncodeDBSubjectKey(derSubject, tmparena, &dbkey);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    
    rv = ReadDBEntry(handle, &entry->common, &dbkey, &dbentry, tmparena);
    if ( rv == SECFailure ) {
	goto loser;
    }

    rv = DecodeDBSubjectEntry(entry, &dbentry, derSubject);
    if ( rv == SECFailure ) {
	goto loser;
    }
    
    PORT_FreeArena(tmparena, PR_FALSE);
    return(entry);
    
loser:
    if ( tmparena ) {
	PORT_FreeArena(tmparena, PR_FALSE);
    }
    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(NULL);
}

/*
 * Encode a subject name entry into byte stream suitable for
 * the database
 */
static SECStatus
WriteDBSubjectEntry(NSSLOWCERTCertDBHandle *handle, certDBEntrySubject *entry)
{
    SECItem dbitem, dbkey;
    PRArenaPool *tmparena = NULL;
    SECStatus rv;
    
    tmparena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( tmparena == NULL ) {
	goto loser;
    }
    
    rv = EncodeDBSubjectEntry(entry, tmparena, &dbitem);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    
    rv = EncodeDBSubjectKey(&entry->derSubject, tmparena, &dbkey);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* now write it to the database */
    rv = WriteDBEntry(handle, &entry->common, &dbkey, &dbitem);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    
    PORT_FreeArena(tmparena, PR_FALSE);
    return(SECSuccess);

loser:
    if ( tmparena ) {
	PORT_FreeArena(tmparena, PR_FALSE);
    }
    return(SECFailure);
    
}

typedef enum { nsslowcert_remove, nsslowcert_add } nsslowcertUpdateType;

static SECStatus
nsslowcert_UpdateSubjectEmailAddr(NSSLOWCERTCertDBHandle *dbhandle, 
	SECItem *derSubject, char *emailAddr, nsslowcertUpdateType updateType)
{
    certDBEntrySubject *entry = NULL;
    int index = -1, i;
    SECStatus rv;
   
    if (emailAddr) { 
	emailAddr = nsslowcert_FixupEmailAddr(emailAddr);
	if (emailAddr == NULL) {
	    return SECFailure;
	}
    } else {
	return SECSuccess;
    }

    entry = ReadDBSubjectEntry(dbhandle,derSubject);    
    if (entry == NULL) {
	rv = SECFailure;
	goto done;
    } 

    for (i=0; i < (int)(entry->nemailAddrs); i++) {
        if (PORT_Strcmp(entry->emailAddrs[i],emailAddr) == 0) {
	    index = i;
	}
    }

    if (updateType == nsslowcert_remove) {
	if (index == -1) {
	    rv = SECSuccess;
	    goto done;
	}
	entry->nemailAddrs--;
	for (i=index; i < (int)(entry->nemailAddrs); i++) {
	   entry->emailAddrs[i] = entry->emailAddrs[i+1];
	}
    } else {
	char **newAddrs = NULL;

	if (index != -1) {
	    rv = SECSuccess;
	    goto done;
	}
	newAddrs = (char **)PORT_ArenaAlloc(entry->common.arena,
		(entry->nemailAddrs+1)* sizeof(char *));
	if (!newAddrs) {
	    rv = SECFailure;
	    goto done;
	}
	for (i=0; i < (int)(entry->nemailAddrs); i++) {
	   newAddrs[i] = entry->emailAddrs[i];
	}
	newAddrs[entry->nemailAddrs] = 
			PORT_ArenaStrdup(entry->common.arena,emailAddr);
	if (!newAddrs[entry->nemailAddrs]) {
	   rv = SECFailure;
	   goto done;
	}
	entry->emailAddrs = newAddrs;
	entry->nemailAddrs++;
    }
	
    /* delete the subject entry */
    DeleteDBSubjectEntry(dbhandle, derSubject);

    /* write the new one */
    rv = WriteDBSubjectEntry(dbhandle, entry);

  done:
    if (entry) DestroyDBEntry((certDBEntry *)entry);
    if (emailAddr) PORT_Free(emailAddr);
    return rv;
}

/*
 * writes a nickname to an existing subject entry that does not currently
 * have one
 */
static SECStatus
AddNicknameToSubject(NSSLOWCERTCertDBHandle *dbhandle,
			NSSLOWCERTCertificate *cert, char *nickname)
{
    certDBEntrySubject *entry;
    SECStatus rv;
    
    if ( nickname == NULL ) {
	return(SECFailure);
    }
    
    entry = ReadDBSubjectEntry(dbhandle,&cert->derSubject);
    PORT_Assert(entry != NULL);
    if ( entry == NULL ) {
	goto loser;
    }
    
    PORT_Assert(entry->nickname == NULL);
    if ( entry->nickname != NULL ) {
	goto loser;
    }
    
    entry->nickname = PORT_ArenaStrdup(entry->common.arena, nickname);
    
    if ( entry->nickname == NULL ) {
	goto loser;
    }
	
    /* delete the subject entry */
    DeleteDBSubjectEntry(dbhandle, &cert->derSubject);

    /* write the new one */
    rv = WriteDBSubjectEntry(dbhandle, entry);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    return(SECSuccess);

loser:
    return(SECFailure);
}

/*
 * create a new version entry
 */
static certDBEntryVersion *
NewDBVersionEntry(unsigned int flags)
{
    PRArenaPool *arena = NULL;
    certDBEntryVersion *entry;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    entry = (certDBEntryVersion *)PORT_ArenaAlloc(arena,
					       sizeof(certDBEntryVersion));
    if ( entry == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    entry->common.arena = arena;
    entry->common.type = certDBEntryTypeVersion;
    entry->common.version = CERT_DB_FILE_VERSION;
    entry->common.flags = flags;

    return(entry);
loser:
    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(NULL);
}

/*
 * Read the version entry
 */
static certDBEntryVersion *
ReadDBVersionEntry(NSSLOWCERTCertDBHandle *handle)
{
    PRArenaPool *arena = NULL;
    PRArenaPool *tmparena = NULL;
    certDBEntryVersion *entry;
    SECItem dbkey;
    SECItem dbentry;
    SECStatus rv;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    tmparena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( tmparena == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    
    entry = PORT_ArenaZNew(arena, certDBEntryVersion);
    if ( entry == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    entry->common.arena = arena;
    entry->common.type = certDBEntryTypeVersion;

    /* now get the database key and format it */
    dbkey.len = SEC_DB_VERSION_KEY_LEN + SEC_DB_KEY_HEADER_LEN;
    dbkey.data = (unsigned char *)PORT_ArenaAlloc(tmparena, dbkey.len);
    if ( dbkey.data == NULL ) {
	goto loser;
    }
    PORT_Memcpy(&dbkey.data[SEC_DB_KEY_HEADER_LEN], SEC_DB_VERSION_KEY,
	      SEC_DB_VERSION_KEY_LEN);

    rv = ReadDBEntry(handle, &entry->common, &dbkey, &dbentry, tmparena);
    if (rv != SECSuccess) {
	goto loser;
    }

    PORT_FreeArena(tmparena, PR_FALSE);
    return(entry);
    
loser:
    if ( tmparena ) {
	PORT_FreeArena(tmparena, PR_FALSE);
    }
    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(NULL);
}


/*
 * Encode a version entry into byte stream suitable for
 * the database
 */
static SECStatus
WriteDBVersionEntry(NSSLOWCERTCertDBHandle *handle, certDBEntryVersion *entry)
{
    SECItem dbitem, dbkey;
    PRArenaPool *tmparena = NULL;
    SECStatus rv;
    
    tmparena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( tmparena == NULL ) {
	goto loser;
    }
    
    /* allocate space for encoded database record, including space
     * for low level header
     */
    dbitem.len = SEC_DB_ENTRY_HEADER_LEN;
    
    dbitem.data = (unsigned char *)PORT_ArenaAlloc(tmparena, dbitem.len);
    if ( dbitem.data == NULL) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    
    /* now get the database key and format it */
    dbkey.len = SEC_DB_VERSION_KEY_LEN + SEC_DB_KEY_HEADER_LEN;
    dbkey.data = (unsigned char *)PORT_ArenaAlloc(tmparena, dbkey.len);
    if ( dbkey.data == NULL ) {
	goto loser;
    }
    PORT_Memcpy(&dbkey.data[SEC_DB_KEY_HEADER_LEN], SEC_DB_VERSION_KEY,
	      SEC_DB_VERSION_KEY_LEN);

    /* now write it to the database */
    rv = WriteDBEntry(handle, &entry->common, &dbkey, &dbitem);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    
    PORT_FreeArena(tmparena, PR_FALSE);
    return(SECSuccess);

loser:
    if ( tmparena ) {
	PORT_FreeArena(tmparena, PR_FALSE);
    }
    return(SECFailure);
}

/*
 * cert is no longer a perm cert, but will remain a temp cert
 */
static SECStatus
RemovePermSubjectNode(NSSLOWCERTCertificate *cert)
{
    certDBEntrySubject *entry;
    unsigned int i;
    SECStatus rv;
    
    entry = ReadDBSubjectEntry(cert->dbhandle,&cert->derSubject);
    if ( entry == NULL ) {
	return(SECFailure);
    }

    PORT_Assert(entry->ncerts);
    rv = SECFailure;
    
    if ( entry->ncerts > 1 ) {
	for ( i = 0; i < entry->ncerts; i++ ) {
	    if ( SECITEM_CompareItem(&entry->certKeys[i], &cert->certKey) ==
		SECEqual ) {
		/* copy rest of list forward one entry */
		for ( i = i + 1; i < entry->ncerts; i++ ) {
		    entry->certKeys[i-1] = entry->certKeys[i];
		    entry->keyIDs[i-1] = entry->keyIDs[i];
		}
		entry->ncerts--;
		DeleteDBSubjectEntry(cert->dbhandle, &cert->derSubject);
		rv = WriteDBSubjectEntry(cert->dbhandle, entry);
		break;
	    }
	}
    } else {
	/* no entries left, delete the perm entry in the DB */
	if ( entry->emailAddrs ) {
	    /* if the subject had an email record, then delete it too */
	    for (i=0; i < entry->nemailAddrs; i++) {
		DeleteDBSMimeEntry(cert->dbhandle, entry->emailAddrs[i]);
	    }
	}
	if ( entry->nickname ) {
	    DeleteDBNicknameEntry(cert->dbhandle, entry->nickname);
	}
	
	DeleteDBSubjectEntry(cert->dbhandle, &cert->derSubject);
    }
    DestroyDBEntry((certDBEntry *)entry);

    return(rv);
}

/*
 * add a cert to the perm subject list
 */
static SECStatus
AddPermSubjectNode(certDBEntrySubject *entry, NSSLOWCERTCertificate *cert, 
								char *nickname)
{
    SECItem *newCertKeys, *newKeyIDs;
    unsigned int i, new_i;
    SECStatus rv;
    unsigned int ncerts;

    PORT_Assert(entry);    
    ncerts = entry->ncerts;
	
    if ( nickname && entry->nickname ) {
	/* nicknames must be the same */
	PORT_Assert(PORT_Strcmp(nickname, entry->nickname) == 0);
    }

    if ( ( entry->nickname == NULL ) && ( nickname != NULL ) ) {
	/* copy nickname into the entry */
	entry->nickname = PORT_ArenaStrdup(entry->common.arena, nickname);
	if ( entry->nickname == NULL ) {
	    return(SECFailure);
	}
    }
	
    /* a DB entry already exists, so add this cert */
    newCertKeys = PORT_ArenaZNewArray(entry->common.arena, SECItem, ncerts + 1);
    newKeyIDs   = PORT_ArenaZNewArray(entry->common.arena, SECItem, ncerts + 1);

    if ( ( newCertKeys == NULL ) || ( newKeyIDs == NULL ) ) {
	    return(SECFailure);
    }

    /* Step 1: copy certs older than "cert" into new entry. */
    for ( i = 0, new_i=0; i < ncerts; i++ ) {
	NSSLOWCERTCertificate *cmpcert;
	PRBool isNewer;
	cmpcert = nsslowcert_FindCertByKey(cert->dbhandle,
						  &entry->certKeys[i]);
	/* The entry has been corrupted, remove it from the list */
	if (!cmpcert) {
	    continue;
	}

	isNewer = nsslowcert_IsNewer(cert, cmpcert);
	nsslowcert_DestroyCertificate(cmpcert);
	if ( isNewer ) 
	    break;
	/* copy this cert entry */
	newCertKeys[new_i] = entry->certKeys[i];
	newKeyIDs[new_i]   = entry->keyIDs[i];
	new_i++; 
    }

    /* Step 2: Add "cert" to the entry. */
    rv = SECITEM_CopyItem(entry->common.arena, &newCertKeys[new_i],
			      &cert->certKey);
    if ( rv != SECSuccess ) {
	return(SECFailure);
    }
    rv = SECITEM_CopyItem(entry->common.arena, &newKeyIDs[new_i],
			      &cert->subjectKeyID);
    if ( rv != SECSuccess ) {
	return(SECFailure);
    }
    new_i++;

    /* Step 3: copy remaining certs (if any) from old entry to new. */
    for ( ; i < ncerts; i++ ,new_i++) {
	newCertKeys[new_i] = entry->certKeys[i];
	newKeyIDs[new_i]   = entry->keyIDs[i];
    }

    /* update certKeys and keyIDs */
    entry->certKeys = newCertKeys;
    entry->keyIDs   = newKeyIDs;

    /* set new count value */
    entry->ncerts = new_i;

    DeleteDBSubjectEntry(cert->dbhandle, &cert->derSubject);
    rv = WriteDBSubjectEntry(cert->dbhandle, entry);
    return(rv);
}


SECStatus
nsslowcert_TraversePermCertsForSubject(NSSLOWCERTCertDBHandle *handle,
				 SECItem *derSubject,
				 NSSLOWCERTCertCallback cb, void *cbarg)
{
    certDBEntrySubject *entry;
    unsigned int i;
    NSSLOWCERTCertificate *cert;
    SECStatus rv = SECSuccess;
    
    entry = ReadDBSubjectEntry(handle, derSubject);

    if ( entry == NULL ) {
	return(SECFailure);
    }
    
    for( i = 0; i < entry->ncerts; i++ ) {
	cert = nsslowcert_FindCertByKey(handle, &entry->certKeys[i]);
	if (!cert) {
	    continue;
	}
	rv = (* cb)(cert, cbarg);
	nsslowcert_DestroyCertificate(cert);
	if ( rv == SECFailure ) {
	    break;
	}
    }

    DestroyDBEntry((certDBEntry *)entry);

    return(rv);
}

int
nsslowcert_NumPermCertsForSubject(NSSLOWCERTCertDBHandle *handle,
							 SECItem *derSubject)
{
    certDBEntrySubject *entry;
    int ret;
    
    entry = ReadDBSubjectEntry(handle, derSubject);

    if ( entry == NULL ) {
	return(SECFailure);
    }

    ret = entry->ncerts;
    
    DestroyDBEntry((certDBEntry *)entry);
    
    return(ret);
}

SECStatus
nsslowcert_TraversePermCertsForNickname(NSSLOWCERTCertDBHandle *handle,
		 	char *nickname, NSSLOWCERTCertCallback cb, void *cbarg)
{
    certDBEntryNickname *nnentry = NULL;
    certDBEntrySMime *smentry = NULL;
    SECStatus rv;
    SECItem *derSubject = NULL;
    
    nnentry = ReadDBNicknameEntry(handle, nickname);
    if ( nnentry ) {
	derSubject = &nnentry->subjectName;
    } else {
	smentry = nsslowcert_ReadDBSMimeEntry(handle, nickname);
	if ( smentry ) {
	    derSubject = &smentry->subjectName;
	}
    }
    
    if ( derSubject ) {
	rv = nsslowcert_TraversePermCertsForSubject(handle, derSubject,
					      cb, cbarg);
    } else {
	rv = SECFailure;
    }

    if ( nnentry ) {
	DestroyDBEntry((certDBEntry *)nnentry);
    }
    if ( smentry ) {
	DestroyDBEntry((certDBEntry *)smentry);
    }
    
    return(rv);
}

int
nsslowcert_NumPermCertsForNickname(NSSLOWCERTCertDBHandle *handle, 
								char *nickname)
{
    certDBEntryNickname *entry;
    int ret;
    
    entry = ReadDBNicknameEntry(handle, nickname);
    
    if ( entry ) {
	ret = nsslowcert_NumPermCertsForSubject(handle, &entry->subjectName);
	DestroyDBEntry((certDBEntry *)entry);
    } else {
	ret = 0;
    }
    return(ret);
}

/*
 * add a nickname to a cert that doesn't have one
 */
static SECStatus
AddNicknameToPermCert(NSSLOWCERTCertDBHandle *dbhandle,
				NSSLOWCERTCertificate *cert, char *nickname)
{
    certDBEntryCert *entry;
    int rv;

    entry = cert->dbEntry;
    PORT_Assert(entry != NULL);
    if ( entry == NULL ) {
	goto loser;
    }

    pkcs11_freeNickname(entry->nickname,entry->nicknameSpace);
    entry->nickname = NULL;
    entry->nickname = pkcs11_copyNickname(nickname,entry->nicknameSpace,
					sizeof(entry->nicknameSpace));

    rv = WriteDBCertEntry(dbhandle, entry);
    if ( rv ) {
	goto loser;
    }

    pkcs11_freeNickname(cert->nickname,cert->nicknameSpace);
    cert->nickname = NULL;
    cert->nickname = pkcs11_copyNickname(nickname,cert->nicknameSpace,
					sizeof(cert->nicknameSpace));

    return(SECSuccess);
    
loser:
    return(SECFailure);
}

/*
 * add a nickname to a cert that is already in the perm database, but doesn't
 * have one yet (it is probably an e-mail cert).
 */
SECStatus
nsslowcert_AddPermNickname(NSSLOWCERTCertDBHandle *dbhandle,
				NSSLOWCERTCertificate *cert, char *nickname)
{
    SECStatus rv = SECFailure;
    certDBEntrySubject *entry = NULL;
    certDBEntryNickname *nicknameEntry = NULL;
    
    nsslowcert_LockDB(dbhandle);

    entry = ReadDBSubjectEntry(dbhandle, &cert->derSubject);
    if (entry == NULL) goto loser;

    if ( entry->nickname == NULL ) {

	/* no nickname for subject */
	rv = AddNicknameToSubject(dbhandle, cert, nickname);
	if ( rv != SECSuccess ) {
	    goto loser;
	}
	rv = AddNicknameToPermCert(dbhandle, cert, nickname);
	if ( rv != SECSuccess ) {
	    goto loser;
	}
	nicknameEntry = NewDBNicknameEntry(nickname, &cert->derSubject, 0);
	if ( nicknameEntry == NULL ) {
	    goto loser;
	}
    
	rv = WriteDBNicknameEntry(dbhandle, nicknameEntry);
	if ( rv != SECSuccess ) {
	    goto loser;
	}
    } else {
	/* subject already has a nickname */
	rv = AddNicknameToPermCert(dbhandle, cert, entry->nickname);
	if ( rv != SECSuccess ) {
	    goto loser;
	}
	/* make sure nickname entry exists. If the database was corrupted,
	 * we may have lost the nickname entry. Add it back now  */
	nicknameEntry = ReadDBNicknameEntry(dbhandle, entry->nickname);
	if (nicknameEntry == NULL ) {
	    nicknameEntry = NewDBNicknameEntry(entry->nickname, 
							&cert->derSubject, 0);
	    if ( nicknameEntry == NULL ) {
		goto loser;
	    }
    
	    rv = WriteDBNicknameEntry(dbhandle, nicknameEntry);
	    if ( rv != SECSuccess ) {
		goto loser;
	    }
	}
    }
    rv = SECSuccess;

loser:
    if (entry) {
	DestroyDBEntry((certDBEntry *)entry);
    }
    if (nicknameEntry) {
	DestroyDBEntry((certDBEntry *)nicknameEntry);
    }
    nsslowcert_UnlockDB(dbhandle);
    return(rv);
}

static certDBEntryCert *
AddCertToPermDB(NSSLOWCERTCertDBHandle *handle, NSSLOWCERTCertificate *cert,
		char *nickname, NSSLOWCERTCertTrust *trust)
{
    certDBEntryCert *certEntry = NULL;
    certDBEntryNickname *nicknameEntry = NULL;
    certDBEntrySubject *subjectEntry = NULL;
    int state = 0;
    SECStatus rv;
    PRBool donnentry = PR_FALSE;

    if ( nickname ) {
	donnentry = PR_TRUE;
    }

    subjectEntry = ReadDBSubjectEntry(handle, &cert->derSubject);
	
    if ( subjectEntry && subjectEntry->nickname ) {
	donnentry = PR_FALSE;
	nickname = subjectEntry->nickname;
    }
    
    certEntry = NewDBCertEntry(&cert->derCert, nickname, trust, 0);
    if ( certEntry == NULL ) {
	goto loser;
    }
    
    if ( donnentry ) {
	nicknameEntry = NewDBNicknameEntry(nickname, &cert->derSubject, 0);
	if ( nicknameEntry == NULL ) {
	    goto loser;
	}
    }
    
    rv = WriteDBCertEntry(handle, certEntry);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    state = 1;
    
    if ( nicknameEntry ) {
	rv = WriteDBNicknameEntry(handle, nicknameEntry);
	if ( rv != SECSuccess ) {
	    goto loser;
	}
    }
    
    state = 2;

    /* "Change" handles if necessary */
    cert->dbhandle = handle;
    
    /* add to or create new subject entry */
    if ( subjectEntry ) {
	/* REWRITE BASED ON SUBJECT ENTRY */
	rv = AddPermSubjectNode(subjectEntry, cert, nickname);
	if ( rv != SECSuccess ) {
	    goto loser;
	}
    } else {
	/* make a new subject entry - this case is only used when updating
	 * an old version of the database.  This is OK because the oldnickname
	 * db format didn't allow multiple certs with the same subject.
	 */
	/* where does subjectKeyID and certKey come from? */
	subjectEntry = NewDBSubjectEntry(&cert->derSubject, &cert->certKey,
					 &cert->subjectKeyID, nickname,
					 NULL, 0);
	if ( subjectEntry == NULL ) {
	    goto loser;
	}
	rv = WriteDBSubjectEntry(handle, subjectEntry);
	if ( rv != SECSuccess ) {
	    goto loser;
	}
    }
    
    state = 3;
    
    if ( nicknameEntry ) {
	DestroyDBEntry((certDBEntry *)nicknameEntry);
    }
    
    if ( subjectEntry ) {
	DestroyDBEntry((certDBEntry *)subjectEntry);
    }

    return(certEntry);

loser:
    /* don't leave partial entry in the database */
    if ( state > 0 ) {
	rv = DeleteDBCertEntry(handle, &cert->certKey);
    }
    if ( ( state > 1 ) && donnentry ) {
	rv = DeleteDBNicknameEntry(handle, nickname);
    }
    if ( state > 2 ) {
	rv = DeleteDBSubjectEntry(handle, &cert->derSubject);
    }
    if ( certEntry ) {
	DestroyDBEntry((certDBEntry *)certEntry);
    }
    if ( nicknameEntry ) {
	DestroyDBEntry((certDBEntry *)nicknameEntry);
    }
    if ( subjectEntry ) {
	DestroyDBEntry((certDBEntry *)subjectEntry);
    }

    return(NULL);
}

/* forward declaration */
static SECStatus
UpdateV7DB(NSSLOWCERTCertDBHandle *handle, DB *updatedb);

/*
 * version 8 uses the same schema as version 7. The only differences are
 * 1) version 8 db uses the blob shim to store data entries > 32k.
 * 2) version 8 db sets the db block size to 32k.
 * both of these are dealt with by the handle.
 */

static SECStatus
UpdateV8DB(NSSLOWCERTCertDBHandle *handle, DB *updatedb)
{
    return UpdateV7DB(handle,updatedb);
}


/*
 * we could just blindly sequence through reading key data pairs and writing
 * them back out, but some cert.db's have gotten quite large and may have some
 * subtle corruption problems, so instead we cycle through the certs and
 * CRL's and S/MIME profiles and rebuild our subject lists from those records.
 */
static SECStatus
UpdateV7DB(NSSLOWCERTCertDBHandle *handle, DB *updatedb)
{
    DBT key, data;
    int ret;
    NSSLOWCERTCertificate *cert;
    PRBool isKRL = PR_FALSE;
    certDBEntryType entryType;
    SECItem dbEntry, dbKey;
    certDBEntryRevocation crlEntry;
    certDBEntryCert certEntry;
    certDBEntrySMime smimeEntry;
    SECStatus rv;

    ret = (* updatedb->seq)(updatedb, &key, &data, R_FIRST);

    if ( ret ) {
	return(SECFailure);
    }
    
    do {
	unsigned char *dataBuf = (unsigned char *)data.data;
	unsigned char *keyBuf = (unsigned char *)key.data;
	dbEntry.data = &dataBuf[SEC_DB_ENTRY_HEADER_LEN];
	dbEntry.len = data.size - SEC_DB_ENTRY_HEADER_LEN;
 	entryType = (certDBEntryType) keyBuf[0];
	dbKey.data = &keyBuf[SEC_DB_KEY_HEADER_LEN];
	dbKey.len = key.size - SEC_DB_KEY_HEADER_LEN;
	if ((dbEntry.len <= 0) || (dbKey.len <= 0)) {
	    continue;
	}

	switch (entryType) {
	/* these entries will get regenerated as we read the 
	 * rest of the data from the database */
	case certDBEntryTypeVersion:
	case certDBEntryTypeSubject:
	case certDBEntryTypeContentVersion:
	case certDBEntryTypeNickname:
	/* smime profiles need entries created after the certs have
         * been imported, loop over them in a second run */
	case certDBEntryTypeSMimeProfile:
	    break;

	case certDBEntryTypeCert:
	    /* decode Entry */
    	    certEntry.common.version = (unsigned int)dataBuf[0];
	    certEntry.common.type = entryType;
	    certEntry.common.flags = (unsigned int)dataBuf[2];
	    rv = DecodeDBCertEntry(&certEntry,&dbEntry);
	    if (rv != SECSuccess) {
		break;
	    }
	    /* should we check for existing duplicates? */
	    cert = nsslowcert_DecodeDERCertificate(&certEntry.derCert, 
						certEntry.nickname);
	    if (cert) {
		nsslowcert_UpdatePermCert(handle, cert, certEntry.nickname,
					 		&certEntry.trust);
		nsslowcert_DestroyCertificate(cert);
	    }
	    /* free any data the decode may have allocated. */
	    pkcs11_freeStaticData(certEntry.derCert.data, 
						certEntry.derCertSpace);
	    pkcs11_freeNickname(certEntry.nickname, certEntry.nicknameSpace);
	    break;

	case certDBEntryTypeKeyRevocation:
	    isKRL = PR_TRUE;
	    /* fall through */
	case certDBEntryTypeRevocation:
    	    crlEntry.common.version = (unsigned int)dataBuf[0];
	    crlEntry.common.type = entryType;
	    crlEntry.common.flags = (unsigned int)dataBuf[2];
	    crlEntry.common.arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
	    if (crlEntry.common.arena == NULL) {
		break;
	    }
	    rv = DecodeDBCrlEntry(&crlEntry,&dbEntry);
	    if (rv != SECSuccess) {
		break;
	    }
	    nsslowcert_UpdateCrl(handle, &crlEntry.derCrl, &dbKey, 
						crlEntry.url, isKRL);
	    /* free data allocated by the decode */
	    PORT_FreeArena(crlEntry.common.arena, PR_FALSE);
	    crlEntry.common.arena = NULL;
	    break;

	default:
	    break;
	}
    } while ( (* updatedb->seq)(updatedb, &key, &data, R_NEXT) == 0 );

    /* now loop again updating just the SMimeProfile. */
    ret = (* updatedb->seq)(updatedb, &key, &data, R_FIRST);

    if ( ret ) {
	return(SECFailure);
    }
    
    do {
	unsigned char *dataBuf = (unsigned char *)data.data;
	unsigned char *keyBuf = (unsigned char *)key.data;
	dbEntry.data = &dataBuf[SEC_DB_ENTRY_HEADER_LEN];
	dbEntry.len = data.size - SEC_DB_ENTRY_HEADER_LEN;
 	entryType = (certDBEntryType) keyBuf[0];
	if (entryType != certDBEntryTypeSMimeProfile) {
	    continue;
	}
	dbKey.data = &keyBuf[SEC_DB_KEY_HEADER_LEN];
	dbKey.len = key.size - SEC_DB_KEY_HEADER_LEN;
	if ((dbEntry.len <= 0) || (dbKey.len <= 0)) {
	    continue;
	}
        smimeEntry.common.version = (unsigned int)dataBuf[0];
	smimeEntry.common.type = entryType;
	smimeEntry.common.flags = (unsigned int)dataBuf[2];
	smimeEntry.common.arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
	/* decode entry */
	rv = DecodeDBSMimeEntry(&smimeEntry,&dbEntry,(char *)dbKey.data);
	if (rv == SECSuccess) {
	    nsslowcert_UpdateSMimeProfile(handle, smimeEntry.emailAddr,
		&smimeEntry.subjectName, &smimeEntry.smimeOptions,
						 &smimeEntry.optionsDate);
	}
	PORT_FreeArena(smimeEntry.common.arena, PR_FALSE);
	smimeEntry.common.arena = NULL;
    } while ( (* updatedb->seq)(updatedb, &key, &data, R_NEXT) == 0 );

    (* updatedb->close)(updatedb);

    /* a database update is a good time to go back and verify the integrity of
     * the keys and certs */
    handle->dbVerify = PR_TRUE; 
    return(SECSuccess);
}

/*
 * NOTE - Version 6 DB did not go out to the real world in a release,
 * so we can remove this function in a later release.
 */
static SECStatus
UpdateV6DB(NSSLOWCERTCertDBHandle *handle, DB *updatedb)
{
    int ret;
    DBT key, data;
    unsigned char *buf, *tmpbuf = NULL;
    certDBEntryType type;
    certDBEntryNickname *nnEntry = NULL;
    certDBEntrySubject *subjectEntry = NULL;
    certDBEntrySMime *emailEntry = NULL;
    char *nickname;
    char *emailAddr;
    SECStatus rv;
    
    /*
     * Sequence through the old database and copy all of the entries
     * to the new database.  Subject name entries will have the new
     * fields inserted into them (with zero length).
     */
    ret = (* updatedb->seq)(updatedb, &key, &data, R_FIRST);
    if ( ret ) {
	return(SECFailure);
    }

    do {
	buf = (unsigned char *)data.data;
	
	if ( data.size >= 3 ) {
	    if ( buf[0] == 6 ) { /* version number */
		type = (certDBEntryType)buf[1];
		if ( type == certDBEntryTypeSubject ) {
		    /* expando subjecto entrieo */
		    tmpbuf = (unsigned char *)PORT_Alloc(data.size + 4);
		    if ( tmpbuf ) {
			/* copy header stuff */
			PORT_Memcpy(tmpbuf, buf, SEC_DB_ENTRY_HEADER_LEN + 2);
			/* insert 4 more bytes of zero'd header */
			PORT_Memset(&tmpbuf[SEC_DB_ENTRY_HEADER_LEN + 2],
				    0, 4);
			/* copy rest of the data */
			PORT_Memcpy(&tmpbuf[SEC_DB_ENTRY_HEADER_LEN + 6],
				    &buf[SEC_DB_ENTRY_HEADER_LEN + 2],
				    data.size - (SEC_DB_ENTRY_HEADER_LEN + 2));

			data.data = (void *)tmpbuf;
			data.size += 4;
			buf = tmpbuf;
		    }
		} else if ( type == certDBEntryTypeCert ) {
		    /* expando certo entrieo */
		    tmpbuf = (unsigned char *)PORT_Alloc(data.size + 3);
		    if ( tmpbuf ) {
			/* copy header stuff */
			PORT_Memcpy(tmpbuf, buf, SEC_DB_ENTRY_HEADER_LEN);

			/* copy trust flage, setting msb's to 0 */
			tmpbuf[SEC_DB_ENTRY_HEADER_LEN] = 0;
			tmpbuf[SEC_DB_ENTRY_HEADER_LEN+1] =
			    buf[SEC_DB_ENTRY_HEADER_LEN];
			tmpbuf[SEC_DB_ENTRY_HEADER_LEN+2] = 0;
			tmpbuf[SEC_DB_ENTRY_HEADER_LEN+3] =
			    buf[SEC_DB_ENTRY_HEADER_LEN+1];
			tmpbuf[SEC_DB_ENTRY_HEADER_LEN+4] = 0;
			tmpbuf[SEC_DB_ENTRY_HEADER_LEN+5] =
			    buf[SEC_DB_ENTRY_HEADER_LEN+2];
			
			/* copy rest of the data */
			PORT_Memcpy(&tmpbuf[SEC_DB_ENTRY_HEADER_LEN + 6],
				    &buf[SEC_DB_ENTRY_HEADER_LEN + 3],
				    data.size - (SEC_DB_ENTRY_HEADER_LEN + 3));

			data.data = (void *)tmpbuf;
			data.size += 3;
			buf = tmpbuf;
		    }

		}

		/* update the record version number */
		buf[0] = CERT_DB_FILE_VERSION;

		/* copy to the new database */
		ret = certdb_Put(handle->permCertDB, &key, &data, 0);
		if ( tmpbuf ) {
		    PORT_Free(tmpbuf);
		    tmpbuf = NULL;
		}
	    }
	}
    } while ( (* updatedb->seq)(updatedb, &key, &data, R_NEXT) == 0 );

    ret = certdb_Sync(handle->permCertDB, 0);

    ret = (* updatedb->seq)(updatedb, &key, &data, R_FIRST);
    if ( ret ) {
	return(SECFailure);
    }

    do {
	buf = (unsigned char *)data.data;
	
	if ( data.size >= 3 ) {
	    if ( buf[0] == CERT_DB_FILE_VERSION ) { /* version number */
		type = (certDBEntryType)buf[1];
		if ( type == certDBEntryTypeNickname ) {
		    nickname = &((char *)key.data)[1];

		    /* get the matching nickname entry in the new DB */
		    nnEntry = ReadDBNicknameEntry(handle, nickname);
		    if ( nnEntry == NULL ) {
			goto endloop;
		    }
		    
		    /* find the subject entry pointed to by nickname */
		    subjectEntry = ReadDBSubjectEntry(handle,
						      &nnEntry->subjectName);
		    if ( subjectEntry == NULL ) {
			goto endloop;
		    }
		    
		    subjectEntry->nickname =
			(char *)PORT_ArenaAlloc(subjectEntry->common.arena,
						key.size - 1);
		    if ( subjectEntry->nickname ) {
			PORT_Memcpy(subjectEntry->nickname, nickname,
				    key.size - 1);
			rv = WriteDBSubjectEntry(handle, subjectEntry);
		    }
		} else if ( type == certDBEntryTypeSMimeProfile ) {
		    emailAddr = &((char *)key.data)[1];

		    /* get the matching smime entry in the new DB */
		    emailEntry = nsslowcert_ReadDBSMimeEntry(handle, emailAddr);
		    if ( emailEntry == NULL ) {
			goto endloop;
		    }
		    
		    /* find the subject entry pointed to by nickname */
		    subjectEntry = ReadDBSubjectEntry(handle,
						      &emailEntry->subjectName);
		    if ( subjectEntry == NULL ) {
			goto endloop;
		    }
		    
		    subjectEntry->emailAddrs = (char **)
				PORT_ArenaAlloc(subjectEntry->common.arena,
						sizeof(char *));
		    if ( subjectEntry->emailAddrs ) {
			subjectEntry->emailAddrs[0] =
			     (char *)PORT_ArenaAlloc(subjectEntry->common.arena,
						key.size - 1);
			if ( subjectEntry->emailAddrs[0] ) {
			    PORT_Memcpy(subjectEntry->emailAddrs[0], emailAddr,
				    key.size - 1);
			    subjectEntry->nemailAddrs = 1;
			    rv = WriteDBSubjectEntry(handle, subjectEntry);
			}
		    }
		}
		
endloop:
		if ( subjectEntry ) {
		    DestroyDBEntry((certDBEntry *)subjectEntry);
		    subjectEntry = NULL;
		}
		if ( nnEntry ) {
		    DestroyDBEntry((certDBEntry *)nnEntry);
		    nnEntry = NULL;
		}
		if ( emailEntry ) {
		    DestroyDBEntry((certDBEntry *)emailEntry);
		    emailEntry = NULL;
		}
	    }
	}
    } while ( (* updatedb->seq)(updatedb, &key, &data, R_NEXT) == 0 );

    ret = certdb_Sync(handle->permCertDB, 0);

    (* updatedb->close)(updatedb);
    return(SECSuccess);
}


static SECStatus
updateV5Callback(NSSLOWCERTCertificate *cert, SECItem *k, void *pdata)
{
    NSSLOWCERTCertDBHandle *handle;
    certDBEntryCert *entry;
    NSSLOWCERTCertTrust *trust;
    
    handle = (NSSLOWCERTCertDBHandle *)pdata;
    trust = &cert->dbEntry->trust;

    /* SSL user certs can be used for email if they have an email addr */
    if ( cert->emailAddr && ( trust->sslFlags & CERTDB_USER ) &&
	( trust->emailFlags == 0 ) ) {
	trust->emailFlags = CERTDB_USER;
    }
    /* servers didn't set the user flags on the server cert.. */
    if (PORT_Strcmp(cert->dbEntry->nickname,"Server-Cert") == 0) {
	trust->sslFlags |= CERTDB_USER;
    }
    
    entry = AddCertToPermDB(handle, cert, cert->dbEntry->nickname,
			    &cert->dbEntry->trust);
    if ( entry ) {
	DestroyDBEntry((certDBEntry *)entry);
    }
    
    return(SECSuccess);
}

static SECStatus
UpdateV5DB(NSSLOWCERTCertDBHandle *handle, DB *updatedb)
{
    NSSLOWCERTCertDBHandle updatehandle;
    SECStatus rv;
    
    updatehandle.permCertDB = updatedb;
    updatehandle.dbMon = PZ_NewMonitor(nssILockCertDB);
    updatehandle.dbVerify = 0;
    updatehandle.ref      = 1; /* prevent premature close */
    
    rv = nsslowcert_TraversePermCerts(&updatehandle, updateV5Callback,
			       (void *)handle);
    
    PZ_DestroyMonitor(updatehandle.dbMon);

    (* updatedb->close)(updatedb);
    return(SECSuccess);
}

static PRBool
isV4DB(DB *db) {
    DBT key,data;
    int ret;

    key.data = "Version";
    key.size = 7;

    ret = (*db->get)(db, &key, &data, 0);
    if (ret) {
	return PR_FALSE;
    }

    if ((data.size == 1) && (*(unsigned char *)data.data <= 4))  {
	return PR_TRUE;
    }

    return PR_FALSE;
}

static SECStatus
UpdateV4DB(NSSLOWCERTCertDBHandle *handle, DB *updatedb)
{
    DBT key, data;
    certDBEntryCert *entry, *entry2;
    int ret;
    PRArenaPool *arena = NULL;
    NSSLOWCERTCertificate *cert;

    ret = (* updatedb->seq)(updatedb, &key, &data, R_FIRST);

    if ( ret ) {
	return(SECFailure);
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	return(SECFailure);
    }
    
    do {
	if ( data.size != 1 ) { /* skip version number */

	    /* decode the old DB entry */
	    entry = (certDBEntryCert *)
		DecodeV4DBCertEntry((unsigned char*)data.data, data.size);
	    
	    if ( entry ) {
		cert = nsslowcert_DecodeDERCertificate(&entry->derCert, 
						 entry->nickname);

		if ( cert != NULL ) {
		    /* add to new database */
		    entry2 = AddCertToPermDB(handle, cert, entry->nickname,
					     &entry->trust);
		    
		    nsslowcert_DestroyCertificate(cert);
		    if ( entry2 ) {
			DestroyDBEntry((certDBEntry *)entry2);
		    }
		}
		DestroyDBEntry((certDBEntry *)entry);
	    }
	}
    } while ( (* updatedb->seq)(updatedb, &key, &data, R_NEXT) == 0 );

    PORT_FreeArena(arena, PR_FALSE);
    (* updatedb->close)(updatedb);
    return(SECSuccess);
}


/*
 * return true if a database key conflict exists
 */
PRBool
nsslowcert_CertDBKeyConflict(SECItem *derCert, NSSLOWCERTCertDBHandle *handle)
{
    SECStatus rv;
    DBT tmpdata;
    DBT namekey;
    int ret;
    SECItem keyitem;
    PRArenaPool *arena = NULL;
    SECItem derKey;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	goto loser;
    }

    /* get the db key of the cert */
    rv = nsslowcert_KeyFromDERCert(arena, derCert, &derKey);
    if ( rv != SECSuccess ) {
        goto loser;
    }

    rv = EncodeDBCertKey(&derKey, arena, &keyitem);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    
    namekey.data = keyitem.data;
    namekey.size = keyitem.len;
    
    ret = certdb_Get(handle->permCertDB, &namekey, &tmpdata, 0);
    if ( ret == 0 ) {
	goto loser;
    }

    PORT_FreeArena(arena, PR_FALSE);
    
    return(PR_FALSE);
loser:
    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(PR_TRUE);
}

/*
 * return true if a nickname conflict exists
 * NOTE: caller must have already made sure that this exact cert
 * doesn't exist in the DB
 */
static PRBool
nsslowcert_CertNicknameConflict(char *nickname, SECItem *derSubject,
			 NSSLOWCERTCertDBHandle *handle)
{
    PRBool rv;
    certDBEntryNickname *entry;
    
    if ( nickname == NULL ) {
	return(PR_FALSE);
    }
    
    entry = ReadDBNicknameEntry(handle, nickname);

    if ( entry == NULL ) {
	/* no entry for this nickname, so no conflict */
	return(PR_FALSE);
    }

    rv = PR_TRUE;
    if ( SECITEM_CompareItem(derSubject, &entry->subjectName) == SECEqual ) {
	/* if subject names are the same, then no conflict */
	rv = PR_FALSE;
    }

    DestroyDBEntry((certDBEntry *)entry);
    return(rv);
}

#ifdef DBM_USING_NSPR
#define NO_RDONLY	PR_RDONLY
#define NO_RDWR		PR_RDWR
#define NO_CREATE	(PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE)
#else
#define NO_RDONLY	O_RDONLY
#define NO_RDWR		O_RDWR
#define NO_CREATE	(O_RDWR | O_CREAT | O_TRUNC)
#endif

/*
 * open an old database that needs to be updated
 */
static DB *
nsslowcert_openolddb(NSSLOWCERTDBNameFunc namecb, void *cbarg, int version)
{
    char * tmpname;
    DB *updatedb = NULL;

    tmpname = (* namecb)(cbarg, version);	/* get v6 db name */
    if ( tmpname ) {
	updatedb = dbopen( tmpname, NO_RDONLY, 0600, DB_HASH, 0 );
	PORT_Free(tmpname);
    }
    return updatedb;
}

static SECStatus
openNewCertDB(const char *appName, const char *prefix, const char *certdbname, 
    NSSLOWCERTCertDBHandle *handle, NSSLOWCERTDBNameFunc namecb, void *cbarg)
{
    SECStatus rv;
    certDBEntryVersion *versionEntry = NULL;
    DB *updatedb = NULL;
    int status = RDB_FAIL;

    if (appName) {
	handle->permCertDB=rdbopen( appName, prefix, "cert", NO_CREATE, &status);
    } else {
	handle->permCertDB=dbsopen(certdbname, NO_CREATE, 0600, DB_HASH, 0);
    }

    /* if create fails then we lose */
    if ( handle->permCertDB == 0 ) {
	return status == RDB_RETRY ? SECWouldBlock : SECFailure;
    }

    /* Verify version number; */
    versionEntry = NewDBVersionEntry(0);
    if ( versionEntry == NULL ) {
	rv = SECFailure;
	goto loser;
    }
	
    rv = WriteDBVersionEntry(handle, versionEntry);

    DestroyDBEntry((certDBEntry *)versionEntry);

    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* rv must already be Success here because of previous if statement */
    /* try to upgrade old db here */
    if (appName &&
       (updatedb = dbsopen(certdbname, NO_RDONLY, 0600, DB_HASH, 0)) != NULL) {
	rv = UpdateV8DB(handle, updatedb);
    } else if ((updatedb = nsslowcert_openolddb(namecb,cbarg,7)) != NULL) {
	rv = UpdateV7DB(handle, updatedb);
    } else if ((updatedb = nsslowcert_openolddb(namecb,cbarg,6)) != NULL) {
	rv = UpdateV6DB(handle, updatedb);
    } else if ((updatedb = nsslowcert_openolddb(namecb,cbarg,5)) != NULL) {
	rv = UpdateV5DB(handle, updatedb);
    } else if ((updatedb = nsslowcert_openolddb(namecb,cbarg,4)) != NULL) {
	/* NES has v5 format db's with v4 db names! */
	if (isV4DB(updatedb)) {
	    rv = UpdateV4DB(handle,updatedb);
	} else {
	    rv = UpdateV5DB(handle,updatedb);
	}
    }


loser:
    db_InitComplete(handle->permCertDB);
    return rv;
}

static int
nsslowcert_GetVersionNumber( NSSLOWCERTCertDBHandle *handle)
{
    certDBEntryVersion *versionEntry = NULL;
    int version = 0;

    versionEntry = ReadDBVersionEntry(handle); 
    if ( versionEntry == NULL ) {
	return 0;
    }
    version = versionEntry->common.version;
    DestroyDBEntry((certDBEntry *)versionEntry);
    return version;
}

/*
 * Open the certificate database and index databases.  Create them if
 * they are not there or bad.
 */
static SECStatus
nsslowcert_OpenPermCertDB(NSSLOWCERTCertDBHandle *handle, PRBool readOnly,
		   		const char *appName, const char *prefix,
				NSSLOWCERTDBNameFunc namecb, void *cbarg)
{
    SECStatus rv;
    int openflags;
    char *certdbname;
    int version = 0;
    
    certdbname = (* namecb)(cbarg, CERT_DB_FILE_VERSION);
    if ( certdbname == NULL ) {
	return(SECFailure);
    }

    openflags = readOnly ? NO_RDONLY : NO_RDWR;

    /*
     * first open the permanent file based database.
     */
    if (appName) {
	handle->permCertDB = rdbopen( appName, prefix, "cert", openflags, NULL);
    } else {
	handle->permCertDB = dbsopen( certdbname, openflags, 0600, DB_HASH, 0 );
    }

    /* check for correct version number */
    if ( handle->permCertDB ) {
	version = nsslowcert_GetVersionNumber(handle);
	if ((version != CERT_DB_FILE_VERSION) &&
		!(appName && version == CERT_DB_V7_FILE_VERSION)) {
	    goto loser;
	}
    } else if ( readOnly ) {
	/* don't create if readonly */
	    /* Try openning a version 7 database */
	    handle->permCertDB = nsslowcert_openolddb(namecb,cbarg, 7);
	    if (!handle->permCertDB) {
		goto loser;
	    }
	    if (nsslowcert_GetVersionNumber(handle) != 7) {
		goto loser;
	    }
    } else {
        /* if first open fails, try to create a new DB */
	rv = openNewCertDB(appName,prefix,certdbname,handle,namecb,cbarg);
	if (rv == SECWouldBlock) {
	    /* only the rdb version can fail with wouldblock */
	    handle->permCertDB = 
			rdbopen( appName, prefix, "cert", openflags, NULL);

	    /* check for correct version number */
	    if ( !handle->permCertDB ) {
		goto loser;
	    }
	    version = nsslowcert_GetVersionNumber(handle);
	    if ((version != CERT_DB_FILE_VERSION) &&
		!(appName && version == CERT_DB_V7_FILE_VERSION)) {
		goto loser;
	    }
	} else if (rv != SECSuccess) {
	    goto loser;
	}
    }

    PORT_Free(certdbname);
    
    return (SECSuccess);
    
loser:

    PORT_SetError(SEC_ERROR_BAD_DATABASE);
    
    if ( handle->permCertDB ) {
	certdb_Close(handle->permCertDB);
	handle->permCertDB = 0;
    }

    PORT_Free(certdbname);

    return(SECFailure);
}

/*
 * delete all DB records associated with a particular certificate
 */
static SECStatus
DeletePermCert(NSSLOWCERTCertificate *cert)
{
    SECStatus rv;
    SECStatus ret;

    ret = SECSuccess;
    
    rv = DeleteDBCertEntry(cert->dbhandle, &cert->certKey);
    if ( rv != SECSuccess ) {
	ret = SECFailure;
    }
    
    rv = RemovePermSubjectNode(cert);


    return(ret);
}

/*
 * Delete a certificate from the permanent database.
 */
SECStatus
nsslowcert_DeletePermCertificate(NSSLOWCERTCertificate *cert)
{
    SECStatus rv;
    
    nsslowcert_LockDB(cert->dbhandle);

    /* delete the records from the permanent database */
    rv = DeletePermCert(cert);

    /* get rid of dbcert and stuff pointing to it */
    DestroyDBEntry((certDBEntry *)cert->dbEntry);
    cert->dbEntry = NULL;
    cert->trust = NULL;
	
    nsslowcert_UnlockDB(cert->dbhandle);
    return(rv);
}

/*
 * Traverse all of the entries in the database of a particular type
 * call the given function for each one.
 */
SECStatus
nsslowcert_TraverseDBEntries(NSSLOWCERTCertDBHandle *handle,
		      certDBEntryType type,
		      SECStatus (* callback)(SECItem *data, SECItem *key,
					    certDBEntryType type, void *pdata),
		      void *udata )
{
    DBT data;
    DBT key;
    SECStatus rv = SECSuccess;
    int ret;
    SECItem dataitem;
    SECItem keyitem;
    unsigned char *buf;
    unsigned char *keybuf;
    
    ret = certdb_Seq(handle->permCertDB, &key, &data, R_FIRST);
    if ( ret ) {
	return(SECFailure);
    }
    /* here, ret is zero and rv is SECSuccess.  
     * Below here, ret is a count of successful calls to the callback function.
     */
    do {
	buf = (unsigned char *)data.data;
	
	if ( buf[1] == (unsigned char)type ) {
	    dataitem.len = data.size;
	    dataitem.data = buf;
            dataitem.type = siBuffer;
	    keyitem.len = key.size - SEC_DB_KEY_HEADER_LEN;
	    keybuf = (unsigned char *)key.data;
	    keyitem.data = &keybuf[SEC_DB_KEY_HEADER_LEN];
            keyitem.type = siBuffer;
	    /* type should equal keybuf[0].  */

	    rv = (* callback)(&dataitem, &keyitem, type, udata);
	    if ( rv == SECSuccess ) {
		++ret;
	    }
	}
    } while ( certdb_Seq(handle->permCertDB, &key, &data, R_NEXT) == 0 );
    /* If any callbacks succeeded, or no calls to callbacks were made, 
     * then report success.  Otherwise, report failure.
     */
    return (ret ? SECSuccess : rv);
}
/*
 * Decode a certificate and enter it into the temporary certificate database.
 * Deal with nicknames correctly
 *
 * This is the private entry point.
 */
static NSSLOWCERTCertificate *
DecodeACert(NSSLOWCERTCertDBHandle *handle, certDBEntryCert *entry)
{
    NSSLOWCERTCertificate *cert = NULL;
    
    cert = nsslowcert_DecodeDERCertificate(&entry->derCert, entry->nickname );
    
    if ( cert == NULL ) {
	goto loser;
    }

    cert->dbhandle = handle;
    cert->dbEntry = entry;
    cert->trust = &entry->trust;

    return(cert);

loser:
    return(0);
}

static NSSLOWCERTTrust *
CreateTrust(void)
{
    NSSLOWCERTTrust *trust = NULL;

    nsslowcert_LockFreeList();
    trust = trustListHead;
    if (trust) {
	trustListCount--;
	trustListHead = trust->next;
    }
    PORT_Assert(trustListCount >= 0);
    nsslowcert_UnlockFreeList();
    if (trust) {
	return trust;
    }

    return PORT_ZNew(NSSLOWCERTTrust);
}

static void
DestroyTrustFreeList(void)
{
    NSSLOWCERTTrust *trust;

    nsslowcert_LockFreeList();
    while (NULL != (trust = trustListHead)) {
	trustListCount--;
	trustListHead = trust->next;
	PORT_Free(trust);
    }
    PORT_Assert(!trustListCount);
    trustListCount = 0;
    nsslowcert_UnlockFreeList();
}

static NSSLOWCERTTrust * 
DecodeTrustEntry(NSSLOWCERTCertDBHandle *handle, certDBEntryCert *entry, 
                 const SECItem *dbKey)
{
    NSSLOWCERTTrust *trust = CreateTrust();
    if (trust == NULL) {
	return trust;
    }
    trust->dbhandle = handle;
    trust->dbEntry = entry;
    trust->dbKey.data = pkcs11_copyStaticData(dbKey->data,dbKey->len,
				trust->dbKeySpace, sizeof(trust->dbKeySpace));
    if (!trust->dbKey.data) {
	PORT_Free(trust);
	return NULL;
    }
    trust->dbKey.len = dbKey->len;
 
    trust->trust = &entry->trust;
    trust->derCert = &entry->derCert;

    return(trust);
}

typedef struct {
    PermCertCallback certfunc;
    NSSLOWCERTCertDBHandle *handle;
    void *data;
} PermCertCallbackState;

/*
 * traversal callback to decode certs and call callers callback
 */
static SECStatus
certcallback(SECItem *dbdata, SECItem *dbkey, certDBEntryType type, void *data)
{
    PermCertCallbackState *mystate;
    SECStatus rv;
    certDBEntryCert *entry;
    SECItem entryitem;
    NSSLOWCERTCertificate *cert;
    PRArenaPool *arena = NULL;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	goto loser;
    }
    
    entry = (certDBEntryCert *)PORT_ArenaAlloc(arena, sizeof(certDBEntryCert));
    mystate = (PermCertCallbackState *)data;
    entry->common.version = (unsigned int)dbdata->data[0];
    entry->common.type = (certDBEntryType)dbdata->data[1];
    entry->common.flags = (unsigned int)dbdata->data[2];
    entry->common.arena = arena;
    
    entryitem.len = dbdata->len - SEC_DB_ENTRY_HEADER_LEN;
    entryitem.data = &dbdata->data[SEC_DB_ENTRY_HEADER_LEN];
    
    rv = DecodeDBCertEntry(entry, &entryitem);
    if (rv != SECSuccess ) {
	goto loser;
    }
    entry->derCert.type = siBuffer;
   
    /* note: Entry is 'inheritted'.  */
    cert = DecodeACert(mystate->handle, entry);

    rv = (* mystate->certfunc)(cert, dbkey, mystate->data);

    /* arena stored in entry destroyed by nsslowcert_DestroyCertificate */
    nsslowcert_DestroyCertificateNoLocking(cert);

    return(rv);

loser:
    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    return(SECFailure);
}

/*
 * Traverse all of the certificates in the permanent database and
 * call the given function for each one; expect the caller to have lock.
 */
static SECStatus
TraversePermCertsNoLocking(NSSLOWCERTCertDBHandle *handle,
			   SECStatus (* certfunc)(NSSLOWCERTCertificate *cert,
						  SECItem *k,
						  void *pdata),
			   void *udata )
{
    SECStatus rv;
    PermCertCallbackState mystate;

    mystate.certfunc = certfunc;
    mystate.handle = handle;
    mystate.data = udata;
    rv = nsslowcert_TraverseDBEntries(handle, certDBEntryTypeCert, certcallback,
			       (void *)&mystate);
    
    return(rv);
}

/*
 * Traverse all of the certificates in the permanent database and
 * call the given function for each one.
 */
SECStatus
nsslowcert_TraversePermCerts(NSSLOWCERTCertDBHandle *handle,
		      SECStatus (* certfunc)(NSSLOWCERTCertificate *cert, SECItem *k,
					    void *pdata),
		      void *udata )
{
    SECStatus rv;

    nsslowcert_LockDB(handle);
    rv = TraversePermCertsNoLocking(handle, certfunc, udata);
    nsslowcert_UnlockDB(handle);
    
    return(rv);
}



/*
 * Close the database
 */
void
nsslowcert_ClosePermCertDB(NSSLOWCERTCertDBHandle *handle)
{
    if ( handle ) {
	if ( handle->permCertDB ) {
	    certdb_Close( handle->permCertDB );
	    handle->permCertDB = NULL;
	}
	if (handle->dbMon) {
    	    PZ_DestroyMonitor(handle->dbMon);
	    handle->dbMon = NULL;
	}
	PORT_Free(handle);
    }
    return;
}

/*
 * Get the trust attributes from a certificate
 */
SECStatus
nsslowcert_GetCertTrust(NSSLOWCERTCertificate *cert, NSSLOWCERTCertTrust *trust)
{
    SECStatus rv;
    
    nsslowcert_LockCertTrust(cert);
    
    if ( cert->trust == NULL ) {
	rv = SECFailure;
    } else {
	*trust = *cert->trust;
	rv = SECSuccess;
    }
    
    nsslowcert_UnlockCertTrust(cert);
    return(rv);
}

/*
 * Change the trust attributes of a certificate and make them permanent
 * in the database.
 */
SECStatus
nsslowcert_ChangeCertTrust(NSSLOWCERTCertDBHandle *handle, 
	 	 	NSSLOWCERTCertificate *cert, NSSLOWCERTCertTrust *trust)
{
    certDBEntryCert *entry;
    int rv;
    SECStatus ret;
    
    nsslowcert_LockDB(handle);
    nsslowcert_LockCertTrust(cert);
    /* only set the trust on permanent certs */
    if ( cert->trust == NULL ) {
	ret = SECFailure;
	goto done;
    }

    *cert->trust = *trust;
    if ( cert->dbEntry == NULL ) {
	ret = SECSuccess; /* not in permanent database */
	goto done;
    }
    
    entry = cert->dbEntry;
    entry->trust = *trust;
    
    rv = WriteDBCertEntry(handle, entry);
    if ( rv ) {
	ret = SECFailure;
	goto done;
    }

    ret = SECSuccess;
    
done:
    nsslowcert_UnlockCertTrust(cert);
    nsslowcert_UnlockDB(handle);
    return(ret);
}


static SECStatus
nsslowcert_UpdatePermCert(NSSLOWCERTCertDBHandle *dbhandle,
    NSSLOWCERTCertificate *cert, char *nickname, NSSLOWCERTCertTrust *trust)
{
    char *oldnn;
    certDBEntryCert *entry;
    PRBool conflict;
    SECStatus ret;

    PORT_Assert(!cert->dbEntry);

    /* don't add a conflicting nickname */
    conflict = nsslowcert_CertNicknameConflict(nickname, &cert->derSubject,
					dbhandle);
    if ( conflict ) {
	ret = SECFailure;
	goto done;
    }
    
    /* save old nickname so that we can delete it */
    oldnn = cert->nickname;

    entry = AddCertToPermDB(dbhandle, cert, nickname, trust);
    
    if ( entry == NULL ) {
	ret = SECFailure;
	goto done;
    }

    pkcs11_freeNickname(oldnn,cert->nicknameSpace);
    
    cert->nickname = (entry->nickname) ? pkcs11_copyNickname(entry->nickname,
		cert->nicknameSpace, sizeof(cert->nicknameSpace)) : NULL;
    cert->trust = &entry->trust;
    cert->dbEntry = entry;
    
    ret = SECSuccess;
done:
    return(ret);
}

SECStatus
nsslowcert_AddPermCert(NSSLOWCERTCertDBHandle *dbhandle,
    NSSLOWCERTCertificate *cert, char *nickname, NSSLOWCERTCertTrust *trust)
{
    SECStatus ret;

    nsslowcert_LockDB(dbhandle);

    ret = nsslowcert_UpdatePermCert(dbhandle, cert, nickname, trust);
    
    nsslowcert_UnlockDB(dbhandle);
    return(ret);
}

/*
 * Open the certificate database and index databases.  Create them if
 * they are not there or bad.
 */
SECStatus
nsslowcert_OpenCertDB(NSSLOWCERTCertDBHandle *handle, PRBool readOnly,
	        const char *appName, const char *prefix,
		NSSLOWCERTDBNameFunc namecb, void *cbarg, PRBool openVolatile)
{
    int rv;

    certdb_InitDBLock(handle);
    
    handle->dbMon = PZ_NewMonitor(nssILockCertDB);
    PORT_Assert(handle->dbMon != NULL);
    handle->dbVerify = PR_FALSE;

    rv = nsslowcert_OpenPermCertDB(handle, readOnly, appName, prefix, 
							namecb, cbarg);
    if ( rv ) {
	goto loser;
    }

    return (SECSuccess);
    
loser:

    PORT_SetError(SEC_ERROR_BAD_DATABASE);
    return(SECFailure);
}

PRBool
nsslowcert_needDBVerify(NSSLOWCERTCertDBHandle *handle)
{
    if (!handle) return PR_FALSE;
    return handle->dbVerify;
}

void
nsslowcert_setDBVerify(NSSLOWCERTCertDBHandle *handle, PRBool value)
{
    handle->dbVerify = value;
}


/*
 * Lookup a certificate in the databases.
 */
static NSSLOWCERTCertificate *
FindCertByKey(NSSLOWCERTCertDBHandle *handle, const SECItem *certKey, PRBool lockdb)
{
    NSSLOWCERTCertificate *cert = NULL;
    certDBEntryCert *entry;
    PRBool locked = PR_FALSE;
    
    if ( lockdb ) {
	locked = PR_TRUE;
	nsslowcert_LockDB(handle);
    }
	
    /* find in perm database */
    entry = ReadDBCertEntry(handle, certKey);
	
    if ( entry == NULL ) {
 	goto loser;
    }
  
    /* inherit entry */  
    cert = DecodeACert(handle, entry);

loser:
    if (cert == NULL) {
	if (entry) {
	    DestroyDBEntry((certDBEntry *)entry);
	}
    }

    if ( locked ) {
	nsslowcert_UnlockDB(handle);
    }
    
    return(cert);
}

/*
 * Lookup a certificate in the databases.
 */
static NSSLOWCERTTrust *
FindTrustByKey(NSSLOWCERTCertDBHandle *handle, const SECItem *certKey, PRBool lockdb)
{
    NSSLOWCERTTrust *trust = NULL;
    certDBEntryCert *entry;
    PRBool locked = PR_FALSE;
    
    if ( lockdb ) {
	locked = PR_TRUE;
	nsslowcert_LockDB(handle);
    }
	
    /* find in perm database */
    entry = ReadDBCertEntry(handle, certKey);
	
    if ( entry == NULL ) {
 	goto loser;
    }

    if (!nsslowcert_hasTrust(&entry->trust)) {
	goto loser;
    }
  
    /* inherit entry */  
    trust = DecodeTrustEntry(handle, entry, certKey);

loser:
    if (trust == NULL) {
	if (entry) {
	    DestroyDBEntry((certDBEntry *)entry);
	}
    }

    if ( locked ) {
	nsslowcert_UnlockDB(handle);
    }
    
    return(trust);
}

/*
 * Lookup a certificate in the databases without locking
 */
NSSLOWCERTCertificate *
nsslowcert_FindCertByKey(NSSLOWCERTCertDBHandle *handle, const SECItem *certKey)
{
    return(FindCertByKey(handle, certKey, PR_FALSE));
}

/*
 * Lookup a trust object in the databases without locking
 */
NSSLOWCERTTrust *
nsslowcert_FindTrustByKey(NSSLOWCERTCertDBHandle *handle, const SECItem *certKey)
{
    return(FindTrustByKey(handle, certKey, PR_FALSE));
}

/*
 * Generate a key from an issuerAndSerialNumber, and find the
 * associated cert in the database.
 */
NSSLOWCERTCertificate *
nsslowcert_FindCertByIssuerAndSN(NSSLOWCERTCertDBHandle *handle, NSSLOWCERTIssuerAndSN *issuerAndSN)
{
    SECItem certKey;
    SECItem *sn = &issuerAndSN->serialNumber;
    SECItem *issuer = &issuerAndSN->derIssuer;
    NSSLOWCERTCertificate *cert;
    int data_left = sn->len-1;
    int data_len = sn->len;
    int index = 0;

    /* automatically detect DER encoded serial numbers and remove the der
     * encoding since the database expects unencoded data. 
     * if it's DER encoded, there must be at least 3 bytes, tag, len, data */
    if ((sn->len >= 3) && (sn->data[0] == 0x2)) {
	/* remove the der encoding of the serial number before generating the
	 * key.. */
	data_left = sn->len-2;
	data_len = sn->data[1];
	index = 2;

	/* extended length ? (not very likely for a serial number) */
	if (data_len & 0x80) {
	    int len_count = data_len & 0x7f;

	    data_len = 0;
	    data_left -= len_count;
	    if (data_left > 0) {
		while (len_count --) {
		    data_len = (data_len << 8) | sn->data[index++];
		}
	    } 
	}
	/* XXX leaving any leading zeros on the serial number for backwards
	 * compatibility
	 */
	/* not a valid der, must be just an unlucky serial number value */
	if (data_len != data_left) {
	    data_len = sn->len;
	    index = 0;
	}
    }

    certKey.type = 0;
    certKey.data = (unsigned char*)PORT_Alloc(sn->len + issuer->len);
    certKey.len = data_len + issuer->len;
    
    if ( certKey.data == NULL ) {
	return(0);
    }

    /* first try the serial number as hand-decoded above*/
    /* copy the serialNumber */
    PORT_Memcpy(certKey.data, &sn->data[index], data_len);

    /* copy the issuer */
    PORT_Memcpy( &certKey.data[data_len],issuer->data,issuer->len);

    cert = nsslowcert_FindCertByKey(handle, &certKey);
    if (cert) {
	PORT_Free(certKey.data);
	return (cert);
    }

    /* didn't find it, try by der encoded serial number */
    /* copy the serialNumber */
    PORT_Memcpy(certKey.data, sn->data, sn->len);

    /* copy the issuer */
    PORT_Memcpy( &certKey.data[sn->len], issuer->data, issuer->len);
    certKey.len = sn->len + issuer->len;

    cert = nsslowcert_FindCertByKey(handle, &certKey);
    
    PORT_Free(certKey.data);
    
    return(cert);
}

/*
 * Generate a key from an issuerAndSerialNumber, and find the
 * associated cert in the database.
 */
NSSLOWCERTTrust *
nsslowcert_FindTrustByIssuerAndSN(NSSLOWCERTCertDBHandle *handle, 
					NSSLOWCERTIssuerAndSN *issuerAndSN)
{
    SECItem certKey;
    SECItem *sn = &issuerAndSN->serialNumber;
    SECItem *issuer = &issuerAndSN->derIssuer;
    NSSLOWCERTTrust *trust;
    unsigned char keyBuf[512];
    int data_left = sn->len-1;
    int data_len = sn->len;
    int index = 0;
    int len;

    /* automatically detect DER encoded serial numbers and remove the der
     * encoding since the database expects unencoded data. 
     * if it's DER encoded, there must be at least 3 bytes, tag, len, data */
    if ((sn->len >= 3) && (sn->data[0] == 0x2)) {
	/* remove the der encoding of the serial number before generating the
	 * key.. */
	data_left = sn->len-2;
	data_len = sn->data[1];
	index = 2;

	/* extended length ? (not very likely for a serial number) */
	if (data_len & 0x80) {
	    int len_count = data_len & 0x7f;

	    data_len = 0;
	    data_left -= len_count;
	    if (data_left > 0) {
		while (len_count --) {
		    data_len = (data_len << 8) | sn->data[index++];
		}
	    } 
	}
	/* XXX leaving any leading zeros on the serial number for backwards
	 * compatibility
	 */
	/* not a valid der, must be just an unlucky serial number value */
	if (data_len != data_left) {
	    data_len = sn->len;
	    index = 0;
	}
    }

    certKey.type = 0;
    certKey.len = data_len + issuer->len;
    len = sn->len + issuer->len;
    if (len > sizeof (keyBuf)) {
	certKey.data = (unsigned char*)PORT_Alloc(len);
    } else {
	certKey.data = keyBuf;
    }
    
    if ( certKey.data == NULL ) {
	return(0);
    }

    /* first try the serial number as hand-decoded above*/
    /* copy the serialNumber */
    PORT_Memcpy(certKey.data, &sn->data[index], data_len);

    /* copy the issuer */
    PORT_Memcpy( &certKey.data[data_len],issuer->data,issuer->len);

    trust = nsslowcert_FindTrustByKey(handle, &certKey);
    if (trust) {
	pkcs11_freeStaticData(certKey.data, keyBuf);
	return (trust);
    }

    if (index == 0) {
	pkcs11_freeStaticData(certKey.data, keyBuf);
	return NULL;
    }

    /* didn't find it, try by der encoded serial number */
    /* copy the serialNumber */
    PORT_Memcpy(certKey.data, sn->data, sn->len);

    /* copy the issuer */
    PORT_Memcpy( &certKey.data[sn->len], issuer->data, issuer->len);
    certKey.len = sn->len + issuer->len;

    trust = nsslowcert_FindTrustByKey(handle, &certKey);
    
    pkcs11_freeStaticData(certKey.data, keyBuf);
    
    return(trust);
}

/*
 * look for the given DER certificate in the database
 */
NSSLOWCERTCertificate *
nsslowcert_FindCertByDERCert(NSSLOWCERTCertDBHandle *handle, SECItem *derCert)
{
    PRArenaPool *arena;
    SECItem certKey;
    SECStatus rv;
    NSSLOWCERTCertificate *cert = NULL;
    
    /* create a scratch arena */
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	return(NULL);
    }
    
    /* extract the database key from the cert */
    rv = nsslowcert_KeyFromDERCert(arena, derCert, &certKey);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* find the certificate */
    cert = nsslowcert_FindCertByKey(handle, &certKey);
    
loser:
    PORT_FreeArena(arena, PR_FALSE);
    return(cert);
}

static void
DestroyCertificate(NSSLOWCERTCertificate *cert, PRBool lockdb)
{
    int refCount;
    NSSLOWCERTCertDBHandle *handle;
    
    if ( cert ) {

	handle = cert->dbhandle;

	/*
	 * handle may be NULL, for example if the cert was created with
	 * nsslowcert_DecodeDERCertificate.
	 */
	if ( lockdb && handle ) {
	    nsslowcert_LockDB(handle);
	}

        nsslowcert_LockCertRefCount(cert);
	PORT_Assert(cert->referenceCount > 0);
	refCount = --cert->referenceCount;
        nsslowcert_UnlockCertRefCount(cert);

	if ( ( refCount == 0 ) ) {
	    certDBEntryCert *entry  = cert->dbEntry;

	    if ( entry ) {
		DestroyDBEntry((certDBEntry *)entry);
            }

	    pkcs11_freeNickname(cert->nickname,cert->nicknameSpace);
	    pkcs11_freeNickname(cert->emailAddr,cert->emailAddrSpace);
	    pkcs11_freeStaticData(cert->certKey.data,cert->certKeySpace);
	    cert->certKey.data = NULL;
	    cert->nickname = NULL;

	    /* zero cert before freeing. Any stale references to this cert
	     * after this point will probably cause an exception.  */
	    PORT_Memset(cert, 0, sizeof *cert);

	    /* use reflock to protect the free list */
	    nsslowcert_LockFreeList();
	    if (certListCount > MAX_CERT_LIST_COUNT) {
		PORT_Free(cert);
	    } else {
		certListCount++;
		cert->next = certListHead;
		certListHead = cert;
	    }
	    nsslowcert_UnlockFreeList();
	    cert = NULL;
        }
	if ( lockdb && handle ) {
	    nsslowcert_UnlockDB(handle);
	}
    }

    return;
}

NSSLOWCERTCertificate *
nsslowcert_CreateCert(void)
{
    NSSLOWCERTCertificate *cert;
    nsslowcert_LockFreeList();
    cert = certListHead;
    if (cert) {
	certListHead = cert->next;
	certListCount--;
    }
    PORT_Assert(certListCount >= 0);
    nsslowcert_UnlockFreeList();
    if (cert) {
	return cert;
    }
    return PORT_ZNew(NSSLOWCERTCertificate);
}

static void
DestroyCertFreeList(void)
{
    NSSLOWCERTCertificate *cert;

    nsslowcert_LockFreeList();
    while (NULL != (cert = certListHead)) {
	certListCount--;
	certListHead = cert->next;
	PORT_Free(cert);
    }
    PORT_Assert(!certListCount);
    certListCount = 0;
    nsslowcert_UnlockFreeList();
}

void
nsslowcert_DestroyTrust(NSSLOWCERTTrust *trust)
{
    certDBEntryCert *entry  = trust->dbEntry;

    if ( entry ) {
	DestroyDBEntry((certDBEntry *)entry);
    }
    pkcs11_freeStaticData(trust->dbKey.data,trust->dbKeySpace);
    PORT_Memset(trust, 0, sizeof(*trust));

    nsslowcert_LockFreeList();
    if (trustListCount > MAX_TRUST_LIST_COUNT) {
	PORT_Free(trust);
    } else {
	trustListCount++;
	trust->next = trustListHead;
	trustListHead = trust;
    }
    nsslowcert_UnlockFreeList();

    return;
}

void
nsslowcert_DestroyCertificate(NSSLOWCERTCertificate *cert)
{
    DestroyCertificate(cert, PR_TRUE);
    return;
}

static void
nsslowcert_DestroyCertificateNoLocking(NSSLOWCERTCertificate *cert)
{
    DestroyCertificate(cert, PR_FALSE);
    return;
}

/*
 * Lookup a CRL in the databases. We mirror the same fast caching data base
 *  caching stuff used by certificates....?
 */
certDBEntryRevocation *
nsslowcert_FindCrlByKey(NSSLOWCERTCertDBHandle *handle, 
						SECItem *crlKey, PRBool isKRL)
{
    SECItem keyitem;
    DBT key;
    SECStatus rv;
    PRArenaPool *arena = NULL;
    certDBEntryRevocation *entry = NULL;
    certDBEntryType crlType = isKRL ? certDBEntryTypeKeyRevocation  
					: certDBEntryTypeRevocation;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	goto loser;
    }
    
    rv = EncodeDBGenericKey(crlKey, arena, &keyitem, crlType);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    
    key.data = keyitem.data;
    key.size = keyitem.len;

    /* find in perm database */
    entry = ReadDBCrlEntry(handle, crlKey, crlType);
	
    if ( entry == NULL ) {
	goto loser;
    }

loser:
    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return entry;
}

/*
 * replace the existing URL in the data base with a new one
 */
static SECStatus
nsslowcert_UpdateCrl(NSSLOWCERTCertDBHandle *handle, SECItem *derCrl, 
			SECItem *crlKey, char *url, PRBool isKRL)
{
    SECStatus rv = SECFailure;
    certDBEntryRevocation *entry = NULL;
    certDBEntryType crlType = isKRL ? certDBEntryTypeKeyRevocation  
					: certDBEntryTypeRevocation;
    DeleteDBCrlEntry(handle, crlKey, crlType);

    /* Write the new entry into the data base */
    entry = NewDBCrlEntry(derCrl, url, crlType, 0);
    if (entry == NULL) goto done;

    rv = WriteDBCrlEntry(handle, entry, crlKey);
    if (rv != SECSuccess) goto done;

done:
    if (entry) {
	DestroyDBEntry((certDBEntry *)entry);
    }
    return rv;
}

SECStatus
nsslowcert_AddCrl(NSSLOWCERTCertDBHandle *handle, SECItem *derCrl, 
			SECItem *crlKey, char *url, PRBool isKRL)
{
    SECStatus rv;

    rv = nsslowcert_UpdateCrl(handle, derCrl, crlKey, url, isKRL);

    return rv;
}

SECStatus
nsslowcert_DeletePermCRL(NSSLOWCERTCertDBHandle *handle, const SECItem *derName,
								 PRBool isKRL)
{
    SECStatus rv;
    certDBEntryType crlType = isKRL ? certDBEntryTypeKeyRevocation  
					: certDBEntryTypeRevocation;
    
    rv = DeleteDBCrlEntry(handle, derName, crlType);
    if (rv != SECSuccess) goto done;
  
done:
    return rv;
}


PRBool
nsslowcert_hasTrust(NSSLOWCERTCertTrust *trust)
{
    if (trust == NULL) {
	return PR_FALSE;
    }
    return !((trust->sslFlags & CERTDB_TRUSTED_UNKNOWN) && 
		(trust->emailFlags & CERTDB_TRUSTED_UNKNOWN) && 
			(trust->objectSigningFlags & CERTDB_TRUSTED_UNKNOWN));
}

/*
 * This function has the logic that decides if another person's cert and
 * email profile from an S/MIME message should be saved.  It can deal with
 * the case when there is no profile.
 */
static SECStatus
nsslowcert_UpdateSMimeProfile(NSSLOWCERTCertDBHandle *dbhandle, 
	char *emailAddr, SECItem *derSubject, SECItem *emailProfile, 
							SECItem *profileTime)
{
    certDBEntrySMime *entry = NULL;
    SECStatus rv = SECFailure;;


    /* find our existing entry */
    entry = nsslowcert_ReadDBSMimeEntry(dbhandle, emailAddr);

    if ( entry ) {
	/* keep our old db entry consistant for old applications. */
	if (!SECITEM_ItemsAreEqual(derSubject, &entry->subjectName)) {
	    nsslowcert_UpdateSubjectEmailAddr(dbhandle, &entry->subjectName, 
				emailAddr, nsslowcert_remove);
	} 
	DestroyDBEntry((certDBEntry *)entry);
	entry = NULL;
    }

    /* now save the entry */
    entry = NewDBSMimeEntry(emailAddr, derSubject, emailProfile,
				profileTime, 0);
    if ( entry == NULL ) {
	rv = SECFailure;
	goto loser;
    }

    nsslowcert_LockDB(dbhandle);

    rv = DeleteDBSMimeEntry(dbhandle, emailAddr);
    /* if delete fails, try to write new entry anyway... */

    /* link subject entry back here */
    rv = nsslowcert_UpdateSubjectEmailAddr(dbhandle, derSubject, emailAddr,
					nsslowcert_add);
    if ( rv != SECSuccess ) {
	    nsslowcert_UnlockDB(dbhandle);
	    goto loser;
    }
	
    rv = WriteDBSMimeEntry(dbhandle, entry);
    if ( rv != SECSuccess ) {
	    nsslowcert_UnlockDB(dbhandle);
	    goto loser;
    }

    nsslowcert_UnlockDB(dbhandle);

    rv = SECSuccess;
    
loser:
    if ( entry ) {
	DestroyDBEntry((certDBEntry *)entry);
    }
    return(rv);
}

SECStatus
nsslowcert_SaveSMimeProfile(NSSLOWCERTCertDBHandle *dbhandle, char *emailAddr, 
	SECItem *derSubject, SECItem *emailProfile, SECItem *profileTime)
{
    SECStatus rv = SECFailure;;


    rv = nsslowcert_UpdateSMimeProfile(dbhandle, emailAddr, 
	 derSubject, emailProfile, profileTime);
    
    return(rv);
}

void
nsslowcert_DestroyFreeLists(void)
{
    if (freeListLock == NULL) {
	return;
    }
    DestroyCertEntryFreeList();
    DestroyTrustFreeList();
    DestroyCertFreeList();
    SKIP_AFTER_FORK(PZ_DestroyLock(freeListLock));
    freeListLock = NULL;
}

void
nsslowcert_DestroyGlobalLocks(void)
{
    if (dbLock) {
	SKIP_AFTER_FORK(PZ_DestroyLock(dbLock));
	dbLock = NULL;
    }
    if (certRefCountLock) {
	SKIP_AFTER_FORK(PZ_DestroyLock(certRefCountLock));
	certRefCountLock = NULL;
    }
    if (certTrustLock) {
	SKIP_AFTER_FORK(PZ_DestroyLock(certTrustLock));
	certTrustLock = NULL;
    }
}

certDBEntry *
nsslowcert_DecodeAnyDBEntry(SECItem *dbData, const SECItem *dbKey, 
                 certDBEntryType entryType, void *pdata)
{
    PLArenaPool *arena = NULL;
    certDBEntry *entry;
    SECStatus rv;
    SECItem dbEntry;


    if ((dbData->len < SEC_DB_ENTRY_HEADER_LEN) || (dbKey->len == 0)) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	goto loser;
    }
    dbEntry.data = &dbData->data[SEC_DB_ENTRY_HEADER_LEN];
    dbEntry.len  = dbData->len - SEC_DB_ENTRY_HEADER_LEN;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	goto loser;
    }
    entry = PORT_ArenaZNew(arena, certDBEntry);
    if (!entry)
    	goto loser;

    entry->common.version = (unsigned int)dbData->data[0];
    entry->common.flags   = (unsigned int)dbData->data[2];
    entry->common.type    = entryType;
    entry->common.arena   = arena;

    switch (entryType) {
    case certDBEntryTypeContentVersion: /* This type appears to be unused */
    case certDBEntryTypeVersion:        /* This type has only the common hdr */
	rv = SECSuccess;
    	break;

    case certDBEntryTypeSubject:
	rv = DecodeDBSubjectEntry(&entry->subject, &dbEntry, dbKey);
    	break;

    case certDBEntryTypeNickname:
	rv = DecodeDBNicknameEntry(&entry->nickname, &dbEntry,
                                   (char *)dbKey->data);
    	break;

    /* smime profiles need entries created after the certs have
     * been imported, loop over them in a second run */
    case certDBEntryTypeSMimeProfile:
	rv = DecodeDBSMimeEntry(&entry->smime, &dbEntry, (char *)dbKey->data);
	break;

    case certDBEntryTypeCert:
	rv = DecodeDBCertEntry(&entry->cert, &dbEntry);
	break;

    case certDBEntryTypeKeyRevocation:
    case certDBEntryTypeRevocation:
	rv = DecodeDBCrlEntry(&entry->revocation, &dbEntry);
	break;

    default:
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	rv = SECFailure;
    }

    if (rv == SECSuccess)
	return entry;

loser:
    if (arena)
	PORT_FreeArena(arena, PR_FALSE);
    return NULL;
}

