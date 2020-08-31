/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * This file PK11Contexts which are  used in multipart hashing,
 * encryption/decryption, and signing/verication operations.
 */

#include "seccomon.h"
#include "secmod.h"
#include "nssilock.h"
#include "secmodi.h"
#include "secmodti.h"
#include "pkcs11.h"
#include "pk11func.h"
#include "secitem.h"
#include "secoid.h"
#include "sechash.h"
#include "secerr.h"
#include "blapit.h"
#include "secport.h"

static const SECItem pk11_null_params = { 0 };

/**********************************************************************
 *
 *                   Now Deal with Crypto Contexts
 *
 **********************************************************************/

/*
 * the monitors...
 */
void
PK11_EnterContextMonitor(PK11Context *cx)
{
    /* if we own the session and our slot is ThreadSafe, only monitor
     * the Context */
    if ((cx->ownSession) && (cx->slot->isThreadSafe)) {
        /* Should this use monitors instead? */
        PZ_Lock(cx->sessionLock);
    } else {
        PK11_EnterSlotMonitor(cx->slot);
    }
}

void
PK11_ExitContextMonitor(PK11Context *cx)
{
    /* if we own the session and our slot is ThreadSafe, only monitor
     * the Context */
    if ((cx->ownSession) && (cx->slot->isThreadSafe)) {
        /* Should this use monitors instead? */
        PZ_Unlock(cx->sessionLock);
    } else {
        PK11_ExitSlotMonitor(cx->slot);
    }
}

/*
 * Free up a Cipher Context
 */
void
PK11_DestroyContext(PK11Context *context, PRBool freeit)
{
    pk11_CloseSession(context->slot, context->session, context->ownSession);
    /* initialize the critical fields of the context */
    if (context->savedData != NULL)
        PORT_Free(context->savedData);
    if (context->key)
        PK11_FreeSymKey(context->key);
    if (context->param && context->param != &pk11_null_params)
        SECITEM_FreeItem(context->param, PR_TRUE);
    if (context->sessionLock)
        PZ_DestroyLock(context->sessionLock);
    PK11_FreeSlot(context->slot);
    if (freeit)
        PORT_Free(context);
}

/*
 * save the current context. Allocate Space if necessary.
 */
static unsigned char *
pk11_saveContextHelper(PK11Context *context, unsigned char *buffer,
                       unsigned long *savedLength)
{
    CK_RV crv;

    /* If buffer is NULL, this will get the length */
    crv = PK11_GETTAB(context->slot)->C_GetOperationState(context->session, (CK_BYTE_PTR)buffer, savedLength);
    if (!buffer || (crv == CKR_BUFFER_TOO_SMALL)) {
        /* the given buffer wasn't big enough (or was NULL), but we
         * have the length, so try again with a new buffer and the
         * correct length
         */
        unsigned long bufLen = *savedLength;
        buffer = PORT_Alloc(bufLen);
        if (buffer == NULL) {
            return (unsigned char *)NULL;
        }
        crv = PK11_GETTAB(context->slot)->C_GetOperationState(context->session, (CK_BYTE_PTR)buffer, savedLength);
        if (crv != CKR_OK) {
            PORT_ZFree(buffer, bufLen);
        }
    }
    if (crv != CKR_OK) {
        PORT_SetError(PK11_MapError(crv));
        return (unsigned char *)NULL;
    }
    return buffer;
}

void *
pk11_saveContext(PK11Context *context, void *space, unsigned long *savedLength)
{
    return pk11_saveContextHelper(context,
                                  (unsigned char *)space, savedLength);
}

/*
 * restore the current context
 */
SECStatus
pk11_restoreContext(PK11Context *context, void *space, unsigned long savedLength)
{
    CK_RV crv;
    CK_OBJECT_HANDLE objectID = (context->key) ? context->key->objectID : CK_INVALID_HANDLE;

    PORT_Assert(space != NULL);
    if (space == NULL) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    crv = PK11_GETTAB(context->slot)->C_SetOperationState(context->session, (CK_BYTE_PTR)space, savedLength, objectID, 0);
    if (crv != CKR_OK) {
        PORT_SetError(PK11_MapError(crv));
        return SECFailure;
    }
    return SECSuccess;
}

SECStatus pk11_Finalize(PK11Context *context);

/*
 *  Initialize a Message function. Particular function is passed in as a
 *  function pointer. Since all C_Message*Init funcitons have the same
 *  prototype, we just pick one of the the prototypes to declare our init
 *  function.
 */
static CK_RV
pk11_contextInitMessage(PK11Context *context, CK_MECHANISM_PTR mech,
                        PK11SymKey *key, CK_C_MessageEncryptInit initFunc,
                        CK_FLAGS flags, CK_RV scrv)
{
    PK11SlotInfo *slot = context->slot;
    CK_VERSION version = slot->module->cryptokiVersion;
    CK_RV crv = CKR_OK;

    context->ivCounter = 0;
    context->ivMaxCount = 0;
    context->ivFixedBits = 0;
    context->ivLen = 0;
    context->ivGen = CKG_NO_GENERATE;
    context->simulate_mechanism = (mech)->mechanism;
    context->simulate_message = PR_FALSE;
    /* check that we can do the Message interface. We need to check
     * for either 1) are we using a PKCS #11 v3 interface and 2) is the
     * Message flag set on the mechanism. If either is false we simulate
     * the message interface for the Encrypt and Decrypt cases using the
     * PKCS #11 V2 interface.
     * Sign and verify do not have V2 interfaces, so we go ahead and fail
     * if those cases */
    if ((version.major >= 3) &&
        PK11_DoesMechanismFlag(slot, (mech)->mechanism, flags)) {
        crv = (*initFunc)((context)->session, (mech), (key)->objectID);
        if ((crv == CKR_FUNCTION_NOT_SUPPORTED) ||
            (crv == CKR_MECHANISM_INVALID)) {
            /* we have a 3.0 interface, and the flag was set (or ignored)
             * but the implementation was not there, use the V2 interface */
            crv = (scrv);
            context->simulate_message = PR_TRUE;
        }
    } else {
        crv = (scrv);
        context->simulate_message = PR_TRUE;
    }
    return crv;
}

/*
 * Context initialization. Used by all flavors of CreateContext
 */
