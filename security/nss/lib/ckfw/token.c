/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * token.c
 *
 * This file implements the NSSCKFWToken type and methods.
 */

#ifndef CK_T
#include "ck.h"
#endif /* CK_T */

/*
 * NSSCKFWToken
 *
 *  -- create/destroy --
 *  nssCKFWToken_Create
 *  nssCKFWToken_Destroy
 *
 *  -- public accessors --
 *  NSSCKFWToken_GetMDToken
 *  NSSCKFWToken_GetFWSlot
 *  NSSCKFWToken_GetMDSlot
 *  NSSCKFWToken_GetSessionState
 *
 *  -- implement public accessors --
 *  nssCKFWToken_GetMDToken
 *  nssCKFWToken_GetFWSlot
 *  nssCKFWToken_GetMDSlot
 *  nssCKFWToken_GetSessionState
 *  nssCKFWToken_SetSessionState
 *
 *  -- private accessors --
 *  nssCKFWToken_SetSessionState
 *  nssCKFWToken_RemoveSession
 *  nssCKFWToken_CloseAllSessions
 *  nssCKFWToken_GetSessionCount
 *  nssCKFWToken_GetRwSessionCount
 *  nssCKFWToken_GetRoSessionCount
 *  nssCKFWToken_GetSessionObjectHash
 *  nssCKFWToken_GetMDObjectHash
 *  nssCKFWToken_GetObjectHandleHash
 *
 *  -- module fronts --
 *  nssCKFWToken_InitToken
 *  nssCKFWToken_GetLabel
 *  nssCKFWToken_GetManufacturerID
 *  nssCKFWToken_GetModel
 *  nssCKFWToken_GetSerialNumber
 *  nssCKFWToken_GetHasRNG
 *  nssCKFWToken_GetIsWriteProtected
 *  nssCKFWToken_GetLoginRequired
 *  nssCKFWToken_GetUserPinInitialized
 *  nssCKFWToken_GetRestoreKeyNotNeeded
 *  nssCKFWToken_GetHasClockOnToken
 *  nssCKFWToken_GetHasProtectedAuthenticationPath
 *  nssCKFWToken_GetSupportsDualCryptoOperations
 *  nssCKFWToken_GetMaxSessionCount
 *  nssCKFWToken_GetMaxRwSessionCount
 *  nssCKFWToken_GetMaxPinLen
 *  nssCKFWToken_GetMinPinLen
 *  nssCKFWToken_GetTotalPublicMemory
 *  nssCKFWToken_GetFreePublicMemory
 *  nssCKFWToken_GetTotalPrivateMemory
 *  nssCKFWToken_GetFreePrivateMemory
 *  nssCKFWToken_GetHardwareVersion
 *  nssCKFWToken_GetFirmwareVersion
 *  nssCKFWToken_GetUTCTime
 *  nssCKFWToken_OpenSession
 *  nssCKFWToken_GetMechanismCount
 *  nssCKFWToken_GetMechanismTypes
 *  nssCKFWToken_GetMechanism
 */

struct NSSCKFWTokenStr {
    NSSCKFWMutex *mutex;
    NSSArena *arena;
    NSSCKMDToken *mdToken;
    NSSCKFWSlot *fwSlot;
    NSSCKMDSlot *mdSlot;
    NSSCKFWInstance *fwInstance;
    NSSCKMDInstance *mdInstance;

    /*
     * Everything above is set at creation time, and then not modified.
     * The invariants the mutex protects are:
     *
     * 1) Each of the cached descriptions (versions, etc.) are in an
     *    internally consistant state.
     *
     * 2) The session counts and hashes are consistant.
     *
     * 3) The object hashes are consistant.
     *
     * Note that the calls accessing the cached descriptions will call
     * the NSSCKMDToken methods with the mutex locked.  Those methods
     * may then call the public NSSCKFWToken routines.  Those public
     * routines only access the constant data above and the atomic
     * CK_STATE session state variable below, so there's no problem.
     * But be careful if you add to this object; mutexes are in
     * general not reentrant, so don't create deadlock situations.
     */

    NSSUTF8 *label;
    NSSUTF8 *manufacturerID;
    NSSUTF8 *model;
    NSSUTF8 *serialNumber;
    CK_VERSION hardwareVersion;
    CK_VERSION firmwareVersion;

    CK_ULONG sessionCount;
    CK_ULONG rwSessionCount;
    nssCKFWHash *sessions;
    nssCKFWHash *sessionObjectHash;
    nssCKFWHash *mdObjectHash;
    nssCKFWHash *mdMechanismHash;

    CK_STATE state;
};

#ifdef DEBUG
/*
 * But first, the pointer-tracking stuff.
 *
 * NOTE: the pointer-tracking support in NSS/base currently relies
 * upon NSPR's CallOnce support.  That, however, relies upon NSPR's
 * locking, which is tied into the runtime.  We need a pointer-tracker
 * implementation that uses the locks supplied through C_Initialize.
 * That support, however, can be filled in later.  So for now, I'll
 * just do this routines as no-ops.
 */

static CK_RV
token_add_pointer(
    const NSSCKFWToken *fwToken)
{
    return CKR_OK;
}

static CK_RV
token_remove_pointer(
    const NSSCKFWToken *fwToken)
{
    return CKR_OK;
}

NSS_IMPLEMENT CK_RV
nssCKFWToken_verifyPointer(
    const NSSCKFWToken *fwToken)
{
    return CKR_OK;
}

#endif /* DEBUG */

/*
 * nssCKFWToken_Create
 *
 */
