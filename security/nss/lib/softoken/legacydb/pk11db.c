/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 *  The following code handles the storage of PKCS 11 modules used by the
 * NSS. This file is written to abstract away how the modules are
 * stored so we can deside that later.
 */

#include "lgdb.h"
#include "mcom_db.h"
#include "secerr.h"
#include "utilpars.h"

#define FREE_CLEAR(p) \
    if (p) {          \
        PORT_Free(p); \
        p = NULL;     \
    }

/* Construct a database key for a given module */
static SECStatus
lgdb_MakeKey(DBT *key, char *module)
{
    int len = 0;
    char *commonName;

    commonName = NSSUTIL_ArgGetParamValue("name", module);
    if (commonName == NULL) {
        commonName = NSSUTIL_ArgGetParamValue("library", module);
    }
    if (commonName == NULL)
        return SECFailure;
    len = PORT_Strlen(commonName);
    key->data = commonName;
    key->size = len;
    return SECSuccess;
}

/* free out constructed database key */
static void
lgdb_FreeKey(DBT *key)
{
    if (key->data) {
        PORT_Free(key->data);
    }
    key->data = NULL;
    key->size = 0;
}

typedef struct lgdbDataStr lgdbData;
typedef struct lgdbSlotDataStr lgdbSlotData;
struct lgdbDataStr {
    unsigned char major;
    unsigned char minor;
    unsigned char nameStart[2];
    unsigned char slotOffset[2];
    unsigned char internal;
    unsigned char fips;
    unsigned char ssl[8];
    unsigned char trustOrder[4];
    unsigned char cipherOrder[4];
    unsigned char reserved1;
    unsigned char isModuleDB;
    unsigned char isModuleDBOnly;
    unsigned char isCritical;
    unsigned char reserved[4];
    unsigned char names[6]; /* enough space for the length fields */
};

struct lgdbSlotDataStr {
    unsigned char slotID[4];
    unsigned char defaultFlags[4];
    unsigned char timeout[4];
    unsigned char askpw;
    unsigned char hasRootCerts;
    unsigned char reserved[18]; /* this makes it a round 32 bytes */
};

#define LGDB_DB_VERSION_MAJOR 0
#define LGDB_DB_VERSION_MINOR 6
#define LGDB_DB_EXT1_VERSION_MAJOR 0
#define LGDB_DB_EXT1_VERSION_MINOR 6
#define LGDB_DB_NOUI_VERSION_MAJOR 0
#define LGDB_DB_NOUI_VERSION_MINOR 4

#define LGDB_PUTSHORT(dest, src)             \
    (dest)[1] = (unsigned char)((src)&0xff); \
    (dest)[0] = (unsigned char)(((src) >> 8) & 0xff);
#define LGDB_PUTLONG(dest, src)                        \
    (dest)[3] = (unsigned char)((src)&0xff);           \
    (dest)[2] = (unsigned char)(((src) >> 8) & 0xff);  \
    (dest)[1] = (unsigned char)(((src) >> 16) & 0xff); \
    (dest)[0] = (unsigned char)(((src) >> 24) & 0xff);
#define LGDB_GETSHORT(src) \
    ((unsigned short)(((src)[0] << 8) | (src)[1]))
#define LGDB_GETLONG(src)                              \
    ((unsigned long)(((unsigned long)(src)[0] << 24) | \
                     ((unsigned long)(src)[1] << 16) | \
                     ((unsigned long)(src)[2] << 8) |  \
                     (unsigned long)(src)[3]))

/*
 * build a data base entry from a module
 */
