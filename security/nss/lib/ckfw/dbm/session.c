/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ckdbm.h"

static void
nss_dbm_mdSession_Close(
    NSSCKMDSession *mdSession,
    NSSCKFWSession *fwSession,
    NSSCKMDToken *mdToken,
    NSSCKFWToken *fwToken,
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance)
{
    nss_dbm_session_t *session = (nss_dbm_session_t *)mdSession->etc;

    struct nss_dbm_dbt_node *w;

    /* Lock */
    {
        if (CKR_OK != NSSCKFWMutex_Lock(session->list_lock)) {
            return;
        }

        w = session->session_objects;
        session->session_objects = (struct nss_dbm_dbt_node *)NULL; /* sanity */

        (void)NSSCKFWMutex_Unlock(session->list_lock);
    }

    for (; (struct nss_dbm_dbt_node *)NULL != w; w = w->next) {
        (void)nss_dbm_db_delete_object(w->dbt);
    }
}

static CK_ULONG
nss_dbm_mdSession_GetDeviceError(
    NSSCKMDSession *mdSession,
    NSSCKFWSession *fwSession,
    NSSCKMDToken *mdToken,
    NSSCKFWToken *fwToken,
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance)
{
    nss_dbm_session_t *session = (nss_dbm_session_t *)mdSession->etc;
    return session->deviceError;
}

/* Login isn't needed */
/* Logout isn't needed */
/* InitPIN is irrelevant */
/* SetPIN is irrelevant */
/* GetOperationStateLen is irrelevant */
/* GetOperationState is irrelevant */
/* SetOperationState is irrelevant */

static NSSCKMDObject *
nss_dbm_mdSession_CreateObject(
    NSSCKMDSession *mdSession,
    NSSCKFWSession *fwSession,
    NSSCKMDToken *mdToken,
    NSSCKFWToken *fwToken,
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance,
    NSSArena *handyArenaPointer,
    CK_ATTRIBUTE_PTR pTemplate,
    CK_ULONG ulAttributeCount,
    CK_RV *pError)
{
    nss_dbm_session_t *session = (nss_dbm_session_t *)mdSession->etc;
    nss_dbm_token_t *token = (nss_dbm_token_t *)mdToken->etc;
    CK_ULONG i;
    CK_BBOOL isToken = CK_FALSE; /* defaults to false */
    NSSCKMDObject *rv;
    struct nss_dbm_dbt_node *node = (struct nss_dbm_dbt_node *)NULL;
    nss_dbm_object_t *object;
    nss_dbm_db_t *which_db;

    /* This framework should really pass this to me */
    for (i = 0; i < ulAttributeCount; i++) {
        if (CKA_TOKEN == pTemplate[i].type) {
            isToken = *(CK_BBOOL *)pTemplate[i].pValue;
            break;
        }
    }

    object = nss_ZNEW(handyArenaPointer, nss_dbm_object_t);
    if ((nss_dbm_object_t *)NULL == object) {
        *pError = CKR_HOST_MEMORY;
        return (NSSCKMDObject *)NULL;
    }

    object->arena = handyArenaPointer;
    which_db = isToken ? token->slot->token_db : token->session_db;

    /* Do this before the actual database call; it's easier to recover from */
    rv = nss_dbm_mdObject_factory(object, pError);
    if ((NSSCKMDObject *)NULL == rv) {
        return (NSSCKMDObject *)NULL;
    }

    if (CK_FALSE == isToken) {
        node = nss_ZNEW(session->arena, struct nss_dbm_dbt_node);
        if ((struct nss_dbm_dbt_node *)NULL == node) {
            *pError = CKR_HOST_MEMORY;
            return (NSSCKMDObject *)NULL;
        }
    }

    object->handle = nss_dbm_db_create_object(handyArenaPointer, which_db,
                                              pTemplate, ulAttributeCount,
                                              pError, &session->deviceError);
    if ((nss_dbm_dbt_t *)NULL == object->handle) {
        return (NSSCKMDObject *)NULL;
    }

    if (CK_FALSE == isToken) {
        node->dbt = object->handle;
        /* Lock */
        {
            *pError = NSSCKFWMutex_Lock(session->list_lock);
            if (CKR_OK != *pError) {
                (void)nss_dbm_db_delete_object(object->handle);
                return (NSSCKMDObject *)NULL;
            }

            node->next = session->session_objects;
            session->session_objects = node;

            *pError = NSSCKFWMutex_Unlock(session->list_lock);
        }
    }

    return rv;
}