NSS_IMPLEMENT NSSCKFWToken *
nssCKFWToken_Create(
    NSSCKFWSlot *fwSlot,
    NSSCKMDToken *mdToken,
    CK_RV *pError)
{
    NSSArena *arena = (NSSArena *)NULL;
    NSSCKFWToken *fwToken = (NSSCKFWToken *)NULL;
    CK_BBOOL called_setup = CK_FALSE;

    /*
     * We have already verified the arguments in nssCKFWSlot_GetToken.
     */

    arena = NSSArena_Create();
    if (!arena) {
        *pError = CKR_HOST_MEMORY;
        goto loser;
    }

    fwToken = nss_ZNEW(arena, NSSCKFWToken);
    if (!fwToken) {
        *pError = CKR_HOST_MEMORY;
        goto loser;
    }

    fwToken->arena = arena;
    fwToken->mdToken = mdToken;
    fwToken->fwSlot = fwSlot;
    fwToken->fwInstance = nssCKFWSlot_GetFWInstance(fwSlot);
    fwToken->mdInstance = nssCKFWSlot_GetMDInstance(fwSlot);
    fwToken->state = CKS_RO_PUBLIC_SESSION; /* some default */
    fwToken->sessionCount = 0;
    fwToken->rwSessionCount = 0;

    fwToken->mutex = nssCKFWInstance_CreateMutex(fwToken->fwInstance, arena, pError);
    if (!fwToken->mutex) {
        if (CKR_OK == *pError) {
            *pError = CKR_GENERAL_ERROR;
        }
        goto loser;
    }

    fwToken->sessions = nssCKFWHash_Create(fwToken->fwInstance, arena, pError);
    if (!fwToken->sessions) {
        if (CKR_OK == *pError) {
            *pError = CKR_GENERAL_ERROR;
        }
        goto loser;
    }

    if (CK_TRUE != nssCKFWInstance_GetModuleHandlesSessionObjects(
                       fwToken->fwInstance)) {
        fwToken->sessionObjectHash = nssCKFWHash_Create(fwToken->fwInstance,
                                                        arena, pError);
        if (!fwToken->sessionObjectHash) {
            if (CKR_OK == *pError) {
                *pError = CKR_GENERAL_ERROR;
            }
            goto loser;
        }
    }

    fwToken->mdObjectHash = nssCKFWHash_Create(fwToken->fwInstance,
                                               arena, pError);
    if (!fwToken->mdObjectHash) {
        if (CKR_OK == *pError) {
            *pError = CKR_GENERAL_ERROR;
        }
        goto loser;
    }

    fwToken->mdMechanismHash = nssCKFWHash_Create(fwToken->fwInstance,
                                                  arena, pError);
    if (!fwToken->mdMechanismHash) {
        if (CKR_OK == *pError) {
            *pError = CKR_GENERAL_ERROR;
        }
        goto loser;
    }

    /* More here */

    if (mdToken->Setup) {
        *pError = mdToken->Setup(mdToken, fwToken, fwToken->mdInstance, fwToken->fwInstance);
        if (CKR_OK != *pError) {
            goto loser;
        }
    }

    called_setup = CK_TRUE;

#ifdef DEBUG
    *pError = token_add_pointer(fwToken);
    if (CKR_OK != *pError) {
        goto loser;
    }
#endif /* DEBUG */

    *pError = CKR_OK;
    return fwToken;

loser:

    if (CK_TRUE == called_setup) {
        if (mdToken->Invalidate) {
            mdToken->Invalidate(mdToken, fwToken, fwToken->mdInstance, fwToken->fwInstance);
        }
    }

    if (arena) {
        (void)NSSArena_Destroy(arena);
    }

    return (NSSCKFWToken *)NULL;
}

static void
nss_ckfwtoken_session_iterator(
    const void *key,
    void *value,
    void *closure)
{
    /*
     * Remember that the fwToken->mutex is locked
     */
    NSSCKFWSession *fwSession = (NSSCKFWSession *)value;
    (void)nssCKFWSession_Destroy(fwSession, CK_FALSE);
    return;
}

static void
nss_ckfwtoken_object_iterator(
    const void *key,
    void *value,
    void *closure)
{
    /*
     * Remember that the fwToken->mutex is locked
     */
    NSSCKFWObject *fwObject = (NSSCKFWObject *)value;
    (void)nssCKFWObject_Finalize(fwObject, CK_FALSE);
    return;
}

/*
 * nssCKFWToken_Destroy
 *
 */
NSS_IMPLEMENT CK_RV
nssCKFWToken_Destroy(
    NSSCKFWToken *fwToken)
{
    CK_RV error = CKR_OK;

#ifdef NSSDEBUG
    error = nssCKFWToken_verifyPointer(fwToken);
    if (CKR_OK != error) {
        return error;
    }
#endif /* NSSDEBUG */

    (void)nssCKFWMutex_Destroy(fwToken->mutex);

    if (fwToken->mdToken->Invalidate) {
        fwToken->mdToken->Invalidate(fwToken->mdToken, fwToken,
                                     fwToken->mdInstance, fwToken->fwInstance);
    }
    /* we can destroy the list without locking now because no one else is
     * referencing us (or _Destroy was invalidly called!)
     */
    nssCKFWHash_Iterate(fwToken->sessions, nss_ckfwtoken_session_iterator,
                        (void *)NULL);
    nssCKFWHash_Destroy(fwToken->sessions);

    /* session objects go away when their sessions are removed */
    if (fwToken->sessionObjectHash) {
        nssCKFWHash_Destroy(fwToken->sessionObjectHash);
    }

    /* free up the token objects */
    if (fwToken->mdObjectHash) {
        nssCKFWHash_Iterate(fwToken->mdObjectHash, nss_ckfwtoken_object_iterator,
                            (void *)NULL);
        nssCKFWHash_Destroy(fwToken->mdObjectHash);
    }
    if (fwToken->mdMechanismHash) {
        nssCKFWHash_Destroy(fwToken->mdMechanismHash);
    }

    nssCKFWSlot_ClearToken(fwToken->fwSlot);

#ifdef DEBUG
    error = token_remove_pointer(fwToken);
#endif /* DEBUG */

    (void)NSSArena_Destroy(fwToken->arena);
    return error;
}

/*
 * nssCKFWToken_GetMDToken
 *
 */
NSS_IMPLEMENT NSSCKMDToken *
nssCKFWToken_GetMDToken(
    NSSCKFWToken *fwToken)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return (NSSCKMDToken *)NULL;
    }
#endif /* NSSDEBUG */

    return fwToken->mdToken;
}

/*
 * nssCKFWToken_GetArena
 *
 */
NSS_IMPLEMENT NSSArena *
nssCKFWToken_GetArena(
    NSSCKFWToken *fwToken,
    CK_RV *pError)
{
#ifdef NSSDEBUG
    if (!pError) {
        return (NSSArena *)NULL;
    }

    *pError = nssCKFWToken_verifyPointer(fwToken);
    if (CKR_OK != *pError) {
        return (NSSArena *)NULL;
    }
#endif /* NSSDEBUG */

    return fwToken->arena;
}

/*
 * nssCKFWToken_GetFWSlot
 *
 */
NSS_IMPLEMENT NSSCKFWSlot *
nssCKFWToken_GetFWSlot(
    NSSCKFWToken *fwToken)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return (NSSCKFWSlot *)NULL;
    }
