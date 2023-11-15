/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * instance.c
 *
 * This file implements the NSSCKFWInstance type and methods.
 */

#ifndef CK_T
#include "ck.h"
#endif /* CK_T */

#include <stdint.h>

/*
 * NSSCKFWInstance
 *
 *  -- create/destroy --
 *  nssCKFWInstance_Create
 *  nssCKFWInstance_Destroy
 *
 *  -- public accessors --
 *  NSSCKFWInstance_GetMDInstance
 *  NSSCKFWInstance_GetArena
 *  NSSCKFWInstance_MayCreatePthreads
 *  NSSCKFWInstance_CreateMutex
 *  NSSCKFWInstance_GetConfigurationData
 *  NSSCKFWInstance_GetInitArgs
 *  NSSCKFWInstance_DestroySessionHandle
 *  NSSCKFWInstance_FindSessionHandle
 *
 *  -- implement public accessors --
 *  nssCKFWInstance_GetMDInstance
 *  nssCKFWInstance_GetArena
 *  nssCKFWInstance_MayCreatePthreads
 *  nssCKFWInstance_CreateMutex
 *  nssCKFWInstance_GetConfigurationData
 *  nssCKFWInstance_GetInitArgs
 *  nssCKFWInstance_DestroySessionHandle
 *  nssCKFWInstance_FindSessionHandle
 *
 *  -- private accessors --
 *  nssCKFWInstance_CreateSessionHandle
 *  nssCKFWInstance_ResolveSessionHandle
 *  nssCKFWInstance_CreateObjectHandle
 *  nssCKFWInstance_ResolveObjectHandle
 *  nssCKFWInstance_DestroyObjectHandle
 *
 *  -- module fronts --
 *  nssCKFWInstance_GetNSlots
 *  nssCKFWInstance_GetCryptokiVersion
 *  nssCKFWInstance_GetManufacturerID
 *  nssCKFWInstance_GetFlags
 *  nssCKFWInstance_GetLibraryDescription
 *  nssCKFWInstance_GetLibraryVersion
 *  nssCKFWInstance_GetModuleHandlesSessionObjects
 *  nssCKFWInstance_GetSlots
 *  nssCKFWInstance_WaitForSlotEvent
 *
 *  -- debugging versions only --
 *  nssCKFWInstance_verifyPointer
 */

struct NSSCKFWInstanceStr {
    NSSCKFWMutex *mutex;
    NSSArena *arena;
    NSSCKMDInstance *mdInstance;
    CK_C_INITIALIZE_ARGS_PTR pInitArgs;
    CK_C_INITIALIZE_ARGS initArgs;
    CryptokiLockingState LockingState;
    CK_BBOOL mayCreatePthreads;
    NSSUTF8 *configurationData;
    CK_ULONG nSlots;
    NSSCKFWSlot **fwSlotList;
    NSSCKMDSlot **mdSlotList;
    CK_BBOOL moduleHandlesSessionObjects;

    /*
     * Everything above is set at creation time, and then not modified.
     * The invariants the mutex protects are:
     *
     *  1) Each of the cached descriptions (versions, etc.) are in an
     *     internally consistant state.
     *
     *  2) The session handle hashes and count are consistant
     *
     *  3) The object handle hashes and count are consistant.
     *
     * I could use multiple locks, but let's wait to see if that's
     * really necessary.
     *
     * Note that the calls accessing the cached descriptions will
     * call the NSSCKMDInstance methods with the mutex locked.  Those
     * methods may then call the public NSSCKFWInstance routines.
     * Those public routines only access the constant data above, so
     * there's no problem.  But be careful if you add to this object;
     * mutexes are in general not reentrant, so don't create deadlock
     * situations.
     */

    CK_VERSION cryptokiVersion;
    NSSUTF8 *manufacturerID;
    NSSUTF8 *libraryDescription;
    CK_VERSION libraryVersion;

    CK_ULONG lastSessionHandle;
    nssCKFWHash *sessionHandleHash;