/* CopyObject isn't needed; the framework will use CreateObject */

static NSSCKMDFindObjects *
nss_dbm_mdSession_FindObjectsInit(
    NSSCKMDSession *mdSession,
    NSSCKFWSession *fwSession,
    NSSCKMDToken *mdToken,
    NSSCKFWToken *fwToken,
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance,
    CK_ATTRIBUTE_PTR pTemplate,
    CK_ULONG ulAttributeCount,
    CK_RV *pError)
{
    nss_dbm_session_t *session = (nss_dbm_session_t *)mdSession->etc;
    nss_dbm_token_t *token = (nss_dbm_token_t *)mdToken->etc;
    NSSArena *arena;
    nss_dbm_find_t *find;
    NSSCKMDFindObjects *rv;

    arena = NSSArena_Create();
    if ((NSSArena *)NULL == arena) {
        *pError = CKR_HOST_MEMORY;
        goto loser;
    }

    find = nss_ZNEW(arena, nss_dbm_find_t);
    if ((nss_dbm_find_t *)NULL == find) {
        *pError = CKR_HOST_MEMORY;
        goto loser;
    }

    find->arena = arena;
    find->list_lock = NSSCKFWInstance_CreateMutex(fwInstance, arena, pError);
    if ((NSSCKFWMutex *)NULL == find->list_lock) {
        goto loser;
    }

    *pError = nss_dbm_db_find_objects(find, token->slot->token_db, pTemplate,
                                      ulAttributeCount, &session->deviceError);
    if (CKR_OK != *pError) {
        goto loser;
    }

    *pError = nss_dbm_db_find_objects(find, token->session_db, pTemplate,
                                      ulAttributeCount, &session->deviceError);
    if (CKR_OK != *pError) {
        goto loser;
    }

    rv = nss_dbm_mdFindObjects_factory(find, pError);
    if ((NSSCKMDFindObjects *)NULL == rv) {
        goto loser;
    }

    return rv;

loser:
    if ((NSSArena *)NULL != arena) {
        (void)NSSArena_Destroy(arena);
    }

    return (NSSCKMDFindObjects *)NULL;
}

/* SeedRandom is irrelevant */
/* GetRandom is irrelevant */

NSS_IMPLEMENT NSSCKMDSession *
nss_dbm_mdSession_factory(
    nss_dbm_token_t *token,
    NSSCKFWSession *fwSession,
    NSSCKFWInstance *fwInstance,
    CK_BBOOL rw,
    CK_RV *pError)
{
    NSSArena *arena;
    nss_dbm_session_t *session;
    NSSCKMDSession *rv;

    arena = NSSCKFWSession_GetArena(fwSession, pError);

    session = nss_ZNEW(arena, nss_dbm_session_t);
    if ((nss_dbm_session_t *)NULL == session) {
        *pError = CKR_HOST_MEMORY;
        return (NSSCKMDSession *)NULL;
    }

    rv = nss_ZNEW(arena, NSSCKMDSession);
    if ((NSSCKMDSession *)NULL == rv) {
        *pError = CKR_HOST_MEMORY;
        return (NSSCKMDSession *)NULL;
    }

    session->arena = arena;
    session->token = token;
    session->list_lock = NSSCKFWInstance_CreateMutex(fwInstance, arena, pError);
    if ((NSSCKFWMutex *)NULL == session->list_lock) {
        return (NSSCKMDSession *)NULL;
    }

    rv->etc = (void *)session;
    rv->Close = nss_dbm_mdSession_Close;
    rv->GetDeviceError = nss_dbm_mdSession_GetDeviceError;
    /*  Login isn't needed */
    /*  Logout isn't needed */
    /*  InitPIN is irrelevant */
    /*  SetPIN is irrelevant */
    /*  GetOperationStateLen is irrelevant */
    /*  GetOperationState is irrelevant */
    /*  SetOperationState is irrelevant */
    rv->CreateObject = nss_dbm_mdSession_CreateObject;
    /*  CopyObject isn't needed; the framework will use CreateObject */
    rv->FindObjectsInit = nss_dbm_mdSession_FindObjectsInit;
    rv->null = NULL;

    return rv;
}