#endif /* NSSDEBUG */

    return fwToken->fwSlot;
}

/*
 * nssCKFWToken_GetMDSlot
 *
 */
NSS_IMPLEMENT NSSCKMDSlot *
nssCKFWToken_GetMDSlot(
    NSSCKFWToken *fwToken)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return (NSSCKMDSlot *)NULL;
    }
#endif /* NSSDEBUG */

    return fwToken->mdSlot;
}

/*
 * nssCKFWToken_GetSessionState
 *
 */
NSS_IMPLEMENT CK_STATE
nssCKFWToken_GetSessionState(
    NSSCKFWToken *fwToken)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return CKS_RO_PUBLIC_SESSION; /* whatever */
    }
#endif /* NSSDEBUG */

    /*
     * BTW, do not lock the token in this method.
     */

    /*
     * Theoretically, there is no state if there aren't any
     * sessions open.  But then we'd need to worry about
     * reporting an error, etc.  What the heck-- let's just
     * revert to CKR_RO_PUBLIC_SESSION as the "default."
     */

    return fwToken->state;
}

/*
 * nssCKFWToken_InitToken
 *
 */
NSS_IMPLEMENT CK_RV
nssCKFWToken_InitToken(
    NSSCKFWToken *fwToken,
    NSSItem *pin,
    NSSUTF8 *label)
{
    CK_RV error;

#ifdef NSSDEBUG
    error = nssCKFWToken_verifyPointer(fwToken);
    if (CKR_OK != error) {
        return CKR_ARGUMENTS_BAD;
    }
#endif /* NSSDEBUG */

    error = nssCKFWMutex_Lock(fwToken->mutex);
    if (CKR_OK != error) {
        return error;
    }

    if (fwToken->sessionCount > 0) {
        error = CKR_SESSION_EXISTS;
        goto done;
    }

    if (!fwToken->mdToken->InitToken) {
        error = CKR_DEVICE_ERROR;
        goto done;
    }

    if (!pin) {
        if (nssCKFWToken_GetHasProtectedAuthenticationPath(fwToken)) {
            ; /* okay */
        } else {
            error = CKR_PIN_INCORRECT;
            goto done;
        }
    }

    if (!label) {
        label = (NSSUTF8 *)"";
    }

    error = fwToken->mdToken->InitToken(fwToken->mdToken, fwToken,
                                        fwToken->mdInstance, fwToken->fwInstance, pin, label);

done:
    (void)nssCKFWMutex_Unlock(fwToken->mutex);
    return error;
}

/*
 * nssCKFWToken_GetLabel
 *
 */
NSS_IMPLEMENT CK_RV
nssCKFWToken_GetLabel(
    NSSCKFWToken *fwToken,
    CK_CHAR label[32])
{
    CK_RV error = CKR_OK;

#ifdef NSSDEBUG
    if ((CK_CHAR_PTR)NULL == label) {
        return CKR_ARGUMENTS_BAD;
    }

    error = nssCKFWToken_verifyPointer(fwToken);
    if (CKR_OK != error) {
        return error;
    }
#endif /* NSSDEBUG */

    error = nssCKFWMutex_Lock(fwToken->mutex);
    if (CKR_OK != error) {
        return error;
    }

    if (!fwToken->label) {
        if (fwToken->mdToken->GetLabel) {
            fwToken->label = fwToken->mdToken->GetLabel(fwToken->mdToken, fwToken,
                                                        fwToken->mdInstance, fwToken->fwInstance, &error);
            if ((!fwToken->label) && (CKR_OK != error)) {
                goto done;
            }
        } else {
            fwToken->label = (NSSUTF8 *)"";
        }
    }

    (void)nssUTF8_CopyIntoFixedBuffer(fwToken->label, (char *)label, 32, ' ');
    error = CKR_OK;

done:
    (void)nssCKFWMutex_Unlock(fwToken->mutex);
    return error;
}

/*
 * nssCKFWToken_GetManufacturerID
 *
 */
NSS_IMPLEMENT CK_RV
nssCKFWToken_GetManufacturerID(
    NSSCKFWToken *fwToken,
    CK_CHAR manufacturerID[32])
{
    CK_RV error = CKR_OK;

#ifdef NSSDEBUG
    if ((CK_CHAR_PTR)NULL == manufacturerID) {
        return CKR_ARGUMENTS_BAD;
    }

    error = nssCKFWToken_verifyPointer(fwToken);
    if (CKR_OK != error) {
        return error;
    }
#endif /* NSSDEBUG */

    error = nssCKFWMutex_Lock(fwToken->mutex);
    if (CKR_OK != error) {
        return error;
    }

    if (!fwToken->manufacturerID) {
        if (fwToken->mdToken->GetManufacturerID) {
            fwToken->manufacturerID = fwToken->mdToken->GetManufacturerID(fwToken->mdToken,
                                                                          fwToken, fwToken->mdInstance, fwToken->fwInstance, &error);
            if ((!fwToken->manufacturerID) && (CKR_OK != error)) {
                goto done;
            }
        } else {
            fwToken->manufacturerID = (NSSUTF8 *)"";
        }
    }

    (void)nssUTF8_CopyIntoFixedBuffer(fwToken->manufacturerID, (char *)manufacturerID, 32, ' ');
    error = CKR_OK;

done:
    (void)nssCKFWMutex_Unlock(fwToken->mutex);
    return error;
}

/*
 * nssCKFWToken_GetModel
 *
 */
NSS_IMPLEMENT CK_RV
nssCKFWToken_GetModel(
    NSSCKFWToken *fwToken,
    CK_CHAR model[16])
{
    CK_RV error = CKR_OK;

#ifdef NSSDEBUG
    if ((CK_CHAR_PTR)NULL == model) {
        return CKR_ARGUMENTS_BAD;
    }

    error = nssCKFWToken_verifyPointer(fwToken);
    if (CKR_OK != error) {
        return error;
    }
#endif /* NSSDEBUG */

    error = nssCKFWMutex_Lock(fwToken->mutex);
    if (CKR_OK != error) {
        return error;
    }

    if (!fwToken->model) {
        if (fwToken->mdToken->GetModel) {
            fwToken->model = fwToken->mdToken->GetModel(fwToken->mdToken, fwToken,
                                                        fwToken->mdInstance, fwToken->fwInstance, &error);
            if ((!fwToken->model) && (CKR_OK != error)) {
                goto done;
            }
        } else {
            fwToken->model = (NSSUTF8 *)"";
        }
    }

    (void)nssUTF8_CopyIntoFixedBuffer(fwToken->model, (char *)model, 16, ' ');
    error = CKR_OK;

done:
    (void)nssCKFWMutex_Unlock(fwToken->mutex);
    return error;
}

