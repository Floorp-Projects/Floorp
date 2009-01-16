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
 * Red Hat Inc.
 * Portions created by the Initial Developer are Copyright (C) 2007
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
#include "sftkdbt.h"
#include "sdb.h"
#include "pkcs11i.h"
#include "pkcs11t.h"

/* raw database stuff */
CK_RV sftkdb_write(SFTKDBHandle *handle, SFTKObject *,CK_OBJECT_HANDLE *);
CK_RV sftkdb_FindObjectsInit(SFTKDBHandle *sdb, const CK_ATTRIBUTE *template,
				 CK_ULONG count, SDBFind **find);
CK_RV sftkdb_FindObjects(SFTKDBHandle *sdb, SDBFind *find, 
			CK_OBJECT_HANDLE *ids, int arraySize, CK_ULONG *count);
CK_RV sftkdb_FindObjectsFinal(SFTKDBHandle *sdb, SDBFind *find);
CK_RV sftkdb_GetAttributeValue(SFTKDBHandle *handle,
	 CK_OBJECT_HANDLE object_id, CK_ATTRIBUTE *template, CK_ULONG count);
CK_RV sftkdb_SetAttributeValue(SFTKDBHandle *handle, SFTKObject *object, 
	 		const CK_ATTRIBUTE *template, CK_ULONG count);
CK_RV sftkdb_DestroyObject(SFTKDBHandle *handle, CK_OBJECT_HANDLE object_id);
CK_RV sftkdb_closeDB(SFTKDBHandle *handle);


/* secmod.db functions */
char ** sftkdb_ReadSecmodDB(SDBType dbType, const char *appName, 
			    const char *filename, const char *dbname, 
			    char *params, PRBool rw);
SECStatus sftkdb_ReleaseSecmodDBData(SDBType dbType, const char *appName, 
				     const char *filename, const char *dbname, 
				     char **moduleSpecList, PRBool rw);
SECStatus sftkdb_DeleteSecmodDB(SDBType dbType, const char *appName, 
				const char *filename, const char *dbname, 
				char *args, PRBool rw);
SECStatus sftkdb_AddSecmodDB(SDBType dbType, const char *appName, 
			     const char *filename, const char *dbname, 
			     char *module, PRBool rw);

/* keydb functions */

SECStatus sftkdb_PWIsInitialized(SFTKDBHandle *keydb);
SECStatus sftkdb_CheckPassword(SFTKDBHandle *keydb, const char *pw,
			       PRBool *tokenRemoved);
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
		PRBool noKeyDB, PRBool forceOpen, 
		SFTKDBHandle **certDB, SFTKDBHandle **keyDB);
CK_RV sftkdb_Shutdown(void);

SFTKDBHandle *sftk_getCertDB(SFTKSlot *slot);
SFTKDBHandle *sftk_getKeyDB(SFTKSlot *slot);
SFTKDBHandle *sftk_getDBForTokenObject(SFTKSlot *slot, 
                                       CK_OBJECT_HANDLE objectID);
void sftk_freeDB(SFTKDBHandle *certHandle);