static SECStatus
pk11_context_init(PK11Context *context, CK_MECHANISM *mech_info)
{
    CK_RV crv;
    PK11SymKey *symKey = context->key;
    SECStatus rv = SECSuccess;

    context->simulate_message = PR_FALSE;
    switch (context->operation) {
        case CKA_ENCRYPT:
            crv = PK11_GETTAB(context->slot)->C_EncryptInit(context->session, mech_info, symKey->objectID);
            break;
        case CKA_DECRYPT:
            if (context->fortezzaHack) {
                CK_ULONG count = 0;
                /* generate the IV for fortezza */
                crv = PK11_GETTAB(context->slot)->C_EncryptInit(context->session, mech_info, symKey->objectID);
                if (crv != CKR_OK)
                    break;
                PK11_GETTAB(context->slot)
                    ->C_EncryptFinal(context->session,
                                     NULL, &count);
            }
            crv = PK11_GETTAB(context->slot)->C_DecryptInit(context->session, mech_info, symKey->objectID);
            break;
        case CKA_SIGN:
            crv = PK11_GETTAB(context->slot)->C_SignInit(context->session, mech_info, symKey->objectID);
            break;
        case CKA_VERIFY:
            crv = PK11_GETTAB(context->slot)->C_SignInit(context->session, mech_info, symKey->objectID);
            break;
        case CKA_DIGEST:
            crv = PK11_GETTAB(context->slot)->C_DigestInit(context->session, mech_info);
            break;

        case CKA_NSS_MESSAGE | CKA_ENCRYPT:
            crv = pk11_contextInitMessage(context, mech_info, symKey,
                                          PK11_GETTAB(context->slot)->C_MessageEncryptInit,
                                          CKF_MESSAGE_ENCRYPT, CKR_OK);
            break;
        case CKA_NSS_MESSAGE | CKA_DECRYPT:
            crv = pk11_contextInitMessage(context, mech_info, symKey,
                                          PK11_GETTAB(context->slot)->C_MessageDecryptInit,
                                          CKF_MESSAGE_DECRYPT, CKR_OK);
            break;
        case CKA_NSS_MESSAGE | CKA_SIGN:
            crv = pk11_contextInitMessage(context, mech_info, symKey,
                                          PK11_GETTAB(context->slot)->C_MessageSignInit,
                                          CKF_MESSAGE_SIGN, CKR_FUNCTION_NOT_SUPPORTED);
            break;
        case CKA_NSS_MESSAGE | CKA_VERIFY:
            crv = pk11_contextInitMessage(context, mech_info, symKey,
                                          PK11_GETTAB(context->slot)->C_MessageVerifyInit,
                                          CKF_MESSAGE_VERIFY, CKR_FUNCTION_NOT_SUPPORTED);
            break;
        default:
            crv = CKR_OPERATION_NOT_INITIALIZED;
            break;
    }

    if (crv != CKR_OK) {
        PORT_SetError(PK11_MapError(crv));
        return SECFailure;
    }

    /* handle the case where the token is using the old NSS mechanism */
    if (context->simulate_message &&
        !PK11_DoesMechanism(context->slot, context->simulate_mechanism)) {
        if ((context->simulate_mechanism == CKM_CHACHA20_POLY1305) &&
            PK11_DoesMechanism(context->slot, CKM_NSS_CHACHA20_POLY1305)) {
            context->simulate_mechanism = CKM_NSS_CHACHA20_POLY1305;
        } else {
            PORT_SetError(PK11_MapError(CKR_MECHANISM_INVALID));
            return SECFailure;
        }
    }

    /*
     * handle session starvation case.. use our last session to multiplex
     */
    if (!context->ownSession) {
        context->savedData = pk11_saveContext(context, context->savedData,
                                              &context->savedLength);
        if (context->savedData == NULL)
            rv = SECFailure;
        /* clear out out session for others to use */
        pk11_Finalize(context);
    }
    return rv;
}

/*
 * Testing interfaces, not for general use. This function forces
 * an AEAD context into simulation mode even though the target token
 * can already do PKCS #11 v3.0 Message (i.e. softoken).
 */
SECStatus
_PK11_ContextSetAEADSimulation(PK11Context *context)
{
    CK_RV crv;
    /* only message encrypt and message decrypt contexts can be simulated */
    if ((context->operation != (CKA_NSS_MESSAGE | CKA_ENCRYPT)) &&
        (context->operation != (CKA_NSS_MESSAGE | CKA_DECRYPT))) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    /* if we are already simulating, return */
    if (context->simulate_message) {
        return SECSuccess;
    }
    /* we need to shutdown the existing AEAD operation */
    switch (context->operation) {
        case CKA_NSS_MESSAGE | CKA_ENCRYPT:
            crv = PK11_GETTAB(context->slot)->C_MessageEncryptFinal(context->session);
            break;
        case CKA_NSS_MESSAGE | CKA_DECRYPT:
            crv = PK11_GETTAB(context->slot)->C_MessageDecryptFinal(context->session);
            break;
        default:
            PORT_SetError(SEC_ERROR_NOT_INITIALIZED);
            return SECFailure;
    }
    if (crv != CKR_OK) {
        PORT_SetError(PK11_MapError(crv));
        return SECFailure;
    }
    context->simulate_message = PR_TRUE;
    return SECSuccess;
}

PRBool
_PK11_ContextGetAEADSimulation(PK11Context *context)
{
    return context->simulate_message;
}

/*
 * Common Helper Function do come up with a new context.
 */
static PK11Context *
pk11_CreateNewContextInSlot(CK_MECHANISM_TYPE type,
                            PK11SlotInfo *slot, CK_ATTRIBUTE_TYPE operation, PK11SymKey *symKey,
                            SECItem *param)
{
    CK_MECHANISM mech_info;
    PK11Context *context;
    SECStatus rv;

    PORT_Assert(slot != NULL);
    if (!slot || (!symKey && ((operation != CKA_DIGEST) ||
                              (type == CKM_SKIPJACK_CBC64)))) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }
    context = (PK11Context *)PORT_Alloc(sizeof(PK11Context));
    if (context == NULL) {
        return NULL;
    }

    /* now deal with the fortezza hack... the fortezza hack is an attempt
     * to get around the issue of the card not allowing you to do a FORTEZZA
     * LoadIV/Encrypt, which was added because such a combination could be
     * use to circumvent the key escrow system. Unfortunately SSL needs to
     * do this kind of operation, so in SSL we do a loadIV (to verify it),
     * Then GenerateIV, and through away the first 8 bytes on either side
     * of the connection.*/
    context->fortezzaHack = PR_FALSE;
    if (type == CKM_SKIPJACK_CBC64) {
        if (symKey->origin == PK11_OriginFortezzaHack) {
            context->fortezzaHack = PR_TRUE;
        }
    }

    /* initialize the critical fields of the context */
    context->operation = operation;
    context->key = symKey ? PK11_ReferenceSymKey(symKey) : NULL;
    context->slot = PK11_ReferenceSlot(slot);
    context->session = pk11_GetNewSession(slot, &context->ownSession);
    context->cx = symKey ? symKey->cx : NULL;
    /* get our session */
    context->savedData = NULL;

    /* save the parameters so that some digesting stuff can do multiple
     * begins on a single context */
    context->type = type;
    if (param) {
        if (param->len > 0) {
            context->param = SECITEM_DupItem(param);
        } else {
            context->param = (SECItem *)&pk11_null_params;
        }
    } else {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        context->param = NULL;
    }
    context->init = PR_FALSE;
    context->sessionLock = PZ_NewLock(nssILockPK11cxt);
    if ((context->param == NULL) || (context->sessionLock == NULL)) {
        PK11_DestroyContext(context, PR_TRUE);
        return NULL;
    }

    mech_info.mechanism = type;
    mech_info.pParameter = param->data;
    mech_info.ulParameterLen = param->len;
    PK11_EnterContextMonitor(context);
    rv = pk11_context_init(context, &mech_info);
    PK11_ExitContextMonitor(context);

    if (rv != SECSuccess) {
        PK11_DestroyContext(context, PR_TRUE);
        return NULL;
    }
    context->init = PR_TRUE;
    return context;
}

/*
 * put together the various PK11_Create_Context calls used by different
 * parts of libsec.
 */