static SECStatus
lgdb_EncodeData(DBT *data, char *module)
{
    lgdbData *encoded = NULL;
    lgdbSlotData *slot;
    unsigned char *dataPtr, *offsetPtr;
    unsigned short len, len2 = 0, len3 = 0;
    int count = 0;
    unsigned short offset;
    int dataLen, i;
    unsigned long order;
    unsigned long ssl[2];
    char *commonName = NULL, *dllName = NULL, *param = NULL, *nss = NULL;
    char *slotParams, *ciphers;
    struct NSSUTILPreSlotInfoStr *slotInfo = NULL;
    SECStatus rv = SECFailure;

    rv = NSSUTIL_ArgParseModuleSpec(module, &dllName, &commonName, &param, &nss);
    if (rv != SECSuccess)
        return rv;
    rv = SECFailure;

    if (commonName == NULL) {
        /* set error */
        goto loser;
    }

    len = PORT_Strlen(commonName);
    if (dllName) {
        len2 = PORT_Strlen(dllName);
    }
    if (param) {
        len3 = PORT_Strlen(param);
    }

    slotParams = NSSUTIL_ArgGetParamValue("slotParams", nss);
    slotInfo = NSSUTIL_ArgParseSlotInfo(NULL, slotParams, &count);
    if (slotParams)
        PORT_Free(slotParams);

    if (count && slotInfo == NULL) {
        /* set error */
        goto loser;
    }

    dataLen = sizeof(lgdbData) + len + len2 + len3 + sizeof(unsigned short) +
              count * sizeof(lgdbSlotData);

    data->data = (unsigned char *)PORT_ZAlloc(dataLen);
    encoded = (lgdbData *)data->data;
    dataPtr = (unsigned char *)data->data;
    data->size = dataLen;

    if (encoded == NULL) {
        /* set error */
        goto loser;
    }

    encoded->major = LGDB_DB_VERSION_MAJOR;
    encoded->minor = LGDB_DB_VERSION_MINOR;
    encoded->internal = (unsigned char)(NSSUTIL_ArgHasFlag("flags", "internal", nss) ? 1 : 0);
    encoded->fips = (unsigned char)(NSSUTIL_ArgHasFlag("flags", "FIPS", nss) ? 1 : 0);
    encoded->isModuleDB = (unsigned char)(NSSUTIL_ArgHasFlag("flags", "isModuleDB", nss) ? 1 : 0);
    encoded->isModuleDBOnly = (unsigned char)(NSSUTIL_ArgHasFlag("flags", "isModuleDBOnly", nss) ? 1 : 0);
    encoded->isCritical = (unsigned char)(NSSUTIL_ArgHasFlag("flags", "critical", nss) ? 1 : 0);

    order = NSSUTIL_ArgReadLong("trustOrder", nss,
                                NSSUTIL_DEFAULT_TRUST_ORDER, NULL);
    LGDB_PUTLONG(encoded->trustOrder, order);
    order = NSSUTIL_ArgReadLong("cipherOrder", nss,
                                NSSUTIL_DEFAULT_CIPHER_ORDER, NULL);
    LGDB_PUTLONG(encoded->cipherOrder, order);

    ciphers = NSSUTIL_ArgGetParamValue("ciphers", nss);
    NSSUTIL_ArgParseCipherFlags(&ssl[0], ciphers);
    LGDB_PUTLONG(encoded->ssl, ssl[0]);
    LGDB_PUTLONG(&encoded->ssl[4], ssl[1]);
    if (ciphers)
        PORT_Free(ciphers);

    offset = (unsigned short)offsetof(lgdbData, names);
    LGDB_PUTSHORT(encoded->nameStart, offset);
    offset = offset + len + len2 + len3 + 3 * sizeof(unsigned short);
    LGDB_PUTSHORT(encoded->slotOffset, offset);

    LGDB_PUTSHORT(&dataPtr[offset], ((unsigned short)count));
    slot = (lgdbSlotData *)(dataPtr + offset + sizeof(unsigned short));

    offsetPtr = encoded->names;
    LGDB_PUTSHORT(encoded->names, len);
    offsetPtr += sizeof(unsigned short);
    PORT_Memcpy(offsetPtr, commonName, len);
    offsetPtr += len;

    LGDB_PUTSHORT(offsetPtr, len2);
    offsetPtr += sizeof(unsigned short);
    if (len2) {
        PORT_Memcpy(offsetPtr, dllName, len2);
    }
    offsetPtr += len2;

    LGDB_PUTSHORT(offsetPtr, len3);
    offsetPtr += sizeof(unsigned short);
    if (len3) {
        PORT_Memcpy(offsetPtr, param, len3);
    }
    offsetPtr += len3;

    if (count) {
        for (i = 0; i < count; i++) {
            LGDB_PUTLONG(slot[i].slotID, slotInfo[i].slotID);
            LGDB_PUTLONG(slot[i].defaultFlags,
                         slotInfo[i].defaultFlags);
            LGDB_PUTLONG(slot[i].timeout, slotInfo[i].timeout);
            slot[i].askpw = slotInfo[i].askpw;
            slot[i].hasRootCerts = slotInfo[i].hasRootCerts;
            PORT_Memset(slot[i].reserved, 0, sizeof(slot[i].reserved));
        }
    }
    rv = SECSuccess;

loser:
    if (commonName)
        PORT_Free(commonName);
    if (dllName)
        PORT_Free(dllName);
    if (param)
        PORT_Free(param);
    if (slotInfo)
        PORT_Free(slotInfo);
    if (nss)
        PORT_Free(nss);
    return rv;
}