/*
 * nssCKFWToken_GetSerialNumber
 *
 */
NSS_IMPLEMENT CK_RV
nssCKFWToken_GetSerialNumber(
    NSSCKFWToken *fwToken,
    CK_CHAR serialNumber[16])
{
    CK_RV error = CKR_OK;

#ifdef NSSDEBUG
    if ((CK_CHAR_PTR)NULL == serialNumber) {
        return CKR_ARGUMENTS_BAD;
    }

    error = nssCKFWToken_verifyPointer(fwToken);
    if (CKR_OK != error) {
        return error;
    }
#endif /* NSSDEBUG */

    error = nssCKFWMutex_Lock(fwToken->mutex);
    if (CKR_OK != error) {
        return error;
    }

    if (!fwToken->serialNumber) {
        if (fwToken->mdToken->GetSerialNumber) {
            fwToken->serialNumber = fwToken->mdToken->GetSerialNumber(fwToken->mdToken,
                                                                      fwToken, fwToken->mdInstance, fwToken->fwInstance, &error);
            if ((!fwToken->serialNumber) && (CKR_OK != error)) {
                goto done;
            }
        } else {
            fwToken->serialNumber = (NSSUTF8 *)"";
        }
    }

    (void)nssUTF8_CopyIntoFixedBuffer(fwToken->serialNumber, (char *)serialNumber, 16, ' ');
    error = CKR_OK;

done:
    (void)nssCKFWMutex_Unlock(fwToken->mutex);
    return error;
}

/*
 * nssCKFWToken_GetHasRNG
 *
 */
NSS_IMPLEMENT CK_BBOOL
nssCKFWToken_GetHasRNG(
    NSSCKFWToken *fwToken)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return CK_FALSE;
    }
#endif /* NSSDEBUG */

    if (!fwToken->mdToken->GetHasRNG) {
        return CK_FALSE;
    }

    return fwToken->mdToken->GetHasRNG(fwToken->mdToken, fwToken,
                                       fwToken->mdInstance, fwToken->fwInstance);
}

/*
 * nssCKFWToken_GetIsWriteProtected
 *
 */
NSS_IMPLEMENT CK_BBOOL
nssCKFWToken_GetIsWriteProtected(
    NSSCKFWToken *fwToken)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return CK_FALSE;
    }
#endif /* NSSDEBUG */

    if (!fwToken->mdToken->GetIsWriteProtected) {
        return CK_FALSE;
    }

    return fwToken->mdToken->GetIsWriteProtected(fwToken->mdToken, fwToken,
                                                 fwToken->mdInstance, fwToken->fwInstance);
}

/*
 * nssCKFWToken_GetLoginRequired
 *
 */
NSS_IMPLEMENT CK_BBOOL
nssCKFWToken_GetLoginRequired(
    NSSCKFWToken *fwToken)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return CK_FALSE;
    }
#endif /* NSSDEBUG */

    if (!fwToken->mdToken->GetLoginRequired) {
        return CK_FALSE;
    }

    return fwToken->mdToken->GetLoginRequired(fwToken->mdToken, fwToken,
                                              fwToken->mdInstance, fwToken->fwInstance);
}

/*
 * nssCKFWToken_GetUserPinInitialized
 *
 */
NSS_IMPLEMENT CK_BBOOL
nssCKFWToken_GetUserPinInitialized(
    NSSCKFWToken *fwToken)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return CK_FALSE;
    }
#endif /* NSSDEBUG */

    if (!fwToken->mdToken->GetUserPinInitialized) {
        return CK_FALSE;
    }

    return fwToken->mdToken->GetUserPinInitialized(fwToken->mdToken, fwToken,
                                                   fwToken->mdInstance, fwToken->fwInstance);
}

/*
 * nssCKFWToken_GetRestoreKeyNotNeeded
 *
 */
NSS_IMPLEMENT CK_BBOOL
nssCKFWToken_GetRestoreKeyNotNeeded(
    NSSCKFWToken *fwToken)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return CK_FALSE;
    }
#endif /* NSSDEBUG */

    if (!fwToken->mdToken->GetRestoreKeyNotNeeded) {
        return CK_FALSE;
    }

    return fwToken->mdToken->GetRestoreKeyNotNeeded(fwToken->mdToken, fwToken,
                                                    fwToken->mdInstance, fwToken->fwInstance);
}

/*
 * nssCKFWToken_GetHasClockOnToken
 *
 */
NSS_IMPLEMENT CK_BBOOL
nssCKFWToken_GetHasClockOnToken(
    NSSCKFWToken *fwToken)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return CK_FALSE;
    }
#endif /* NSSDEBUG */

    if (!fwToken->mdToken->GetHasClockOnToken) {
        return CK_FALSE;
    }

    return fwToken->mdToken->GetHasClockOnToken(fwToken->mdToken, fwToken,
                                                fwToken->mdInstance, fwToken->fwInstance);
}

/*
 * nssCKFWToken_GetHasProtectedAuthenticationPath
 *
 */
NSS_IMPLEMENT CK_BBOOL
nssCKFWToken_GetHasProtectedAuthenticationPath(
    NSSCKFWToken *fwToken)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return CK_FALSE;
    }
#endif /* NSSDEBUG */

    if (!fwToken->mdToken->GetHasProtectedAuthenticationPath) {
        return CK_FALSE;
    }

    return fwToken->mdToken->GetHasProtectedAuthenticationPath(fwToken->mdToken,
                                                               fwToken, fwToken->mdInstance, fwToken->fwInstance);
}

/*
 * nssCKFWToken_GetSupportsDualCryptoOperations
 *
 */
NSS_IMPLEMENT CK_BBOOL
nssCKFWToken_GetSupportsDualCryptoOperations(
    NSSCKFWToken *fwToken)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return CK_FALSE;
    }
#endif /* NSSDEBUG */

    if (!fwToken->mdToken->GetSupportsDualCryptoOperations) {
        return CK_FALSE;
    }

    return fwToken->mdToken->GetSupportsDualCryptoOperations(fwToken->mdToken,
                                                             fwToken, fwToken->mdInstance, fwToken->fwInstance);
}

/*
 * nssCKFWToken_GetMaxSessionCount
 *
 */