PK11Context *
__PK11_CreateContextByRawKey(PK11SlotInfo *slot, CK_MECHANISM_TYPE type,
                             PK11Origin origin, CK_ATTRIBUTE_TYPE operation, SECItem *key,
                             SECItem *param, void *wincx)
{
    PK11SymKey *symKey = NULL;
    PK11Context *context = NULL;

    /* first get a slot */
    if (slot == NULL) {
        slot = PK11_GetBestSlot(type, wincx);
        if (slot == NULL) {
            PORT_SetError(SEC_ERROR_NO_MODULE);
            goto loser;
        }
    } else {
        PK11_ReferenceSlot(slot);
    }

    /* now import the key */
    symKey = PK11_ImportSymKey(slot, type, origin, operation, key, wincx);
    if (symKey == NULL)
        goto loser;

    context = PK11_CreateContextBySymKey(type, operation, symKey, param);

loser:
    if (symKey) {
        PK11_FreeSymKey(symKey);
    }
    if (slot) {
        PK11_FreeSlot(slot);
    }

    return context;
}

PK11Context *
PK11_CreateContextByRawKey(PK11SlotInfo *slot, CK_MECHANISM_TYPE type,
                           PK11Origin origin, CK_ATTRIBUTE_TYPE operation, SECItem *key,
                           SECItem *param, void *wincx)
{
    return __PK11_CreateContextByRawKey(slot, type, origin, operation,
                                        key, param, wincx);
}

/*
 * Create a context from a key. We really should make sure we aren't using
 * the same key in multiple session!
 */
PK11Context *
PK11_CreateContextBySymKey(CK_MECHANISM_TYPE type, CK_ATTRIBUTE_TYPE operation,
                           PK11SymKey *symKey, SECItem *param)
{
    PK11SymKey *newKey;
    PK11Context *context;

    /* if this slot doesn't support the mechanism, go to a slot that does */
    newKey = pk11_ForceSlot(symKey, type, operation);
    if (newKey == NULL) {
        PK11_ReferenceSymKey(symKey);
    } else {
        symKey = newKey;
    }

    /* Context Adopts the symKey.... */
    context = pk11_CreateNewContextInSlot(type, symKey->slot, operation, symKey,
                                          param);
    PK11_FreeSymKey(symKey);
    return context;
}

/*
 * Digest contexts don't need keys, but the do need to find a slot.
 * Macing should use PK11_CreateContextBySymKey.
 */
PK11Context *
PK11_CreateDigestContext(SECOidTag hashAlg)
{
    /* digesting has to work without authentication to the slot */
    CK_MECHANISM_TYPE type;
    PK11SlotInfo *slot;
    PK11Context *context;
    SECItem param;

    type = PK11_AlgtagToMechanism(hashAlg);
    slot = PK11_GetBestSlot(type, NULL);
    if (slot == NULL) {
        PORT_SetError(SEC_ERROR_NO_MODULE);
        return NULL;
    }

    /* maybe should really be PK11_GenerateNewParam?? */
    param.data = NULL;
    param.len = 0;
    param.type = 0;

    context = pk11_CreateNewContextInSlot(type, slot, CKA_DIGEST, NULL, &param);
    PK11_FreeSlot(slot);
    return context;
}

/*
 * create a new context which is the clone of the state of old context.
 */
PK11Context *
PK11_CloneContext(PK11Context *old)
{
    PK11Context *newcx;
    PRBool needFree = PR_FALSE;
    SECStatus rv = SECSuccess;
    void *data;
    unsigned long len;

    newcx = pk11_CreateNewContextInSlot(old->type, old->slot, old->operation,
                                        old->key, old->param);
    if (newcx == NULL)
        return NULL;

    /* now clone the save state. First we need to find the save state
      * of the old session. If the old context owns it's session,
      * the state needs to be saved, otherwise the state is in saveData. */
    if (old->ownSession) {
        PK11_EnterContextMonitor(old);
        data = pk11_saveContext(old, NULL, &len);
        PK11_ExitContextMonitor(old);
        needFree = PR_TRUE;
    } else {
        data = old->savedData;
        len = old->savedLength;
    }

    if (data == NULL) {
        PK11_DestroyContext(newcx, PR_TRUE);
        return NULL;
    }

    /* now copy that state into our new context. Again we have different
      * work if the new context owns it's own session. If it does, we
      * restore the state gathered above. If it doesn't, we copy the
      * saveData pointer... */
    if (newcx->ownSession) {
        PK11_EnterContextMonitor(newcx);
        rv = pk11_restoreContext(newcx, data, len);
        PK11_ExitContextMonitor(newcx);
    } else {
        PORT_Assert(newcx->savedData != NULL);
        if ((newcx->savedData == NULL) || (newcx->savedLength < len)) {
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            rv = SECFailure;
        } else {
            PORT_Memcpy(newcx->savedData, data, len);
            newcx->savedLength = len;
        }
    }

    if (needFree)
        PORT_Free(data);

    if (rv != SECSuccess) {
        PK11_DestroyContext(newcx, PR_TRUE);
        return NULL;
    }
    return newcx;
}

/*
 * save the current context state into a variable. Required to make FORTEZZA
 * work.
 */
SECStatus
PK11_SaveContext(PK11Context *cx, unsigned char *save, int *len, int saveLength)
{
    unsigned char *data = NULL;
    CK_ULONG length = saveLength;

    if (cx->ownSession) {
        PK11_EnterContextMonitor(cx);
        data = pk11_saveContextHelper(cx, save, &length);
        PK11_ExitContextMonitor(cx);
        if (data)
            *len = length;
    } else if ((unsigned)saveLength >= cx->savedLength) {
        data = (unsigned char *)cx->savedData;
        if (cx->savedData) {
            PORT_Memcpy(save, cx->savedData, cx->savedLength);
        }
        *len = cx->savedLength;
    }
    if (data != NULL) {
        if (cx->ownSession) {
            PORT_ZFree(data, length);
        }
        return SECSuccess;
    } else {
        return SECFailure;
    }
}

/* same as above, but may allocate the return buffer. */
unsigned char *
PK11_SaveContextAlloc(PK11Context *cx,
                      unsigned char *preAllocBuf, unsigned int pabLen,
                      unsigned int *stateLen)
{
    unsigned char *stateBuf = NULL;
    unsigned long length = (unsigned long)pabLen;

    if (cx->ownSession) {
        PK11_EnterContextMonitor(cx);
        stateBuf = pk11_saveContextHelper(cx, preAllocBuf, &length);
        PK11_ExitContextMonitor(cx);
        *stateLen = (stateBuf != NULL) ? length : 0;
    } else {
        if (pabLen < cx->savedLength) {
            stateBuf = (unsigned char *)PORT_Alloc(cx->savedLength);
            if (!stateBuf) {
                return (unsigned char *)NULL;
            }
        } else {
            stateBuf = preAllocBuf;
        }
        if (cx->savedData) {
            PORT_Memcpy(stateBuf, cx->savedData, cx->savedLength);
        }
        *stateLen = cx->savedLength;
    }
    return stateBuf;
}

/*
 * restore the context state into a new running context. Also required for
 * FORTEZZA .
 */
