/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pub use crate::*;
use std::os::raw::{c_int, c_uchar, c_uint, c_void};

extern "C" {
    pub fn PK11_FreeSlot(slot: *mut PK11SlotInfo);
    pub fn PK11_GetInternalSlot() -> *mut PK11SlotInfo;
    pub fn PK11_GenerateRandom(data: *mut c_uchar, len: c_int) -> SECStatus;
    pub fn PK11_FreeSymKey(key: *mut PK11SymKey);
    pub fn PK11_ImportSymKey(
        slot: *mut PK11SlotInfo,
        type_: CK_MECHANISM_TYPE,
        origin: u32, /* PK11Origin */
        operation: CK_ATTRIBUTE_TYPE,
        key: *mut SECItem,
        wincx: *mut c_void,
    ) -> *mut PK11SymKey;
    pub fn PK11_Derive(
        baseKey: *mut PK11SymKey,
        mechanism: CK_MECHANISM_TYPE,
        param: *mut SECItem,
        target: CK_MECHANISM_TYPE,
        operation: CK_ATTRIBUTE_TYPE,
        keySize: c_int,
    ) -> *mut PK11SymKey;
    pub fn PK11_PubDeriveWithKDF(
        privKey: *mut SECKEYPrivateKey,
        pubKey: *mut SECKEYPublicKey,
        isSender: PRBool,
        randomA: *mut SECItem,
        randomB: *mut SECItem,
        derive: CK_MECHANISM_TYPE,
        target: CK_MECHANISM_TYPE,
        operation: CK_ATTRIBUTE_TYPE,
        keySize: c_int,
        kdf: CK_ULONG,
        sharedData: *mut SECItem,
        wincx: *mut c_void,
    ) -> *mut PK11SymKey;
    pub fn PK11_ExtractKeyValue(symKey: *mut PK11SymKey) -> SECStatus;
    pub fn PK11_GetKeyData(symKey: *mut PK11SymKey) -> *mut SECItem;
    pub fn PK11_GenerateKeyPair(
        slot: *mut PK11SlotInfo,
        type_: CK_MECHANISM_TYPE,
        param: *mut c_void,
        pubk: *mut *mut SECKEYPublicKey,
        isPerm: PRBool,
        isSensitive: PRBool,
        wincx: *mut c_void,
    ) -> *mut SECKEYPrivateKey;
    pub fn PK11_FindKeyByKeyID(
        slot: *mut PK11SlotInfo,
        keyID: *mut SECItem,
        wincx: *mut c_void,
    ) -> *mut SECKEYPrivateKey;
    pub fn PK11_Decrypt(
        symkey: *mut PK11SymKey,
        mechanism: CK_MECHANISM_TYPE,
        param: *mut SECItem,
        out: *mut c_uchar,
        outLen: *mut c_uint,
        maxLen: c_uint,
        enc: *const c_uchar,
        encLen: c_uint,
    ) -> SECStatus;
    pub fn PK11_Encrypt(
        symKey: *mut PK11SymKey,
        mechanism: CK_MECHANISM_TYPE,
        param: *mut SECItem,
        out: *mut c_uchar,
        outLen: *mut c_uint,
        maxLen: c_uint,
        data: *const c_uchar,
        dataLen: c_uint,
    ) -> SECStatus;
    pub fn PK11_VerifyWithMechanism(
        key: *mut SECKEYPublicKey,
        mechanism: CK_MECHANISM_TYPE,
        param: *const SECItem,
        sig: *const SECItem,
        hash: *const SECItem,
        wincx: *mut c_void,
    ) -> SECStatus;
    pub fn PK11_MapSignKeyType(keyType: u32 /* KeyType */) -> CK_MECHANISM_TYPE;
    pub fn PK11_DestroyContext(context: *mut PK11Context, freeit: PRBool);
    pub fn PK11_CreateContextBySymKey(
        type_: CK_MECHANISM_TYPE,
        operation: CK_ATTRIBUTE_TYPE,
        symKey: *mut PK11SymKey,
        param: *mut SECItem,
    ) -> *mut PK11Context;
    pub fn PK11_DigestBegin(cx: *mut PK11Context) -> SECStatus;
    pub fn PK11_HashBuf(
        hashAlg: u32, /* SECOidTag */
        out: *mut c_uchar,
        in_: *const c_uchar,
        len: PRInt32,
    ) -> SECStatus;
    pub fn PK11_DigestOp(context: *mut PK11Context, in_: *const c_uchar, len: c_uint) -> SECStatus;
    pub fn PK11_DigestFinal(
        context: *mut PK11Context,
        data: *mut c_uchar,
        outLen: *mut c_uint,
        length: c_uint,
    ) -> SECStatus;
    pub fn PK11_DestroyGenericObject(object: *mut PK11GenericObject) -> SECStatus;
    pub fn PK11_CreateGenericObject(
        slot: *mut PK11SlotInfo,
        pTemplate: *const CK_ATTRIBUTE,
        count: c_int,
        token: PRBool,
    ) -> *mut PK11GenericObject;
    pub fn PK11_ReadRawAttribute(
        type_: u32, /* PK11ObjectType */
        object: *mut c_void,
        attr: CK_ATTRIBUTE_TYPE,
        item: *mut SECItem,
    ) -> SECStatus;
    pub fn PK11_CreatePBEV2AlgorithmID(
        pbeAlgTag: u32,    /* SECOidTag */
        cipherAlgTag: u32, /* SECOidTag */
        prfAlgTag: u32,    /* SECOidTag */
        keyLength: c_int,
        iteration: c_int,
        salt: *mut SECItem,
    ) -> *mut SECAlgorithmID;

    pub fn PK11_PBEKeyGen(
        slot: *mut PK11SlotInfo,
        algid: *mut SECAlgorithmID,
        pwitem: *mut SECItem,
        faulty3DES: PRBool,
        wincx: *mut c_void,
    ) -> *mut PK11SymKey;
}