NSS_IMPLEMENT CK_ULONG
nssCKFWToken_GetMaxSessionCount(
    NSSCKFWToken *fwToken)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return CK_UNAVAILABLE_INFORMATION;
    }
#endif /* NSSDEBUG */

    if (!fwToken->mdToken->GetMaxSessionCount) {
        return CK_UNAVAILABLE_INFORMATION;
    }

    return fwToken->mdToken->GetMaxSessionCount(fwToken->mdToken, fwToken,
                                                fwToken->mdInstance, fwToken->fwInstance);
}

/*
 * nssCKFWToken_GetMaxRwSessionCount
 *
 */
NSS_IMPLEMENT CK_ULONG
nssCKFWToken_GetMaxRwSessionCount(
    NSSCKFWToken *fwToken)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return CK_UNAVAILABLE_INFORMATION;
    }
#endif /* NSSDEBUG */

    if (!fwToken->mdToken->GetMaxRwSessionCount) {
        return CK_UNAVAILABLE_INFORMATION;
    }

    return fwToken->mdToken->GetMaxRwSessionCount(fwToken->mdToken, fwToken,
                                                  fwToken->mdInstance, fwToken->fwInstance);
}

/*
 * nssCKFWToken_GetMaxPinLen
 *
 */
NSS_IMPLEMENT CK_ULONG
nssCKFWToken_GetMaxPinLen(
    NSSCKFWToken *fwToken)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return CK_UNAVAILABLE_INFORMATION;
    }
#endif /* NSSDEBUG */

    if (!fwToken->mdToken->GetMaxPinLen) {
        return CK_UNAVAILABLE_INFORMATION;
    }

    return fwToken->mdToken->GetMaxPinLen(fwToken->mdToken, fwToken,
                                          fwToken->mdInstance, fwToken->fwInstance);
}

/*
 * nssCKFWToken_GetMinPinLen
 *
 */
NSS_IMPLEMENT CK_ULONG
nssCKFWToken_GetMinPinLen(
    NSSCKFWToken *fwToken)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return CK_UNAVAILABLE_INFORMATION;
    }
#endif /* NSSDEBUG */

    if (!fwToken->mdToken->GetMinPinLen) {
        return CK_UNAVAILABLE_INFORMATION;
    }

    return fwToken->mdToken->GetMinPinLen(fwToken->mdToken, fwToken,
                                          fwToken->mdInstance, fwToken->fwInstance);
}

/*
 * nssCKFWToken_GetTotalPublicMemory
 *
 */
NSS_IMPLEMENT CK_ULONG
nssCKFWToken_GetTotalPublicMemory(
    NSSCKFWToken *fwToken)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return CK_UNAVAILABLE_INFORMATION;
    }
#endif /* NSSDEBUG */

    if (!fwToken->mdToken->GetTotalPublicMemory) {
        return CK_UNAVAILABLE_INFORMATION;
    }

    return fwToken->mdToken->GetTotalPublicMemory(fwToken->mdToken, fwToken,
                                                  fwToken->mdInstance, fwToken->fwInstance);
}

/*
 * nssCKFWToken_GetFreePublicMemory
 *
 */
NSS_IMPLEMENT CK_ULONG
nssCKFWToken_GetFreePublicMemory(
    NSSCKFWToken *fwToken)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return CK_UNAVAILABLE_INFORMATION;
    }
#endif /* NSSDEBUG */

    if (!fwToken->mdToken->GetFreePublicMemory) {
        return CK_UNAVAILABLE_INFORMATION;
    }

    return fwToken->mdToken->GetFreePublicMemory(fwToken->mdToken, fwToken,
                                                 fwToken->mdInstance, fwToken->fwInstance);
}

/*
 * nssCKFWToken_GetTotalPrivateMemory
 *
 */
NSS_IMPLEMENT CK_ULONG
nssCKFWToken_GetTotalPrivateMemory(
    NSSCKFWToken *fwToken)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return CK_UNAVAILABLE_INFORMATION;
    }
#endif /* NSSDEBUG */

    if (!fwToken->mdToken->GetTotalPrivateMemory) {
        return CK_UNAVAILABLE_INFORMATION;
    }

    return fwToken->mdToken->GetTotalPrivateMemory(fwToken->mdToken, fwToken,
                                                   fwToken->mdInstance, fwToken->fwInstance);
}

/*
 * nssCKFWToken_GetFreePrivateMemory
 *
 */
NSS_IMPLEMENT CK_ULONG
nssCKFWToken_GetFreePrivateMemory(
    NSSCKFWToken *fwToken)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return CK_UNAVAILABLE_INFORMATION;
    }
#endif /* NSSDEBUG */

    if (!fwToken->mdToken->GetFreePrivateMemory) {
        return CK_UNAVAILABLE_INFORMATION;
    }

    return fwToken->mdToken->GetFreePrivateMemory(fwToken->mdToken, fwToken,
                                                  fwToken->mdInstance, fwToken->fwInstance);
}

/*
 * nssCKFWToken_GetHardwareVersion
 *
 */
NSS_IMPLEMENT CK_VERSION
nssCKFWToken_GetHardwareVersion(
    NSSCKFWToken *fwToken)
{
    CK_VERSION rv;

#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        rv.major = rv.minor = 0;
        return rv;
    }
#endif /* NSSDEBUG */

    if (CKR_OK != nssCKFWMutex_Lock(fwToken->mutex)) {
        rv.major = rv.minor = 0;
        return rv;
    }

    if ((0 != fwToken->hardwareVersion.major) ||
        (0 != fwToken->hardwareVersion.minor)) {
        rv = fwToken->hardwareVersion;
        goto done;
    }

    if (fwToken->mdToken->GetHardwareVersion) {
        fwToken->hardwareVersion = fwToken->mdToken->GetHardwareVersion(
            fwToken->mdToken, fwToken, fwToken->mdInstance, fwToken->fwInstance);
    } else {
        fwToken->hardwareVersion.major = 0;
        fwToken->hardwareVersion.minor = 1;
    }

    rv = fwToken->hardwareVersion;

done:
    (void)nssCKFWMutex_Unlock(fwToken->mutex);
    return rv;
}

/*
 * nssCKFWToken_GetFirmwareVersion
 *
 */
NSS_IMPLEMENT CK_VERSION
nssCKFWToken_GetFirmwareVersion(
    NSSCKFWToken *fwToken)
{
    CK_VERSION rv;

#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        rv.major = rv.minor = 0;
        return rv;
    }
