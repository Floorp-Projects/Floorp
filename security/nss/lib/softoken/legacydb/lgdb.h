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
#ifndef _LGDB_H_
#define _LGDB_H_ 1

#include "nssilock.h"
#include "seccomon.h"
#include "secoidt.h"
#include "lowkeyti.h"
#include "pkcs11t.h"
#include "sdb.h"
#include "cdbhdl.h" 


#define MULTIACCESS "multiaccess:"


/* path stuff (was machine dependent) used by dbinit.c and pk11db.c */
#define PATH_SEPARATOR "/"
#define SECMOD_DB "secmod.db"
#define CERT_DB_FMT "%scert%s.db"
#define KEY_DB_FMT "%skey%s.db"

SEC_BEGIN_PROTOS


/* internal utility functions used by pkcs11.c */
extern const CK_ATTRIBUTE *lg_FindAttribute(CK_ATTRIBUTE_TYPE type,
			const CK_ATTRIBUTE *templ, CK_ULONG count);
extern CK_RV lg_Attribute2SecItem(PLArenaPool *,CK_ATTRIBUTE_TYPE type,
			const CK_ATTRIBUTE *templ, CK_ULONG count,
			SECItem *item);
extern CK_RV lg_Attribute2SSecItem(PLArenaPool *,CK_ATTRIBUTE_TYPE type,
			const CK_ATTRIBUTE *templ, CK_ULONG count,
			SECItem *item);
extern CK_RV lg_PrivAttr2SecItem(PLArenaPool *,CK_ATTRIBUTE_TYPE type,
			const CK_ATTRIBUTE *templ, CK_ULONG count,
			SECItem *item, SDB *sdbpw);
extern CK_RV lg_PrivAttr2SSecItem(PLArenaPool *,CK_ATTRIBUTE_TYPE type,
			const CK_ATTRIBUTE *templ, CK_ULONG count,
			SECItem *item, SDB *sdbpw);
extern CK_RV lg_GetULongAttribute(CK_ATTRIBUTE_TYPE type,
			const CK_ATTRIBUTE *templ, CK_ULONG count, 
			CK_ULONG *out);
extern PRBool lg_hasAttribute(CK_ATTRIBUTE_TYPE type,
			const CK_ATTRIBUTE *templ, CK_ULONG count);
extern PRBool lg_isTrue(CK_ATTRIBUTE_TYPE type,
			const CK_ATTRIBUTE *templ, CK_ULONG count);
extern PRBool lg_isSensitive(CK_ATTRIBUTE_TYPE type, CK_OBJECT_CLASS inClass);
extern char *lg_getString(CK_ATTRIBUTE_TYPE type,
			const CK_ATTRIBUTE *templ, CK_ULONG count);
extern unsigned int lg_MapTrust(CK_TRUST trust, PRBool clientAuth);

/* clear out all the existing object ID to database key mappings.
 * used to reinit a token */
extern CK_RV lg_ClearTokenKeyHashTable(SDB *sdb);


extern void lg_FreeSearch(SDBFind *search);

NSSLOWCERTCertDBHandle *lg_getCertDB(SDB *sdb);
NSSLOWKEYDBHandle *lg_getKeyDB(SDB *sdb);

const char *lg_EvaluateConfigDir(const char *configdir, char **domain);


/*
 * object handle modifiers
 */
#define LG_TOKEN_MASK		0xc0000000L
#define LG_TOKEN_TYPE_MASK	0x38000000L
#define LG_TOKEN_TYPE_SHIFT	27
/* keydb (high bit == 0) */
#define LG_TOKEN_TYPE_PRIV	0x08000000L
#define LG_TOKEN_TYPE_PUB	0x10000000L
#define LG_TOKEN_TYPE_KEY	0x18000000L
/* certdb (high bit == 1) */
#define LG_TOKEN_TYPE_TRUST	0x20000000L
#define LG_TOKEN_TYPE_CRL	0x28000000L
#define LG_TOKEN_TYPE_SMIME	0x30000000L
#define LG_TOKEN_TYPE_CERT	0x38000000L

