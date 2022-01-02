/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * This file implements the Symkey wrapper and the PKCS context
 * Interfaces.
 */

#include <stddef.h>

#include "seccomon.h"
#include "secmod.h"
#include "nssilock.h"
#include "secmodi.h"
#include "secmodti.h"
#include "pkcs11.h"
#include "pk11func.h"
#include "secitem.h"
#include "secoid.h"
#include "secerr.h"
#include "hasht.h"

static ECPointEncoding pk11_ECGetPubkeyEncoding(const SECKEYPublicKey *pubKey);

static void
pk11_EnterKeyMonitor(PK11SymKey *symKey)
{
    if (!symKey->sessionOwner || !(symKey->slot->isThreadSafe))
        PK11_EnterSlotMonitor(symKey->slot);
}

static void
pk11_ExitKeyMonitor(PK11SymKey *symKey)
{
    if (!symKey->sessionOwner || !(symKey->slot->isThreadSafe))
        PK11_ExitSlotMonitor(symKey->slot);
}

/*
 * pk11_getKeyFromList returns a symKey that has a session (if needSession
 * was specified), or explicitly does not have a session (if needSession
 * was not specified).
 */
static PK11SymKey *
pk11_getKeyFromList(PK11SlotInfo *slot, PRBool needSession)
{
    PK11SymKey *symKey = NULL;

    PZ_Lock(slot->freeListLock);
    /* own session list are symkeys with sessions that the symkey owns.
     * 'most' symkeys will own their own session. */
    if (needSession) {
        if (slot->freeSymKeysWithSessionHead) {
            symKey = slot->freeSymKeysWithSessionHead;
            slot->freeSymKeysWithSessionHead = symKey->next;
            slot->keyCount--;
        }
    }
    /* if we don't need a symkey with its own session, or we couldn't find
     * one on the owner list, get one from the non-owner free list. */
    if (!symKey) {
        if (slot->freeSymKeysHead) {
            symKey = slot->freeSymKeysHead;
            slot->freeSymKeysHead = symKey->next;
            slot->keyCount--;
        }
    }
    PZ_Unlock(slot->freeListLock);
    if (symKey) {
        symKey->next = NULL;
        if (!needSession) {
            return symKey;
        }
        /* if we are getting an owner key, make sure we have a valid session.
         * session could be invalid if the token has been removed or because
         * we got it from the non-owner free list */
        if ((symKey->series != slot->series) ||
            (symKey->session == CK_INVALID_HANDLE)) {
            symKey->session = pk11_GetNewSession(slot, &symKey->sessionOwner);
        }
        PORT_Assert(symKey->session != CK_INVALID_HANDLE);
        if (symKey->session != CK_INVALID_HANDLE)
            return symKey;
        PK11_FreeSymKey(symKey);
        /* if we are here, we need a session, but couldn't get one, it's
         * unlikely we pk11_GetNewSession will succeed if we call it a second
         * time. */
        return NULL;
    }

    symKey = PORT_New(PK11SymKey);
    if (symKey == NULL) {
        return NULL;
    }

    symKey->next = NULL;
    if (needSession) {
        symKey->session = pk11_GetNewSession(slot, &symKey->sessionOwner);
        PORT_Assert(symKey->session != CK_INVALID_HANDLE);
        if (symKey->session == CK_INVALID_HANDLE) {
            PK11_FreeSymKey(symKey);
            symKey = NULL;
        }
    } else {
        symKey->session = CK_INVALID_HANDLE;
    }
    return symKey;
}

/* Caller MUST hold slot->freeListLock (or ref count == 0?) !! */
void
PK11_CleanKeyList(PK11SlotInfo *slot)
{
    PK11SymKey *symKey = NULL;

    while (slot->freeSymKeysWithSessionHead) {
        symKey = slot->freeSymKeysWithSessionHead;
        slot->freeSymKeysWithSessionHead = symKey->next;
        pk11_CloseSession(slot, symKey->session, symKey->sessionOwner);
        PORT_Free(symKey);
    }
    while (slot->freeSymKeysHead) {
        symKey = slot->freeSymKeysHead;
        slot->freeSymKeysHead = symKey->next;
        pk11_CloseSession(slot, symKey->session, symKey->sessionOwner);
        PORT_Free(symKey);
    }
    return;
}

/*
 * create a symetric key:
 *      Slot is the slot to create the key in.
 *      type is the mechanism type
 *      owner is does this symKey structure own it's object handle (rare
 *        that this is false).
 *      needSession means the returned symKey will return with a valid session
 *        allocated already.
 */
static PK11SymKey *
pk11_CreateSymKey(PK11SlotInfo *slot, CK_MECHANISM_TYPE type,
                  PRBool owner, PRBool needSession, void *wincx)
{

    PK11SymKey *symKey = pk11_getKeyFromList(slot, needSession);

    if (symKey == NULL) {
        return NULL;
    }
    /* if needSession was specified, make sure we have a valid session.
     * callers which specify needSession as false should do their own
     * check of the session before returning the symKey */
    if (needSession && symKey->session == CK_INVALID_HANDLE) {
        PK11_FreeSymKey(symKey);
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return NULL;
    }

    symKey->type = type;
    symKey->data.type = siBuffer;
    symKey->data.data = NULL;
    symKey->data.len = 0;
    symKey->owner = owner;
    symKey->objectID = CK_INVALID_HANDLE;
    symKey->slot = slot;
    symKey->series = slot->series;
    symKey->cx = wincx;
    symKey->size = 0;
    symKey->refCount = 1;
    symKey->origin = PK11_OriginNULL;
    symKey->parent = NULL;
    symKey->freeFunc = NULL;
    symKey->userData = NULL;
    PK11_ReferenceSlot(slot);
    return symKey;
}

/*
 * destroy a symetric key
 */
void
PK11_FreeSymKey(PK11SymKey *symKey)
{
    PK11SlotInfo *slot;
    PRBool freeit = PR_TRUE;

    if (!symKey) {
        return;
    }

    if (PR_ATOMIC_DECREMENT(&symKey->refCount) == 0) {
        PK11SymKey *parent = symKey->parent;

        symKey->parent = NULL;
        if ((symKey->owner) && symKey->objectID != CK_INVALID_HANDLE) {
            pk11_EnterKeyMonitor(symKey);
            (void)PK11_GETTAB(symKey->slot)->C_DestroyObject(symKey->session, symKey->objectID);
            pk11_ExitKeyMonitor(symKey);
        }
        if (symKey->data.data) {
            PORT_Memset(symKey->data.data, 0, symKey->data.len);
            PORT_Free(symKey->data.data);
        }
        /* free any existing data */
        if (symKey->userData && symKey->freeFunc) {
            (*symKey->freeFunc)(symKey->userData);
        }
        slot = symKey->slot;
        PZ_Lock(slot->freeListLock);
        if (slot->keyCount < slot->maxKeyCount) {
            /*
             * freeSymkeysWithSessionHead contain a list of reusable
             *  SymKey structures with valid sessions.
             *    sessionOwner must be true.
             *    session must be valid.
             * freeSymKeysHead contain a list of SymKey structures without
             *  valid session.
             *    session must be CK_INVALID_HANDLE.
             *    though sessionOwner is false, callers should not depend on
             *    this fact.
             */
            if (symKey->sessionOwner) {
                PORT_Assert(symKey->session != CK_INVALID_HANDLE);
                symKey->next = slot->freeSymKeysWithSessionHead;
                slot->freeSymKeysWithSessionHead = symKey;
            } else {
                symKey->session = CK_INVALID_HANDLE;
                symKey->next = slot->freeSymKeysHead;
                slot->freeSymKeysHead = symKey;
            }
            slot->keyCount++;
            symKey->slot = NULL;
            freeit = PR_FALSE;
        }
        PZ_Unlock(slot->freeListLock);
        if (freeit) {
            pk11_CloseSession(symKey->slot, symKey->session,
                              symKey->sessionOwner);
            PORT_Free(symKey);
        }
        PK11_FreeSlot(slot);

        if (parent) {
            PK11_FreeSymKey(parent);
        }
    }
}

PK11SymKey *
PK11_ReferenceSymKey(PK11SymKey *symKey)
{
    PR_ATOMIC_INCREMENT(&symKey->refCount);
    return symKey;
}

/*
 * Accessors
 */
CK_MECHANISM_TYPE
PK11_GetMechanism(PK11SymKey *symKey)
{
    return symKey->type;
}

/*
 * return the slot associated with a symetric key
 */
PK11SlotInfo *
PK11_GetSlotFromKey(PK11SymKey *symKey)
{
    return PK11_ReferenceSlot(symKey->slot);
}

CK_KEY_TYPE
PK11_GetSymKeyType(PK11SymKey *symKey)
{
    return PK11_GetKeyType(symKey->type, symKey->size);
}

PK11SymKey *
PK11_GetNextSymKey(PK11SymKey *symKey)
{
    return symKey ? symKey->next : NULL;
}

char *
PK11_GetSymKeyNickname(PK11SymKey *symKey)
{
    return PK11_GetObjectNickname(symKey->slot, symKey->objectID);
}

SECStatus
PK11_SetSymKeyNickname(PK11SymKey *symKey, const char *nickname)
{
    return PK11_SetObjectNickname(symKey->slot, symKey->objectID, nickname);
}

void *
PK11_GetSymKeyUserData(PK11SymKey *symKey)
{
    return symKey->userData;
}

void
PK11_SetSymKeyUserData(PK11SymKey *symKey, void *userData,
                       PK11FreeDataFunc freeFunc)
{
    /* free any existing data */
    if (symKey->userData && symKey->freeFunc) {
        (*symKey->freeFunc)(symKey->userData);
    }
    symKey->userData = userData;
    symKey->freeFunc = freeFunc;
    return;
}

/*
 * turn key handle into an appropriate key object
 */
PK11SymKey *
PK11_SymKeyFromHandle(PK11SlotInfo *slot, PK11SymKey *parent, PK11Origin origin,
                      CK_MECHANISM_TYPE type, CK_OBJECT_HANDLE keyID, PRBool owner, void *wincx)
{
    PK11SymKey *symKey;
    PRBool needSession = !(owner && parent);

    if (keyID == CK_INVALID_HANDLE) {
        return NULL;
    }

    symKey = pk11_CreateSymKey(slot, type, owner, needSession, wincx);
    if (symKey == NULL) {
        return NULL;
    }

    symKey->objectID = keyID;
    symKey->origin = origin;

    /* adopt the parent's session */
    /* This is only used by SSL. What we really want here is a session
     * structure with a ref count so  the session goes away only after all the
     * keys do. */
    if (!needSession) {
        symKey->sessionOwner = PR_FALSE;
        symKey->session = parent->session;
        symKey->parent = PK11_ReferenceSymKey(parent);
        /* This is the only case where pk11_CreateSymKey does not explicitly
         * check symKey->session. We need to assert here to make sure.
         * the session isn't invalid. */
        PORT_Assert(parent->session != CK_INVALID_HANDLE);
        if (parent->session == CK_INVALID_HANDLE) {
            PK11_FreeSymKey(symKey);
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            return NULL;
        }
    }

    return symKey;
}

/*
 * Restore a symmetric wrapping key that was saved using PK11_SetWrapKey.
 *
 * This function is provided for ABI compatibility; see PK11_SetWrapKey below.
 */
PK11SymKey *
PK11_GetWrapKey(PK11SlotInfo *slot, int wrap, CK_MECHANISM_TYPE type,
                int series, void *wincx)
{
    PK11SymKey *symKey = NULL;
    CK_OBJECT_HANDLE keyHandle;

    PK11_EnterSlotMonitor(slot);
    if (slot->series != series ||
        slot->refKeys[wrap] == CK_INVALID_HANDLE) {
        PK11_ExitSlotMonitor(slot);
        return NULL;
    }

    if (type == CKM_INVALID_MECHANISM) {
        type = slot->wrapMechanism;
    }

    keyHandle = slot->refKeys[wrap];
    PK11_ExitSlotMonitor(slot);
    symKey = PK11_SymKeyFromHandle(slot, NULL, PK11_OriginDerive,
                                   slot->wrapMechanism, keyHandle, PR_FALSE, wincx);
    return symKey;
}

/*
 * This function sets an attribute on the current slot with a wrapping key.  The
 * data saved is ephemeral; it needs to be run every time the program is
 * invoked.
 *
 * Since NSS 3.45, this function is marginally more thread safe.  It uses the
 * slot lock (if present) and fails silently if a value is already set.  Use
 * PK11_GetWrapKey() after calling this function to get the current wrapping key
 * in case there was an update on another thread.
 *
 * Either way, using this function is inadvisable.  It's provided for ABI
 * compatibility only.
 */
void
PK11_SetWrapKey(PK11SlotInfo *slot, int wrap, PK11SymKey *wrapKey)
{
    PK11_EnterSlotMonitor(slot);
    if (wrap >= 0) {
        size_t uwrap = (size_t)wrap;
        if (uwrap < PR_ARRAY_SIZE(slot->refKeys) &&
            slot->refKeys[uwrap] == CK_INVALID_HANDLE) {
            /* save the handle and mechanism for the wrapping key */
            /* mark the key and session as not owned by us so they don't get
             * freed when the key goes way... that lets us reuse the key
             * later */
            slot->refKeys[uwrap] = wrapKey->objectID;
            wrapKey->owner = PR_FALSE;
            wrapKey->sessionOwner = PR_FALSE;
            slot->wrapMechanism = wrapKey->type;
        }
    }
    PK11_ExitSlotMonitor(slot);
}

/*
 * figure out if a key is still valid or if it is stale.
 */
PRBool
PK11_VerifyKeyOK(PK11SymKey *key)
{
    if (!PK11_IsPresent(key->slot)) {
        return PR_FALSE;
    }
    return (PRBool)(key->series == key->slot->series);
}

static PK11SymKey *
pk11_ImportSymKeyWithTempl(PK11SlotInfo *slot, CK_MECHANISM_TYPE type,
                           PK11Origin origin, PRBool isToken, CK_ATTRIBUTE *keyTemplate,
                           unsigned int templateCount, SECItem *key, void *wincx)
{
    PK11SymKey *symKey;
    SECStatus rv;

    symKey = pk11_CreateSymKey(slot, type, !isToken, PR_TRUE, wincx);
    if (symKey == NULL) {
        return NULL;
    }

    symKey->size = key->len;

    PK11_SETATTRS(&keyTemplate[templateCount], CKA_VALUE, key->data, key->len);
    templateCount++;

    if (SECITEM_CopyItem(NULL, &symKey->data, key) != SECSuccess) {
        PK11_FreeSymKey(symKey);
        return NULL;
    }

    symKey->origin = origin;

    /* import the keys */
    rv = PK11_CreateNewObject(slot, symKey->session, keyTemplate,
                              templateCount, isToken, &symKey->objectID);
    if (rv != SECSuccess) {
        PK11_FreeSymKey(symKey);
        return NULL;
    }

    return symKey;
}