#endif /* NSSDEBUG */

    if (CKR_OK != nssCKFWMutex_Lock(fwToken->mutex)) {
        rv.major = rv.minor = 0;
        return rv;
    }

    if ((0 != fwToken->firmwareVersion.major) ||
        (0 != fwToken->firmwareVersion.minor)) {
        rv = fwToken->firmwareVersion;
        goto done;
    }

    if (fwToken->mdToken->GetFirmwareVersion) {
        fwToken->firmwareVersion = fwToken->mdToken->GetFirmwareVersion(
            fwToken->mdToken, fwToken, fwToken->mdInstance, fwToken->fwInstance);
    } else {
        fwToken->firmwareVersion.major = 0;
        fwToken->firmwareVersion.minor = 1;
    }

    rv = fwToken->firmwareVersion;

done:
    (void)nssCKFWMutex_Unlock(fwToken->mutex);
    return rv;
}

/*
 * nssCKFWToken_GetUTCTime
 *
 */
NSS_IMPLEMENT CK_RV
nssCKFWToken_GetUTCTime(
    NSSCKFWToken *fwToken,
    CK_CHAR utcTime[16])
{
    CK_RV error = CKR_OK;

#ifdef NSSDEBUG
    error = nssCKFWToken_verifyPointer(fwToken);
    if (CKR_OK != error) {
        return error;
    }

    if ((CK_CHAR_PTR)NULL == utcTime) {
        return CKR_ARGUMENTS_BAD;
    }
#endif /* DEBUG */

    if (CK_TRUE != nssCKFWToken_GetHasClockOnToken(fwToken)) {
        /* return CKR_DEVICE_ERROR; */
        (void)nssUTF8_CopyIntoFixedBuffer((NSSUTF8 *)NULL, (char *)utcTime, 16, ' ');
        return CKR_OK;
    }

    if (!fwToken->mdToken->GetUTCTime) {
        /* It said it had one! */
        return CKR_GENERAL_ERROR;
    }

    error = fwToken->mdToken->GetUTCTime(fwToken->mdToken, fwToken,
                                         fwToken->mdInstance, fwToken->fwInstance, utcTime);
    if (CKR_OK != error) {
        return error;
    }

    /* Sanity-check the data */
    {
        /* Format is YYYYMMDDhhmmss00 */
        int i;
        int Y, M, D, h, m, s;
        static int dims[] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

        for (i = 0; i < 16; i++) {
            if ((utcTime[i] < '0') || (utcTime[i] > '9')) {
                goto badtime;
            }
        }

        Y = ((utcTime[0] - '0') * 1000) + ((utcTime[1] - '0') * 100) +
            ((utcTime[2] - '0') * 10) + (utcTime[3] - '0');
        M = ((utcTime[4] - '0') * 10) + (utcTime[5] - '0');
        D = ((utcTime[6] - '0') * 10) + (utcTime[7] - '0');
        h = ((utcTime[8] - '0') * 10) + (utcTime[9] - '0');
        m = ((utcTime[10] - '0') * 10) + (utcTime[11] - '0');
        s = ((utcTime[12] - '0') * 10) + (utcTime[13] - '0');

        if ((Y < 1990) || (Y > 3000))
            goto badtime; /* Y3K problem.  heh heh heh */
        if ((M < 1) || (M > 12))
            goto badtime;
        if ((D < 1) || (D > 31))
            goto badtime;

        if (D > dims[M - 1])
            goto badtime; /* per-month check */
        if ((2 == M) && (((Y % 4) || !(Y % 100)) && (Y % 400)) &&
            (D > 28))
            goto badtime; /* leap years */

        if ((h < 0) || (h > 23))
            goto badtime;
        if ((m < 0) || (m > 60))
            goto badtime;
        if ((s < 0) || (s > 61))
            goto badtime;

        /* 60m and 60 or 61s is only allowed for leap seconds. */
        if ((60 == m) || (s >= 60)) {
            if ((23 != h) || (60 != m) || (s < 60))
                goto badtime;
            /* leap seconds can only happen on June 30 or Dec 31.. I think */
            /* if( ((6 != M) || (30 != D)) && ((12 != M) || (31 != D)) ) goto badtime; */
        }
    }

    return CKR_OK;

badtime:
    return CKR_GENERAL_ERROR;
}

/*
 * nssCKFWToken_OpenSession
 *
 */
NSS_IMPLEMENT NSSCKFWSession *
nssCKFWToken_OpenSession(
    NSSCKFWToken *fwToken,
    CK_BBOOL rw,
    CK_VOID_PTR pApplication,
    CK_NOTIFY Notify,
    CK_RV *pError)
{
    NSSCKFWSession *fwSession = (NSSCKFWSession *)NULL;
    NSSCKMDSession *mdSession;

#ifdef NSSDEBUG
    if (!pError) {
        return (NSSCKFWSession *)NULL;
    }

    *pError = nssCKFWToken_verifyPointer(fwToken);
    if (CKR_OK != *pError) {
        return (NSSCKFWSession *)NULL;
    }

    switch (rw) {
        case CK_TRUE:
        case CK_FALSE:
            break;
        default:
            *pError = CKR_ARGUMENTS_BAD;
            return (NSSCKFWSession *)NULL;
    }
#endif /* NSSDEBUG */

    *pError = nssCKFWMutex_Lock(fwToken->mutex);
    if (CKR_OK != *pError) {
        return (NSSCKFWSession *)NULL;
    }

    if (CK_TRUE == rw) {
        /* Read-write session desired */
        if (CK_TRUE == nssCKFWToken_GetIsWriteProtected(fwToken)) {
            *pError = CKR_TOKEN_WRITE_PROTECTED;
            goto done;
        }
    } else {
        /* Read-only session desired */
        if (CKS_RW_SO_FUNCTIONS == nssCKFWToken_GetSessionState(fwToken)) {
            *pError = CKR_SESSION_READ_WRITE_SO_EXISTS;
            goto done;
        }
    }

    /* We could compare sesion counts to any limits we know of, I guess.. */

    if (!fwToken->mdToken->OpenSession) {
        /*
         * I'm not sure that the Module actually needs to implement
         * mdSessions -- the Framework can keep track of everything
         * needed, really.  But I'll sort out that detail later..
         */
        *pError = CKR_GENERAL_ERROR;
        goto done;
    }

    fwSession = nssCKFWSession_Create(fwToken, rw, pApplication, Notify, pError);
    if (!fwSession) {
        if (CKR_OK == *pError) {
            *pError = CKR_GENERAL_ERROR;
        }
        goto done;
    }

    mdSession = fwToken->mdToken->OpenSession(fwToken->mdToken, fwToken,
                                              fwToken->mdInstance, fwToken->fwInstance, fwSession,
                                              rw, pError);
    if (!mdSession) {
        (void)nssCKFWSession_Destroy(fwSession, CK_FALSE);
        if (CKR_OK == *pError) {
            *pError = CKR_GENERAL_ERROR;
        }
        goto done;
    }

    *pError = nssCKFWSession_SetMDSession(fwSession, mdSession);
    if (CKR_OK != *pError) {
        if (mdSession->Close) {
            mdSession->Close(mdSession, fwSession, fwToken->mdToken, fwToken,
                             fwToken->mdInstance, fwToken->fwInstance);
        }
        (void)nssCKFWSession_Destroy(fwSession, CK_FALSE);
        goto done;
    }

    *pError = nssCKFWHash_Add(fwToken->sessions, fwSession, fwSession);
    if (CKR_OK != *pError) {
        (void)nssCKFWSession_Destroy(fwSession, CK_FALSE);
        fwSession = (NSSCKFWSession *)NULL;
        goto done;
    }

done:
    (void)nssCKFWMutex_Unlock(fwToken->mutex);
    return fwSession;
}