static void
lgdb_FreeData(DBT *data)
{
    if (data->data) {
        PORT_Free(data->data);
    }
}

static void
lgdb_FreeSlotStrings(char **slotStrings, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        if (slotStrings[i]) {
            PR_smprintf_free(slotStrings[i]);
            slotStrings[i] = NULL;
        }
    }
}

/*
 * build a module from the data base entry.
 */
static char *
lgdb_DecodeData(char *defParams, DBT *data, PRBool *retInternal)
{
    lgdbData *encoded;
    lgdbSlotData *slots;
    PLArenaPool *arena;
    char *commonName = NULL;
    char *dllName = NULL;
    char *parameters = NULL;
    char *nss;
    char *moduleSpec;
    char **slotStrings = NULL;
    unsigned char *names;
    unsigned long slotCount;
    unsigned long ssl0 = 0;
    unsigned long ssl1 = 0;
    unsigned long slotID;
    unsigned long defaultFlags;
    unsigned long timeout;
    unsigned long trustOrder = NSSUTIL_DEFAULT_TRUST_ORDER;
    unsigned long cipherOrder = NSSUTIL_DEFAULT_CIPHER_ORDER;
    unsigned short len;
    unsigned short namesOffset = 0;   /* start of the names block */
    unsigned long namesRunningOffset; /* offset to name we are
                     * currently processing */
    unsigned short slotOffset;
    PRBool isOldVersion = PR_FALSE;
    PRBool internal;
    PRBool isFIPS;
    PRBool isModuleDB = PR_FALSE;
    PRBool isModuleDBOnly = PR_FALSE;
    PRBool extended = PR_FALSE;
    int i;

    arena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if (arena == NULL)
        return NULL;

#define CHECK_SIZE(x)                                 \
    if ((unsigned int)data->size < (unsigned int)(x)) \
    goto db_loser

    /* -------------------------------------------------------------
    ** Process the buffer header, which is the lgdbData struct.
    ** It may be an old or new version.  Check the length for each.
    */

    CHECK_SIZE(offsetof(lgdbData, trustOrder[0]));

    encoded = (lgdbData *)data->data;

    internal = (encoded->internal != 0) ? PR_TRUE : PR_FALSE;
    isFIPS = (encoded->fips != 0) ? PR_TRUE : PR_FALSE;

    if (retInternal)
        *retInternal = internal;
    if (internal) {
        parameters = PORT_ArenaStrdup(arena, defParams);
        if (parameters == NULL)
            goto loser;
    }
    if (internal && (encoded->major == LGDB_DB_NOUI_VERSION_MAJOR) &&
        (encoded->minor <= LGDB_DB_NOUI_VERSION_MINOR)) {
        isOldVersion = PR_TRUE;
    }
    if ((encoded->major == LGDB_DB_EXT1_VERSION_MAJOR) &&
        (encoded->minor >= LGDB_DB_EXT1_VERSION_MINOR)) {
        CHECK_SIZE(sizeof(lgdbData));
        trustOrder = LGDB_GETLONG(encoded->trustOrder);
        cipherOrder = LGDB_GETLONG(encoded->cipherOrder);
        isModuleDB = (encoded->isModuleDB != 0) ? PR_TRUE : PR_FALSE;
        isModuleDBOnly = (encoded->isModuleDBOnly != 0) ? PR_TRUE : PR_FALSE;
        extended = PR_TRUE;
    }
    if (internal && !extended) {
        trustOrder = 0;
        cipherOrder = 100;
    }
    /* decode SSL cipher enable flags */
    ssl0 = LGDB_GETLONG(encoded->ssl);
    ssl1 = LGDB_GETLONG(encoded->ssl + 4);

    slotOffset = LGDB_GETSHORT(encoded->slotOffset);
    namesOffset = LGDB_GETSHORT(encoded->nameStart);

    /*--------------------------------------------------------------
    ** Now process the variable length set of names.
    ** The names have this structure:
    ** struct {
    **     BYTE  commonNameLen[ 2 ];
    **     BYTE  commonName   [ commonNameLen ];
    **     BTTE  libNameLen   [ 2 ];
    **     BYTE  libName      [ libNameLen ];
    ** If it is "extended" it also has these members:
    **     BYTE  initStringLen[ 2 ];
    **     BYTE  initString   [ initStringLen ];
    ** }
    */

    namesRunningOffset = namesOffset;
    /* copy the module's common name */
    CHECK_SIZE(namesRunningOffset + 2);
    names = (unsigned char *)data->data;
    len = LGDB_GETSHORT(names + namesRunningOffset);

    CHECK_SIZE(namesRunningOffset + 2 + len);
    commonName = (char *)PORT_ArenaAlloc(arena, len + 1);
    if (commonName == NULL)
        goto loser;
    PORT_Memcpy(commonName, names + namesRunningOffset + 2, len);
    commonName[len] = 0;
    namesRunningOffset += len + 2;

    /* copy the module's shared library file name. */
    CHECK_SIZE(namesRunningOffset + 2);
    len = LGDB_GETSHORT(names + namesRunningOffset);
    if (len) {
        CHECK_SIZE(namesRunningOffset + 2 + len);
        dllName = (char *)PORT_ArenaAlloc(arena, len + 1);
        if (dllName == NULL)
            goto loser;
        PORT_Memcpy(dllName, names + namesRunningOffset + 2, len);
        dllName[len] = 0;
    }
    namesRunningOffset += len + 2;

    /* copy the module's initialization string, if present. */
    if (!internal && extended) {
        CHECK_SIZE(namesRunningOffset + 2);
        len = LGDB_GETSHORT(names + namesRunningOffset);
        if (len) {
            CHECK_SIZE(namesRunningOffset + 2 + len);
            parameters = (char *)PORT_ArenaAlloc(arena, len + 1);
            if (parameters == NULL)
                goto loser;
            PORT_Memcpy(parameters, names + namesRunningOffset + 2, len);
            parameters[len] = 0;
        }
        namesRunningOffset += len + 2;
    }

    /*
     * Consistency check: Make sure the slot and names blocks don't
     * overlap. These blocks can occur in any order, so this check is made
     * in 2 parts. First we check the case where the slot block starts
     * after the name block. Later, when we have the slot block length,
     * we check the case where slot block starts before the name block.
     * NOTE: in most cases any overlap will likely be detected by invalid
     * data read from the blocks, but it's better to find out sooner
     * than later.
     */
    if (slotOffset >= namesOffset) { /* slot block starts after name block */
        if (slotOffset < namesRunningOffset) {
            goto db_loser;
        }
    }

    /* ------------------------------------------------------------------
    ** Part 3, process the slot table.
    ** This part has this structure:
    ** struct {
    **     BYTE slotCount [ 2 ];
    **     lgdbSlotData [ slotCount ];
    ** {
    */

    CHECK_SIZE(slotOffset + 2);
    slotCount = LGDB_GETSHORT((unsigned char *)data->data + slotOffset);

    /*
     * Consistency check: Part 2. We now have the slot block length, we can
     * check the case where the slotblock procedes the name block.
     */
    if (slotOffset < namesOffset) { /* slot block starts before name block */
        if (namesOffset < slotOffset + 2 + slotCount * sizeof(lgdbSlotData)) {
            goto db_loser;
        }
    }

    CHECK_SIZE((slotOffset + 2 + slotCount * sizeof(lgdbSlotData)));
    slots = (lgdbSlotData *)((unsigned char *)data->data + slotOffset + 2);

    /*  slotCount; */
    slotStrings = (char **)PORT_ArenaZAlloc(arena, slotCount * sizeof(char *));
    if (slotStrings == NULL)
        goto loser;
    for (i = 0; i < (int)slotCount; i++, slots++) {
        PRBool hasRootCerts = PR_FALSE;
        PRBool hasRootTrust = PR_FALSE;
        slotID = LGDB_GETLONG(slots->slotID);
        defaultFlags = LGDB_GETLONG(slots->defaultFlags);
        timeout = LGDB_GETLONG(slots->timeout);
        hasRootCerts = slots->hasRootCerts;
        if (isOldVersion && internal && (slotID != 2)) {
            unsigned long internalFlags =
                NSSUTIL_ArgParseSlotFlags("slotFlags",
                                          NSSUTIL_DEFAULT_SFTKN_FLAGS);
            defaultFlags |= internalFlags;
        }
        if (hasRootCerts && !extended) {
            trustOrder = 100;
        }

        slotStrings[i] = NSSUTIL_MkSlotString(slotID, defaultFlags, timeout,
                                              (unsigned char)slots->askpw,
                                              hasRootCerts, hasRootTrust);
        if (slotStrings[i] == NULL) {
            lgdb_FreeSlotStrings(slotStrings, i);
            goto loser;
        }
    }

    nss = NSSUTIL_MkNSSString(slotStrings, slotCount, internal, isFIPS,
                              isModuleDB, isModuleDBOnly, internal, trustOrder,
                              cipherOrder, ssl0, ssl1);
    lgdb_FreeSlotStrings(slotStrings, slotCount);
    /* it's permissible (and normal) for nss to be NULL. it simply means
     * there are no NSS specific parameters in the database */
    moduleSpec = NSSUTIL_MkModuleSpec(dllName, commonName, parameters, nss);
    PR_smprintf_free(nss);
    PORT_FreeArena(arena, PR_TRUE);
    return moduleSpec;

db_loser:
    PORT_SetError(SEC_ERROR_BAD_DATABASE);
loser:
    PORT_FreeArena(arena, PR_TRUE);
    return NULL;
}

