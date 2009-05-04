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
 * This code defines the glue layer between softoken and the legacy DB library
 */
#include "sdb.h"

/*
 * function prototypes for the callbacks into softoken from the legacyDB
 */

typedef SECStatus (*LGEncryptFunc)(PRArenaPool *arena, SDB *sdb, 
				  SECItem *plainText, SECItem **cipherText);
typedef SECStatus (*LGDecryptFunc)(SDB *sdb, SECItem *cipherText, 
				   SECItem **plainText);

/*
 * function prototypes for the exported functions.
 */
typedef CK_RV (*LGOpenFunc) (const char *dir, const char *certPrefix, 
		const char *keyPrefix, 
		int certVersion, int keyVersion, int flags, 
		SDB **certDB, SDB **keyDB);
typedef char ** (*LGReadSecmodFunc)(const char *appName, 
			const char *filename, 
			const char *dbname, char *params, PRBool rw);
typedef SECStatus (*LGReleaseSecmodFunc)(const char *appName,
			const char *filename, 
			const char *dbname, char **params, PRBool rw);
typedef SECStatus (*LGDeleteSecmodFunc)(const char *appName,
			const char *filename, 
			const char *dbname, char *params, PRBool rw);
typedef SECStatus (*LGAddSecmodFunc)(const char *appName, 
			const char *filename, 
			const char *dbname, char *params, PRBool rw);
typedef SECStatus (*LGShutdownFunc)(PRBool forked);
typedef void (*LGSetForkStateFunc)(PRBool);
typedef void (*LGSetCryptFunc)(LGEncryptFunc, LGDecryptFunc);

/*
 * Softoken Glue Functions
 */
CK_RV sftkdbCall_open(const char *dir, const char *certPrefix, 
		const char *keyPrefix, 
		int certVersion, int keyVersion, int flags, PRBool isFIPS,
		SDB **certDB, SDB **keyDB);
char ** sftkdbCall_ReadSecmodDB(const char *appName, const char *filename, 
			const char *dbname, char *params, PRBool rw);
SECStatus sftkdbCall_ReleaseSecmodDBData(const char *appName, 
			const char *filename, const char *dbname, 
			char **moduleSpecList, PRBool rw);
SECStatus sftkdbCall_DeleteSecmodDB(const char *appName, 
		      const char *filename, const char *dbname, 
		      char *args, PRBool rw);
SECStatus sftkdbCall_AddSecmodDB(const char *appName, 
		   const char *filename, const char *dbname, 
		   char *module, PRBool rw);
CK_RV sftkdbCall_Shutdown(void);