/*
 * nssCKFWToken_GetMechanismCount
 *
 */
NSS_IMPLEMENT CK_ULONG
nssCKFWToken_GetMechanismCount(
    NSSCKFWToken *fwToken)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return 0;
    }
#endif /* NSSDEBUG */

    if (!fwToken->mdToken->GetMechanismCount) {
        return 0;
    }

    return fwToken->mdToken->GetMechanismCount(fwToken->mdToken, fwToken,
                                               fwToken->mdInstance, fwToken->fwInstance);
}

/*
 * nssCKFWToken_GetMechanismTypes
 *
 */
NSS_IMPLEMENT CK_RV
nssCKFWToken_GetMechanismTypes(
    NSSCKFWToken *fwToken,
    CK_MECHANISM_TYPE types[])
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return CKR_ARGUMENTS_BAD;
    }

    if (!types) {
        return CKR_ARGUMENTS_BAD;
    }
#endif /* NSSDEBUG */

    if (!fwToken->mdToken->GetMechanismTypes) {
        /*
         * This should only be called with a sufficiently-large
         * "types" array, which can only be done if GetMechanismCount
         * is implemented.  If that's implemented (and returns nonzero),
         * then this should be too.  So return an error.
         */
        return CKR_GENERAL_ERROR;
    }

    return fwToken->mdToken->GetMechanismTypes(fwToken->mdToken, fwToken,
                                               fwToken->mdInstance, fwToken->fwInstance, types);
}

/*
 * nssCKFWToken_GetMechanism
 *
 */
NSS_IMPLEMENT NSSCKFWMechanism *
nssCKFWToken_GetMechanism(
    NSSCKFWToken *fwToken,
    CK_MECHANISM_TYPE which,
    CK_RV *pError)
{
    NSSCKMDMechanism *mdMechanism;
    if (!fwToken->mdMechanismHash) {
        *pError = CKR_GENERAL_ERROR;
        return (NSSCKFWMechanism *)NULL;
    }

    if (!fwToken->mdToken->GetMechanism) {
        /*
         * If we don't implement any GetMechanism function, then we must
         * not support any.
         */
        *pError = CKR_MECHANISM_INVALID;
        return (NSSCKFWMechanism *)NULL;
    }

    /* lookup in hash table */
    mdMechanism = fwToken->mdToken->GetMechanism(fwToken->mdToken, fwToken,
                                                 fwToken->mdInstance, fwToken->fwInstance, which, pError);
    if (!mdMechanism) {
        return (NSSCKFWMechanism *)NULL;
    }
    /* store in hash table */
    return nssCKFWMechanism_Create(mdMechanism, fwToken->mdToken, fwToken,
                                   fwToken->mdInstance, fwToken->fwInstance);
}

NSS_IMPLEMENT CK_RV
nssCKFWToken_SetSessionState(
    NSSCKFWToken *fwToken,
    CK_STATE newState)
{
    CK_RV error = CKR_OK;

#ifdef NSSDEBUG
    error = nssCKFWToken_verifyPointer(fwToken);
    if (CKR_OK != error) {
        return error;
    }

    switch (newState) {
        case CKS_RO_PUBLIC_SESSION:
        case CKS_RO_USER_FUNCTIONS:
        case CKS_RW_PUBLIC_SESSION:
        case CKS_RW_USER_FUNCTIONS:
        case CKS_RW_SO_FUNCTIONS:
            break;
        default:
            return CKR_ARGUMENTS_BAD;
    }
#endif /* NSSDEBUG */

    error = nssCKFWMutex_Lock(fwToken->mutex);
    if (CKR_OK != error) {
        return error;
    }

    fwToken->state = newState;
    (void)nssCKFWMutex_Unlock(fwToken->mutex);
    return CKR_OK;
}

/*
 * nssCKFWToken_RemoveSession
 *
 */
NSS_IMPLEMENT CK_RV
nssCKFWToken_RemoveSession(
    NSSCKFWToken *fwToken,
    NSSCKFWSession *fwSession)
{
    CK_RV error = CKR_OK;

#ifdef NSSDEBUG
    error = nssCKFWToken_verifyPointer(fwToken);
    if (CKR_OK != error) {
        return error;
    }

    error = nssCKFWSession_verifyPointer(fwSession);
    if (CKR_OK != error) {
        return error;
    }
#endif /* NSSDEBUG */

    error = nssCKFWMutex_Lock(fwToken->mutex);
    if (CKR_OK != error) {
        return error;
    }

    if (CK_TRUE != nssCKFWHash_Exists(fwToken->sessions, fwSession)) {
        error = CKR_SESSION_HANDLE_INVALID;
        goto done;
    }

    nssCKFWHash_Remove(fwToken->sessions, fwSession);
    fwToken->sessionCount--;

    if (nssCKFWSession_IsRWSession(fwSession)) {
        fwToken->rwSessionCount--;
    }

    if (0 == fwToken->sessionCount) {
        fwToken->rwSessionCount = 0;            /* sanity */
        fwToken->state = CKS_RO_PUBLIC_SESSION; /* some default */
    }

    error = CKR_OK;

done:
    (void)nssCKFWMutex_Unlock(fwToken->mutex);
    return error;
}