static DB *
lgdb_OpenDB(const char *appName, const char *filename, const char *dbName,
            PRBool readOnly, PRBool update)
{
    DB *pkcs11db = NULL;

    if (appName) {
        char *secname = PORT_Strdup(filename);
        int len = strlen(secname);
        int status = RDB_FAIL;

        if (len >= 3 && PORT_Strcmp(&secname[len - 3], ".db") == 0) {
            secname[len - 3] = 0;
        }
        pkcs11db =
            rdbopen(appName, "", secname, readOnly ? NO_RDONLY : NO_RDWR, NULL);
        if (update && !pkcs11db) {
            DB *updatedb;

            pkcs11db = rdbopen(appName, "", secname, NO_CREATE, &status);
            if (!pkcs11db) {
                if (status == RDB_RETRY) {
                    pkcs11db = rdbopen(appName, "", secname,
                                       readOnly ? NO_RDONLY : NO_RDWR, NULL);
                }
                PORT_Free(secname);
                return pkcs11db;
            }
            updatedb = dbopen(dbName, NO_RDONLY, 0600, DB_HASH, 0);
            if (updatedb) {
                db_Copy(pkcs11db, updatedb);
                (*updatedb->close)(updatedb);
            } else {
                (*pkcs11db->close)(pkcs11db);
                PORT_Free(secname);
                return NULL;
            }
        }
        PORT_Free(secname);
        return pkcs11db;
    }

    /* I'm sure we should do more checks here sometime... */
    pkcs11db = dbopen(dbName, readOnly ? NO_RDONLY : NO_RDWR, 0600, DB_HASH, 0);

    /* didn't exist? create it */
    if (pkcs11db == NULL) {
        if (readOnly)
            return NULL;

        pkcs11db = dbopen(dbName, NO_CREATE, 0600, DB_HASH, 0);
        if (pkcs11db)
            (*pkcs11db->sync)(pkcs11db, 0);
    }
    return pkcs11db;
}

