/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "sftkdbt.h"
#include "sdb.h"
#include "pkcs11i.h"
#include "pkcs11t.h"

/* raw database stuff */
CK_RV sftkdb_write(SFTKDBHandle *handle, SFTKObject *, CK_OBJECT_HANDLE *);
CK_RV sftkdb_FindObjectsInit(SFTKDBHandle *sdb, const CK_ATTRIBUTE *template,
                             CK_ULONG count, SDBFind **find);
CK_RV sftkdb_FindObjects(SFTKDBHandle *sdb, SDBFind *find,
                         CK_OBJECT_HANDLE *ids, int arraySize, CK_ULONG *count);
CK_RV sftkdb_FindObjectsFinal(SFTKDBHandle *sdb, SDBFind *find);
CK_RV sftkdb_GetAttributeValue(SFTKDBHandle *handle,
                               CK_OBJECT_HANDLE object_id, CK_ATTRIBUTE *template, CK_ULONG count);
CK_RV sftkdb_SetAttributeValue(SFTKDBHandle *handle, SFTKObject *object,
                               const CK_ATTRIBUTE *template, CK_ULONG count);
CK_RV sftkdb_DestroyObject(SFTKDBHandle *handle, CK_OBJECT_HANDLE object_id,
                           CK_OBJECT_CLASS objclass);
CK_RV sftkdb_closeDB(SFTKDBHandle *handle);

/* keydb functions */

SECStatus sftkdb_PWIsInitialized(SFTKDBHandle *keydb);
SECStatus sftkdb_CheckPassword(SFTKDBHandle *keydb, const char *pw,
                               PRBool *tokenRemoved);
SECStatus sftkdb_CheckPasswordNull(SFTKDBHandle *keydb, PRBool *tokenRemoved);
SECStatus sftkdb_PWCached(SFTKDBHandle *keydb);
SECStatus sftkdb_HasPasswordSet(SFTKDBHandle *keydb);
SECStatus sftkdb_ResetKeyDB(SFTKDBHandle *keydb);
SECStatus sftkdb_ChangePassword(SFTKDBHandle *keydb,
                                char *oldPin, char *newPin,
                                PRBool *tokenRemoved);
SECStatus sftkdb_ClearPassword(SFTKDBHandle *keydb);
PRBool sftkdb_InUpdateMerge(SFTKDBHandle *keydb);
PRBool sftkdb_NeedUpdateDBPassword(SFTKDBHandle *keydb);
const char *sftkdb_GetUpdateID(SFTKDBHandle *keydb);
SECItem *sftkdb_GetUpdatePasswordKey(SFTKDBHandle *keydb);
void sftkdb_FreeUpdatePasswordKey(SFTKDBHandle *keydb);

/* Utility functions */
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
                  const char *keyPrefix, const char *updatedir,
                  const char *updCertPrefix, const char *updKeyPrefix,
                  const char *updateID, PRBool readOnly, PRBool noCertDB,
                  PRBool noKeyDB, PRBool forceOpen, PRBool isFIPS,
                  SFTKDBHandle **certDB, SFTKDBHandle **keyDB);
CK_RV sftkdb_Shutdown(void);

SFTKDBHandle *sftk_getCertDB(SFTKSlot *slot);
SFTKDBHandle *sftk_getKeyDB(SFTKSlot *slot);
SFTKDBHandle *sftk_getDBForTokenObject(SFTKSlot *slot,
                                       CK_OBJECT_HANDLE objectID);
void sftk_freeDB(SFTKDBHandle *certHandle);

PRBool sftk_isLegacyIterationCountAllowed(void);