SECStatus
PK11_RestoreContext(PK11Context *cx, unsigned char *save, int len)
{
    SECStatus rv = SECSuccess;
    if (cx->ownSession) {
        PK11_EnterContextMonitor(cx);
        pk11_Finalize(cx);
        rv = pk11_restoreContext(cx, save, len);
        PK11_ExitContextMonitor(cx);
    } else {
        PORT_Assert(cx->savedData != NULL);
        if ((cx->savedData == NULL) || (cx->savedLength < (unsigned)len)) {
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            rv = SECFailure;
        } else {
            PORT_Memcpy(cx->savedData, save, len);
            cx->savedLength = len;
        }
    }
    return rv;
}

/*
 * This is  to get FIPS compliance until we can convert
 * libjar to use PK11_ hashing functions. It returns PR_FALSE
 * if we can't get a PK11 Context.
 */
PRBool
PK11_HashOK(SECOidTag algID)
{
    PK11Context *cx;

    cx = PK11_CreateDigestContext(algID);
    if (cx == NULL)
        return PR_FALSE;
    PK11_DestroyContext(cx, PR_TRUE);
    return PR_TRUE;
}

/*
 * start a new digesting or Mac'ing operation on this context
 */
SECStatus
PK11_DigestBegin(PK11Context *cx)
{
    CK_MECHANISM mech_info;
    SECStatus rv;

    if (cx->init == PR_TRUE) {
        return SECSuccess;
    }

    /*
     * make sure the old context is clear first
     */
    PK11_EnterContextMonitor(cx);
    pk11_Finalize(cx);

    mech_info.mechanism = cx->type;
    mech_info.pParameter = cx->param->data;
    mech_info.ulParameterLen = cx->param->len;
    rv = pk11_context_init(cx, &mech_info);
    PK11_ExitContextMonitor(cx);

    if (rv != SECSuccess) {
        return SECFailure;
    }
    cx->init = PR_TRUE;
    return SECSuccess;
}

SECStatus
PK11_HashBuf(SECOidTag hashAlg, unsigned char *out, const unsigned char *in,
             PRInt32 len)
{
    PK11Context *context;
    unsigned int max_length;
    unsigned int out_length;
    SECStatus rv;

    /* len will be passed to PK11_DigestOp as unsigned. */
    if (len < 0) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    context = PK11_CreateDigestContext(hashAlg);
    if (context == NULL)
        return SECFailure;

    rv = PK11_DigestBegin(context);
    if (rv != SECSuccess) {
        PK11_DestroyContext(context, PR_TRUE);
        return rv;
    }

    rv = PK11_DigestOp(context, in, len);
    if (rv != SECSuccess) {
        PK11_DestroyContext(context, PR_TRUE);
        return rv;
    }

    /* XXX This really should have been an argument to this function! */
    max_length = HASH_ResultLenByOidTag(hashAlg);
    PORT_Assert(max_length);
    if (!max_length)
        max_length = HASH_LENGTH_MAX;

    rv = PK11_DigestFinal(context, out, &out_length, max_length);
    PK11_DestroyContext(context, PR_TRUE);
    return rv;
}

/*
 * execute a bulk encryption operation
 */
SECStatus
PK11_CipherOp(PK11Context *context, unsigned char *out, int *outlen,
              int maxout, const unsigned char *in, int inlen)
{
    CK_RV crv = CKR_OK;
    CK_ULONG length = maxout;
    CK_ULONG offset = 0;
    SECStatus rv = SECSuccess;
    unsigned char *saveOut = out;
    unsigned char *allocOut = NULL;

    /* if we ran out of session, we need to restore our previously stored
     * state.
     */
    PK11_EnterContextMonitor(context);
    if (!context->ownSession) {
        rv = pk11_restoreContext(context, context->savedData,
                                 context->savedLength);
        if (rv != SECSuccess) {
            PK11_ExitContextMonitor(context);
            return rv;
        }
    }

    /*
     * The fortezza hack is to send 8 extra bytes on the first encrypted and
     * lose them on the first decrypt.
     */
    if (context->fortezzaHack) {
        unsigned char random[8];
        if (context->operation == CKA_ENCRYPT) {
            PK11_ExitContextMonitor(context);
            rv = PK11_GenerateRandom(random, sizeof(random));
            PK11_EnterContextMonitor(context);

            /* since we are offseting the output, we can't encrypt back into
             * the same buffer... allocate a temporary buffer just for this
             * call. */
            allocOut = out = (unsigned char *)PORT_Alloc(maxout);
            if (out == NULL) {
                PK11_ExitContextMonitor(context);
                return SECFailure;
            }
            crv = PK11_GETTAB(context->slot)->C_EncryptUpdate(context->session, random, sizeof(random), out, &length);

            out += length;
            maxout -= length;
            offset = length;
        } else if (context->operation == CKA_DECRYPT) {
            length = sizeof(random);
            crv = PK11_GETTAB(context->slot)->C_DecryptUpdate(context->session, (CK_BYTE_PTR)in, sizeof(random), random, &length);
            inlen -= length;
            in += length;
            context->fortezzaHack = PR_FALSE;
        }
    }

    switch (context->operation) {
        case CKA_ENCRYPT:
            length = maxout;
            crv = PK11_GETTAB(context->slot)->C_EncryptUpdate(context->session, (CK_BYTE_PTR)in, inlen, out, &length);
            length += offset;
            break;
        case CKA_DECRYPT:
            length = maxout;
            crv = PK11_GETTAB(context->slot)->C_DecryptUpdate(context->session, (CK_BYTE_PTR)in, inlen, out, &length);
            break;
        default:
            crv = CKR_OPERATION_NOT_INITIALIZED;
            break;
    }

    if (crv != CKR_OK) {
        PORT_SetError(PK11_MapError(crv));
        *outlen = 0;
        rv = SECFailure;
    } else {
        *outlen = length;
    }

    if (context->fortezzaHack) {
        if (context->operation == CKA_ENCRYPT) {
            PORT_Assert(allocOut);
            PORT_Memcpy(saveOut, allocOut, length);
            PORT_Free(allocOut);
        }
        context->fortezzaHack = PR_FALSE;
    }

    /*
     * handle session starvation case.. use our last session to multiplex
     */
    if (!context->ownSession) {
        context->savedData = pk11_saveContext(context, context->savedData,
                                              &context->savedLength);
        if (context->savedData == NULL)
            rv = SECFailure;

        /* clear out out session for others to use */
        pk11_Finalize(context);
    }
    PK11_ExitContextMonitor(context);
    return rv;
}

/*
 * Simulate the IV generation that normally would happen in the token.
 *
 * This is a modifed copy of what is in freebl/gcm.c. We can't use the
 * version in freebl because of layering, since freebl is inside the token
 * boundary. These issues are traditionally handled by moving them to util,
 * but we also have two different Random functions we have two switch between.
 * Since this is primarily here for tokens that don't support the PKCS #11
 * Message Interface, it's OK if they diverge a bit. Slight semantic
 * differences from the freebl/gcm.c version shouldn't be much more than the
 * sematic differences between freebl and other tokens which do implement the
 * Message Interface. */
