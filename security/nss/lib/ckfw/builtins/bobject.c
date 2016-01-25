/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtins.h"

/*
 * builtins/object.c
 *
 * This file implements the NSSCKMDObject object for the
 * "builtin objects" cryptoki module.
 */

/*
 * Finalize - unneeded
 * Destroy - CKR_SESSION_READ_ONLY
 * IsTokenObject - CK_TRUE
 * GetAttributeCount
 * GetAttributeTypes
 * GetAttributeSize
 * GetAttribute
 * SetAttribute - unneeded
 * GetObjectSize
 */

static CK_RV
builtins_mdObject_Destroy(
    NSSCKMDObject *mdObject,
    NSSCKFWObject *fwObject,
    NSSCKMDSession *mdSession,
    NSSCKFWSession *fwSession,
    NSSCKMDToken *mdToken,
    NSSCKFWToken *fwToken,
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance)
{
    return CKR_SESSION_READ_ONLY;
}

static CK_BBOOL
builtins_mdObject_IsTokenObject(
    NSSCKMDObject *mdObject,
    NSSCKFWObject *fwObject,
    NSSCKMDSession *mdSession,
    NSSCKFWSession *fwSession,
    NSSCKMDToken *mdToken,
    NSSCKFWToken *fwToken,
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance)
{
    return CK_TRUE;
}

static CK_ULONG
builtins_mdObject_GetAttributeCount(
    NSSCKMDObject *mdObject,
    NSSCKFWObject *fwObject,
    NSSCKMDSession *mdSession,
    NSSCKFWSession *fwSession,
    NSSCKMDToken *mdToken,
    NSSCKFWToken *fwToken,
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance,
    CK_RV *pError)
{
    builtinsInternalObject *io = (builtinsInternalObject *)mdObject->etc;
    return io->n;
}

static CK_RV
builtins_mdObject_GetAttributeTypes(
    NSSCKMDObject *mdObject,
    NSSCKFWObject *fwObject,
    NSSCKMDSession *mdSession,
    NSSCKFWSession *fwSession,
    NSSCKMDToken *mdToken,
    NSSCKFWToken *fwToken,
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance,
    CK_ATTRIBUTE_TYPE_PTR typeArray,
    CK_ULONG ulCount)
{
    builtinsInternalObject *io = (builtinsInternalObject *)mdObject->etc;
    CK_ULONG i;

    if (io->n != ulCount) {
        return CKR_BUFFER_TOO_SMALL;
    }

    for (i = 0; i < io->n; i++) {
        typeArray[i] = io->types[i];
    }

    return CKR_OK;
}

static CK_ULONG
builtins_mdObject_GetAttributeSize(
    NSSCKMDObject *mdObject,
    NSSCKFWObject *fwObject,
    NSSCKMDSession *mdSession,
    NSSCKFWSession *fwSession,
    NSSCKMDToken *mdToken,
    NSSCKFWToken *fwToken,
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance,
    CK_ATTRIBUTE_TYPE attribute,
    CK_RV *pError)
{
    builtinsInternalObject *io = (builtinsInternalObject *)mdObject->etc;
    CK_ULONG i;

    for (i = 0; i < io->n; i++) {
        if (attribute == io->types[i]) {
            return (CK_ULONG)(io->items[i].size);
        }
    }

    *pError = CKR_ATTRIBUTE_TYPE_INVALID;
    return 0;
}

static NSSCKFWItem
builtins_mdObject_GetAttribute(
    NSSCKMDObject *mdObject,
    NSSCKFWObject *fwObject,
    NSSCKMDSession *mdSession,
    NSSCKFWSession *fwSession,
    NSSCKMDToken *mdToken,
    NSSCKFWToken *fwToken,
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance,
    CK_ATTRIBUTE_TYPE attribute,
    CK_RV *pError)
{
    NSSCKFWItem mdItem;
    builtinsInternalObject *io = (builtinsInternalObject *)mdObject->etc;
    CK_ULONG i;

    mdItem.needsFreeing = PR_FALSE;
    mdItem.item = (NSSItem *)NULL;

    for (i = 0; i < io->n; i++) {
        if (attribute == io->types[i]) {
            mdItem.item = (NSSItem *)&io->items[i];
            return mdItem;
        }
    }

    *pError = CKR_ATTRIBUTE_TYPE_INVALID;
    return mdItem;
}

static CK_ULONG
builtins_mdObject_GetObjectSize(
    NSSCKMDObject *mdObject,
    NSSCKFWObject *fwObject,
    NSSCKMDSession *mdSession,
    NSSCKFWSession *fwSession,
    NSSCKMDToken *mdToken,
    NSSCKFWToken *fwToken,
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance,
    CK_RV *pError)
{
    builtinsInternalObject *io = (builtinsInternalObject *)mdObject->etc;
    CK_ULONG i;
    CK_ULONG rv = sizeof(CK_ULONG);

    for (i = 0; i < io->n; i++) {
        rv += sizeof(CK_ATTRIBUTE_TYPE) + sizeof(NSSItem) + io->items[i].size;
    }

    return rv;
}

static const NSSCKMDObject
    builtins_prototype_mdObject = {
        (void *)NULL, /* etc */
        NULL,         /* Finalize */
        builtins_mdObject_Destroy,
        builtins_mdObject_IsTokenObject,
        builtins_mdObject_GetAttributeCount,
        builtins_mdObject_GetAttributeTypes,
        builtins_mdObject_GetAttributeSize,
        builtins_mdObject_GetAttribute,
        NULL, /* FreeAttribute */
        NULL, /* SetAttribute */
        builtins_mdObject_GetObjectSize,
        (void *)NULL /* null terminator */
    };

NSS_IMPLEMENT NSSCKMDObject *
nss_builtins_CreateMDObject(
    NSSArena *arena,
    builtinsInternalObject *io,
    CK_RV *pError)
{
    if ((void *)NULL == io->mdObject.etc) {
        (void)nsslibc_memcpy(&io->mdObject, &builtins_prototype_mdObject,
                             sizeof(builtins_prototype_mdObject));
        io->mdObject.etc = (void *)io;
    }

    return &io->mdObject;
}
