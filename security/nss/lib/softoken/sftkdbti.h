/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SFTKDBTI_H
#define SFTKDBTI_H 1

/*
 * private defines
 */
struct SFTKDBHandleStr {
    SDB *db;
    PRInt32 ref;
    CK_OBJECT_HANDLE type;
    SECItem passwordKey;
    int defaultIterationCount;
    SECItem *newKey;
    int newDefaultIterationCount;
    SECItem *oldKey;
    SECItem *updatePasswordKey;
    PZLock *passwordLock;
    SFTKDBHandle *peerDB;
    SDB *update;
    char *updateID;
    PRBool updateDBIsInit;
    PRBool usesLegacyStorage;
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

SECStatus sftkdb_DecryptAttribute(SFTKDBHandle *handle,
                                  SECItem *passKey,
                                  CK_OBJECT_HANDLE id,
                                  CK_ATTRIBUTE_TYPE attrType,
                                  SECItem *cipherText, SECItem **plainText);
SECStatus sftkdb_EncryptAttribute(PLArenaPool *arena,
                                  SFTKDBHandle *handle, SDB *db,
                                  SECItem *passKey,
                                  int iterationCount,
                                  CK_OBJECT_HANDLE id,
                                  CK_ATTRIBUTE_TYPE attrType,
                                  SECItem *plainText, SECItem **cipherText);
SECStatus sftkdb_SignAttribute(PLArenaPool *arena,
                               SFTKDBHandle *handle, SDB *db,
                               SECItem *passKey,
                               int iterationCount, CK_OBJECT_HANDLE objectID,
                               CK_ATTRIBUTE_TYPE attrType,
                               SECItem *plainText, SECItem **sigText);
SECStatus sftkdb_VerifyAttribute(SFTKDBHandle *handle,
                                 SECItem *passKey,
                                 CK_OBJECT_HANDLE objectID,
                                 CK_ATTRIBUTE_TYPE attrType,
                                 SECItem *plainText, SECItem *sigText);

PRBool sftkdb_isULONGAttribute(CK_ATTRIBUTE_TYPE type);
void sftk_ULong2SDBULong(unsigned char *data, CK_ULONG value);
CK_RV sftkdb_Update(SFTKDBHandle *handle, SECItem *key);
CK_RV sftkdb_PutAttributeSignature(SFTKDBHandle *handle,
                                   SDB *keyTarget, CK_OBJECT_HANDLE objectID,
                                   CK_ATTRIBUTE_TYPE type, SECItem *signText);
CK_RV sftkdb_GetAttributeSignature(SFTKDBHandle *handle,
                                   SFTKDBHandle *keyHandle,
                                   CK_OBJECT_HANDLE objectID,
                                   CK_ATTRIBUTE_TYPE type,
                                   SECItem *signText);
CK_RV
sftkdb_DestroyAttributeSignature(SFTKDBHandle *handle, SDB *db,
                                 CK_OBJECT_HANDLE objectID,
                                 CK_ATTRIBUTE_TYPE type);

#endif