    CK_ULONG lastObjectHandle;
    nssCKFWHash *objectHandleHash;
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
instance_add_pointer(
    const NSSCKFWInstance *fwInstance)
{
    return CKR_OK;
}

static CK_RV
instance_remove_pointer(
    const NSSCKFWInstance *fwInstance)
{
    return CKR_OK;
}

NSS_IMPLEMENT CK_RV
nssCKFWInstance_verifyPointer(
    const NSSCKFWInstance *fwInstance)
{
    return CKR_OK;
}

#endif /* DEBUG */

/*
 * nssCKFWInstance_Create
 *
 */
NSS_IMPLEMENT NSSCKFWInstance *
nssCKFWInstance_Create(
    CK_C_INITIALIZE_ARGS_PTR pInitArgs,
    CryptokiLockingState LockingState,
    NSSCKMDInstance *mdInstance,
    CK_RV *pError)
{
    NSSCKFWInstance *fwInstance;
    NSSArena *arena = (NSSArena *)NULL;
    CK_ULONG i;
    CK_BBOOL called_Initialize = CK_FALSE;

#ifdef NSSDEBUG
    if ((CK_RV)NULL == pError) {
        return (NSSCKFWInstance *)NULL;
    }

    if (!mdInstance) {
        *pError = CKR_ARGUMENTS_BAD;
        return (NSSCKFWInstance *)NULL;
    }
#endif /* NSSDEBUG */

    arena = NSSArena_Create();
    if (!arena) {
        *pError = CKR_HOST_MEMORY;
        return (NSSCKFWInstance *)NULL;
    }

    fwInstance = nss_ZNEW(arena, NSSCKFWInstance);
    if (!fwInstance) {
        goto nomem;
    }

    fwInstance->arena = arena;
    fwInstance->mdInstance = mdInstance;

    fwInstance->LockingState = LockingState;
    if ((CK_C_INITIALIZE_ARGS_PTR)NULL != pInitArgs) {
        fwInstance->initArgs = *pInitArgs;
        fwInstance->pInitArgs = &fwInstance->initArgs;
        if (pInitArgs->flags & CKF_LIBRARY_CANT_CREATE_OS_THREADS) {
            fwInstance->mayCreatePthreads = CK_FALSE;
        } else {
            fwInstance->mayCreatePthreads = CK_TRUE;
        }
        fwInstance->configurationData = (NSSUTF8 *)(pInitArgs->pReserved);
    } else {
        fwInstance->mayCreatePthreads = CK_TRUE;
    }

    fwInstance->mutex = nssCKFWMutex_Create(pInitArgs, LockingState, arena,
                                            pError);
    if (!fwInstance->mutex) {
        if (CKR_OK == *pError) {
            *pError = CKR_GENERAL_ERROR;
        }
        goto loser;
    }

    if (mdInstance->Initialize) {
        *pError = mdInstance->Initialize(mdInstance, fwInstance, fwInstance->configurationData);
        if (CKR_OK != *pError) {
            goto loser;
        }

        called_Initialize = CK_TRUE;
    }

    if (mdInstance->ModuleHandlesSessionObjects) {
        fwInstance->moduleHandlesSessionObjects =
            mdInstance->ModuleHandlesSessionObjects(mdInstance, fwInstance);
    } else {
        fwInstance->moduleHandlesSessionObjects = CK_FALSE;
    }

    if (!mdInstance->GetNSlots) {
        /* That routine is required */
        *pError = CKR_GENERAL_ERROR;
        goto loser;
    }

    fwInstance->nSlots = mdInstance->GetNSlots(mdInstance, fwInstance, pError);
    if ((CK_ULONG)0 == fwInstance->nSlots) {
        if (CKR_OK == *pError) {
            /* Zero is not a legitimate answer */
            *pError = CKR_GENERAL_ERROR;
        }
        goto loser;
    }

    fwInstance->fwSlotList = nss_ZNEWARRAY(arena, NSSCKFWSlot *, fwInstance->nSlots);
    if ((NSSCKFWSlot **)NULL == fwInstance->fwSlotList) {
        goto nomem;
    }

    fwInstance->mdSlotList = nss_ZNEWARRAY(arena, NSSCKMDSlot *, fwInstance->nSlots);
    if ((NSSCKMDSlot **)NULL == fwInstance->mdSlotList) {
        goto nomem;
    }

    fwInstance->sessionHandleHash = nssCKFWHash_Create(fwInstance,
                                                       fwInstance->arena, pError);
    if (!fwInstance->sessionHandleHash) {
        goto loser;
    }

    fwInstance->objectHandleHash = nssCKFWHash_Create(fwInstance,
                                                      fwInstance->arena, pError);
    if (!fwInstance->objectHandleHash) {
        goto loser;
    }

    if (!mdInstance->GetSlots) {
        /* That routine is required */
        *pError = CKR_GENERAL_ERROR;
        goto loser;
    }

    *pError = mdInstance->GetSlots(mdInstance, fwInstance, fwInstance->mdSlotList);
    if (CKR_OK != *pError) {
        goto loser;
    }

    for (i = 0; i < fwInstance->nSlots; i++) {
        NSSCKMDSlot *mdSlot = fwInstance->mdSlotList[i];

        if (!mdSlot) {
            *pError = CKR_GENERAL_ERROR;
            goto loser;
        }

        fwInstance->fwSlotList[i] = nssCKFWSlot_Create(fwInstance, mdSlot, i, pError);
        if (CKR_OK != *pError) {
            CK_ULONG j;

            for (j = 0; j < i; j++) {
                (void)nssCKFWSlot_Destroy(fwInstance->fwSlotList[j]);
            }

            for (j = i; j < fwInstance->nSlots; j++) {
                NSSCKMDSlot *mds = fwInstance->mdSlotList[j];
                if (mds->Destroy) {
                    mds->Destroy(mds, (NSSCKFWSlot *)NULL, mdInstance, fwInstance);
                }
            }

            goto loser;
        }
    }

#ifdef DEBUG
    *pError = instance_add_pointer(fwInstance);
    if (CKR_OK != *pError) {
        for (i = 0; i < fwInstance->nSlots; i++) {
            (void)nssCKFWSlot_Destroy(fwInstance->fwSlotList[i]);
        }

        goto loser;
    }
#endif /* DEBUG */

    *pError = CKR_OK;
    return fwInstance;

nomem:
    *pError = CKR_HOST_MEMORY;
/*FALLTHROUGH*/
loser:

    if (CK_TRUE == called_Initialize) {
        if (mdInstance->Finalize) {
            mdInstance->Finalize(mdInstance, fwInstance);
        }
    }

    if (fwInstance && fwInstance->mutex) {
        nssCKFWMutex_Destroy(fwInstance->mutex);
    }

    if (arena) {
        (void)NSSArena_Destroy(arena);
    }
    return (NSSCKFWInstance *)NULL;
}

/*
 * nssCKFWInstance_Destroy
 *
 */
NSS_IMPLEMENT CK_RV
nssCKFWInstance_Destroy(
    NSSCKFWInstance *fwInstance)
{
#ifdef NSSDEBUG
    CK_RV error = CKR_OK;
#endif /* NSSDEBUG */
    CK_ULONG i;

#ifdef NSSDEBUG
    error = nssCKFWInstance_verifyPointer(fwInstance);
    if (CKR_OK != error) {
        return error;
    }
#endif /* NSSDEBUG */

    nssCKFWMutex_Destroy(fwInstance->mutex);

    for (i = 0; i < fwInstance->nSlots; i++) {
        (void)nssCKFWSlot_Destroy(fwInstance->fwSlotList[i]);
    }

    if (fwInstance->mdInstance->Finalize) {
        fwInstance->mdInstance->Finalize(fwInstance->mdInstance, fwInstance);
    }

    if (fwInstance->sessionHandleHash) {
        nssCKFWHash_Destroy(fwInstance->sessionHandleHash);
    }

    if (fwInstance->objectHandleHash) {
        nssCKFWHash_Destroy(fwInstance->objectHandleHash);
    }

#ifdef DEBUG
    (void)instance_remove_pointer(fwInstance);
#endif /* DEBUG */

    (void)NSSArena_Destroy(fwInstance->arena);
    return CKR_OK;
}

/*
 * nssCKFWInstance_GetMDInstance
 *
 */
NSS_IMPLEMENT NSSCKMDInstance *
nssCKFWInstance_GetMDInstance(
    NSSCKFWInstance *fwInstance)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWInstance_verifyPointer(fwInstance)) {
        return (NSSCKMDInstance *)NULL;
    }