#define LG_TOKEN_KRL_HANDLE	(LG_TOKEN_TYPE_CRL|1)

#define LG_SEARCH_BLOCK_SIZE   10
#define LG_BUF_SPACE	  50
#define LG_STRICT   PR_FALSE

/*
 * token object utilities
 */
void lg_addHandle(SDBFind *search, CK_OBJECT_HANDLE handle);
PRBool lg_poisonHandle(SDB *sdb, SECItem *dbkey, CK_OBJECT_HANDLE handle);
PRBool lg_tokenMatch(SDB *sdb, const SECItem *dbKey, CK_OBJECT_HANDLE class,
				const CK_ATTRIBUTE *templ, CK_ULONG count);
const SECItem *lg_lookupTokenKeyByHandle(SDB *sdb, CK_OBJECT_HANDLE handle);
CK_OBJECT_HANDLE lg_mkHandle(SDB *sdb, SECItem *dbKey, CK_OBJECT_HANDLE class);
SECStatus lg_deleteTokenKeyByHandle(SDB *sdb, CK_OBJECT_HANDLE handle);

SECStatus lg_util_encrypt(PLArenaPool *arena, SDB *sdbpw, 
			  SECItem *plainText, SECItem **cipherText);
SECStatus lg_util_decrypt(SDB *sdbpw, 
			  SECItem *cipherText, SECItem **plainText);
PLHashTable *lg_GetHashTable(SDB *sdb);
void lg_DBLock(SDB *sdb);
void lg_DBUnlock(SDB *sdb);

typedef void (*LGFreeFunc)(void *);


/*
 * database functions
 */

/* lg_FindObjectsInit initializes a search for token and session objects 
 * that match a template. */
CK_RV lg_FindObjectsInit(SDB *sdb, const CK_ATTRIBUTE *pTemplate, 
			 CK_ULONG ulCount, SDBFind **search);
/* lg_FindObjects continues a search for token and session objects 
 * that match a template, obtaining additional object handles. */
CK_RV lg_FindObjects(SDB *sdb, SDBFind *search, 
    CK_OBJECT_HANDLE *phObject,CK_ULONG ulMaxObjectCount,
    CK_ULONG *pulObjectCount);

/* lg_FindObjectsFinal finishes a search for token and session objects. */
CK_RV lg_FindObjectsFinal(SDB* lgdb, SDBFind *search);

/* lg_CreateObject parses the template and create an object stored in the 
 * DB that reflects the object specified in the template.  */
CK_RV lg_CreateObject(SDB *sdb, CK_OBJECT_HANDLE *handle,
			const CK_ATTRIBUTE *templ, CK_ULONG count);

CK_RV lg_GetAttributeValue(SDB *sdb, CK_OBJECT_HANDLE object_id, 
				CK_ATTRIBUTE *template, CK_ULONG count);
CK_RV lg_SetAttributeValue(SDB *sdb, CK_OBJECT_HANDLE object_id, 
			const CK_ATTRIBUTE *template, CK_ULONG count);
CK_RV lg_DestroyObject(SDB *sdb, CK_OBJECT_HANDLE object_id);

CK_RV lg_Close(SDB *sdb);
CK_RV lg_Reset(SDB *sdb);

/*
 * The old database doesn't share and doesn't support
 * transactions.
 */
CK_RV lg_Begin(SDB *sdb);
CK_RV lg_Commit(SDB *sdb);
CK_RV lg_Abort(SDB *sdb);
CK_RV lg_GetMetaData(SDB *sdb, const char *id, SECItem *item1, SECItem *item2);
CK_RV lg_PutMetaData(SDB *sdb, const char *id, 
			const SECItem *item1, const SECItem *item2);

SEC_END_PROTOS

#ifndef XP_UNIX

#define NO_FORK_CHECK

#endif

#ifndef NO_FORK_CHECK

extern PRBool lg_parentForkedAfterC_Initialize;
#define SKIP_AFTER_FORK(x) if (!lg_parentForkedAfterC_Initialize) x

#else

#define SKIP_AFTER_FORK(x) x

#endif /* NO_FORK_CHECK */

#endif /* _LGDB_H_ */