/*
 * turn key bits into an appropriate key object
 */
PK11SymKey *
PK11_ImportSymKey(PK11SlotInfo *slot, CK_MECHANISM_TYPE type,
                  PK11Origin origin, CK_ATTRIBUTE_TYPE operation, SECItem *key, void *wincx)
{
    PK11SymKey *symKey;
    unsigned int templateCount = 0;
    CK_OBJECT_CLASS keyClass = CKO_SECRET_KEY;
    CK_KEY_TYPE keyType = CKK_GENERIC_SECRET;
    CK_BBOOL cktrue = CK_TRUE; /* sigh */
    CK_ATTRIBUTE keyTemplate[5];
    CK_ATTRIBUTE *attrs = keyTemplate;

    /* CKA_NSS_MESSAGE is a fake operation to distinguish between
     * Normal Encrypt/Decrypt and MessageEncrypt/Decrypt. Don't try to set
     * it as a real attribute */
    if ((operation & CKA_NSS_MESSAGE_MASK) == CKA_NSS_MESSAGE) {
        /* Message is or'd with a real Attribute (CKA_ENCRYPT, CKA_DECRYPT),
         * etc. Strip out the real attribute here */
        operation &= ~CKA_NSS_MESSAGE_MASK;
    }

    PK11_SETATTRS(attrs, CKA_CLASS, &keyClass, sizeof(keyClass));
    attrs++;
    PK11_SETATTRS(attrs, CKA_KEY_TYPE, &keyType, sizeof(keyType));
    attrs++;
    PK11_SETATTRS(attrs, operation, &cktrue, 1);
    attrs++;
    templateCount = attrs - keyTemplate;
    PR_ASSERT(templateCount + 1 <= sizeof(keyTemplate) / sizeof(CK_ATTRIBUTE));

    keyType = PK11_GetKeyType(type, key->len);
    symKey = pk11_ImportSymKeyWithTempl(slot, type, origin, PR_FALSE,
                                        keyTemplate, templateCount, key, wincx);
    return symKey;
}
/* Import a PKCS #11 data object and return it as a key. This key is
 * only useful in a limited number of mechanisms, such as HKDF. */
PK11SymKey *
PK11_ImportDataKey(PK11SlotInfo *slot, CK_MECHANISM_TYPE type, PK11Origin origin,
                   CK_ATTRIBUTE_TYPE operation, SECItem *key, void *wincx)
{
    CK_OBJECT_CLASS ckoData = CKO_DATA;
    CK_ATTRIBUTE template[2] = { { CKA_CLASS, (CK_BYTE_PTR)&ckoData, sizeof(ckoData) },
                                 { CKA_VALUE, (CK_BYTE_PTR)key->data, key->len } };
    CK_OBJECT_HANDLE handle;
    PK11GenericObject *genObject;

    genObject = PK11_CreateGenericObject(slot, template, PR_ARRAY_SIZE(template), PR_FALSE);
    if (genObject == NULL) {
        return NULL;
    }
    handle = PK11_GetObjectHandle(PK11_TypeGeneric, genObject, NULL);
    if (handle == CK_INVALID_HANDLE) {
        return NULL;
    }
    /* A note about ownership of the PKCS #11 handle:
     * PK11_CreateGenericObject() will not destroy the object it creates
     * on Free, For that you want PK11_CreateManagedGenericObject().
     * Below we import the handle into the symKey structure. We pass
     * PR_TRUE as the owner so that the symKey will destroy the object
     * once it's freed. This is way it's safe to free now. */
    PK11_DestroyGenericObject(genObject);
    return PK11_SymKeyFromHandle(slot, NULL, origin, type, handle, PR_TRUE, wincx);
}

/* turn key bits into an appropriate key object */
PK11SymKey *
PK11_ImportSymKeyWithFlags(PK11SlotInfo *slot, CK_MECHANISM_TYPE type,
                           PK11Origin origin, CK_ATTRIBUTE_TYPE operation, SECItem *key,
                           CK_FLAGS flags, PRBool isPerm, void *wincx)
{
    PK11SymKey *symKey;
    unsigned int templateCount = 0;
    CK_OBJECT_CLASS keyClass = CKO_SECRET_KEY;
    CK_KEY_TYPE keyType = CKK_GENERIC_SECRET;
    CK_BBOOL cktrue = CK_TRUE; /* sigh */
    CK_ATTRIBUTE keyTemplate[MAX_TEMPL_ATTRS];
    CK_ATTRIBUTE *attrs = keyTemplate;

    /* CKA_NSS_MESSAGE is a fake operation to distinguish between
     * Normal Encrypt/Decrypt and MessageEncrypt/Decrypt. Don't try to set
     * it as a real attribute */
    if ((operation & CKA_NSS_MESSAGE_MASK) == CKA_NSS_MESSAGE) {
        /* Message is or'd with a real Attribute (CKA_ENCRYPT, CKA_DECRYPT),
         * etc. Strip out the real attribute here */
        operation &= ~CKA_NSS_MESSAGE_MASK;
    }

    PK11_SETATTRS(attrs, CKA_CLASS, &keyClass, sizeof(keyClass));
    attrs++;
    PK11_SETATTRS(attrs, CKA_KEY_TYPE, &keyType, sizeof(keyType));
    attrs++;
    if (isPerm) {
        PK11_SETATTRS(attrs, CKA_TOKEN, &cktrue, sizeof(cktrue));
        attrs++;
        /* sigh some tokens think CKA_PRIVATE = false is a reasonable
         * default for secret keys */
        PK11_SETATTRS(attrs, CKA_PRIVATE, &cktrue, sizeof(cktrue));
        attrs++;
    }
    attrs += pk11_OpFlagsToAttributes(flags, attrs, &cktrue);
    if ((operation != CKA_FLAGS_ONLY) &&
        !pk11_FindAttrInTemplate(keyTemplate, attrs - keyTemplate, operation)) {
        PK11_SETATTRS(attrs, operation, &cktrue, sizeof(cktrue));
        attrs++;
    }
    templateCount = attrs - keyTemplate;
    PR_ASSERT(templateCount + 1 <= sizeof(keyTemplate) / sizeof(CK_ATTRIBUTE));

    keyType = PK11_GetKeyType(type, key->len);
    symKey = pk11_ImportSymKeyWithTempl(slot, type, origin, isPerm,
                                        keyTemplate, templateCount, key, wincx);
    if (symKey && isPerm) {
        symKey->owner = PR_FALSE;
    }
    return symKey;
}

PK11SymKey *
PK11_FindFixedKey(PK11SlotInfo *slot, CK_MECHANISM_TYPE type, SECItem *keyID,
                  void *wincx)
{
    CK_ATTRIBUTE findTemp[4];
    CK_ATTRIBUTE *attrs;
    CK_BBOOL ckTrue = CK_TRUE;
    CK_OBJECT_CLASS keyclass = CKO_SECRET_KEY;
    size_t tsize = 0;
    CK_OBJECT_HANDLE key_id;

    attrs = findTemp;
    PK11_SETATTRS(attrs, CKA_CLASS, &keyclass, sizeof(keyclass));
    attrs++;
    PK11_SETATTRS(attrs, CKA_TOKEN, &ckTrue, sizeof(ckTrue));
    attrs++;
    if (keyID) {
        PK11_SETATTRS(attrs, CKA_ID, keyID->data, keyID->len);
        attrs++;
    }
    tsize = attrs - findTemp;
    PORT_Assert(tsize <= sizeof(findTemp) / sizeof(CK_ATTRIBUTE));

    key_id = pk11_FindObjectByTemplate(slot, findTemp, tsize);
    if (key_id == CK_INVALID_HANDLE) {
        return NULL;
    }
    return PK11_SymKeyFromHandle(slot, NULL, PK11_OriginDerive, type, key_id,
                                 PR_FALSE, wincx);
}

PK11SymKey *
PK11_ListFixedKeysInSlot(PK11SlotInfo *slot, char *nickname, void *wincx)
{
    CK_ATTRIBUTE findTemp[4];
    CK_ATTRIBUTE *attrs;
    CK_BBOOL ckTrue = CK_TRUE;
    CK_OBJECT_CLASS keyclass = CKO_SECRET_KEY;
    int tsize = 0;
    int objCount = 0;
    CK_OBJECT_HANDLE *key_ids;
    PK11SymKey *nextKey = NULL;
    PK11SymKey *topKey = NULL;
    int i, len;

    attrs = findTemp;
    PK11_SETATTRS(attrs, CKA_CLASS, &keyclass, sizeof(keyclass));
    attrs++;
    PK11_SETATTRS(attrs, CKA_TOKEN, &ckTrue, sizeof(ckTrue));
    attrs++;
    if (nickname) {
        len = PORT_Strlen(nickname);
        PK11_SETATTRS(attrs, CKA_LABEL, nickname, len);
        attrs++;
    }
    tsize = attrs - findTemp;
    PORT_Assert(tsize <= sizeof(findTemp) / sizeof(CK_ATTRIBUTE));

    key_ids = pk11_FindObjectsByTemplate(slot, findTemp, tsize, &objCount);
    if (key_ids == NULL) {
        return NULL;
    }

    for (i = 0; i < objCount; i++) {
        SECItem typeData;
        CK_KEY_TYPE type = CKK_GENERIC_SECRET;
        SECStatus rv = PK11_ReadAttribute(slot, key_ids[i],
                                          CKA_KEY_TYPE, NULL, &typeData);
        if (rv == SECSuccess) {
            if (typeData.len == sizeof(CK_KEY_TYPE)) {
                type = *(CK_KEY_TYPE *)typeData.data;
            }
            PORT_Free(typeData.data);
        }
        nextKey = PK11_SymKeyFromHandle(slot, NULL, PK11_OriginDerive,
                                        PK11_GetKeyMechanism(type), key_ids[i], PR_FALSE, wincx);
        if (nextKey) {
            nextKey->next = topKey;
            topKey = nextKey;
        }
    }
    PORT_Free(key_ids);
    return topKey;
}

void *
PK11_GetWindow(PK11SymKey *key)
{
    return key->cx;
}

/*
 * extract a symmetric key value. NOTE: if the key is sensitive, we will
 * not be able to do this operation. This function is used to move
 * keys from one token to another */