#endif /* NSSDEBUG */

    return fwInstance->mdInstance;
}

/*
 * nssCKFWInstance_GetArena
 *
 */
NSS_IMPLEMENT NSSArena *
nssCKFWInstance_GetArena(
    NSSCKFWInstance *fwInstance,
    CK_RV *pError)
{
#ifdef NSSDEBUG
    if (!pError) {
        return (NSSArena *)NULL;
    }

    *pError = nssCKFWInstance_verifyPointer(fwInstance);
    if (CKR_OK != *pError) {
        return (NSSArena *)NULL;
    }
#endif /* NSSDEBUG */

    *pError = CKR_OK;
    return fwInstance->arena;
}

/*
 * nssCKFWInstance_MayCreatePthreads
 *
 */
NSS_IMPLEMENT CK_BBOOL
nssCKFWInstance_MayCreatePthreads(
    NSSCKFWInstance *fwInstance)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWInstance_verifyPointer(fwInstance)) {
        return CK_FALSE;
    }
#endif /* NSSDEBUG */

    return fwInstance->mayCreatePthreads;
}

/*
 * nssCKFWInstance_CreateMutex
 *
 */
NSS_IMPLEMENT NSSCKFWMutex *
nssCKFWInstance_CreateMutex(
    NSSCKFWInstance *fwInstance,
    NSSArena *arena,
    CK_RV *pError)
{
    NSSCKFWMutex *mutex;

#ifdef NSSDEBUG
    if (!pError) {
        return (NSSCKFWMutex *)NULL;
    }

    *pError = nssCKFWInstance_verifyPointer(fwInstance);
    if (CKR_OK != *pError) {
        return (NSSCKFWMutex *)NULL;
    }
#endif /* NSSDEBUG */

    mutex = nssCKFWMutex_Create(fwInstance->pInitArgs, fwInstance->LockingState,
                                arena, pError);
    if (!mutex) {
        if (CKR_OK == *pError) {
            *pError = CKR_GENERAL_ERROR;
        }

        return (NSSCKFWMutex *)NULL;
    }

    return mutex;
}

/*
 * nssCKFWInstance_GetConfigurationData
 *
 */