static SECStatus
pk11_GenerateIV(PK11Context *context, CK_GENERATOR_FUNCTION ivgen,
                int fixedBits, unsigned char *iv, int ivLen)
{
    unsigned int i;
    unsigned int flexBits;
    unsigned int ivOffset;
    unsigned int ivNewCount;
    unsigned char ivMask;
    unsigned char ivSave;
    SECStatus rv;

    if (context->ivCounter != 0) {
        /* If we've already generated a message, make sure all subsequent
         * messages are using the same generator */
        if ((context->ivGen != ivgen) ||
            (context->ivFixedBits != fixedBits) ||
            (context->ivLen != ivLen)) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
        }
    } else {
        /* remember these values */
        context->ivGen = ivgen;
        context->ivFixedBits = fixedBits;
        context->ivLen = ivLen;
        /* now calculate how may bits of IV we have to supply */
        flexBits = ivLen * PR_BITS_PER_BYTE;
        /* first make sure we aren't going to overflow */
        if (flexBits < fixedBits) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
        }
        flexBits -= fixedBits;
        /* if we are generating a random number reduce the acceptable bits to
         * avoid birthday attacks */
        if (ivgen == CKG_GENERATE_RANDOM) {
            if (flexBits <= GCMIV_RANDOM_BIRTHDAY_BITS) {
                PORT_SetError(SEC_ERROR_INVALID_ARGS);
                return SECFailure;
            }
            /* see freebl/blapit.h for how GCMIV_RANDOM_BIRTHDAY_BITS is
             * calculated. */
            flexBits -= GCMIV_RANDOM_BIRTHDAY_BITS;
            flexBits = flexBits >> 1;
        }
        if (flexBits == 0) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
        }
        /* Turn those bits into the number of IV's we can safely return */
        if (flexBits >= sizeof(context->ivMaxCount) * PR_BITS_PER_BYTE) {
            context->ivMaxCount = PR_UINT64(0xffffffffffffffff);
        } else {
            context->ivMaxCount = (PR_UINT64(1) << flexBits);
        }
    }

    /* no generate, accept the IV from the source */
    if (ivgen == CKG_NO_GENERATE) {
        context->ivCounter = 1;
        return SECSuccess;
    }

    /* make sure we haven't exceeded the number of IVs we can return
     * for this key, generator, and IV size */
    if (context->ivCounter >= context->ivMaxCount) {
        /* use a unique error from just bad user input */
        PORT_SetError(SEC_ERROR_EXTRA_INPUT);
        return SECFailure;
    }

    /* build to mask to handle the first byte of the IV */
    ivOffset = fixedBits / PR_BITS_PER_BYTE;
    ivMask = 0xff >> ((PR_BITS_PER_BYTE - (fixedBits & 7)) & 7);
    ivNewCount = ivLen - ivOffset;

    /* finally generate the IV */
    switch (ivgen) {
        case CKG_GENERATE: /* default to counter */
        case CKG_GENERATE_COUNTER:
            iv[ivOffset] = (iv[ivOffset] & ~ivMask) |
                           (PORT_GET_BYTE_BE(context->ivCounter, 0, ivNewCount) & ivMask);
            for (i = 1; i < ivNewCount; i++) {
                iv[ivOffset + i] =
                    PORT_GET_BYTE_BE(context->ivCounter, i, ivNewCount);
            }
            break;
        case CKG_GENERATE_COUNTER_XOR:
            iv[ivOffset] ^=
                (PORT_GET_BYTE_BE(context->ivCounter, 0, ivNewCount) & ivMask);
            for (i = 1; i < ivNewCount; i++) {
                iv[ivOffset + i] ^=
                    PORT_GET_BYTE_BE(context->ivCounter, i, ivNewCount);
            }
            break;
        case CKG_GENERATE_RANDOM:
            ivSave = iv[ivOffset] & ~ivMask;
            rv = PK11_GenerateRandom(iv + ivOffset, ivNewCount);
            iv[ivOffset] = ivSave | (iv[ivOffset] & ivMask);
            if (rv != SECSuccess) {
                return rv;
            }
            break;
    }
    context->ivCounter++;
    return SECSuccess;
}

/*
 * PKCS #11 v2.40 did not have a message interface. If our module can't
 * do the message interface use the old method of doing AEAD */
