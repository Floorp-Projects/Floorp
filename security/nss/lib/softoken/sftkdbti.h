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

#ifndef SFTKDBTI_H
#define SFTKDBTI_H 1

/*
 * private defines
 */
struct SFTKDBHandleStr {
    SDB   *db;
    PRInt32 ref;
    CK_OBJECT_HANDLE  type;
    SECItem passwordKey;
    SECItem *newKey;
    SECItem *oldKey;
    SECItem *updatePasswordKey;
    PZLock *passwordLock;
    SFTKDBHandle *peerDB;
    SDB   *update;
    char  *updateID;
    PRBool updateDBIsInit;
};

#define SFTK_KEYDB_TYPE 0x40000000
#define SFTK_CERTDB_TYPE 0x00000000
#define SFTK_OBJ_TYPE_MASK 0xc0000000
#define SFTK_OBJ_ID_MASK (~SFTK_OBJ_TYPE_MASK)
#define SFTK_TOKEN_TYPE 0x80000000

/* the following is the number of id's to handle on the stack at a time,
 * it's not an upper limit of IDS that can be stored in the database */
#define SFTK_MAX_IDS 10

#define SFTK_GET_SDB(handle) \
	((handle)->update ? (handle)->update : (handle)->db)

SECStatus sftkdb_DecryptAttribute(SECItem *passKey, SECItem *cipherText,
			SECItem **plainText);
SECStatus sftkdb_EncryptAttribute(PLArenaPool *arena, SECItem *passKey,
			SECItem *plainText, SECItem **cipherText);
SECStatus sftkdb_SignAttribute(PLArenaPool *arena, SECItem *passKey,
			CK_OBJECT_HANDLE objectID,
			CK_ATTRIBUTE_TYPE attrType,
			SECItem *plainText, SECItem **sigText);
SECStatus sftkdb_VerifyAttribute(SECItem *passKey,
			CK_OBJECT_HANDLE objectID,
			CK_ATTRIBUTE_TYPE attrType,
			SECItem *plainText, SECItem *sigText);

void sftk_ULong2SDBULong(unsigned char *data, CK_ULONG value);
CK_RV sftkdb_Update(SFTKDBHandle *handle, SECItem *key);
CK_RV sftkdb_PutAttributeSignature(SFTKDBHandle *handle, 
		SDB *keyTarget, CK_OBJECT_HANDLE objectID, 
		CK_ATTRIBUTE_TYPE type, SECItem *signText);



#endif