NSS_IMPLEMENT NSSUTF8 *
nssCKFWInstance_GetConfigurationData(
    NSSCKFWInstance *fwInstance)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWInstance_verifyPointer(fwInstance)) {
        return (NSSUTF8 *)NULL;
    }
#endif /* NSSDEBUG */

    return fwInstance->configurationData;
}

/*
 * nssCKFWInstance_GetInitArgs
 *
 */
CK_C_INITIALIZE_ARGS_PTR
nssCKFWInstance_GetInitArgs(
    NSSCKFWInstance *fwInstance)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWInstance_verifyPointer(fwInstance)) {
        return (CK_C_INITIALIZE_ARGS_PTR)NULL;
    }
#endif /* NSSDEBUG */

    return fwInstance->pInitArgs;
}

/*
 * nssCKFWInstance_CreateSessionHandle
 *
 */
NSS_IMPLEMENT CK_SESSION_HANDLE
nssCKFWInstance_CreateSessionHandle(
    NSSCKFWInstance *fwInstance,
    NSSCKFWSession *fwSession,
    CK_RV *pError)
{
    CK_SESSION_HANDLE hSession;

#ifdef NSSDEBUG
    if (!pError) {
        return (CK_SESSION_HANDLE)0;
    }

    *pError = nssCKFWInstance_verifyPointer(fwInstance);
    if (CKR_OK != *pError) {
        return (CK_SESSION_HANDLE)0;
    }
#endif /* NSSDEBUG */

    *pError = nssCKFWMutex_Lock(fwInstance->mutex);
    if (CKR_OK != *pError) {
        return (CK_SESSION_HANDLE)0;
    }

    hSession = ++(fwInstance->lastSessionHandle);

    /* Alan would say I should unlock for this call. */

    *pError = nssCKFWSession_SetHandle(fwSession, hSession);
    if (CKR_OK != *pError) {
        goto done;
    }

    *pError = nssCKFWHash_Add(fwInstance->sessionHandleHash,
                              (const void *)(uintptr_t)hSession, (const void *)fwSession);
    if (CKR_OK != *pError) {
        hSession = (CK_SESSION_HANDLE)0;
        goto done;
    }

done:
    nssCKFWMutex_Unlock(fwInstance->mutex);
    return hSession;
}

/*
 * nssCKFWInstance_ResolveSessionHandle
 *
 */
NSS_IMPLEMENT NSSCKFWSession *
nssCKFWInstance_ResolveSessionHandle(
    NSSCKFWInstance *fwInstance,
    CK_SESSION_HANDLE hSession)
{
    NSSCKFWSession *fwSession;

#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWInstance_verifyPointer(fwInstance)) {
        return (NSSCKFWSession *)NULL;
    }
#endif /* NSSDEBUG */

    if (CKR_OK != nssCKFWMutex_Lock(fwInstance->mutex)) {
        return (NSSCKFWSession *)NULL;
    }

    fwSession = (NSSCKFWSession *)nssCKFWHash_Lookup(
        fwInstance->sessionHandleHash, (const void *)(uintptr_t)hSession);

    /* Assert(hSession == nssCKFWSession_GetHandle(fwSession)) */

    (void)nssCKFWMutex_Unlock(fwInstance->mutex);

    return fwSession;
}

/*
 * nssCKFWInstance_DestroySessionHandle
 *
 */
NSS_IMPLEMENT void
nssCKFWInstance_DestroySessionHandle(
    NSSCKFWInstance *fwInstance,
    CK_SESSION_HANDLE hSession)
{
    NSSCKFWSession *fwSession;

#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWInstance_verifyPointer(fwInstance)) {
        return;
    }
#endif /* NSSDEBUG */

    if (CKR_OK != nssCKFWMutex_Lock(fwInstance->mutex)) {
        return;
    }

    fwSession = (NSSCKFWSession *)nssCKFWHash_Lookup(
        fwInstance->sessionHandleHash, (const void *)(uintptr_t)hSession);
    if (fwSession) {
        nssCKFWHash_Remove(fwInstance->sessionHandleHash, (const void *)(uintptr_t)hSession);
        nssCKFWSession_SetHandle(fwSession, (CK_SESSION_HANDLE)0);
    }

    (void)nssCKFWMutex_Unlock(fwInstance->mutex);

    return;
}

/*
 * nssCKFWInstance_FindSessionHandle
 *
 */
NSS_IMPLEMENT CK_SESSION_HANDLE
nssCKFWInstance_FindSessionHandle(
    NSSCKFWInstance *fwInstance,
    NSSCKFWSession *fwSession)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWInstance_verifyPointer(fwInstance)) {
        return (CK_SESSION_HANDLE)0;
    }

    if (CKR_OK != nssCKFWSession_verifyPointer(fwSession)) {
        return (CK_SESSION_HANDLE)0;
    }
#endif /* NSSDEBUG */

    return nssCKFWSession_GetHandle(fwSession);
    /* look it up and assert? */
}