static SECStatus
pk11_AEADSimulateOp(PK11Context *context, void *params, int paramslen,
                    const unsigned char *aad, int aadlen,
                    unsigned char *out, int *outlen,
                    int maxout, const unsigned char *in, int inlen)
{
    unsigned int length = maxout;
    SECStatus rv = SECSuccess;
    unsigned char *saveOut = out;
    unsigned char *allocOut = NULL;

    /*
     * first we need to convert the single shot (v2.40) parameters into
     * the message version of the parameters. This usually involves
     * copying the Nonce or IV, setting the AAD from our parameter list
     * and handling the tag differences */
    CK_GCM_PARAMS_V3 gcm;
    CK_GCM_MESSAGE_PARAMS *gcm_message;
    CK_CCM_PARAMS ccm;
    CK_CCM_MESSAGE_PARAMS *ccm_message;
    CK_SALSA20_CHACHA20_POLY1305_PARAMS chacha_poly;
    CK_SALSA20_CHACHA20_POLY1305_MSG_PARAMS *chacha_poly_message;
    CK_NSS_AEAD_PARAMS nss_chacha_poly;
    CK_MECHANISM_TYPE mechanism = context->simulate_mechanism;
    SECItem sim_params = { 0, NULL, 0 };
    unsigned char *tag = NULL;
    unsigned int taglen;
    PRBool encrypt;

    *outlen = 0;
    /* figure out if we are encrypting or decrypting, as tags are
     * handled differently in both */
    switch (context->operation) {
        case CKA_NSS_MESSAGE | CKA_ENCRYPT:
            encrypt = PR_TRUE;
            break;
        case CKA_NSS_MESSAGE | CKA_DECRYPT:
            encrypt = PR_FALSE;
            break;
        default:
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
    }

    switch (mechanism) {
        case CKM_CHACHA20_POLY1305:
        case CKM_SALSA20_POLY1305:
            if (paramslen != sizeof(CK_SALSA20_CHACHA20_POLY1305_MSG_PARAMS)) {
                PORT_SetError(SEC_ERROR_INVALID_ARGS);
                return SECFailure;
            }
            chacha_poly_message =
                (CK_SALSA20_CHACHA20_POLY1305_MSG_PARAMS *)params;
            chacha_poly.pNonce = chacha_poly_message->pNonce;
            chacha_poly.ulNonceLen = chacha_poly_message->ulNonceLen;
            chacha_poly.pAAD = (CK_BYTE_PTR)aad;
            chacha_poly.ulAADLen = aadlen;
            tag = chacha_poly_message->pTag;
            taglen = 16;
            sim_params.data = (unsigned char *)&chacha_poly;
            sim_params.len = sizeof(chacha_poly);
            /* SALSA20_POLY1305 and CHACHA20_POLY1305 do not generate the iv
         * internally, don't simulate it either */
            break;
        case CKM_NSS_CHACHA20_POLY1305:
            if (paramslen != sizeof(CK_SALSA20_CHACHA20_POLY1305_MSG_PARAMS)) {
                PORT_SetError(SEC_ERROR_INVALID_ARGS);
                return SECFailure;
            }
            chacha_poly_message =
                (CK_SALSA20_CHACHA20_POLY1305_MSG_PARAMS *)params;
            tag = chacha_poly_message->pTag;
            taglen = 16;
            nss_chacha_poly.pNonce = chacha_poly_message->pNonce;
            nss_chacha_poly.ulNonceLen = chacha_poly_message->ulNonceLen;
            nss_chacha_poly.pAAD = (CK_BYTE_PTR)aad;
            nss_chacha_poly.ulAADLen = aadlen;
            nss_chacha_poly.ulTagLen = taglen;
            sim_params.data = (unsigned char *)&nss_chacha_poly;
            sim_params.len = sizeof(nss_chacha_poly);
            /* CKM_NSS_CHACHA20_POLY1305 does not generate the iv
             * internally, don't simulate it either */
            break;
        case CKM_AES_CCM:
            if (paramslen != sizeof(CK_CCM_MESSAGE_PARAMS)) {
                PORT_SetError(SEC_ERROR_INVALID_ARGS);
                return SECFailure;
            }
            ccm_message = (CK_CCM_MESSAGE_PARAMS *)params;
            ccm.ulDataLen = ccm_message->ulDataLen;
            ccm.pNonce = ccm_message->pNonce;
            ccm.ulNonceLen = ccm_message->ulNonceLen;
            ccm.pAAD = (CK_BYTE_PTR)aad;
            ccm.ulAADLen = aadlen;
            ccm.ulMACLen = ccm_message->ulMACLen;
            tag = ccm_message->pMAC;
            taglen = ccm_message->ulMACLen;
            sim_params.data = (unsigned char *)&ccm;
            sim_params.len = sizeof(ccm);
            if (encrypt) {
                /* simulate generating the IV */
                rv = pk11_GenerateIV(context, ccm_message->nonceGenerator,
                                     ccm_message->ulNonceFixedBits,
                                     ccm_message->pNonce,
                                     ccm_message->ulNonceLen);
                if (rv != SECSuccess) {
                    return rv;
                }
            }
            break;
        case CKM_AES_GCM:
            if (paramslen != sizeof(CK_GCM_MESSAGE_PARAMS)) {
                PORT_SetError(SEC_ERROR_INVALID_ARGS);
                return SECFailure;
            }
            gcm_message = (CK_GCM_MESSAGE_PARAMS *)params;
            gcm.pIv = gcm_message->pIv;
            gcm.ulIvLen = gcm_message->ulIvLen;
            gcm.ulIvBits = gcm.ulIvLen * PR_BITS_PER_BYTE;
            gcm.pAAD = (CK_BYTE_PTR)aad;
            gcm.ulAADLen = aadlen;
            gcm.ulTagBits = gcm_message->ulTagBits;
            tag = gcm_message->pTag;
            taglen = (gcm_message->ulTagBits + (PR_BITS_PER_BYTE - 1)) / PR_BITS_PER_BYTE;
            sim_params.data = (unsigned char *)&gcm;
            sim_params.len = sizeof(gcm);
            if (encrypt) {
                /* simulate generating the IV */
                rv = pk11_GenerateIV(context, gcm_message->ivGenerator,
                                     gcm_message->ulIvFixedBits,
                                     gcm_message->pIv, gcm_message->ulIvLen);
                if (rv != SECSuccess) {
                    return rv;
                }
            }
            break;
        default:
            PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
            return SECFailure;
    }
    /* now handle the tag. The message interface separates the tag from
     * the data, while the single shot gets and puts the tag at the end of
     * the encrypted data. */
    if (!encrypt) {
        /* In the decrypt case, if the tag is already at the end of the
         * input buffer we are golden, otherwise we'll need a new input
         * buffer and copy the tag at the end of it */
        if (tag != in + inlen) {
            allocOut = PORT_Alloc(inlen + taglen);
            if (allocOut == NULL) {
                return SECFailure;
            }
            PORT_Memcpy(allocOut, in, inlen);
            PORT_Memcpy(allocOut + inlen, tag, taglen);
            in = allocOut;
        }
        inlen = inlen + taglen;
    } else {
        /* if we end up allocating, we don't want to overrun this buffer,
         * so we fail early here */
        if (maxout < inlen) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
        }
        /* in the encrypt case, we are fine if maxout is big enough to hold
         * the tag. We'll copy the tag after the operation */
        if (maxout < inlen + taglen) {
            allocOut = PORT_Alloc(inlen + taglen);
            if (allocOut == NULL) {
                return SECFailure;
            }
            out = allocOut;
            length = maxout = inlen + taglen;
        }
    }
    /* now do the operation */
    if (encrypt) {
        rv = PK11_Encrypt(context->key, mechanism, &sim_params, out, &length,
                          maxout, in, inlen);
    } else {
        rv = PK11_Decrypt(context->key, mechanism, &sim_params, out, &length,
                          maxout, in, inlen);
    }
    if (rv != SECSuccess) {
        /* If the mechanism was CKM_AES_GCM, the module may have been
         * following the same error as old versions of NSS. Retry with
         * the CK_NSS_GCM_PARAMS */
        if ((mechanism == CKM_AES_GCM) &&
            (PORT_GetError() == SEC_ERROR_BAD_DATA)) {
            CK_NSS_GCM_PARAMS gcm_nss;
            gcm_message = (CK_GCM_MESSAGE_PARAMS *)params;
            gcm_nss.pIv = gcm_message->pIv;
            gcm_nss.ulIvLen = gcm_message->ulIvLen;
            gcm_nss.pAAD = (CK_BYTE_PTR)aad;
            gcm_nss.ulAADLen = aadlen;
            gcm_nss.ulTagBits = gcm_message->ulTagBits;
            sim_params.data = (unsigned char *)&gcm_nss;
            sim_params.len = sizeof(gcm_nss);
            if (encrypt) {
                rv = PK11_Encrypt(context->key, mechanism, &sim_params, out,
                                  &length, maxout, in, inlen);
            } else {
                rv = PK11_Decrypt(context->key, mechanism, &sim_params, out,
                                  &length, maxout, in, inlen);
            }
            if (rv != SECSuccess) {
                goto fail;
            }
        } else {
            goto fail;
        }
    }

    /* on encrypt, separate the output buffer from the tag */
    if (encrypt) {
        if ((length < taglen) || (length > inlen + taglen)) {
            /* PKCS #11 module should not return a length smaller than
             * taglen, or bigger than inlen+taglen */
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            rv = SECFailure;
            goto fail;
        }
        length = length - taglen;
        if (allocOut) {
            /*
             * If we used a temporary buffer, copy it out to the original
             * buffer.
             */
            PORT_Memcpy(saveOut, allocOut, length);
        }
        /* if the tag isn't in the right place, copy it out */
        if (tag != out + length) {
            PORT_Memcpy(tag, out + length, taglen);
        }
    }
    *outlen = length;
    rv = SECSuccess;
fail:
    if (allocOut) {
        PORT_Free(allocOut);
    }
    return rv;
}

/*
 * Do an AEAD operation. This function optionally returns
 * and IV on Encrypt for all mechanism. NSS knows which mechanisms
 * generate IV's in the token and which don't. This allows the
 * applications to make a single call without special handling for
 * each AEAD mechanism (the special handling is all contained here.
 */
