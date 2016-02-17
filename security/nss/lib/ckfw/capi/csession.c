/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ckcapi.h"

/*
 * ckcapi/csession.c
 *
 * This file implements the NSSCKMDSession object for the
 * "nss to capi" cryptoki module.
 */

static NSSCKMDFindObjects *
ckcapi_mdSession_FindObjectsInit(
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
    return nss_ckcapi_FindObjectsInit(fwSession, pTemplate, ulAttributeCount, pError);
}

static NSSCKMDObject *
ckcapi_mdSession_CreateObject(
    NSSCKMDSession *mdSession,
    NSSCKFWSession *fwSession,
    NSSCKMDToken *mdToken,
    NSSCKFWToken *fwToken,
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance,
    NSSArena *arena,
    CK_ATTRIBUTE_PTR pTemplate,
    CK_ULONG ulAttributeCount,
    CK_RV *pError)
{
    return nss_ckcapi_CreateObject(fwSession, pTemplate, ulAttributeCount, pError);
}

NSS_IMPLEMENT NSSCKMDSession *
nss_ckcapi_CreateSession(
    NSSCKFWSession *fwSession,
    CK_RV *pError)
{
    NSSArena *arena;
    NSSCKMDSession *rv;

    arena = NSSCKFWSession_GetArena(fwSession, pError);
    if ((NSSArena *)NULL == arena) {
        return (NSSCKMDSession *)NULL;
    }

    rv = nss_ZNEW(arena, NSSCKMDSession);
    if ((NSSCKMDSession *)NULL == rv) {
        *pError = CKR_HOST_MEMORY;
        return (NSSCKMDSession *)NULL;
    }

    /*
     * rv was zeroed when allocated, so we only
     * need to set the non-zero members.
     */

    rv->etc = (void *)fwSession;
    /* rv->Close */
    /* rv->GetDeviceError */
    /* rv->Login */
    /* rv->Logout */
    /* rv->InitPIN */
    /* rv->SetPIN */
    /* rv->GetOperationStateLen */
    /* rv->GetOperationState */
    /* rv->SetOperationState */
    rv->CreateObject = ckcapi_mdSession_CreateObject;
    /* rv->CopyObject */
    rv->FindObjectsInit = ckcapi_mdSession_FindObjectsInit;
    /* rv->SeedRandom */
    /* rv->GetRandom */
    /* rv->null */

    return rv;
}