/*
 * nssCKFWInstance_CreateObjectHandle
 *
 */
NSS_IMPLEMENT CK_OBJECT_HANDLE
nssCKFWInstance_CreateObjectHandle(
    NSSCKFWInstance *fwInstance,
    NSSCKFWObject *fwObject,
    CK_RV *pError)
{
    CK_OBJECT_HANDLE hObject;

#ifdef NSSDEBUG
    if (!pError) {
        return (CK_OBJECT_HANDLE)0;
    }

    *pError = nssCKFWInstance_verifyPointer(fwInstance);
    if (CKR_OK != *pError) {
        return (CK_OBJECT_HANDLE)0;
    }
#endif /* NSSDEBUG */

    *pError = nssCKFWMutex_Lock(fwInstance->mutex);
    if (CKR_OK != *pError) {
        return (CK_OBJECT_HANDLE)0;
    }

    hObject = ++(fwInstance->lastObjectHandle);

    *pError = nssCKFWObject_SetHandle(fwObject, hObject);
    if (CKR_OK != *pError) {
        hObject = (CK_OBJECT_HANDLE)0;
        goto done;
    }

    *pError = nssCKFWHash_Add(fwInstance->objectHandleHash,
                              (const void *)(uintptr_t)hObject, (const void *)fwObject);
    if (CKR_OK != *pError) {
        hObject = (CK_OBJECT_HANDLE)0;
        goto done;
    }

done:
    (void)nssCKFWMutex_Unlock(fwInstance->mutex);
    return hObject;
}

/*
 * nssCKFWInstance_ResolveObjectHandle
 *
 */
NSS_IMPLEMENT NSSCKFWObject *
nssCKFWInstance_ResolveObjectHandle(
    NSSCKFWInstance *fwInstance,
    CK_OBJECT_HANDLE hObject)
{
    NSSCKFWObject *fwObject;

#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWInstance_verifyPointer(fwInstance)) {
        return (NSSCKFWObject *)NULL;
    }
#endif /* NSSDEBUG */

    if (CKR_OK != nssCKFWMutex_Lock(fwInstance->mutex)) {
        return (NSSCKFWObject *)NULL;
    }

    fwObject = (NSSCKFWObject *)nssCKFWHash_Lookup(
        fwInstance->objectHandleHash, (const void *)(uintptr_t)hObject);

    /* Assert(hObject == nssCKFWObject_GetHandle(fwObject)) */

    (void)nssCKFWMutex_Unlock(fwInstance->mutex);
    return fwObject;
}

/*
 * nssCKFWInstance_ReassignObjectHandle
 *
 */
NSS_IMPLEMENT CK_RV
nssCKFWInstance_ReassignObjectHandle(
    NSSCKFWInstance *fwInstance,
    CK_OBJECT_HANDLE hObject,
    NSSCKFWObject *fwObject)
{
    CK_RV error = CKR_OK;
    NSSCKFWObject *oldObject;

#ifdef NSSDEBUG
    error = nssCKFWInstance_verifyPointer(fwInstance);
    if (CKR_OK != error) {
        return error;
    }
#endif /* NSSDEBUG */

    error = nssCKFWMutex_Lock(fwInstance->mutex);
    if (CKR_OK != error) {
        return error;
    }

    oldObject = (NSSCKFWObject *)nssCKFWHash_Lookup(
        fwInstance->objectHandleHash, (const void *)(uintptr_t)hObject);
    if (oldObject) {
        /* Assert(hObject == nssCKFWObject_GetHandle(oldObject) */
        (void)nssCKFWObject_SetHandle(oldObject, (CK_SESSION_HANDLE)0);
        nssCKFWHash_Remove(fwInstance->objectHandleHash, (const void *)(uintptr_t)hObject);
    }

    error = nssCKFWObject_SetHandle(fwObject, hObject);
    if (CKR_OK != error) {
        goto done;
    }
    error = nssCKFWHash_Add(fwInstance->objectHandleHash,
                            (const void *)(uintptr_t)hObject, (const void *)fwObject);

done:
    (void)nssCKFWMutex_Unlock(fwInstance->mutex);
    return error;
}

/*
 * nssCKFWInstance_DestroyObjectHandle
 *
 */
NSS_IMPLEMENT void
nssCKFWInstance_DestroyObjectHandle(
    NSSCKFWInstance *fwInstance,
    CK_OBJECT_HANDLE hObject)
{
    NSSCKFWObject *fwObject;

#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWInstance_verifyPointer(fwInstance)) {
        return;
    }
#endif /* NSSDEBUG */

    if (CKR_OK != nssCKFWMutex_Lock(fwInstance->mutex)) {
        return;
    }

    fwObject = (NSSCKFWObject *)nssCKFWHash_Lookup(
        fwInstance->objectHandleHash, (const void *)(uintptr_t)hObject);
    if (fwObject) {
        /* Assert(hObject = nssCKFWObject_GetHandle(fwObject)) */
        nssCKFWHash_Remove(fwInstance->objectHandleHash, (const void *)(uintptr_t)hObject);
        (void)nssCKFWObject_SetHandle(fwObject, (CK_SESSION_HANDLE)0);
    }

    (void)nssCKFWMutex_Unlock(fwInstance->mutex);
    return;
}