SECStatus
PK11_AEADOp(PK11Context *context, CK_GENERATOR_FUNCTION ivgen,
            int fixedbits, unsigned char *iv, int ivlen,
            const unsigned char *aad, int aadlen,
            unsigned char *out, int *outlen,
            int maxout, unsigned char *tag, int taglen,
            const unsigned char *in, int inlen)
{
    CK_GCM_MESSAGE_PARAMS gcm_message;
    CK_CCM_MESSAGE_PARAMS ccm_message;
    CK_SALSA20_CHACHA20_POLY1305_MSG_PARAMS chacha_poly_message;
    void *params;
    int paramslen;
    SECStatus rv;

    switch (context->simulate_mechanism) {
        case CKM_CHACHA20_POLY1305:
        case CKM_SALSA20_POLY1305:
        case CKM_NSS_CHACHA20_POLY1305:
            chacha_poly_message.pNonce = iv;
            chacha_poly_message.ulNonceLen = ivlen;
            chacha_poly_message.pTag = tag;
            params = &chacha_poly_message;
            paramslen = sizeof(CK_SALSA20_CHACHA20_POLY1305_MSG_PARAMS);
            /* SALSA20_POLY1305 and CHACHA20_POLY1305 do not generate the iv
         * internally, Do it here. */
            if (context->operation == (CKA_NSS_MESSAGE | CKA_ENCRYPT)) {
                /* simulate generating the IV */
                rv = pk11_GenerateIV(context, ivgen, fixedbits, iv, ivlen);
                if (rv != SECSuccess) {
                    return rv;
                }
            }
            break;
        case CKM_AES_GCM:
            gcm_message.pIv = iv;
            gcm_message.ulIvLen = ivlen;
            gcm_message.ivGenerator = ivgen;
            gcm_message.ulIvFixedBits = fixedbits;
            gcm_message.pTag = tag;
            gcm_message.ulTagBits = taglen * 8;
            params = &gcm_message;
            paramslen = sizeof(CK_GCM_MESSAGE_PARAMS);
            /* GCM generates IV internally */
            break;
        case CKM_AES_CCM:
            ccm_message.ulDataLen = inlen;
            ccm_message.pNonce = iv;
            ccm_message.ulNonceLen = ivlen;
            ccm_message.nonceGenerator = ivgen;
            ccm_message.ulNonceFixedBits = fixedbits;
            ccm_message.pMAC = tag;
            ccm_message.ulMACLen = taglen;
            params = &ccm_message;
            paramslen = sizeof(CK_GCM_MESSAGE_PARAMS);
            /* CCM generates IV internally */
            break;

        default:
            PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
            return SECFailure;
    }
    return PK11_AEADRawOp(context, params, paramslen, aad, aadlen, out, outlen,
                          maxout, in, inlen);
}

/* Do and AED operation. The application builds the params on it's own
 * and passes them in. This allows applications direct access to the params
 * so they can use mechanisms not yet understood by, NSS, or get semantics
 * not suppied by PK11_AEAD. */
SECStatus
PK11_AEADRawOp(PK11Context *context, void *params, int paramslen,
               const unsigned char *aad, int aadlen,
               unsigned char *out, int *outlen,
               int maxout, const unsigned char *in, int inlen)
{
    CK_RV crv = CKR_OK;
    CK_ULONG length = maxout;
    SECStatus rv = SECSuccess;

    PORT_Assert(outlen != NULL);
    *outlen = 0;
    if (((context->operation) & CKA_NSS_MESSAGE_MASK) != CKA_NSS_MESSAGE) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /*
     * The PKCS 11 module does not support the message interface, fall
     * back to using single shot operation */
    if (context->simulate_message) {
        return pk11_AEADSimulateOp(context, params, paramslen, aad, aadlen,
                                   out, outlen, maxout, in, inlen);
    }

    /* if we ran out of session, we need to restore our previously stored
     * state.
     */
    PK11_EnterContextMonitor(context);
    if (!context->ownSession) {
        rv = pk11_restoreContext(context, context->savedData,
                                 context->savedLength);
        if (rv != SECSuccess) {
            PK11_ExitContextMonitor(context);
            return rv;
        }
    }

    switch (context->operation) {
        case CKA_NSS_MESSAGE | CKA_ENCRYPT:
            length = maxout;
            crv = PK11_GETTAB(context->slot)->C_EncryptMessage(context->session, params, paramslen, (CK_BYTE_PTR)aad, aadlen, (CK_BYTE_PTR)in, inlen, out, &length);
            break;
        case CKA_NSS_MESSAGE | CKA_DECRYPT:
            length = maxout;
            crv = PK11_GETTAB(context->slot)->C_DecryptMessage(context->session, params, paramslen, (CK_BYTE_PTR)aad, aadlen, (CK_BYTE_PTR)in, inlen, out, &length);
            break;
        case CKA_NSS_MESSAGE | CKA_SIGN:
            length = maxout;
            crv = PK11_GETTAB(context->slot)->C_SignMessage(context->session, params, paramslen, (CK_BYTE_PTR)in, inlen, out, &length);
            break;
        case CKA_NSS_MESSAGE | CKA_VERIFY:
            length = maxout; /* sig length */
            crv = PK11_GETTAB(context->slot)->C_VerifyMessage(context->session, params, paramslen, (CK_BYTE_PTR)in, inlen, out /* sig */, length);
            break;
        default:
            crv = CKR_OPERATION_NOT_INITIALIZED;
            break;
    }

    if (crv != CKR_OK) {
        PORT_SetError(PK11_MapError(crv));
        rv = SECFailure;
    } else {
        *outlen = length;
    }

    /*
     * handle session starvation case.. use our last session to multiplex
     */
    if (!context->ownSession) {
        context->savedData = pk11_saveContext(context, context->savedData,
                                              &context->savedLength);
        if (context->savedData == NULL)
            rv = SECFailure;

        /* clear out out session for others to use */
        pk11_Finalize(context);
    }
    PK11_ExitContextMonitor(context);
    return rv;
}

/*
 * execute a digest/signature operation
 */
SECStatus
PK11_DigestOp(PK11Context *context, const unsigned char *in, unsigned inLen)
{
    CK_RV crv = CKR_OK;
    SECStatus rv = SECSuccess;

    if (inLen == 0) {
        return SECSuccess;
    }
    if (!in) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* if we ran out of session, we need to restore our previously stored
     * state.
     */
    context->init = PR_FALSE;
    PK11_EnterContextMonitor(context);
    if (!context->ownSession) {
        rv = pk11_restoreContext(context, context->savedData,
                                 context->savedLength);
        if (rv != SECSuccess) {
            PK11_ExitContextMonitor(context);
            return rv;
        }
    }

    switch (context->operation) {
        /* also for MAC'ing */
        case CKA_SIGN:
            crv = PK11_GETTAB(context->slot)->C_SignUpdate(context->session, (unsigned char *)in, inLen);
            break;
        case CKA_VERIFY:
            crv = PK11_GETTAB(context->slot)->C_VerifyUpdate(context->session, (unsigned char *)in, inLen);
            break;
        case CKA_DIGEST:
            crv = PK11_GETTAB(context->slot)->C_DigestUpdate(context->session, (unsigned char *)in, inLen);
            break;
        default:
            crv = CKR_OPERATION_NOT_INITIALIZED;
            break;
    }

    if (crv != CKR_OK) {
        PORT_SetError(PK11_MapError(crv));
        rv = SECFailure;
    }

    /*
     * handle session starvation case.. use our last session to multiplex
     */
    if (!context->ownSession) {
        context->savedData = pk11_saveContext(context, context->savedData,
                                              &context->savedLength);
        if (context->savedData == NULL)
            rv = SECFailure;

        /* clear out out session for others to use */
        pk11_Finalize(context);
    }
    PK11_ExitContextMonitor(context);
    return rv;
}