static void
lgdb_CloseDB(DB *pkcs11db)
{
    (*pkcs11db->close)(pkcs11db);
}

SECStatus legacy_AddSecmodDB(const char *appName, const char *filename,
                             const char *dbname, char *module, PRBool rw);

#define LGDB_STEP 10
/*
 * Read all the existing modules in
 */
char **
legacy_ReadSecmodDB(const char *appName, const char *filename,
                    const char *dbname, char *params, PRBool rw)
{
    DBT key, data;
    int ret;
    DB *pkcs11db = NULL;
    char **moduleList = NULL, **newModuleList = NULL;
    int moduleCount = 1;
    int useCount = LGDB_STEP;

    moduleList = (char **)PORT_ZAlloc(useCount * sizeof(char **));
    if (moduleList == NULL)
        return NULL;

    pkcs11db = lgdb_OpenDB(appName, filename, dbname, PR_TRUE, rw);
    if (pkcs11db == NULL)
        goto done;

    /* read and parse the file or data base */
    ret = (*pkcs11db->seq)(pkcs11db, &key, &data, R_FIRST);
    if (ret)
        goto done;

    do {
        char *moduleString;
        PRBool internal = PR_FALSE;
        if ((moduleCount + 1) >= useCount) {
            useCount += LGDB_STEP;
            newModuleList =
                (char **)PORT_Realloc(moduleList, useCount * sizeof(char *));
            if (newModuleList == NULL)
                goto done;
            moduleList = newModuleList;
            PORT_Memset(&moduleList[moduleCount + 1], 0,
                        sizeof(char *) * LGDB_STEP);
        }
        moduleString = lgdb_DecodeData(params, &data, &internal);
        if (internal) {
            moduleList[0] = moduleString;
        } else {
            moduleList[moduleCount] = moduleString;
            moduleCount++;
        }
    } while ((*pkcs11db->seq)(pkcs11db, &key, &data, R_NEXT) == 0);

done:
    if (!moduleList[0]) {
        char *newparams = NSSUTIL_Quote(params, '"');
        if (newparams) {
            moduleList[0] = PR_smprintf(
                NSSUTIL_DEFAULT_INTERNAL_INIT1 "%s" NSSUTIL_DEFAULT_INTERNAL_INIT2 "%s" NSSUTIL_DEFAULT_INTERNAL_INIT3,
                newparams, NSSUTIL_DEFAULT_SFTKN_FLAGS);
            PORT_Free(newparams);
        }
    }
    /* deal with trust cert db here */

    if (pkcs11db) {
        lgdb_CloseDB(pkcs11db);
    } else if (moduleList[0] && rw) {
        legacy_AddSecmodDB(appName, filename, dbname, moduleList[0], rw);
    }
    if (!moduleList[0]) {
        PORT_Free(moduleList);
        moduleList = NULL;
    }
    return moduleList;
}