/*
 * nssCKFWInstance_FindObjectHandle
 *
 */
NSS_IMPLEMENT CK_OBJECT_HANDLE
nssCKFWInstance_FindObjectHandle(
    NSSCKFWInstance *fwInstance,
    NSSCKFWObject *fwObject)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWInstance_verifyPointer(fwInstance)) {
        return (CK_OBJECT_HANDLE)0;
    }

    if (CKR_OK != nssCKFWObject_verifyPointer(fwObject)) {
        return (CK_OBJECT_HANDLE)0;
    }
#endif /* NSSDEBUG */

    return nssCKFWObject_GetHandle(fwObject);
}

/*
 * nssCKFWInstance_GetNSlots
 *
 */
NSS_IMPLEMENT CK_ULONG
nssCKFWInstance_GetNSlots(
    NSSCKFWInstance *fwInstance,
    CK_RV *pError)
{
#ifdef NSSDEBUG
    if (!pError) {
        return (CK_ULONG)0;
    }

    *pError = nssCKFWInstance_verifyPointer(fwInstance);
    if (CKR_OK != *pError) {
        return (CK_ULONG)0;
    }
#endif /* NSSDEBUG */

    *pError = CKR_OK;
    return fwInstance->nSlots;
}

/*
 * nssCKFWInstance_GetCryptokiVersion
 *
 */
NSS_IMPLEMENT CK_VERSION
nssCKFWInstance_GetCryptokiVersion(
    NSSCKFWInstance *fwInstance)
{
    CK_VERSION rv;

#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWInstance_verifyPointer(fwInstance)) {
        rv.major = rv.minor = 0;
        return rv;
    }
#endif /* NSSDEBUG */

    if (CKR_OK != nssCKFWMutex_Lock(fwInstance->mutex)) {
        rv.major = rv.minor = 0;
        return rv;
    }

    if ((0 != fwInstance->cryptokiVersion.major) ||
        (0 != fwInstance->cryptokiVersion.minor)) {
        rv = fwInstance->cryptokiVersion;
        goto done;
    }

    if (fwInstance->mdInstance->GetCryptokiVersion) {
        fwInstance->cryptokiVersion = fwInstance->mdInstance->GetCryptokiVersion(
            fwInstance->mdInstance, fwInstance);
    } else {
        fwInstance->cryptokiVersion.major = 2;
        fwInstance->cryptokiVersion.minor = 1;
    }

    rv = fwInstance->cryptokiVersion;

done:
    (void)nssCKFWMutex_Unlock(fwInstance->mutex);
    return rv;
}

/*
 * nssCKFWInstance_GetManufacturerID
 *
 */
NSS_IMPLEMENT CK_RV
nssCKFWInstance_GetManufacturerID(
    NSSCKFWInstance *fwInstance,
    CK_CHAR manufacturerID[32])
{
    CK_RV error = CKR_OK;

#ifdef NSSDEBUG
    if ((CK_CHAR_PTR)NULL == manufacturerID) {
        return CKR_ARGUMENTS_BAD;
    }

    error = nssCKFWInstance_verifyPointer(fwInstance);
    if (CKR_OK != error) {
        return error;
    }
#endif /* NSSDEBUG */

    error = nssCKFWMutex_Lock(fwInstance->mutex);
    if (CKR_OK != error) {
        return error;
    }

    if (!fwInstance->manufacturerID) {
        if (fwInstance->mdInstance->GetManufacturerID) {
            fwInstance->manufacturerID = fwInstance->mdInstance->GetManufacturerID(
                fwInstance->mdInstance, fwInstance, &error);
            if ((!fwInstance->manufacturerID) && (CKR_OK != error)) {
                goto done;
            }
        } else {
            fwInstance->manufacturerID = (NSSUTF8 *)"";
        }
    }

    (void)nssUTF8_CopyIntoFixedBuffer(fwInstance->manufacturerID, (char *)manufacturerID, 32, ' ');
    error = CKR_OK;

done:
    (void)nssCKFWMutex_Unlock(fwInstance->mutex);
    return error;
}

/*
 * nssCKFWInstance_GetFlags
 *
 */
NSS_IMPLEMENT CK_ULONG
nssCKFWInstance_GetFlags(
    NSSCKFWInstance *fwInstance)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWInstance_verifyPointer(fwInstance)) {
        return (CK_ULONG)0;
    }
#endif /* NSSDEBUG */

    /* No "instance flags" are yet defined by Cryptoki. */
    return (CK_ULONG)0;
}

/*
 * nssCKFWInstance_GetLibraryDescription
 *
 */