/*
 * Digest a key if possible./
 */
SECStatus
PK11_DigestKey(PK11Context *context, PK11SymKey *key)
{
    CK_RV crv = CKR_OK;
    SECStatus rv = SECSuccess;
    PK11SymKey *newKey = NULL;

    if (!context || !key) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* if we ran out of session, we need to restore our previously stored
     * state.
     */
    if (context->slot != key->slot) {
        newKey = pk11_CopyToSlot(context->slot, CKM_SSL3_SHA1_MAC, CKA_SIGN, key);
    } else {
        newKey = PK11_ReferenceSymKey(key);
    }

    context->init = PR_FALSE;
    PK11_EnterContextMonitor(context);
    if (!context->ownSession) {
        rv = pk11_restoreContext(context, context->savedData,
                                 context->savedLength);
        if (rv != SECSuccess) {
            PK11_ExitContextMonitor(context);
            PK11_FreeSymKey(newKey);
            return rv;
        }
    }

    if (newKey == NULL) {
        crv = CKR_KEY_TYPE_INCONSISTENT;
        if (key->data.data) {
            crv = PK11_GETTAB(context->slot)->C_DigestUpdate(context->session, key->data.data, key->data.len);
        }
    } else {
        crv = PK11_GETTAB(context->slot)->C_DigestKey(context->session, newKey->objectID);
    }

    if (crv != CKR_OK) {
        PORT_SetError(PK11_MapError(crv));
        rv = SECFailure;
    }

    /*
     * handle session starvation case.. use our last session to multiplex
     */
    if (!context->ownSession) {
        context->savedData = pk11_saveContext(context, context->savedData,
                                              &context->savedLength);
        if (context->savedData == NULL)
            rv = SECFailure;

        /* clear out out session for others to use */
        pk11_Finalize(context);
    }
    PK11_ExitContextMonitor(context);
    if (newKey)
        PK11_FreeSymKey(newKey);
    return rv;
}

/*
 * externally callable version of the lowercase pk11_finalize().
 */
SECStatus
PK11_Finalize(PK11Context *context)
{
    SECStatus rv;

    PK11_EnterContextMonitor(context);
    rv = pk11_Finalize(context);
    PK11_ExitContextMonitor(context);
    return rv;
}

/*
 * clean up a cipher operation, so the session can be used by
 * someone new.
 */
SECStatus
pk11_Finalize(PK11Context *context)
{
    CK_ULONG count = 0;
    CK_RV crv;
    unsigned char stackBuf[256];
    unsigned char *buffer = NULL;

    if (!context->ownSession) {
        return SECSuccess;
    }

finalize:
    switch (context->operation) {
        case CKA_ENCRYPT:
            crv = PK11_GETTAB(context->slot)->C_EncryptFinal(context->session, buffer, &count);
            break;
        case CKA_DECRYPT:
            crv = PK11_GETTAB(context->slot)->C_DecryptFinal(context->session, buffer, &count);
            break;
        case CKA_SIGN:
            crv = PK11_GETTAB(context->slot)->C_SignFinal(context->session, buffer, &count);
            break;
        case CKA_VERIFY:
            crv = PK11_GETTAB(context->slot)->C_VerifyFinal(context->session, buffer, count);
            break;
        case CKA_DIGEST:
            crv = PK11_GETTAB(context->slot)->C_DigestFinal(context->session, buffer, &count);
            break;
        case CKA_NSS_MESSAGE | CKA_ENCRYPT:
            crv = PK11_GETTAB(context->slot)->C_MessageEncryptFinal(context->session);
            break;
        case CKA_NSS_MESSAGE | CKA_DECRYPT:
            crv = PK11_GETTAB(context->slot)->C_MessageDecryptFinal(context->session);
            break;
        case CKA_NSS_MESSAGE | CKA_SIGN:
            crv = PK11_GETTAB(context->slot)->C_MessageSignFinal(context->session);
            break;
        case CKA_NSS_MESSAGE | CKA_VERIFY:
            crv = PK11_GETTAB(context->slot)->C_MessageVerifyFinal(context->session);
            break;
        default:
            crv = CKR_OPERATION_NOT_INITIALIZED;
            break;
    }

    if (crv != CKR_OK) {
        if (buffer != stackBuf) {
            PORT_Free(buffer);
        }
        if (crv == CKR_OPERATION_NOT_INITIALIZED) {
            /* if there's no operation, it is finalized */
            return SECSuccess;
        }
        PORT_SetError(PK11_MapError(crv));
        return SECFailure;
    }

    /* Message interface does not need to allocate a final buffer */
    if (((context->operation) & CKA_NSS_MESSAGE_MASK) == CKA_NSS_MESSAGE) {
        return SECSuccess;
    }

    /* try to finalize the session with a buffer */
    if (buffer == NULL) {
        if (count <= sizeof stackBuf) {
            buffer = stackBuf;
        } else {
            buffer = PORT_Alloc(count);
            if (buffer == NULL) {
                return SECFailure;
            }
        }
        goto finalize;
    }
    if (buffer != stackBuf) {
        PORT_Free(buffer);
    }
    return SECSuccess;
}

/*
 *  Return the final digested or signed data...
 *  this routine can either take pre initialized data, or allocate data
 *  either out of an arena or out of the standard heap.
 */
SECStatus
PK11_DigestFinal(PK11Context *context, unsigned char *data,
                 unsigned int *outLen, unsigned int length)
{
    CK_ULONG len;
    CK_RV crv;
    SECStatus rv;

    /* message interface returns no data on Final, Should not use DigestFinal
     * in this case */
    if (((context->operation) & CKA_NSS_MESSAGE_MASK) == CKA_NSS_MESSAGE) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* if we ran out of session, we need to restore our previously stored
     * state.
     */
    PK11_EnterContextMonitor(context);
    if (!context->ownSession) {
        rv = pk11_restoreContext(context, context->savedData,
                                 context->savedLength);
        if (rv != SECSuccess) {
            PK11_ExitContextMonitor(context);
            return rv;
        }
    }

    len = length;
    switch (context->operation) {
        case CKA_SIGN:
            crv = PK11_GETTAB(context->slot)->C_SignFinal(context->session, data, &len);
            break;
        case CKA_VERIFY:
            crv = PK11_GETTAB(context->slot)->C_VerifyFinal(context->session, data, len);
            break;
        case CKA_DIGEST:
            crv = PK11_GETTAB(context->slot)->C_DigestFinal(context->session, data, &len);
            break;
        case CKA_ENCRYPT:
            crv = PK11_GETTAB(context->slot)->C_EncryptFinal(context->session, data, &len);
            break;
        case CKA_DECRYPT:
            crv = PK11_GETTAB(context->slot)->C_DecryptFinal(context->session, data, &len);
            break;
        default:
            crv = CKR_OPERATION_NOT_INITIALIZED;
            break;
    }
    PK11_ExitContextMonitor(context);

    context->init = PR_FALSE; /* allow Begin to start up again */

    if (crv != CKR_OK) {
        PORT_SetError(PK11_MapError(crv));
        return SECFailure;
    }
    *outLen = (unsigned int)len;
    return SECSuccess;
}