SECStatus
legacy_ReleaseSecmodDBData(const char *appName, const char *filename,
                           const char *dbname, char **moduleSpecList, PRBool rw)
{
    if (moduleSpecList) {
        char **index;
        for (index = moduleSpecList; *index; index++) {
            PR_smprintf_free(*index);
        }
        PORT_Free(moduleSpecList);
    }
    return SECSuccess;
}

/*
 * Delete a module from the Data Base
 */
SECStatus
legacy_DeleteSecmodDB(const char *appName, const char *filename,
                      const char *dbname, char *args, PRBool rw)
{
    DBT key;
    SECStatus rv = SECFailure;
    DB *pkcs11db = NULL;
    int ret;

    if (!rw)
        return SECFailure;

    /* make sure we have a db handle */
    pkcs11db = lgdb_OpenDB(appName, filename, dbname, PR_FALSE, PR_FALSE);
    if (pkcs11db == NULL) {
        return SECFailure;
    }

    rv = lgdb_MakeKey(&key, args);
    if (rv != SECSuccess)
        goto done;
    rv = SECFailure;
    ret = (*pkcs11db->del)(pkcs11db, &key, 0);
    lgdb_FreeKey(&key);
    if (ret != 0)
        goto done;

    ret = (*pkcs11db->sync)(pkcs11db, 0);
    if (ret == 0)
        rv = SECSuccess;

done:
    lgdb_CloseDB(pkcs11db);
    return rv;
}

/*
 * Add a module to the Data base
 */
SECStatus
legacy_AddSecmodDB(const char *appName, const char *filename,
                   const char *dbname, char *module, PRBool rw)
{
    DBT key, data;
    SECStatus rv = SECFailure;
    DB *pkcs11db = NULL;
    int ret;

    if (!rw)
        return SECFailure;

    /* make sure we have a db handle */
    pkcs11db = lgdb_OpenDB(appName, filename, dbname, PR_FALSE, PR_FALSE);
    if (pkcs11db == NULL) {
        return SECFailure;
    }

    rv = lgdb_MakeKey(&key, module);
    if (rv != SECSuccess)
        goto done;
    rv = lgdb_EncodeData(&data, module);
    if (rv != SECSuccess) {
        lgdb_FreeKey(&key);
        goto done;
    }
    rv = SECFailure;
    ret = (*pkcs11db->put)(pkcs11db, &key, &data, 0);
    lgdb_FreeKey(&key);
    lgdb_FreeData(&data);
    if (ret != 0)
        goto done;

    ret = (*pkcs11db->sync)(pkcs11db, 0);
    if (ret == 0)
        rv = SECSuccess;

done:
    lgdb_CloseDB(pkcs11db);
    return rv;
}