NSS_IMPLEMENT CK_RV
nssCKFWInstance_GetLibraryDescription(
    NSSCKFWInstance *fwInstance,
    CK_CHAR libraryDescription[32])
{
    CK_RV error = CKR_OK;

#ifdef NSSDEBUG
    if ((CK_CHAR_PTR)NULL == libraryDescription) {
        return CKR_ARGUMENTS_BAD;
    }

    error = nssCKFWInstance_verifyPointer(fwInstance);
    if (CKR_OK != error) {
        return error;
    }
#endif /* NSSDEBUG */

    error = nssCKFWMutex_Lock(fwInstance->mutex);
    if (CKR_OK != error) {
        return error;
    }

    if (!fwInstance->libraryDescription) {
        if (fwInstance->mdInstance->GetLibraryDescription) {
            fwInstance->libraryDescription = fwInstance->mdInstance->GetLibraryDescription(
                fwInstance->mdInstance, fwInstance, &error);
            if ((!fwInstance->libraryDescription) && (CKR_OK != error)) {
                goto done;
            }
        } else {
            fwInstance->libraryDescription = (NSSUTF8 *)"";
        }
    }

    (void)nssUTF8_CopyIntoFixedBuffer(fwInstance->libraryDescription, (char *)libraryDescription, 32, ' ');
    error = CKR_OK;

done:
    (void)nssCKFWMutex_Unlock(fwInstance->mutex);
    return error;
}

/*
 * nssCKFWInstance_GetLibraryVersion
 *
 */
NSS_IMPLEMENT CK_VERSION
nssCKFWInstance_GetLibraryVersion(
    NSSCKFWInstance *fwInstance)
{
    CK_VERSION rv;

#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWInstance_verifyPointer(fwInstance)) {
        rv.major = rv.minor = 0;
        return rv;
    }
#endif /* NSSDEBUG */

    if (CKR_OK != nssCKFWMutex_Lock(fwInstance->mutex)) {
        rv.major = rv.minor = 0;
        return rv;
    }

    if ((0 != fwInstance->libraryVersion.major) ||
        (0 != fwInstance->libraryVersion.minor)) {
        rv = fwInstance->libraryVersion;
        goto done;
    }

    if (fwInstance->mdInstance->GetLibraryVersion) {
        fwInstance->libraryVersion = fwInstance->mdInstance->GetLibraryVersion(
            fwInstance->mdInstance, fwInstance);
    } else {
        fwInstance->libraryVersion.major = 0;
        fwInstance->libraryVersion.minor = 3;
    }

    rv = fwInstance->libraryVersion;
done:
    (void)nssCKFWMutex_Unlock(fwInstance->mutex);
    return rv;
}

/*
 * nssCKFWInstance_GetModuleHandlesSessionObjects
 *
 */
NSS_IMPLEMENT CK_BBOOL
nssCKFWInstance_GetModuleHandlesSessionObjects(
    NSSCKFWInstance *fwInstance)
{
#ifdef NSSDEBUG
    if (CKR_OK != nssCKFWInstance_verifyPointer(fwInstance)) {
        return CK_FALSE;
    }
#endif /* NSSDEBUG */

    return fwInstance->moduleHandlesSessionObjects;
}

/*
 * nssCKFWInstance_GetSlots
 *
 */
NSS_IMPLEMENT NSSCKFWSlot **
nssCKFWInstance_GetSlots(
    NSSCKFWInstance *fwInstance,
    CK_RV *pError)
{
#ifdef NSSDEBUG
    if (!pError) {
        return (NSSCKFWSlot **)NULL;
    }

    *pError = nssCKFWInstance_verifyPointer(fwInstance);
    if (CKR_OK != *pError) {
        return (NSSCKFWSlot **)NULL;
    }
#endif /* NSSDEBUG */

    return fwInstance->fwSlotList;
}

/*
 * nssCKFWInstance_WaitForSlotEvent
 *
 */
NSS_IMPLEMENT NSSCKFWSlot *
nssCKFWInstance_WaitForSlotEvent(
    NSSCKFWInstance *fwInstance,
    CK_BBOOL block,
    CK_RV *pError)
{
    NSSCKFWSlot *fwSlot = (NSSCKFWSlot *)NULL;
    NSSCKMDSlot *mdSlot;
    CK_ULONG i, n;

#ifdef NSSDEBUG
    if (!pError) {
        return (NSSCKFWSlot *)NULL;
    }

    *pError = nssCKFWInstance_verifyPointer(fwInstance);
    if (CKR_OK != *pError) {
        return (NSSCKFWSlot *)NULL;
    }

    switch (block) {
        case CK_TRUE:
        case CK_FALSE:
            break;
        default:
            *pError = CKR_ARGUMENTS_BAD;
            return (NSSCKFWSlot *)NULL;
    }
#endif /* NSSDEBUG */

    if (!fwInstance->mdInstance->WaitForSlotEvent) {
        *pError = CKR_NO_EVENT;
        return (NSSCKFWSlot *)NULL;
    }

    mdSlot = fwInstance->mdInstance->WaitForSlotEvent(
        fwInstance->mdInstance,
        fwInstance,
        block,
        pError);

    if (!mdSlot) {
        return (NSSCKFWSlot *)NULL;
    }

    n = nssCKFWInstance_GetNSlots(fwInstance, pError);
    if (((CK_ULONG)0 == n) && (CKR_OK != *pError)) {
        return (NSSCKFWSlot *)NULL;
    }

    for (i = 0; i < n; i++) {
        if (fwInstance->mdSlotList[i] == mdSlot) {
            fwSlot = fwInstance->fwSlotList[i];
            break;
        }
    }

    if (!fwSlot) {
        /* Internal error */
        *pError = CKR_GENERAL_ERROR;
        return (NSSCKFWSlot *)NULL;
    }

    return fwSlot;
}