/*
 * nssCKFWToken_CloseAllSessions
 *
 */
NSS_IMPLEMENT CK_RV
nssCKFWToken_CloseAllSessions(
    NSSCKFWToken *fwToken)
{
    CK_RV error = CKR_OK;

#ifdef NSSDEBUG
    error = nssCKFWToken_verifyPointer(fwToken);
    if (CKR_OK != error) {
        return error;
    }
#endif /* NSSDEBUG */

    error = nssCKFWMutex_Lock(fwToken->mutex);
    if (CKR_OK != error) {
        return error;
    }

    nssCKFWHash_Iterate(fwToken->sessions, nss_ckfwtoken_session_iterator, (void *)NULL);

    nssCKFWHash_Destroy(fwToken->sessions);

    fwToken->sessions = nssCKFWHash_Create(fwToken->fwInstance, fwToken->arena, &error);
    if (!fwToken->sessions) {
        if (CKR_OK == error) {
            error = CKR_GENERAL_ERROR;
        }
        goto done;
    }

    fwToken->state = CKS_RO_PUBLIC_SESSION; /* some default */
    fwToken->sessionCount = 0;
    fwToken->rwSessionCount = 0;

    error = CKR_OK;

done:
    (void)nssCKFWMutex_Unlock(fwToken->mutex);
    return error;
}

/*
 * nssCKFWToken_GetSessionCount
 *
 */
NSS_IMPLEMENT CK_ULONG
nssCKFWToken_GetSessionCount(
    NSSCKFWToken *fwToken)
{
    CK_ULONG rv;

#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return (CK_ULONG)0;
    }
#endif /* NSSDEBUG */

    if (CKR_OK != nssCKFWMutex_Lock(fwToken->mutex)) {
        return (CK_ULONG)0;
    }

    rv = fwToken->sessionCount;
    (void)nssCKFWMutex_Unlock(fwToken->mutex);
    return rv;
}

/*
 * nssCKFWToken_GetRwSessionCount
 *
 */
NSS_IMPLEMENT CK_ULONG
nssCKFWToken_GetRwSessionCount(
    NSSCKFWToken *fwToken)
{
    CK_ULONG rv;

#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return (CK_ULONG)0;
    }
#endif /* NSSDEBUG */

    if (CKR_OK != nssCKFWMutex_Lock(fwToken->mutex)) {
        return (CK_ULONG)0;
    }

    rv = fwToken->rwSessionCount;
    (void)nssCKFWMutex_Unlock(fwToken->mutex);
    return rv;
}

/*
 * nssCKFWToken_GetRoSessionCount
 *
 */
NSS_IMPLEMENT CK_ULONG
nssCKFWToken_GetRoSessionCount(
    NSSCKFWToken *fwToken)
{
    CK_ULONG rv;

#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return (CK_ULONG)0;
    }
#endif /* NSSDEBUG */

    if (CKR_OK != nssCKFWMutex_Lock(fwToken->mutex)) {
        return (CK_ULONG)0;
    }

    rv = fwToken->sessionCount - fwToken->rwSessionCount;
    (void)nssCKFWMutex_Unlock(fwToken->mutex);
    return rv;
}

/*
 * nssCKFWToken_GetSessionObjectHash
 *
 */
NSS_IMPLEMENT nssCKFWHash *
nssCKFWToken_GetSessionObjectHash(
    NSSCKFWToken *fwToken)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return (nssCKFWHash *)NULL;
    }
#endif /* NSSDEBUG */

    return fwToken->sessionObjectHash;
}

/*
 * nssCKFWToken_GetMDObjectHash
 *
 */
NSS_IMPLEMENT nssCKFWHash *
nssCKFWToken_GetMDObjectHash(
    NSSCKFWToken *fwToken)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return (nssCKFWHash *)NULL;
    }
#endif /* NSSDEBUG */

    return fwToken->mdObjectHash;
}

/*
 * nssCKFWToken_GetObjectHandleHash
 *
 */
NSS_IMPLEMENT nssCKFWHash *
nssCKFWToken_GetObjectHandleHash(
    NSSCKFWToken *fwToken)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return (nssCKFWHash *)NULL;
    }
#endif /* NSSDEBUG */

    return fwToken->mdObjectHash;
}

/*
 * NSSCKFWToken_GetMDToken
 *
 */

NSS_IMPLEMENT NSSCKMDToken *
NSSCKFWToken_GetMDToken(
    NSSCKFWToken *fwToken)
{
#ifdef DEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return (NSSCKMDToken *)NULL;
    }
#endif /* DEBUG */

    return nssCKFWToken_GetMDToken(fwToken);
}

/*
 * NSSCKFWToken_GetArena
 *
 */

NSS_IMPLEMENT NSSArena *
NSSCKFWToken_GetArena(
    NSSCKFWToken *fwToken,
    CK_RV *pError)
{
#ifdef DEBUG
    if (!pError) {
        return (NSSArena *)NULL;
    }

    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        *pError = CKR_ARGUMENTS_BAD;
        return (NSSArena *)NULL;
    }
#endif /* DEBUG */

    return nssCKFWToken_GetArena(fwToken, pError);
}

/*
 * NSSCKFWToken_GetFWSlot
 *
 */

NSS_IMPLEMENT NSSCKFWSlot *
NSSCKFWToken_GetFWSlot(
    NSSCKFWToken *fwToken)
{
#ifdef DEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return (NSSCKFWSlot *)NULL;
    }
#endif /* DEBUG */

    return nssCKFWToken_GetFWSlot(fwToken);
}

/*
 * NSSCKFWToken_GetMDSlot
 *
 */

NSS_IMPLEMENT NSSCKMDSlot *
NSSCKFWToken_GetMDSlot(
    NSSCKFWToken *fwToken)
{
#ifdef DEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return (NSSCKMDSlot *)NULL;
    }
#endif /* DEBUG */

    return nssCKFWToken_GetMDSlot(fwToken);
}

/*
 * NSSCKFWToken_GetSessionState
 *
 */

NSS_IMPLEMENT CK_STATE
NSSCKFWSession_GetSessionState(
    NSSCKFWToken *fwToken)
{
#ifdef DEBUG
    if (CKR_OK != nssCKFWToken_verifyPointer(fwToken)) {
        return CKS_RO_PUBLIC_SESSION;
    }
#endif /* DEBUG */

    return nssCKFWToken_GetSessionState(fwToken);
}