SECStatus
PK11_ExtractKeyValue(PK11SymKey *symKey)
{
    SECStatus rv;

    if (symKey == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (symKey->data.data != NULL) {
        if (symKey->size == 0) {
            symKey->size = symKey->data.len;
        }
        return SECSuccess;
    }

    if (symKey->slot == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        return SECFailure;
    }

    rv = PK11_ReadAttribute(symKey->slot, symKey->objectID, CKA_VALUE, NULL,
                            &symKey->data);
    if (rv == SECSuccess) {
        symKey->size = symKey->data.len;
    }
    return rv;
}

SECStatus
PK11_DeleteTokenSymKey(PK11SymKey *symKey)
{
    if (!PK11_IsPermObject(symKey->slot, symKey->objectID)) {
        return SECFailure;
    }
    PK11_DestroyTokenObject(symKey->slot, symKey->objectID);
    symKey->objectID = CK_INVALID_HANDLE;
    return SECSuccess;
}

SECItem *
PK11_GetKeyData(PK11SymKey *symKey)
{
    return &symKey->data;
}

/* This symbol is exported for backward compatibility. */
SECItem *
__PK11_GetKeyData(PK11SymKey *symKey)
{
    return PK11_GetKeyData(symKey);
}

/*
 * PKCS #11 key Types with predefined length
 */
unsigned int
pk11_GetPredefinedKeyLength(CK_KEY_TYPE keyType)
{
    int length = 0;
    switch (keyType) {
        case CKK_DES:
            length = 8;
            break;
        case CKK_DES2:
            length = 16;
            break;
        case CKK_DES3:
            length = 24;
            break;
        case CKK_SKIPJACK:
            length = 10;
            break;
        case CKK_BATON:
            length = 20;
            break;
        case CKK_JUNIPER:
            length = 20;
            break;
        default:
            break;
    }
    return length;
}

/* return the keylength if possible.  '0' if not */
unsigned int
PK11_GetKeyLength(PK11SymKey *key)
{
    CK_KEY_TYPE keyType;

    if (key->size != 0)
        return key->size;

    /* First try to figure out the key length from its type */
    keyType = PK11_ReadULongAttribute(key->slot, key->objectID, CKA_KEY_TYPE);
    key->size = pk11_GetPredefinedKeyLength(keyType);
    if ((keyType == CKK_GENERIC_SECRET) &&
        (key->type == CKM_SSL3_PRE_MASTER_KEY_GEN)) {
        key->size = 48;
    }

    if (key->size != 0)
        return key->size;

    if (key->data.data == NULL) {
        PK11_ExtractKeyValue(key);
    }
    /* key is probably secret. Look up its length */
    /*  this is new PKCS #11 version 2.0 functionality. */
    if (key->size == 0) {
        CK_ULONG keyLength;

        keyLength = PK11_ReadULongAttribute(key->slot, key->objectID, CKA_VALUE_LEN);
        if (keyLength != CK_UNAVAILABLE_INFORMATION) {
            key->size = (unsigned int)keyLength;
        }
    }

    return key->size;
}

/* return the strength of a key. This is different from length in that
 * 1) it returns the size in bits, and 2) it returns only the secret portions
 * of the key minus any checksums or parity.
 */
unsigned int
PK11_GetKeyStrength(PK11SymKey *key, SECAlgorithmID *algid)
{
    int size = 0;
    CK_MECHANISM_TYPE mechanism = CKM_INVALID_MECHANISM; /* RC2 only */
    SECItem *param = NULL;                               /* RC2 only */
    CK_RC2_CBC_PARAMS *rc2_params = NULL;                /* RC2 ONLY */
    unsigned int effectiveBits = 0;                      /* RC2 ONLY */

    switch (PK11_GetKeyType(key->type, 0)) {
        case CKK_CDMF:
            return 40;
        case CKK_DES:
            return 56;
        case CKK_DES3:
        case CKK_DES2:
            size = PK11_GetKeyLength(key);
            if (size == 16) {
                /* double des */
                return 112; /* 16*7 */
            }
            return 168;
        /*
         * RC2 has is different than other ciphers in that it allows the user
         * to deprecating keysize while still requiring all the bits for the
         * original key. The info
         * on what the effective key strength is in the parameter for the key.
         * In S/MIME this parameter is stored in the DER encoded algid. In Our
         * other uses of RC2, effectiveBits == keyBits, so this code functions
         * correctly without an algid.
         */
        case CKK_RC2:
            /* if no algid was provided, fall through to default */
            if (!algid) {
                break;
            }
            /* verify that the algid is for RC2 */
            mechanism = PK11_AlgtagToMechanism(SECOID_GetAlgorithmTag(algid));
            if ((mechanism != CKM_RC2_CBC) && (mechanism != CKM_RC2_ECB)) {
                break;
            }

            /* now get effective bits from the algorithm ID. */
            param = PK11_ParamFromAlgid(algid);
            /* if we couldn't get memory just use key length */
            if (param == NULL) {
                break;
            }

            rc2_params = (CK_RC2_CBC_PARAMS *)param->data;
            /* paranoia... shouldn't happen */
            PORT_Assert(param->data != NULL);
            if (param->data == NULL) {
                SECITEM_FreeItem(param, PR_TRUE);
                break;
            }
            effectiveBits = (unsigned int)rc2_params->ulEffectiveBits;
            SECITEM_FreeItem(param, PR_TRUE);
            param = NULL;
            rc2_params = NULL; /* paranoia */

            /* we have effective bits, is and allocated memory is free, now
             * we need to return the smaller of effective bits and keysize */
            size = PK11_GetKeyLength(key);
            if ((unsigned int)size * 8 > effectiveBits) {
                return effectiveBits;
            }

            return size * 8; /* the actual key is smaller, the strength can't be
                              * greater than the actual key size */

        default:
            break;
    }
    return PK11_GetKeyLength(key) * 8;
}

/*
 * The next three utilities are to deal with the fact that a given operation
 * may be a multi-slot affair. This creates a new key object that is copied
 * into the new slot.
 */
PK11SymKey *
pk11_CopyToSlotPerm(PK11SlotInfo *slot, CK_MECHANISM_TYPE type,
                    CK_ATTRIBUTE_TYPE operation, CK_FLAGS flags,
                    PRBool isPerm, PK11SymKey *symKey)
{
    SECStatus rv;
    PK11SymKey *newKey = NULL;

    /* Extract the raw key data if possible */
    if (symKey->data.data == NULL) {
        rv = PK11_ExtractKeyValue(symKey);
        /* KEY is sensitive, we're try key exchanging it. */
        if (rv != SECSuccess) {
            return pk11_KeyExchange(slot, type, operation,
                                    flags, isPerm, symKey);
        }
    }

    newKey = PK11_ImportSymKeyWithFlags(slot, type, symKey->origin,
                                        operation, &symKey->data, flags, isPerm, symKey->cx);
    if (newKey == NULL) {
        newKey = pk11_KeyExchange(slot, type, operation, flags, isPerm, symKey);
    }
    return newKey;
}

PK11SymKey *
pk11_CopyToSlot(PK11SlotInfo *slot, CK_MECHANISM_TYPE type,
                CK_ATTRIBUTE_TYPE operation, PK11SymKey *symKey)
{
    return pk11_CopyToSlotPerm(slot, type, operation, 0, PR_FALSE, symKey);
}

/*
 * Make sure the slot we are in is the correct slot for the operation
 * by verifying that it supports all of the specified mechanism types.
 */
PK11SymKey *
pk11_ForceSlotMultiple(PK11SymKey *symKey, CK_MECHANISM_TYPE *type,
                       int mechCount, CK_ATTRIBUTE_TYPE operation)
{
    PK11SlotInfo *slot = symKey->slot;
    PK11SymKey *newKey = NULL;
    PRBool needToCopy = PR_FALSE;
    int i;

    if (slot == NULL) {
        needToCopy = PR_TRUE;
    } else {
        i = 0;
        while ((i < mechCount) && (needToCopy == PR_FALSE)) {
            if (!PK11_DoesMechanism(slot, type[i])) {
                needToCopy = PR_TRUE;
            }
            i++;
        }
    }

    if (needToCopy == PR_TRUE) {
        slot = PK11_GetBestSlotMultiple(type, mechCount, symKey->cx);
        if (slot == NULL) {
            PORT_SetError(SEC_ERROR_NO_MODULE);
            return NULL;
        }
        newKey = pk11_CopyToSlot(slot, type[0], operation, symKey);
        PK11_FreeSlot(slot);
    }
    return newKey;
}

/*
 * Make sure the slot we are in is the correct slot for the operation
 */
PK11SymKey *
pk11_ForceSlot(PK11SymKey *symKey, CK_MECHANISM_TYPE type,
               CK_ATTRIBUTE_TYPE operation)
{
    return pk11_ForceSlotMultiple(symKey, &type, 1, operation);
}

PK11SymKey *
PK11_MoveSymKey(PK11SlotInfo *slot, CK_ATTRIBUTE_TYPE operation,
                CK_FLAGS flags, PRBool perm, PK11SymKey *symKey)
{
    if (symKey->slot == slot) {
        if (perm) {
            return PK11_ConvertSessionSymKeyToTokenSymKey(symKey, symKey->cx);
        } else {
            return PK11_ReferenceSymKey(symKey);
        }
    }

    return pk11_CopyToSlotPerm(slot, symKey->type,
                               operation, flags, perm, symKey);
}

/*
 * Use the token to generate a key.
 *
 * keySize must be 'zero' for fixed key length algorithms. A nonzero
 *  keySize causes the CKA_VALUE_LEN attribute to be added to the template
 *  for the key. Most PKCS #11 modules fail if you specify the CKA_VALUE_LEN
 *  attribute for keys with fixed length. The exception is DES2. If you
 *  select a CKM_DES3_CBC mechanism, this code will not add the CKA_VALUE_LEN
 *  parameter and use the key size to determine which underlying DES keygen
 *  function to use (CKM_DES2_KEY_GEN or CKM_DES3_KEY_GEN).
 *
 * keyType must be -1 for most algorithms. Some PBE algorthims cannot
 *  determine the correct key type from the mechanism or the parameters,
 *  so key type must be specified. Other PKCS #11 mechanisms may do so in
 *  the future. Currently there is no need to export this publically.
 *  Keep it private until there is a need in case we need to expand the
 *  keygen parameters again...
 *
 * CK_FLAGS flags: key operation flags
 * PK11AttrFlags attrFlags: PK11_ATTR_XXX key attribute flags
 */
PK11SymKey *
pk11_TokenKeyGenWithFlagsAndKeyType(PK11SlotInfo *slot, CK_MECHANISM_TYPE type,
                                    SECItem *param, CK_KEY_TYPE keyType, int keySize, SECItem *keyid,
                                    CK_FLAGS opFlags, PK11AttrFlags attrFlags, void *wincx)
{
    PK11SymKey *symKey;
    CK_ATTRIBUTE genTemplate[MAX_TEMPL_ATTRS];
    CK_ATTRIBUTE *attrs = genTemplate;
    int count = sizeof(genTemplate) / sizeof(genTemplate[0]);
    CK_MECHANISM_TYPE keyGenType;
    CK_BBOOL cktrue = CK_TRUE;
    CK_BBOOL ckfalse = CK_FALSE;
    CK_ULONG ck_key_size; /* only used for variable-length keys */

    if (pk11_BadAttrFlags(attrFlags)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    if ((keySize != 0) && (type != CKM_DES3_CBC) &&
        (type != CKM_DES3_CBC_PAD) && (type != CKM_DES3_ECB)) {
        ck_key_size = keySize; /* Convert to PK11 type */

        PK11_SETATTRS(attrs, CKA_VALUE_LEN, &ck_key_size, sizeof(ck_key_size));
        attrs++;
    }

    if (keyType != -1) {
        PK11_SETATTRS(attrs, CKA_KEY_TYPE, &keyType, sizeof(CK_KEY_TYPE));
        attrs++;
    }

    /* Include key id value if provided */
    if (keyid) {
        PK11_SETATTRS(attrs, CKA_ID, keyid->data, keyid->len);
        attrs++;
    }

    attrs += pk11_AttrFlagsToAttributes(attrFlags, attrs, &cktrue, &ckfalse);
    attrs += pk11_OpFlagsToAttributes(opFlags, attrs, &cktrue);

    count = attrs - genTemplate;
    PR_ASSERT(count <= sizeof(genTemplate) / sizeof(CK_ATTRIBUTE));

    keyGenType = PK11_GetKeyGenWithSize(type, keySize);
    if (keyGenType == CKM_FAKE_RANDOM) {
        PORT_SetError(SEC_ERROR_NO_MODULE);
        return NULL;
    }
    symKey = PK11_KeyGenWithTemplate(slot, type, keyGenType,
                                     param, genTemplate, count, wincx);
    if (symKey != NULL) {
        symKey->size = keySize;
    }
    return symKey;
}

/*
 * Use the token to generate a key.  - Public
 *
 * keySize must be 'zero' for fixed key length algorithms. A nonzero
 *  keySize causes the CKA_VALUE_LEN attribute to be added to the template
 *  for the key. Most PKCS #11 modules fail if you specify the CKA_VALUE_LEN
 *  attribute for keys with fixed length. The exception is DES2. If you
 *  select a CKM_DES3_CBC mechanism, this code will not add the CKA_VALUE_LEN
 *  parameter and use the key size to determine which underlying DES keygen
 *  function to use (CKM_DES2_KEY_GEN or CKM_DES3_KEY_GEN).
 *
 * CK_FLAGS flags: key operation flags
 * PK11AttrFlags attrFlags: PK11_ATTR_XXX key attribute flags
 */
PK11SymKey *
PK11_TokenKeyGenWithFlags(PK11SlotInfo *slot, CK_MECHANISM_TYPE type,
                          SECItem *param, int keySize, SECItem *keyid, CK_FLAGS opFlags,
                          PK11AttrFlags attrFlags, void *wincx)
{
    return pk11_TokenKeyGenWithFlagsAndKeyType(slot, type, param, -1, keySize,
                                               keyid, opFlags, attrFlags, wincx);
}

/*
 * Use the token to generate a key. keySize must be 'zero' for fixed key
 * length algorithms. A nonzero keySize causes the CKA_VALUE_LEN attribute
 * to be added to the template for the key. PKCS #11 modules fail if you
 * specify the CKA_VALUE_LEN attribute for keys with fixed length.
 * NOTE: this means to generate a DES2 key from this interface you must
 * specify CKM_DES2_KEY_GEN as the mechanism directly; specifying
 * CKM_DES3_CBC as the mechanism and 16 as keySize currently doesn't work.
 */
PK11SymKey *
PK11_TokenKeyGen(PK11SlotInfo *slot, CK_MECHANISM_TYPE type, SECItem *param,
                 int keySize, SECItem *keyid, PRBool isToken, void *wincx)
{
    PK11SymKey *symKey;
    PRBool weird = PR_FALSE; /* hack for fortezza */
    CK_FLAGS opFlags = CKF_SIGN;
    PK11AttrFlags attrFlags = 0;

    if ((keySize == -1) && (type == CKM_SKIPJACK_CBC64)) {
        weird = PR_TRUE;
        keySize = 0;
    }

    opFlags |= weird ? CKF_DECRYPT : CKF_ENCRYPT;

    if (isToken) {
        attrFlags |= (PK11_ATTR_TOKEN | PK11_ATTR_PRIVATE);
    }

    symKey = pk11_TokenKeyGenWithFlagsAndKeyType(slot, type, param,
                                                 -1, keySize, keyid, opFlags, attrFlags, wincx);
    if (symKey && weird) {
        PK11_SetFortezzaHack(symKey);
    }

    return symKey;
}

PK11SymKey *
PK11_KeyGen(PK11SlotInfo *slot, CK_MECHANISM_TYPE type, SECItem *param,
            int keySize, void *wincx)
{
    return PK11_TokenKeyGen(slot, type, param, keySize, 0, PR_FALSE, wincx);
}

PK11SymKey *
PK11_KeyGenWithTemplate(PK11SlotInfo *slot, CK_MECHANISM_TYPE type,
                        CK_MECHANISM_TYPE keyGenType,
                        SECItem *param, CK_ATTRIBUTE *attrs,
                        unsigned int attrsCount, void *wincx)
{
    PK11SymKey *symKey;
    CK_SESSION_HANDLE session;
    CK_MECHANISM mechanism;
    CK_RV crv;
    PRBool isToken = CK_FALSE;
    CK_ULONG keySize = 0;
    unsigned i;

    /* Extract the template's CKA_VALUE_LEN into keySize and CKA_TOKEN into
       isToken. */
    for (i = 0; i < attrsCount; ++i) {
        switch (attrs[i].type) {
            case CKA_VALUE_LEN:
                if (attrs[i].pValue == NULL ||
                    attrs[i].ulValueLen != sizeof(CK_ULONG)) {
                    PORT_SetError(PK11_MapError(CKR_TEMPLATE_INCONSISTENT));
                    return NULL;
                }
                keySize = *(CK_ULONG *)attrs[i].pValue;
                break;
            case CKA_TOKEN:
                if (attrs[i].pValue == NULL ||
                    attrs[i].ulValueLen != sizeof(CK_BBOOL)) {
                    PORT_SetError(PK11_MapError(CKR_TEMPLATE_INCONSISTENT));
                    return NULL;
                }
                isToken = (*(CK_BBOOL *)attrs[i].pValue) ? PR_TRUE : PR_FALSE;
                break;
        }
    }

    /* find a slot to generate the key into */
    /* Only do slot management if this is not a token key */
    if (!isToken && (slot == NULL || !PK11_DoesMechanism(slot, type))) {
        PK11SlotInfo *bestSlot = PK11_GetBestSlot(type, wincx);
        if (bestSlot == NULL) {
            PORT_SetError(SEC_ERROR_NO_MODULE);
            return NULL;
        }
        symKey = pk11_CreateSymKey(bestSlot, type, !isToken, PR_TRUE, wincx);
        PK11_FreeSlot(bestSlot);
    } else {
        symKey = pk11_CreateSymKey(slot, type, !isToken, PR_TRUE, wincx);
    }
    if (symKey == NULL)
        return NULL;

    symKey->size = keySize;
    symKey->origin = PK11_OriginGenerated;

    /* Set the parameters for the key gen if provided */
    mechanism.mechanism = keyGenType;
    mechanism.pParameter = NULL;
    mechanism.ulParameterLen = 0;
    if (param) {
        mechanism.pParameter = param->data;
        mechanism.ulParameterLen = param->len;
    }

    /* Get session and perform locking */
    if (isToken) {
        PK11_Authenticate(symKey->slot, PR_TRUE, wincx);
        /* Should always be original slot */
        session = PK11_GetRWSession(symKey->slot);
        symKey->owner = PR_FALSE;
    } else {
        session = symKey->session;
        if (session != CK_INVALID_HANDLE)
            pk11_EnterKeyMonitor(symKey);
    }
    if (session == CK_INVALID_HANDLE) {
        PK11_FreeSymKey(symKey);
        PORT_SetError(SEC_ERROR_BAD_DATA);
        return NULL;
    }

    crv = PK11_GETTAB(symKey->slot)->C_GenerateKey(session, &mechanism, attrs, attrsCount, &symKey->objectID);

    /* Release lock and session */
    if (isToken) {
        PK11_RestoreROSession(symKey->slot, session);
    } else {
        pk11_ExitKeyMonitor(symKey);
    }

    if (crv != CKR_OK) {
        PK11_FreeSymKey(symKey);
        PORT_SetError(PK11_MapError(crv));
        return NULL;
    }

    return symKey;
}

/* --- */
PK11SymKey *
PK11_GenDES3TokenKey(PK11SlotInfo *slot, SECItem *keyid, void *cx)
{
    return PK11_TokenKeyGen(slot, CKM_DES3_CBC, 0, 0, keyid, PR_TRUE, cx);
}

PK11SymKey *
PK11_ConvertSessionSymKeyToTokenSymKey(PK11SymKey *symk, void *wincx)
{
    PK11SlotInfo *slot = symk->slot;
    CK_ATTRIBUTE template[1];
    CK_ATTRIBUTE *attrs = template;
    CK_BBOOL cktrue = CK_TRUE;
    CK_RV crv;
    CK_OBJECT_HANDLE newKeyID;
    CK_SESSION_HANDLE rwsession;

    PK11_SETATTRS(attrs, CKA_TOKEN, &cktrue, sizeof(cktrue));
    attrs++;

    PK11_Authenticate(slot, PR_TRUE, wincx);
    rwsession = PK11_GetRWSession(slot);
    if (rwsession == CK_INVALID_HANDLE) {
        PORT_SetError(SEC_ERROR_BAD_DATA);
        return NULL;
    }
    crv = PK11_GETTAB(slot)->C_CopyObject(rwsession, symk->objectID,
                                          template, 1, &newKeyID);
    PK11_RestoreROSession(slot, rwsession);

    if (crv != CKR_OK) {
        PORT_SetError(PK11_MapError(crv));
        return NULL;
    }

    return PK11_SymKeyFromHandle(slot, NULL /*parent*/, symk->origin,
                                 symk->type, newKeyID, PR_FALSE /*owner*/, NULL /*wincx*/);
}

/* This function does a straight public key wrap with the CKM_RSA_PKCS
 * mechanism. */
SECStatus
PK11_PubWrapSymKey(CK_MECHANISM_TYPE type, SECKEYPublicKey *pubKey,
                   PK11SymKey *symKey, SECItem *wrappedKey)
{
    CK_MECHANISM_TYPE inferred = pk11_mapWrapKeyType(pubKey->keyType);
    return PK11_PubWrapSymKeyWithMechanism(pubKey, inferred, NULL, symKey,
                                           wrappedKey);
}

/* This function wraps a symmetric key with a public key, such as with the
 * CKM_RSA_PKCS and CKM_RSA_PKCS_OAEP mechanisms. */
SECStatus
PK11_PubWrapSymKeyWithMechanism(SECKEYPublicKey *pubKey,
                                CK_MECHANISM_TYPE mechType, SECItem *param,
                                PK11SymKey *symKey, SECItem *wrappedKey)
{
    PK11SlotInfo *slot;
    CK_ULONG len = wrappedKey->len;
    PK11SymKey *newKey = NULL;
    CK_OBJECT_HANDLE id;
    CK_MECHANISM mechanism;
    PRBool owner = PR_TRUE;
    CK_SESSION_HANDLE session;
    CK_RV crv;

    if (symKey == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* if this slot doesn't support the mechanism, go to a slot that does */
    newKey = pk11_ForceSlot(symKey, mechType, CKA_ENCRYPT);
    if (newKey != NULL) {
        symKey = newKey;
    }

    if (symKey->slot == NULL) {
        PORT_SetError(SEC_ERROR_NO_MODULE);
        return SECFailure;
    }

    slot = symKey->slot;

    mechanism.mechanism = mechType;
    if (param == NULL) {
        mechanism.pParameter = NULL;
        mechanism.ulParameterLen = 0;
    } else {
        mechanism.pParameter = param->data;
        mechanism.ulParameterLen = param->len;
    }

    id = PK11_ImportPublicKey(slot, pubKey, PR_FALSE);
    if (id == CK_INVALID_HANDLE) {
        if (newKey) {
            PK11_FreeSymKey(newKey);
        }
        return SECFailure; /* Error code has been set. */
    }

    session = pk11_GetNewSession(slot, &owner);
    if (!owner || !(slot->isThreadSafe))
        PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_WrapKey(session, &mechanism,
                                       id, symKey->objectID, wrappedKey->data, &len);
    if (!owner || !(slot->isThreadSafe))
        PK11_ExitSlotMonitor(slot);
    pk11_CloseSession(slot, session, owner);
    if (newKey) {
        PK11_FreeSymKey(newKey);
    }

    if (crv != CKR_OK) {
        PORT_SetError(PK11_MapError(crv));
        return SECFailure;
    }
    wrappedKey->len = len;
    return SECSuccess;
}

/*
 * this little function uses the Encrypt function to wrap a key, just in
 * case we have problems with the wrap implementation for a token.
 */
static SECStatus
pk11_HandWrap(PK11SymKey *wrappingKey, SECItem *param, CK_MECHANISM_TYPE type,
              SECItem *inKey, SECItem *outKey)
{
    PK11SlotInfo *slot;
    CK_ULONG len;
    SECItem *data;
    CK_MECHANISM mech;
    PRBool owner = PR_TRUE;
    CK_SESSION_HANDLE session;
    CK_RV crv;

    slot = wrappingKey->slot;
    /* use NULL IV's for wrapping */
    mech.mechanism = type;
    if (param) {
        mech.pParameter = param->data;
        mech.ulParameterLen = param->len;
    } else {
        mech.pParameter = NULL;
        mech.ulParameterLen = 0;
    }
    session = pk11_GetNewSession(slot, &owner);
    if (!owner || !(slot->isThreadSafe))
        PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_EncryptInit(session, &mech,
                                           wrappingKey->objectID);
    if (crv != CKR_OK) {
        if (!owner || !(slot->isThreadSafe))
            PK11_ExitSlotMonitor(slot);
        pk11_CloseSession(slot, session, owner);
        PORT_SetError(PK11_MapError(crv));
        return SECFailure;
    }

    /* keys are almost always aligned, but if we get this far,
     * we've gone above and beyond anyway... */
    data = PK11_BlockData(inKey, PK11_GetBlockSize(type, param));
    if (data == NULL) {
        if (!owner || !(slot->isThreadSafe))
            PK11_ExitSlotMonitor(slot);
        pk11_CloseSession(slot, session, owner);
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }
    len = outKey->len;
    crv = PK11_GETTAB(slot)->C_Encrypt(session, data->data, data->len,
                                       outKey->data, &len);
    if (!owner || !(slot->isThreadSafe))
        PK11_ExitSlotMonitor(slot);
    pk11_CloseSession(slot, session, owner);
    SECITEM_FreeItem(data, PR_TRUE);
    outKey->len = len;
    if (crv != CKR_OK) {
        PORT_SetError(PK11_MapError(crv));
        return SECFailure;
    }
    return SECSuccess;
}

/*
 * helper function which moves two keys into a new slot based on the
 * desired mechanism.
 */
static SECStatus
pk11_moveTwoKeys(CK_MECHANISM_TYPE mech,
                 CK_ATTRIBUTE_TYPE preferedOperation,
                 CK_ATTRIBUTE_TYPE movingOperation,
                 PK11SymKey *preferedKey, PK11SymKey *movingKey,
                 PK11SymKey **newPreferedKey, PK11SymKey **newMovingKey)
{
    PK11SlotInfo *newSlot;
    *newMovingKey = NULL;
    *newPreferedKey = NULL;

    newSlot = PK11_GetBestSlot(mech, preferedKey->cx);
    if (newSlot == NULL) {
        return SECFailure;
    }
    *newMovingKey = pk11_CopyToSlot(newSlot, movingKey->type,
                                    movingOperation, movingKey);
    if (*newMovingKey == NULL) {
        goto loser;
    }
    *newPreferedKey = pk11_CopyToSlot(newSlot, preferedKey->type,
                                      preferedOperation, preferedKey);
    if (*newPreferedKey == NULL) {
        goto loser;
    }

    PK11_FreeSlot(newSlot);
    return SECSuccess;
loser:
    PK11_FreeSlot(newSlot);
    PK11_FreeSymKey(*newMovingKey);
    PK11_FreeSymKey(*newPreferedKey);
    *newMovingKey = NULL;
    *newPreferedKey = NULL;
    return SECFailure;
}

/*
 * To do joint operations, we often need two keys in the same slot.
 * Usually the PKCS #11 wrappers handle this correctly (like for PK11_WrapKey),
 * but sometimes the wrappers don't know about mechanism specific keys in
 * the Mechanism params. This function makes sure the two keys are in the
 * same slot by copying one or both of the keys into a common slot. This
 * functions makes sure the slot can handle the target mechanism. If the copy
 * is warranted, this function will prefer to move the movingKey first, then
 * the preferedKey. If the keys are moved, the new keys are returned in
 * newMovingKey and/or newPreferedKey. The application is responsible
 * for freeing those keys once the operation is complete.
 */
SECStatus
PK11_SymKeysToSameSlot(CK_MECHANISM_TYPE mech,
                       CK_ATTRIBUTE_TYPE preferedOperation,
                       CK_ATTRIBUTE_TYPE movingOperation,
                       PK11SymKey *preferedKey, PK11SymKey *movingKey,
                       PK11SymKey **newPreferedKey, PK11SymKey **newMovingKey)
{
    /* usually don't return new keys */
    *newMovingKey = NULL;
    *newPreferedKey = NULL;
    if (movingKey->slot == preferedKey->slot) {

        /* this should be the most common case */
        if ((preferedKey->slot != NULL) &&
            PK11_DoesMechanism(preferedKey->slot, mech)) {
            return SECSuccess;
        }

        /* we are in the same slot, but it doesn't do the operation,
         * move both keys to an appropriate target slot */
        return pk11_moveTwoKeys(mech, preferedOperation, movingOperation,
                                preferedKey, movingKey,
                                newPreferedKey, newMovingKey);
    }

    /* keys are in different slot, try moving the moving key to the prefered
     * key's slot */
    if ((preferedKey->slot != NULL) &&
        PK11_DoesMechanism(preferedKey->slot, mech)) {
        *newMovingKey = pk11_CopyToSlot(preferedKey->slot, movingKey->type,
                                        movingOperation, movingKey);
        if (*newMovingKey != NULL) {
            return SECSuccess;
        }
    }
    /* couldn't moving the moving key to the prefered slot, try moving
     * the prefered key */
    if ((movingKey->slot != NULL) &&
        PK11_DoesMechanism(movingKey->slot, mech)) {
        *newPreferedKey = pk11_CopyToSlot(movingKey->slot, preferedKey->type,
                                          preferedOperation, preferedKey);
        if (*newPreferedKey != NULL) {
            return SECSuccess;
        }
    }
    /* Neither succeeded, but that could be that they were not in slots that
     * supported the operation, try moving both keys into a common slot that
     * can do the operation. */
    return pk11_moveTwoKeys(mech, preferedOperation, movingOperation,
                            preferedKey, movingKey,
                            newPreferedKey, newMovingKey);
}

/*
 * This function does a symetric based wrap.
 */
SECStatus
PK11_WrapSymKey(CK_MECHANISM_TYPE type, SECItem *param,
                PK11SymKey *wrappingKey, PK11SymKey *symKey,
                SECItem *wrappedKey)
{
    PK11SlotInfo *slot;
    CK_ULONG len = wrappedKey->len;
    PK11SymKey *newSymKey = NULL;
    PK11SymKey *newWrappingKey = NULL;
    SECItem *param_save = NULL;
    CK_MECHANISM mechanism;
    PRBool owner = PR_TRUE;
    CK_SESSION_HANDLE session;
    CK_RV crv;
    SECStatus rv;

    /* force the keys into same slot */
    rv = PK11_SymKeysToSameSlot(type, CKA_ENCRYPT, CKA_WRAP,
                                symKey, wrappingKey,
                                &newSymKey, &newWrappingKey);
    if (rv != SECSuccess) {
        /* Couldn't move the keys as desired, try to hand unwrap if possible */
        if (symKey->data.data == NULL) {
            rv = PK11_ExtractKeyValue(symKey);
            if (rv != SECSuccess) {
                PORT_SetError(SEC_ERROR_NO_MODULE);
                return SECFailure;
            }
        }
        if (param == NULL) {
            param_save = param = PK11_ParamFromIV(type, NULL);
        }
        rv = pk11_HandWrap(wrappingKey, param, type, &symKey->data, wrappedKey);
        if (param_save)
            SECITEM_FreeItem(param_save, PR_TRUE);
        return rv;
    }
    if (newSymKey) {
        symKey = newSymKey;
    }
    if (newWrappingKey) {
        wrappingKey = newWrappingKey;
    }

    /* at this point both keys are in the same token */
    slot = wrappingKey->slot;
    mechanism.mechanism = type;
    /* use NULL IV's for wrapping */
    if (param == NULL) {
        param_save = param = PK11_ParamFromIV(type, NULL);
    }
    if (param) {
        mechanism.pParameter = param->data;
        mechanism.ulParameterLen = param->len;
    } else {
        mechanism.pParameter = NULL;
        mechanism.ulParameterLen = 0;
    }

    len = wrappedKey->len;

    session = pk11_GetNewSession(slot, &owner);
    if (!owner || !(slot->isThreadSafe))
        PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_WrapKey(session, &mechanism,
                                       wrappingKey->objectID, symKey->objectID,
                                       wrappedKey->data, &len);
    if (!owner || !(slot->isThreadSafe))
        PK11_ExitSlotMonitor(slot);
    pk11_CloseSession(slot, session, owner);
    rv = SECSuccess;
    if (crv != CKR_OK) {
        /* can't wrap it? try hand wrapping it... */
        do {
            if (symKey->data.data == NULL) {
                rv = PK11_ExtractKeyValue(symKey);
                if (rv != SECSuccess)
                    break;
            }
            rv = pk11_HandWrap(wrappingKey, param, type, &symKey->data,
                               wrappedKey);
        } while (PR_FALSE);
    } else {
        wrappedKey->len = len;
    }
    PK11_FreeSymKey(newSymKey);
    PK11_FreeSymKey(newWrappingKey);
    if (param_save)
        SECITEM_FreeItem(param_save, PR_TRUE);
    return rv;
}

/*
 * This Generates a new key based on a symetricKey
 */
PK11SymKey *
PK11_Derive(PK11SymKey *baseKey, CK_MECHANISM_TYPE derive, SECItem *param,
            CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation,
            int keySize)
{
    return PK11_DeriveWithTemplate(baseKey, derive, param, target, operation,
                                   keySize, NULL, 0, PR_FALSE);
}

PK11SymKey *
PK11_DeriveWithFlags(PK11SymKey *baseKey, CK_MECHANISM_TYPE derive,
                     SECItem *param, CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation,
                     int keySize, CK_FLAGS flags)
{
    CK_BBOOL ckTrue = CK_TRUE;
    CK_ATTRIBUTE keyTemplate[MAX_TEMPL_ATTRS];
    unsigned int templateCount;

    templateCount = pk11_OpFlagsToAttributes(flags, keyTemplate, &ckTrue);
    return PK11_DeriveWithTemplate(baseKey, derive, param, target, operation,
                                   keySize, keyTemplate, templateCount, PR_FALSE);
}

PK11SymKey *
PK11_DeriveWithFlagsPerm(PK11SymKey *baseKey, CK_MECHANISM_TYPE derive,
                         SECItem *param, CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation,
                         int keySize, CK_FLAGS flags, PRBool isPerm)
{
    CK_BBOOL cktrue = CK_TRUE;
    CK_ATTRIBUTE keyTemplate[MAX_TEMPL_ATTRS];
    CK_ATTRIBUTE *attrs;
    unsigned int templateCount = 0;

    attrs = keyTemplate;
    if (isPerm) {
        PK11_SETATTRS(attrs, CKA_TOKEN, &cktrue, sizeof(CK_BBOOL));
        attrs++;
    }
    templateCount = attrs - keyTemplate;
    templateCount += pk11_OpFlagsToAttributes(flags, attrs, &cktrue);
    return PK11_DeriveWithTemplate(baseKey, derive, param, target, operation,
                                   keySize, keyTemplate, templateCount, isPerm);
}

PK11SymKey *
PK11_DeriveWithTemplate(PK11SymKey *baseKey, CK_MECHANISM_TYPE derive,
                        SECItem *param, CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation,
                        int keySize, CK_ATTRIBUTE *userAttr, unsigned int numAttrs,
                        PRBool isPerm)
{
    PK11SlotInfo *slot = baseKey->slot;
    PK11SymKey *symKey;
    PK11SymKey *newBaseKey = NULL;
    CK_BBOOL cktrue = CK_TRUE;
    CK_OBJECT_CLASS keyClass = CKO_SECRET_KEY;
    CK_KEY_TYPE keyType = CKK_GENERIC_SECRET;
    CK_ULONG valueLen = 0;
    CK_MECHANISM mechanism;
    CK_RV crv;
#define MAX_ADD_ATTRS 4
    CK_ATTRIBUTE keyTemplate[MAX_TEMPL_ATTRS + MAX_ADD_ATTRS];
#undef MAX_ADD_ATTRS
    CK_ATTRIBUTE *attrs = keyTemplate;
    CK_SESSION_HANDLE session;
    unsigned int templateCount;

    if (numAttrs > MAX_TEMPL_ATTRS) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }
    /* CKA_NSS_MESSAGE is a fake operation to distinguish between
     * Normal Encrypt/Decrypt and MessageEncrypt/Decrypt. Don't try to set
     * it as a real attribute */
    if ((operation & CKA_NSS_MESSAGE_MASK) == CKA_NSS_MESSAGE) {
        /* Message is or'd with a real Attribute (CKA_ENCRYPT, CKA_DECRYPT),
         * etc. Strip out the real attribute here */
        operation &= ~CKA_NSS_MESSAGE_MASK;
    }

    /* first copy caller attributes in. */
    for (templateCount = 0; templateCount < numAttrs; ++templateCount) {
        *attrs++ = *userAttr++;
    }

    /* We only add the following attributes to the template if the caller
    ** didn't already supply them.
    */
    if (!pk11_FindAttrInTemplate(keyTemplate, numAttrs, CKA_CLASS)) {
        PK11_SETATTRS(attrs, CKA_CLASS, &keyClass, sizeof keyClass);
        attrs++;
    }
    if (!pk11_FindAttrInTemplate(keyTemplate, numAttrs, CKA_KEY_TYPE)) {
        keyType = PK11_GetKeyType(target, keySize);
        PK11_SETATTRS(attrs, CKA_KEY_TYPE, &keyType, sizeof keyType);
        attrs++;
    }
    if (keySize > 0 &&
        !pk11_FindAttrInTemplate(keyTemplate, numAttrs, CKA_VALUE_LEN)) {
        valueLen = (CK_ULONG)keySize;
        PK11_SETATTRS(attrs, CKA_VALUE_LEN, &valueLen, sizeof valueLen);
        attrs++;
    }
    if ((operation != CKA_FLAGS_ONLY) &&
        !pk11_FindAttrInTemplate(keyTemplate, numAttrs, operation)) {
        PK11_SETATTRS(attrs, operation, &cktrue, sizeof cktrue);
        attrs++;
    }

    templateCount = attrs - keyTemplate;
    PR_ASSERT(templateCount <= sizeof(keyTemplate) / sizeof(CK_ATTRIBUTE));

    /* move the key to a slot that can do the function */
    if (!PK11_DoesMechanism(slot, derive)) {
        /* get a new base key & slot */
        PK11SlotInfo *newSlot = PK11_GetBestSlot(derive, baseKey->cx);

        if (newSlot == NULL)
            return NULL;

        newBaseKey = pk11_CopyToSlot(newSlot, derive, CKA_DERIVE,
                                     baseKey);
        PK11_FreeSlot(newSlot);
        if (newBaseKey == NULL)
            return NULL;
        baseKey = newBaseKey;
        slot = baseKey->slot;
    }

    /* get our key Structure */
    symKey = pk11_CreateSymKey(slot, target, !isPerm, PR_TRUE, baseKey->cx);
    if (symKey == NULL) {
        return NULL;
    }

    symKey->size = keySize;

    mechanism.mechanism = derive;
    if (param) {
        mechanism.pParameter = param->data;
        mechanism.ulParameterLen = param->len;
    } else {
        mechanism.pParameter = NULL;
        mechanism.ulParameterLen = 0;
    }
    symKey->origin = PK11_OriginDerive;

    if (isPerm) {
        session = PK11_GetRWSession(slot);
    } else {
        pk11_EnterKeyMonitor(symKey);
        session = symKey->session;
    }
    if (session == CK_INVALID_HANDLE) {
        if (!isPerm)
            pk11_ExitKeyMonitor(symKey);
        crv = CKR_SESSION_HANDLE_INVALID;
    } else {
        crv = PK11_GETTAB(slot)->C_DeriveKey(session, &mechanism,
                                             baseKey->objectID, keyTemplate, templateCount, &symKey->objectID);
        if (isPerm) {
            PK11_RestoreROSession(slot, session);
        } else {
            pk11_ExitKeyMonitor(symKey);
        }
    }
    if (newBaseKey)
        PK11_FreeSymKey(newBaseKey);
    if (crv != CKR_OK) {
        PK11_FreeSymKey(symKey);
        PORT_SetError(PK11_MapError(crv));
        return NULL;
    }
    return symKey;
}

/* Create a new key by concatenating base and data
 */
static PK11SymKey *
pk11_ConcatenateBaseAndData(PK11SymKey *base,
                            CK_BYTE *data, CK_ULONG dataLen, CK_MECHANISM_TYPE target,
                            CK_ATTRIBUTE_TYPE operation)
{
    CK_KEY_DERIVATION_STRING_DATA mechParams;
    SECItem param;

    if (base == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    mechParams.pData = data;
    mechParams.ulLen = dataLen;
    param.data = (unsigned char *)&mechParams;
    param.len = sizeof(CK_KEY_DERIVATION_STRING_DATA);

    return PK11_Derive(base, CKM_CONCATENATE_BASE_AND_DATA,
                       &param, target, operation, 0);
}

/* Create a new key by concatenating base and key
 */
static PK11SymKey *
pk11_ConcatenateBaseAndKey(PK11SymKey *base,
                           PK11SymKey *key, CK_MECHANISM_TYPE target,
                           CK_ATTRIBUTE_TYPE operation, CK_ULONG keySize)
{
    SECItem param;

    if ((base == NULL) || (key == NULL)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    param.data = (unsigned char *)&(key->objectID);
    param.len = sizeof(CK_OBJECT_HANDLE);

    return PK11_Derive(base, CKM_CONCATENATE_BASE_AND_KEY,
                       &param, target, operation, keySize);
}

/* Create a new key whose value is the hash of tobehashed.
 * type is the mechanism for the derived key.
 */
static PK11SymKey *
pk11_HashKeyDerivation(PK11SymKey *toBeHashed,
                       CK_MECHANISM_TYPE hashMechanism, CK_MECHANISM_TYPE target,
                       CK_ATTRIBUTE_TYPE operation, CK_ULONG keySize)
{
    return PK11_Derive(toBeHashed, hashMechanism, NULL, target, operation, keySize);
}

/* This function implements the ANSI X9.63 key derivation function
 */
static PK11SymKey *
pk11_ANSIX963Derive(PK11SymKey *sharedSecret,
                    CK_EC_KDF_TYPE kdf, SECItem *sharedData,
                    CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation,
                    CK_ULONG keySize)
{
    CK_KEY_TYPE keyType;
    CK_MECHANISM_TYPE hashMechanism, mechanismArray[4];
    CK_ULONG derivedKeySize, HashLen, counter, maxCounter, bufferLen;
    CK_ULONG SharedInfoLen;
    CK_BYTE *buffer = NULL;
    PK11SymKey *toBeHashed, *hashOutput;
    PK11SymKey *newSharedSecret = NULL;
    PK11SymKey *oldIntermediateResult, *intermediateResult = NULL;

    if (sharedSecret == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    switch (kdf) {
        case CKD_SHA1_KDF:
            HashLen = SHA1_LENGTH;
            hashMechanism = CKM_SHA1_KEY_DERIVATION;
            break;
        case CKD_SHA224_KDF:
            HashLen = SHA224_LENGTH;
            hashMechanism = CKM_SHA224_KEY_DERIVATION;
            break;
        case CKD_SHA256_KDF:
            HashLen = SHA256_LENGTH;
            hashMechanism = CKM_SHA256_KEY_DERIVATION;
            break;
        case CKD_SHA384_KDF:
            HashLen = SHA384_LENGTH;
            hashMechanism = CKM_SHA384_KEY_DERIVATION;
            break;
        case CKD_SHA512_KDF:
            HashLen = SHA512_LENGTH;
            hashMechanism = CKM_SHA512_KEY_DERIVATION;
            break;
        default:
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return NULL;
    }

    derivedKeySize = keySize;
    if (derivedKeySize == 0) {
        keyType = PK11_GetKeyType(target, keySize);
        derivedKeySize = pk11_GetPredefinedKeyLength(keyType);
        if (derivedKeySize == 0) {
            derivedKeySize = HashLen;
        }
    }

    /* Check that key_len isn't too long.  The maximum key length could be
     * greatly increased if the code below did not limit the 4-byte counter
     * to a maximum value of 255. */
    if (derivedKeySize > 254 * HashLen) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    maxCounter = derivedKeySize / HashLen;
    if (derivedKeySize > maxCounter * HashLen)
        maxCounter++;

    if ((sharedData == NULL) || (sharedData->data == NULL))
        SharedInfoLen = 0;
    else
        SharedInfoLen = sharedData->len;

    bufferLen = SharedInfoLen + 4;

    /* Populate buffer with Counter || sharedData
     * where Counter is 0x00000001. */
    buffer = (unsigned char *)PORT_Alloc(bufferLen);
    if (buffer == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return NULL;
    }

    buffer[0] = 0;
    buffer[1] = 0;
    buffer[2] = 0;
    buffer[3] = 1;
    if (SharedInfoLen > 0) {
        PORT_Memcpy(&buffer[4], sharedData->data, SharedInfoLen);
    }

    /* Look for a slot that supports the mechanisms needed
     * to implement the ANSI X9.63 KDF as well as the
     * target mechanism.
     */
    mechanismArray[0] = CKM_CONCATENATE_BASE_AND_DATA;
    mechanismArray[1] = hashMechanism;
    mechanismArray[2] = CKM_CONCATENATE_BASE_AND_KEY;
    mechanismArray[3] = target;

    newSharedSecret = pk11_ForceSlotMultiple(sharedSecret,
                                             mechanismArray, 4, operation);
    if (newSharedSecret != NULL) {
        sharedSecret = newSharedSecret;
    }

    for (counter = 1; counter <= maxCounter; counter++) {
        /* Concatenate shared_secret and buffer */
        toBeHashed = pk11_ConcatenateBaseAndData(sharedSecret, buffer,
                                                 bufferLen, hashMechanism, operation);
        if (toBeHashed == NULL) {
            goto loser;
        }

        /* Hash value */
        if (maxCounter == 1) {
            /* In this case the length of the key to be derived is
             * less than or equal to the length of the hash output.
             * So, the output of the hash operation will be the
             * dervied key. */
            hashOutput = pk11_HashKeyDerivation(toBeHashed, hashMechanism,
                                                target, operation, keySize);
        } else {
            /* In this case, the output of the hash operation will be
             * concatenated with other data to create the derived key. */
            hashOutput = pk11_HashKeyDerivation(toBeHashed, hashMechanism,
                                                CKM_CONCATENATE_BASE_AND_KEY, operation, 0);
        }
        PK11_FreeSymKey(toBeHashed);
        if (hashOutput == NULL) {
            goto loser;
        }

        /* Append result to intermediate result, if necessary */
        oldIntermediateResult = intermediateResult;

        if (oldIntermediateResult == NULL) {
            intermediateResult = hashOutput;
        } else {
            if (counter == maxCounter) {
                /* This is the final concatenation, and so the output
                 * will be the derived key. */
                intermediateResult =
                    pk11_ConcatenateBaseAndKey(oldIntermediateResult,
                                               hashOutput, target, operation, keySize);
            } else {
                /* The output of this concatenation will be concatenated
                 * with other data to create the derived key. */
                intermediateResult =
                    pk11_ConcatenateBaseAndKey(oldIntermediateResult,
                                               hashOutput, CKM_CONCATENATE_BASE_AND_KEY,
                                               operation, 0);
            }

            PK11_FreeSymKey(hashOutput);
            PK11_FreeSymKey(oldIntermediateResult);
            if (intermediateResult == NULL) {
                goto loser;
            }
        }

        /* Increment counter (assumes maxCounter < 255) */
        buffer[3]++;
    }

    PORT_ZFree(buffer, bufferLen);
    if (newSharedSecret != NULL)
        PK11_FreeSymKey(newSharedSecret);
    return intermediateResult;

loser:
    PORT_ZFree(buffer, bufferLen);
    if (newSharedSecret != NULL)
        PK11_FreeSymKey(newSharedSecret);
    if (intermediateResult != NULL)
        PK11_FreeSymKey(intermediateResult);
    return NULL;
}

/*
 * This regenerate a public key from a private key. This function is currently
 * NSS private. If we want to make it public, we need to add and optional
 * template or at least flags (a.la. PK11_DeriveWithFlags).
 */
CK_OBJECT_HANDLE
PK11_DerivePubKeyFromPrivKey(SECKEYPrivateKey *privKey)
{
    PK11SlotInfo *slot = privKey->pkcs11Slot;
    CK_MECHANISM mechanism;
    CK_OBJECT_HANDLE objectID = CK_INVALID_HANDLE;
    CK_RV crv;

    mechanism.mechanism = CKM_NSS_PUB_FROM_PRIV;
    mechanism.pParameter = NULL;
    mechanism.ulParameterLen = 0;

    PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_DeriveKey(slot->session, &mechanism,
                                         privKey->pkcs11ID, NULL, 0,
                                         &objectID);
    PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) {
        PORT_SetError(PK11_MapError(crv));
        return CK_INVALID_HANDLE;
    }
    return objectID;
}

/*
 * This Generates a wrapping key based on a privateKey, publicKey, and two
 * random numbers. For Mail usage RandomB should be NULL. In the Sender's
 * case RandomA is generate, outherwize it is passed.
 */
PK11SymKey *
PK11_PubDerive(SECKEYPrivateKey *privKey, SECKEYPublicKey *pubKey,
               PRBool isSender, SECItem *randomA, SECItem *randomB,
               CK_MECHANISM_TYPE derive, CK_MECHANISM_TYPE target,
               CK_ATTRIBUTE_TYPE operation, int keySize, void *wincx)
{
    PK11SlotInfo *slot = privKey->pkcs11Slot;
    CK_MECHANISM mechanism;
    PK11SymKey *symKey;
    CK_RV crv;

    /* get our key Structure */
    symKey = pk11_CreateSymKey(slot, target, PR_TRUE, PR_TRUE, wincx);
    if (symKey == NULL) {
        return NULL;
    }

    /* CKA_NSS_MESSAGE is a fake operation to distinguish between
     * Normal Encrypt/Decrypt and MessageEncrypt/Decrypt. Don't try to set
     * it as a real attribute */
    if ((operation & CKA_NSS_MESSAGE_MASK) == CKA_NSS_MESSAGE) {
        /* Message is or'd with a real Attribute (CKA_ENCRYPT, CKA_DECRYPT),
         * etc. Strip out the real attribute here */
        operation &= ~CKA_NSS_MESSAGE_MASK;
    }

    symKey->origin = PK11_OriginDerive;

    switch (privKey->keyType) {
        case rsaKey:
        case rsaPssKey:
        case rsaOaepKey:
        case nullKey:
            PORT_SetError(SEC_ERROR_BAD_KEY);
            break;
        case dsaKey:
        case keaKey:
        case fortezzaKey: {
            static unsigned char rb_email[128] = { 0 };
            CK_KEA_DERIVE_PARAMS param;
            param.isSender = (CK_BBOOL)isSender;
            param.ulRandomLen = randomA->len;
            param.pRandomA = randomA->data;
            param.pRandomB = rb_email;
            param.pRandomB[127] = 1;
            if (randomB)
                param.pRandomB = randomB->data;
            if (pubKey->keyType == fortezzaKey) {
                param.ulPublicDataLen = pubKey->u.fortezza.KEAKey.len;
                param.pPublicData = pubKey->u.fortezza.KEAKey.data;
            } else {
                /* assert type == keaKey */
                /* XXX change to match key key types */
                param.ulPublicDataLen = pubKey->u.fortezza.KEAKey.len;
                param.pPublicData = pubKey->u.fortezza.KEAKey.data;
            }

            mechanism.mechanism = derive;
            mechanism.pParameter = &param;
            mechanism.ulParameterLen = sizeof(param);

            /* get a new symKey structure */
            pk11_EnterKeyMonitor(symKey);
            crv = PK11_GETTAB(slot)->C_DeriveKey(symKey->session, &mechanism,
                                                 privKey->pkcs11ID, NULL, 0,
                                                 &symKey->objectID);
            pk11_ExitKeyMonitor(symKey);
            if (crv == CKR_OK)
                return symKey;
            PORT_SetError(PK11_MapError(crv));
        } break;
        case dhKey: {
            CK_BBOOL cktrue = CK_TRUE;
            CK_OBJECT_CLASS keyClass = CKO_SECRET_KEY;
            CK_KEY_TYPE keyType = CKK_GENERIC_SECRET;
            CK_ULONG key_size = 0;
            CK_ATTRIBUTE keyTemplate[4];
            int templateCount;
            CK_ATTRIBUTE *attrs = keyTemplate;

            if (pubKey->keyType != dhKey) {
                PORT_SetError(SEC_ERROR_BAD_KEY);
                break;
            }

            PK11_SETATTRS(attrs, CKA_CLASS, &keyClass, sizeof(keyClass));
            attrs++;
            PK11_SETATTRS(attrs, CKA_KEY_TYPE, &keyType, sizeof(keyType));
            attrs++;
            PK11_SETATTRS(attrs, operation, &cktrue, 1);
            attrs++;
            PK11_SETATTRS(attrs, CKA_VALUE_LEN, &key_size, sizeof(key_size));
            attrs++;
            templateCount = attrs - keyTemplate;
            PR_ASSERT(templateCount <= sizeof(keyTemplate) / sizeof(CK_ATTRIBUTE));

            keyType = PK11_GetKeyType(target, keySize);
            key_size = keySize;
            symKey->size = keySize;
            if (key_size == 0)
                templateCount--;

            mechanism.mechanism = derive;

            /* we can undefine these when we define diffie-helman keys */

            mechanism.pParameter = pubKey->u.dh.publicValue.data;
            mechanism.ulParameterLen = pubKey->u.dh.publicValue.len;

            pk11_EnterKeyMonitor(symKey);
            crv = PK11_GETTAB(slot)->C_DeriveKey(symKey->session, &mechanism,
                                                 privKey->pkcs11ID, keyTemplate,
                                                 templateCount, &symKey->objectID);
            pk11_ExitKeyMonitor(symKey);
            if (crv == CKR_OK)
                return symKey;
            PORT_SetError(PK11_MapError(crv));
        } break;
        case ecKey: {
            CK_BBOOL cktrue = CK_TRUE;
            CK_OBJECT_CLASS keyClass = CKO_SECRET_KEY;
            CK_KEY_TYPE keyType = CKK_GENERIC_SECRET;
            CK_ULONG key_size = 0;
            CK_ATTRIBUTE keyTemplate[4];
            int templateCount;
            CK_ATTRIBUTE *attrs = keyTemplate;
            CK_ECDH1_DERIVE_PARAMS *mechParams = NULL;

            if (pubKey->keyType != ecKey) {
                PORT_SetError(SEC_ERROR_BAD_KEY);
                break;
            }

            PK11_SETATTRS(attrs, CKA_CLASS, &keyClass, sizeof(keyClass));
            attrs++;
            PK11_SETATTRS(attrs, CKA_KEY_TYPE, &keyType, sizeof(keyType));
            attrs++;
            PK11_SETATTRS(attrs, operation, &cktrue, 1);
            attrs++;
            PK11_SETATTRS(attrs, CKA_VALUE_LEN, &key_size, sizeof(key_size));
            attrs++;
            templateCount = attrs - keyTemplate;
            PR_ASSERT(templateCount <= sizeof(keyTemplate) / sizeof(CK_ATTRIBUTE));

            keyType = PK11_GetKeyType(target, keySize);
            key_size = keySize;
            if (key_size == 0) {
                if ((key_size = pk11_GetPredefinedKeyLength(keyType))) {
                    templateCount--;
                } else {
                    /* sigh, some tokens can't figure this out and require
                     * CKA_VALUE_LEN to be set */
                    key_size = SHA1_LENGTH;
                }
            }
            symKey->size = key_size;

            mechParams = PORT_ZNew(CK_ECDH1_DERIVE_PARAMS);
            mechParams->kdf = CKD_SHA1_KDF;
            mechParams->ulSharedDataLen = 0;
            mechParams->pSharedData = NULL;
            mechParams->ulPublicDataLen = pubKey->u.ec.publicValue.len;
            mechParams->pPublicData = pubKey->u.ec.publicValue.data;

            mechanism.mechanism = derive;
            mechanism.pParameter = mechParams;
            mechanism.ulParameterLen = sizeof(CK_ECDH1_DERIVE_PARAMS);

            pk11_EnterKeyMonitor(symKey);
            crv = PK11_GETTAB(slot)->C_DeriveKey(symKey->session,
                                                 &mechanism, privKey->pkcs11ID, keyTemplate,
                                                 templateCount, &symKey->objectID);
            pk11_ExitKeyMonitor(symKey);

            /* old PKCS #11 spec was ambiguous on what needed to be passed,
             * try this again with and encoded public key */
            if (crv != CKR_OK && pk11_ECGetPubkeyEncoding(pubKey) != ECPoint_XOnly) {
                SECItem *pubValue = SEC_ASN1EncodeItem(NULL, NULL,
                                                       &pubKey->u.ec.publicValue,
                                                       SEC_ASN1_GET(SEC_OctetStringTemplate));
                if (pubValue == NULL) {
                    PORT_ZFree(mechParams, sizeof(CK_ECDH1_DERIVE_PARAMS));
                    break;
                }
                mechParams->ulPublicDataLen = pubValue->len;
                mechParams->pPublicData = pubValue->data;

                pk11_EnterKeyMonitor(symKey);
                crv = PK11_GETTAB(slot)->C_DeriveKey(symKey->session,
                                                     &mechanism, privKey->pkcs11ID, keyTemplate,
                                                     templateCount, &symKey->objectID);
                pk11_ExitKeyMonitor(symKey);

                SECITEM_FreeItem(pubValue, PR_TRUE);
            }

            PORT_ZFree(mechParams, sizeof(CK_ECDH1_DERIVE_PARAMS));

            if (crv == CKR_OK)
                return symKey;
            PORT_SetError(PK11_MapError(crv));
        }
    }

    PK11_FreeSymKey(symKey);
    return NULL;
}

/* Test for curves that are known to use a special encoding.
 * Extend this function when additional curves are added. */
static ECPointEncoding
pk11_ECGetPubkeyEncoding(const SECKEYPublicKey *pubKey)
{
    SECItem oid;
    SECStatus rv;
    PORTCheapArenaPool tmpArena;
    ECPointEncoding encoding = ECPoint_Undefined;

    PORT_InitCheapArena(&tmpArena, DER_DEFAULT_CHUNKSIZE);

    /* decode the OID tag */
    rv = SEC_QuickDERDecodeItem(&tmpArena.arena, &oid,
                                SEC_ASN1_GET(SEC_ObjectIDTemplate),
                                &pubKey->u.ec.DEREncodedParams);
    if (rv == SECSuccess) {
        SECOidTag tag = SECOID_FindOIDTag(&oid);
        switch (tag) {
            case SEC_OID_CURVE25519:
                encoding = ECPoint_XOnly;
                break;
            case SEC_OID_SECG_EC_SECP256R1:
            case SEC_OID_SECG_EC_SECP384R1:
            case SEC_OID_SECG_EC_SECP521R1:
            default:
                /* unknown curve, default to uncompressed */
                encoding = ECPoint_Uncompressed;
        }
    }
    PORT_DestroyCheapArena(&tmpArena);
    return encoding;
}

/* Returns the size of the public key, or 0 if there
 * is an error. */
static CK_ULONG
pk11_ECPubKeySize(SECKEYPublicKey *pubKey)
{
    SECItem *publicValue = &pubKey->u.ec.publicValue;

    ECPointEncoding encoding = pk11_ECGetPubkeyEncoding(pubKey);
    if (encoding == ECPoint_XOnly) {
        return publicValue->len;
    }
    if (encoding == ECPoint_Uncompressed) {
        /* key encoded in uncompressed form */
        return ((publicValue->len - 1) / 2);
    }
    /* key encoding not recognized */
    return 0;
}

static PK11SymKey *
pk11_PubDeriveECKeyWithKDF(
    SECKEYPrivateKey *privKey, SECKEYPublicKey *pubKey,
    PRBool isSender, SECItem *randomA, SECItem *randomB,
    CK_MECHANISM_TYPE derive, CK_MECHANISM_TYPE target,
    CK_ATTRIBUTE_TYPE operation, int keySize,
    CK_ULONG kdf, SECItem *sharedData, void *wincx)
{
    PK11SlotInfo *slot = privKey->pkcs11Slot;
    PK11SymKey *symKey;
    PK11SymKey *SharedSecret;
    CK_MECHANISM mechanism;
    CK_RV crv;
    CK_BBOOL cktrue = CK_TRUE;
    CK_OBJECT_CLASS keyClass = CKO_SECRET_KEY;
    CK_KEY_TYPE keyType = CKK_GENERIC_SECRET;
    CK_ULONG key_size = 0;
    CK_ATTRIBUTE keyTemplate[4];
    int templateCount;
    CK_ATTRIBUTE *attrs = keyTemplate;
    CK_ECDH1_DERIVE_PARAMS *mechParams = NULL;

    if (pubKey->keyType != ecKey) {
        PORT_SetError(SEC_ERROR_BAD_KEY);
        return NULL;
    }
    if ((kdf != CKD_NULL) && (kdf != CKD_SHA1_KDF) &&
        (kdf != CKD_SHA224_KDF) && (kdf != CKD_SHA256_KDF) &&
        (kdf != CKD_SHA384_KDF) && (kdf != CKD_SHA512_KDF)) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        return NULL;
    }

    /* get our key Structure */
    symKey = pk11_CreateSymKey(slot, target, PR_TRUE, PR_TRUE, wincx);
    if (symKey == NULL) {
        return NULL;
    }
    /* CKA_NSS_MESSAGE is a fake operation to distinguish between
     * Normal Encrypt/Decrypt and MessageEncrypt/Decrypt. Don't try to set
     * it as a real attribute */
    if ((operation & CKA_NSS_MESSAGE_MASK) == CKA_NSS_MESSAGE) {
        /* Message is or'd with a real Attribute (CKA_ENCRYPT, CKA_DECRYPT),
         * etc. Strip out the real attribute here */
        operation &= ~CKA_NSS_MESSAGE_MASK;
    }

    symKey->origin = PK11_OriginDerive;

    PK11_SETATTRS(attrs, CKA_CLASS, &keyClass, sizeof(keyClass));
    attrs++;
    PK11_SETATTRS(attrs, CKA_KEY_TYPE, &keyType, sizeof(keyType));
    attrs++;
    PK11_SETATTRS(attrs, operation, &cktrue, 1);
    attrs++;
    PK11_SETATTRS(attrs, CKA_VALUE_LEN, &key_size, sizeof(key_size));
    attrs++;
    templateCount = attrs - keyTemplate;
    PR_ASSERT(templateCount <= sizeof(keyTemplate) / sizeof(CK_ATTRIBUTE));

    keyType = PK11_GetKeyType(target, keySize);
    key_size = keySize;
    if (key_size == 0) {
        if ((key_size = pk11_GetPredefinedKeyLength(keyType))) {
            templateCount--;
        } else {
            /* sigh, some tokens can't figure this out and require
             * CKA_VALUE_LEN to be set */
            switch (kdf) {
                case CKD_NULL:
                    key_size = pk11_ECPubKeySize(pubKey);
                    if (key_size == 0) {
                        PK11_FreeSymKey(symKey);
                        return NULL;
                    }
                    break;
                case CKD_SHA1_KDF:
                    key_size = SHA1_LENGTH;
                    break;
                case CKD_SHA224_KDF:
                    key_size = SHA224_LENGTH;
                    break;
                case CKD_SHA256_KDF:
                    key_size = SHA256_LENGTH;
                    break;
                case CKD_SHA384_KDF:
                    key_size = SHA384_LENGTH;
                    break;
                case CKD_SHA512_KDF:
                    key_size = SHA512_LENGTH;
                    break;
                default:
                    PORT_AssertNotReached("Invalid CKD");
                    PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
                    return NULL;
            }
        }
    }
    symKey->size = key_size;

    mechParams = PORT_ZNew(CK_ECDH1_DERIVE_PARAMS);
    if (!mechParams) {
        PK11_FreeSymKey(symKey);
        return NULL;
    }
    mechParams->kdf = kdf;
    if (sharedData == NULL) {
        mechParams->ulSharedDataLen = 0;
        mechParams->pSharedData = NULL;
    } else {
        mechParams->ulSharedDataLen = sharedData->len;
        mechParams->pSharedData = sharedData->data;
    }
    mechParams->ulPublicDataLen = pubKey->u.ec.publicValue.len;
    mechParams->pPublicData = pubKey->u.ec.publicValue.data;

    mechanism.mechanism = derive;
    mechanism.pParameter = mechParams;
    mechanism.ulParameterLen = sizeof(CK_ECDH1_DERIVE_PARAMS);

    pk11_EnterKeyMonitor(symKey);
    crv = PK11_GETTAB(slot)->C_DeriveKey(symKey->session, &mechanism,
                                         privKey->pkcs11ID, keyTemplate,
                                         templateCount, &symKey->objectID);
    pk11_ExitKeyMonitor(symKey);

    /* old PKCS #11 spec was ambiguous on what needed to be passed,
     * try this again with an encoded public key */
    if (crv != CKR_OK) {
        /* For curves that only use X as public value and no encoding we don't
         * have to try again. (Currently only Curve25519) */
        if (pk11_ECGetPubkeyEncoding(pubKey) == ECPoint_XOnly) {
            goto loser;
        }
        SECItem *pubValue = SEC_ASN1EncodeItem(NULL, NULL,
                                               &pubKey->u.ec.publicValue,
                                               SEC_ASN1_GET(SEC_OctetStringTemplate));
        if (pubValue == NULL) {
            goto loser;
        }
        mechParams->ulPublicDataLen = pubValue->len;
        mechParams->pPublicData = pubValue->data;

        pk11_EnterKeyMonitor(symKey);
        crv = PK11_GETTAB(slot)->C_DeriveKey(symKey->session,
                                             &mechanism, privKey->pkcs11ID, keyTemplate,
                                             templateCount, &symKey->objectID);
        pk11_ExitKeyMonitor(symKey);

        if ((crv != CKR_OK) && (kdf != CKD_NULL)) {
            /* Some PKCS #11 libraries cannot perform the key derivation
             * function. So, try calling C_DeriveKey with CKD_NULL and then
             * performing the KDF separately.
             */
            CK_ULONG derivedKeySize = key_size;

            keyType = CKK_GENERIC_SECRET;
            key_size = pk11_ECPubKeySize(pubKey);
            if (key_size == 0) {
                SECITEM_FreeItem(pubValue, PR_TRUE);
                goto loser;
            }
            SharedSecret = symKey;
            SharedSecret->size = key_size;

            mechParams->kdf = CKD_NULL;
            mechParams->ulSharedDataLen = 0;
            mechParams->pSharedData = NULL;
            mechParams->ulPublicDataLen = pubKey->u.ec.publicValue.len;
            mechParams->pPublicData = pubKey->u.ec.publicValue.data;

            pk11_EnterKeyMonitor(SharedSecret);
            crv = PK11_GETTAB(slot)->C_DeriveKey(SharedSecret->session,
                                                 &mechanism, privKey->pkcs11ID, keyTemplate,
                                                 templateCount, &SharedSecret->objectID);
            pk11_ExitKeyMonitor(SharedSecret);

            if (crv != CKR_OK) {
                /* old PKCS #11 spec was ambiguous on what needed to be passed,
                 * try this one final time with an encoded public key */
                mechParams->ulPublicDataLen = pubValue->len;
                mechParams->pPublicData = pubValue->data;

                pk11_EnterKeyMonitor(SharedSecret);
                crv = PK11_GETTAB(slot)->C_DeriveKey(SharedSecret->session,
                                                     &mechanism, privKey->pkcs11ID, keyTemplate,
                                                     templateCount, &SharedSecret->objectID);
                pk11_ExitKeyMonitor(SharedSecret);
            }

            /* Perform KDF. */
            if (crv == CKR_OK) {
                symKey = pk11_ANSIX963Derive(SharedSecret, kdf,
                                             sharedData, target, operation,
                                             derivedKeySize);
                PK11_FreeSymKey(SharedSecret);
                if (symKey == NULL) {
                    SECITEM_FreeItem(pubValue, PR_TRUE);
                    PORT_ZFree(mechParams, sizeof(CK_ECDH1_DERIVE_PARAMS));
                    return NULL;
                }
            }
        }
        SECITEM_FreeItem(pubValue, PR_TRUE);
    }

loser:
    PORT_ZFree(mechParams, sizeof(CK_ECDH1_DERIVE_PARAMS));

    if (crv != CKR_OK) {
        PK11_FreeSymKey(symKey);
        symKey = NULL;
        PORT_SetError(PK11_MapError(crv));
    }
    return symKey;
}

PK11SymKey *
PK11_PubDeriveWithKDF(SECKEYPrivateKey *privKey, SECKEYPublicKey *pubKey,
                      PRBool isSender, SECItem *randomA, SECItem *randomB,
                      CK_MECHANISM_TYPE derive, CK_MECHANISM_TYPE target,
                      CK_ATTRIBUTE_TYPE operation, int keySize,
                      CK_ULONG kdf, SECItem *sharedData, void *wincx)
{

    switch (privKey->keyType) {
        case rsaKey:
        case nullKey:
        case dsaKey:
        case keaKey:
        case fortezzaKey:
        case dhKey:
            return PK11_PubDerive(privKey, pubKey, isSender, randomA, randomB,
                                  derive, target, operation, keySize, wincx);
        case ecKey:
            return pk11_PubDeriveECKeyWithKDF(privKey, pubKey, isSender,
                                              randomA, randomB, derive, target,
                                              operation, keySize,
                                              kdf, sharedData, wincx);
        default:
            PORT_SetError(SEC_ERROR_BAD_KEY);
            break;
    }

    return NULL;
}

/*
 * this little function uses the Decrypt function to unwrap a key, just in
 * case we are having problem with unwrap. NOTE: The key size may
 * not be preserved properly for some algorithms!
 */
static PK11SymKey *
pk11_HandUnwrap(PK11SlotInfo *slot, CK_OBJECT_HANDLE wrappingKey,
                CK_MECHANISM *mech, SECItem *inKey, CK_MECHANISM_TYPE target,
                CK_ATTRIBUTE *keyTemplate, unsigned int templateCount,
                int key_size, void *wincx, CK_RV *crvp, PRBool isPerm)
{
    CK_ULONG len;
    SECItem outKey;
    PK11SymKey *symKey;
    CK_RV crv;
    PRBool owner = PR_TRUE;
    CK_SESSION_HANDLE session;

    /* remove any VALUE_LEN parameters */
    if (keyTemplate[templateCount - 1].type == CKA_VALUE_LEN) {
        templateCount--;
    }

    /* keys are almost always aligned, but if we get this far,
     * we've gone above and beyond anyway... */
    outKey.data = (unsigned char *)PORT_Alloc(inKey->len);
    if (outKey.data == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        if (crvp)
            *crvp = CKR_HOST_MEMORY;
        return NULL;
    }
    len = inKey->len;

    /* use NULL IV's for wrapping */
    session = pk11_GetNewSession(slot, &owner);
    if (!owner || !(slot->isThreadSafe))
        PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_DecryptInit(session, mech, wrappingKey);
    if (crv != CKR_OK) {
        if (!owner || !(slot->isThreadSafe))
            PK11_ExitSlotMonitor(slot);
        pk11_CloseSession(slot, session, owner);
        PORT_Free(outKey.data);
        PORT_SetError(PK11_MapError(crv));
        if (crvp)
            *crvp = crv;
        return NULL;
    }
    crv = PK11_GETTAB(slot)->C_Decrypt(session, inKey->data, inKey->len,
                                       outKey.data, &len);
    if (!owner || !(slot->isThreadSafe))
        PK11_ExitSlotMonitor(slot);
    pk11_CloseSession(slot, session, owner);
    if (crv != CKR_OK) {
        PORT_Free(outKey.data);
        PORT_SetError(PK11_MapError(crv));
        if (crvp)
            *crvp = crv;
        return NULL;
    }

    outKey.len = (key_size == 0) ? len : key_size;
    outKey.type = siBuffer;

    if (PK11_DoesMechanism(slot, target)) {
        symKey = pk11_ImportSymKeyWithTempl(slot, target, PK11_OriginUnwrap,
                                            isPerm, keyTemplate,
                                            templateCount, &outKey, wincx);
    } else {
        slot = PK11_GetBestSlot(target, wincx);
        if (slot == NULL) {
            PORT_SetError(SEC_ERROR_NO_MODULE);
            PORT_Free(outKey.data);
            if (crvp)
                *crvp = CKR_DEVICE_ERROR;
            return NULL;
        }
        symKey = pk11_ImportSymKeyWithTempl(slot, target, PK11_OriginUnwrap,
                                            isPerm, keyTemplate,
                                            templateCount, &outKey, wincx);
        PK11_FreeSlot(slot);
    }
    PORT_Free(outKey.data);

    if (crvp)
        *crvp = symKey ? CKR_OK : CKR_DEVICE_ERROR;
    return symKey;
}

/*
 * The wrap/unwrap function is pretty much the same for private and
 * public keys. It's just getting the Object ID and slot right. This is
 * the combined unwrap function.
 */
static PK11SymKey *
pk11_AnyUnwrapKey(PK11SlotInfo *slot, CK_OBJECT_HANDLE wrappingKey,
                  CK_MECHANISM_TYPE wrapType, SECItem *param, SECItem *wrappedKey,
                  CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation, int keySize,
                  void *wincx, CK_ATTRIBUTE *userAttr, unsigned int numAttrs, PRBool isPerm)
{
    PK11SymKey *symKey;
    SECItem *param_free = NULL;
    CK_BBOOL cktrue = CK_TRUE;
    CK_OBJECT_CLASS keyClass = CKO_SECRET_KEY;
    CK_KEY_TYPE keyType = CKK_GENERIC_SECRET;
    CK_ULONG valueLen = 0;
    CK_MECHANISM mechanism;
    CK_SESSION_HANDLE rwsession;
    CK_RV crv;
    CK_MECHANISM_INFO mechanism_info;
#define MAX_ADD_ATTRS 4
    CK_ATTRIBUTE keyTemplate[MAX_TEMPL_ATTRS + MAX_ADD_ATTRS];
#undef MAX_ADD_ATTRS
    CK_ATTRIBUTE *attrs = keyTemplate;
    unsigned int templateCount;

    if (numAttrs > MAX_TEMPL_ATTRS) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }
    /* CKA_NSS_MESSAGE is a fake operation to distinguish between
     * Normal Encrypt/Decrypt and MessageEncrypt/Decrypt. Don't try to set
     * it as a real attribute */
    if ((operation & CKA_NSS_MESSAGE_MASK) == CKA_NSS_MESSAGE) {
        /* Message is or'd with a real Attribute (CKA_ENCRYPT, CKA_DECRYPT),
         * etc. Strip out the real attribute here */
        operation &= ~CKA_NSS_MESSAGE_MASK;
    }

    /* first copy caller attributes in. */
    for (templateCount = 0; templateCount < numAttrs; ++templateCount) {
        *attrs++ = *userAttr++;
    }

    /* We only add the following attributes to the template if the caller
    ** didn't already supply them.
    */
    if (!pk11_FindAttrInTemplate(keyTemplate, numAttrs, CKA_CLASS)) {
        PK11_SETATTRS(attrs, CKA_CLASS, &keyClass, sizeof keyClass);
        attrs++;
    }
    if (!pk11_FindAttrInTemplate(keyTemplate, numAttrs, CKA_KEY_TYPE)) {
        keyType = PK11_GetKeyType(target, keySize);
        PK11_SETATTRS(attrs, CKA_KEY_TYPE, &keyType, sizeof keyType);
        attrs++;
    }
    if ((operation != CKA_FLAGS_ONLY) &&
        !pk11_FindAttrInTemplate(keyTemplate, numAttrs, operation)) {
        PK11_SETATTRS(attrs, operation, &cktrue, 1);
        attrs++;
    }

    /*
     * must be last in case we need to use this template to import the key
     */
    if (keySize > 0 &&
        !pk11_FindAttrInTemplate(keyTemplate, numAttrs, CKA_VALUE_LEN)) {
        valueLen = (CK_ULONG)keySize;
        PK11_SETATTRS(attrs, CKA_VALUE_LEN, &valueLen, sizeof valueLen);
        attrs++;
    }

    templateCount = attrs - keyTemplate;
    PR_ASSERT(templateCount <= sizeof(keyTemplate) / sizeof(CK_ATTRIBUTE));

    /* find out if we can do wrap directly. Because the RSA case if *very*
     * common, cache the results for it. */
    if ((wrapType == CKM_RSA_PKCS) && (slot->hasRSAInfo)) {
        mechanism_info.flags = slot->RSAInfoFlags;
    } else {
        if (!slot->isThreadSafe)
            PK11_EnterSlotMonitor(slot);
        crv = PK11_GETTAB(slot)->C_GetMechanismInfo(slot->slotID, wrapType,
                                                    &mechanism_info);
        if (!slot->isThreadSafe)
            PK11_ExitSlotMonitor(slot);
        if (crv != CKR_OK) {
            mechanism_info.flags = 0;
        }
        if (wrapType == CKM_RSA_PKCS) {
            slot->RSAInfoFlags = mechanism_info.flags;
            slot->hasRSAInfo = PR_TRUE;
        }
    }

    /* initialize the mechanism structure */
    mechanism.mechanism = wrapType;
    /* use NULL IV's for wrapping */
    if (param == NULL)
        param = param_free = PK11_ParamFromIV(wrapType, NULL);
    if (param) {
        mechanism.pParameter = param->data;
        mechanism.ulParameterLen = param->len;
    } else {
        mechanism.pParameter = NULL;
        mechanism.ulParameterLen = 0;
    }

    if ((mechanism_info.flags & CKF_DECRYPT) && !PK11_DoesMechanism(slot, target)) {
        symKey = pk11_HandUnwrap(slot, wrappingKey, &mechanism, wrappedKey,
                                 target, keyTemplate, templateCount, keySize,
                                 wincx, &crv, isPerm);
        if (symKey) {
            if (param_free)
                SECITEM_FreeItem(param_free, PR_TRUE);
            return symKey;
        }
        /*
         * if the RSA OP simply failed, don't try to unwrap again
         * with this module.
         */
        if (crv == CKR_DEVICE_ERROR) {
            if (param_free)
                SECITEM_FreeItem(param_free, PR_TRUE);
            return NULL;
        }
        /* fall through, maybe they incorrectly set CKF_DECRYPT */
    }

    /* get our key Structure */
    symKey = pk11_CreateSymKey(slot, target, !isPerm, PR_TRUE, wincx);
    if (symKey == NULL) {
        if (param_free)
            SECITEM_FreeItem(param_free, PR_TRUE);
        return NULL;
    }

    symKey->size = keySize;
    symKey->origin = PK11_OriginUnwrap;

    if (isPerm) {
        rwsession = PK11_GetRWSession(slot);
    } else {
        pk11_EnterKeyMonitor(symKey);
        rwsession = symKey->session;
    }
    PORT_Assert(rwsession != CK_INVALID_HANDLE);
    if (rwsession == CK_INVALID_HANDLE)
        crv = CKR_SESSION_HANDLE_INVALID;
    else
        crv = PK11_GETTAB(slot)->C_UnwrapKey(rwsession, &mechanism, wrappingKey,
                                             wrappedKey->data, wrappedKey->len,
                                             keyTemplate, templateCount,
                                             &symKey->objectID);
    if (isPerm) {
        if (rwsession != CK_INVALID_HANDLE)
            PK11_RestoreROSession(slot, rwsession);
    } else {
        pk11_ExitKeyMonitor(symKey);
    }
    if (param_free)
        SECITEM_FreeItem(param_free, PR_TRUE);
    if (crv != CKR_OK) {
        PK11_FreeSymKey(symKey);
        symKey = NULL;
        if (crv != CKR_DEVICE_ERROR) {
            /* try hand Unwrapping */
            symKey = pk11_HandUnwrap(slot, wrappingKey, &mechanism, wrappedKey,
                                     target, keyTemplate, templateCount,
                                     keySize, wincx, NULL, isPerm);
        }
    }

    return symKey;
}

/* use a symetric key to unwrap another symetric key */
PK11SymKey *
PK11_UnwrapSymKey(PK11SymKey *wrappingKey, CK_MECHANISM_TYPE wrapType,
                  SECItem *param, SECItem *wrappedKey,
                  CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation,
                  int keySize)
{
    return pk11_AnyUnwrapKey(wrappingKey->slot, wrappingKey->objectID,
                             wrapType, param, wrappedKey, target, operation, keySize,
                             wrappingKey->cx, NULL, 0, PR_FALSE);
}

/* use a symetric key to unwrap another symetric key */
PK11SymKey *
PK11_UnwrapSymKeyWithFlags(PK11SymKey *wrappingKey, CK_MECHANISM_TYPE wrapType,
                           SECItem *param, SECItem *wrappedKey,
                           CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation,
                           int keySize, CK_FLAGS flags)
{
    CK_BBOOL ckTrue = CK_TRUE;
    CK_ATTRIBUTE keyTemplate[MAX_TEMPL_ATTRS];
    unsigned int templateCount;

    templateCount = pk11_OpFlagsToAttributes(flags, keyTemplate, &ckTrue);
    return pk11_AnyUnwrapKey(wrappingKey->slot, wrappingKey->objectID,
                             wrapType, param, wrappedKey, target, operation, keySize,
                             wrappingKey->cx, keyTemplate, templateCount, PR_FALSE);
}

PK11SymKey *
PK11_UnwrapSymKeyWithFlagsPerm(PK11SymKey *wrappingKey,
                               CK_MECHANISM_TYPE wrapType,
                               SECItem *param, SECItem *wrappedKey,
                               CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation,
                               int keySize, CK_FLAGS flags, PRBool isPerm)
{
    CK_BBOOL cktrue = CK_TRUE;
    CK_ATTRIBUTE keyTemplate[MAX_TEMPL_ATTRS];
    CK_ATTRIBUTE *attrs;
    unsigned int templateCount;

    attrs = keyTemplate;
    if (isPerm) {
        PK11_SETATTRS(attrs, CKA_TOKEN, &cktrue, sizeof(CK_BBOOL));
        attrs++;
    }
    templateCount = attrs - keyTemplate;
    templateCount += pk11_OpFlagsToAttributes(flags, attrs, &cktrue);

    return pk11_AnyUnwrapKey(wrappingKey->slot, wrappingKey->objectID,
                             wrapType, param, wrappedKey, target, operation, keySize,
                             wrappingKey->cx, keyTemplate, templateCount, isPerm);
}

/* unwrap a symmetric key with a private key. Only supports CKM_RSA_PKCS. */
PK11SymKey *
PK11_PubUnwrapSymKey(SECKEYPrivateKey *wrappingKey, SECItem *wrappedKey,
                     CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation, int keySize)
{
    CK_MECHANISM_TYPE wrapType = pk11_mapWrapKeyType(wrappingKey->keyType);

    return PK11_PubUnwrapSymKeyWithMechanism(wrappingKey, wrapType, NULL,
                                             wrappedKey, target, operation,
                                             keySize);
}

/* unwrap a symmetric key with a private key with the given parameters. */
PK11SymKey *
PK11_PubUnwrapSymKeyWithMechanism(SECKEYPrivateKey *wrappingKey,
                                  CK_MECHANISM_TYPE mechType, SECItem *param,
                                  SECItem *wrappedKey, CK_MECHANISM_TYPE target,
                                  CK_ATTRIBUTE_TYPE operation, int keySize)
{
    PK11SlotInfo *slot = wrappingKey->pkcs11Slot;

    if (SECKEY_HAS_ATTRIBUTE_SET(wrappingKey, CKA_PRIVATE)) {
        PK11_HandlePasswordCheck(slot, wrappingKey->wincx);
    }

    return pk11_AnyUnwrapKey(slot, wrappingKey->pkcs11ID, mechType, param,
                             wrappedKey, target, operation, keySize,
                             wrappingKey->wincx, NULL, 0, PR_FALSE);
}

/* unwrap a symetric key with a private key. */
PK11SymKey *
PK11_PubUnwrapSymKeyWithFlags(SECKEYPrivateKey *wrappingKey,
                              SECItem *wrappedKey, CK_MECHANISM_TYPE target,
                              CK_ATTRIBUTE_TYPE operation, int keySize, CK_FLAGS flags)
{
    CK_MECHANISM_TYPE wrapType = pk11_mapWrapKeyType(wrappingKey->keyType);
    CK_BBOOL ckTrue = CK_TRUE;
    CK_ATTRIBUTE keyTemplate[MAX_TEMPL_ATTRS];
    unsigned int templateCount;
    PK11SlotInfo *slot = wrappingKey->pkcs11Slot;

    templateCount = pk11_OpFlagsToAttributes(flags, keyTemplate, &ckTrue);

    if (SECKEY_HAS_ATTRIBUTE_SET(wrappingKey, CKA_PRIVATE)) {
        PK11_HandlePasswordCheck(slot, wrappingKey->wincx);
    }

    return pk11_AnyUnwrapKey(slot, wrappingKey->pkcs11ID,
                             wrapType, NULL, wrappedKey, target, operation, keySize,
                             wrappingKey->wincx, keyTemplate, templateCount, PR_FALSE);
}

PK11SymKey *
PK11_PubUnwrapSymKeyWithFlagsPerm(SECKEYPrivateKey *wrappingKey,
                                  SECItem *wrappedKey, CK_MECHANISM_TYPE target,
                                  CK_ATTRIBUTE_TYPE operation, int keySize,
                                  CK_FLAGS flags, PRBool isPerm)
{
    CK_MECHANISM_TYPE wrapType = pk11_mapWrapKeyType(wrappingKey->keyType);
    CK_BBOOL cktrue = CK_TRUE;
    CK_ATTRIBUTE keyTemplate[MAX_TEMPL_ATTRS];
    CK_ATTRIBUTE *attrs;
    unsigned int templateCount;
    PK11SlotInfo *slot = wrappingKey->pkcs11Slot;

    attrs = keyTemplate;
    if (isPerm) {
        PK11_SETATTRS(attrs, CKA_TOKEN, &cktrue, sizeof(CK_BBOOL));
        attrs++;
    }
    templateCount = attrs - keyTemplate;

    templateCount += pk11_OpFlagsToAttributes(flags, attrs, &cktrue);

    if (SECKEY_HAS_ATTRIBUTE_SET(wrappingKey, CKA_PRIVATE)) {
        PK11_HandlePasswordCheck(slot, wrappingKey->wincx);
    }

    return pk11_AnyUnwrapKey(slot, wrappingKey->pkcs11ID,
                             wrapType, NULL, wrappedKey, target, operation, keySize,
                             wrappingKey->wincx, keyTemplate, templateCount, isPerm);
}

PK11SymKey *
PK11_CopySymKeyForSigning(PK11SymKey *originalKey, CK_MECHANISM_TYPE mech)
{
    CK_RV crv;
    CK_ATTRIBUTE setTemplate;
    CK_BBOOL ckTrue = CK_TRUE;
    PK11SlotInfo *slot = originalKey->slot;

    /* first just try to set this key up for signing */
    PK11_SETATTRS(&setTemplate, CKA_SIGN, &ckTrue, sizeof(ckTrue));
    pk11_EnterKeyMonitor(originalKey);
    crv = PK11_GETTAB(slot)->C_SetAttributeValue(originalKey->session,
                                                 originalKey->objectID, &setTemplate, 1);
    pk11_ExitKeyMonitor(originalKey);
    if (crv == CKR_OK) {
        return PK11_ReferenceSymKey(originalKey);
    }

    /* nope, doesn't like it, use the pk11 copy object command */
    return pk11_CopyToSlot(slot, mech, CKA_SIGN, originalKey);
}

void
PK11_SetFortezzaHack(PK11SymKey *symKey)
{
    symKey->origin = PK11_OriginFortezzaHack;
}

/*
 * This is required to allow FORTEZZA_NULL and FORTEZZA_RC4
 * working. This function simply gets a valid IV for the keys.
 */
SECStatus
PK11_GenerateFortezzaIV(PK11SymKey *symKey, unsigned char *iv, int len)
{
    CK_MECHANISM mech_info;
    CK_ULONG count = 0;
    CK_RV crv;
    SECStatus rv = SECFailure;

    mech_info.mechanism = CKM_SKIPJACK_CBC64;
    mech_info.pParameter = iv;
    mech_info.ulParameterLen = len;

    /* generate the IV for fortezza */
    PK11_EnterSlotMonitor(symKey->slot);
    crv = PK11_GETTAB(symKey->slot)->C_EncryptInit(symKey->slot->session, &mech_info, symKey->objectID);
    if (crv == CKR_OK) {
        PK11_GETTAB(symKey->slot)->C_EncryptFinal(symKey->slot->session, NULL, &count);
        rv = SECSuccess;
    }
    PK11_ExitSlotMonitor(symKey->slot);
    return rv;
}

CK_OBJECT_HANDLE
PK11_GetSymKeyHandle(PK11SymKey *symKey)
{
    return symKey->objectID;
}