/*
 * NSSCKFWInstance_GetMDInstance
 *
 */
NSS_IMPLEMENT NSSCKMDInstance *
NSSCKFWInstance_GetMDInstance(
    NSSCKFWInstance *fwInstance)
{
#ifdef DEBUG
    if (CKR_OK != nssCKFWInstance_verifyPointer(fwInstance)) {
        return (NSSCKMDInstance *)NULL;
    }
#endif /* DEBUG */

    return nssCKFWInstance_GetMDInstance(fwInstance);
}

/*
 * NSSCKFWInstance_GetArena
 *
 */
NSS_IMPLEMENT NSSArena *
NSSCKFWInstance_GetArena(
    NSSCKFWInstance *fwInstance,
    CK_RV *pError)
{
#ifdef DEBUG
    if (!pError) {
        return (NSSArena *)NULL;
    }

    *pError = nssCKFWInstance_verifyPointer(fwInstance);
    if (CKR_OK != *pError) {
        return (NSSArena *)NULL;
    }
#endif /* DEBUG */

    return nssCKFWInstance_GetArena(fwInstance, pError);
}

/*
 * NSSCKFWInstance_MayCreatePthreads
 *
 */
NSS_IMPLEMENT CK_BBOOL
NSSCKFWInstance_MayCreatePthreads(
    NSSCKFWInstance *fwInstance)
{
#ifdef DEBUG
    if (CKR_OK != nssCKFWInstance_verifyPointer(fwInstance)) {
        return CK_FALSE;
    }
#endif /* DEBUG */

    return nssCKFWInstance_MayCreatePthreads(fwInstance);
}

/*
 * NSSCKFWInstance_CreateMutex
 *
 */
NSS_IMPLEMENT NSSCKFWMutex *
NSSCKFWInstance_CreateMutex(
    NSSCKFWInstance *fwInstance,
    NSSArena *arena,
    CK_RV *pError)
{
#ifdef DEBUG
    if (!pError) {
        return (NSSCKFWMutex *)NULL;
    }

    *pError = nssCKFWInstance_verifyPointer(fwInstance);
    if (CKR_OK != *pError) {
        return (NSSCKFWMutex *)NULL;
    }
#endif /* DEBUG */

    return nssCKFWInstance_CreateMutex(fwInstance, arena, pError);
}

/*
 * NSSCKFWInstance_GetConfigurationData
 *
 */
NSS_IMPLEMENT NSSUTF8 *
NSSCKFWInstance_GetConfigurationData(
    NSSCKFWInstance *fwInstance)
{
#ifdef DEBUG
    if (CKR_OK != nssCKFWInstance_verifyPointer(fwInstance)) {
        return (NSSUTF8 *)NULL;
    }
#endif /* DEBUG */

    return nssCKFWInstance_GetConfigurationData(fwInstance);
}

/*
 * NSSCKFWInstance_GetInitArgs
 *
 */
NSS_IMPLEMENT CK_C_INITIALIZE_ARGS_PTR
NSSCKFWInstance_GetInitArgs(
    NSSCKFWInstance *fwInstance)
{
#ifdef DEBUG
    if (CKR_OK != nssCKFWInstance_verifyPointer(fwInstance)) {
        return (CK_C_INITIALIZE_ARGS_PTR)NULL;
    }
#endif /* DEBUG */

    return nssCKFWInstance_GetInitArgs(fwInstance);
}

/*
 * nssCKFWInstance_DestroySessionHandle
 *
 */
NSS_IMPLEMENT void
NSSCKFWInstance_DestroySessionHandle(
    NSSCKFWInstance *fwInstance,
    CK_SESSION_HANDLE hSession)
{
    nssCKFWInstance_DestroySessionHandle(fwInstance, hSession);
}

/*
 * nssCKFWInstance_FindSessionHandle
 *
 */
NSS_IMPLEMENT CK_SESSION_HANDLE
NSSCKFWInstance_FindSessionHandle(
    NSSCKFWInstance *fwInstance,
    NSSCKFWSession *fwSession)
{
    return nssCKFWInstance_FindSessionHandle(fwInstance, fwSession);
}
