/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ckdbm.h"

static void
nss_dbm_mdObject_Finalize(
    NSSCKMDObject *mdObject,
    NSSCKFWObject *fwObject,
    NSSCKMDSession *mdSession,
    NSSCKFWSession *fwSession,
    NSSCKMDToken *mdToken,
    NSSCKFWToken *fwToken,
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance)
{
    ;
}

static CK_RV
nss_dbm_mdObject_Destroy(
    NSSCKMDObject *mdObject,
    NSSCKFWObject *fwObject,
    NSSCKMDSession *mdSession,
    NSSCKFWSession *fwSession,
    NSSCKMDToken *mdToken,
    NSSCKFWToken *fwToken,
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance)
{
    nss_dbm_object_t *object = (nss_dbm_object_t *)mdObject->etc;
    return nss_dbm_db_delete_object(object->handle);
}

static CK_ULONG
nss_dbm_mdObject_GetAttributeCount(
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
    nss_dbm_object_t *object = (nss_dbm_object_t *)mdObject->etc;
    nss_dbm_session_t *session = (nss_dbm_session_t *)mdSession->etc;
    return nss_dbm_db_get_object_attribute_count(object->handle, pError,
                                                 &session->deviceError);
}

static CK_RV
nss_dbm_mdObject_GetAttributeTypes(
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
    nss_dbm_object_t *object = (nss_dbm_object_t *)mdObject->etc;
    nss_dbm_session_t *session = (nss_dbm_session_t *)mdSession->etc;
    return nss_dbm_db_get_object_attribute_types(object->handle, typeArray,
                                                 ulCount, &session->deviceError);
}

static CK_ULONG
nss_dbm_mdObject_GetAttributeSize(
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
    nss_dbm_object_t *object = (nss_dbm_object_t *)mdObject->etc;
    nss_dbm_session_t *session = (nss_dbm_session_t *)mdSession->etc;
    return nss_dbm_db_get_object_attribute_size(object->handle, attribute, pError,
                                                &session->deviceError);
}

static NSSItem *
nss_dbm_mdObject_GetAttribute(
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
    nss_dbm_object_t *object = (nss_dbm_object_t *)mdObject->etc;
    nss_dbm_session_t *session = (nss_dbm_session_t *)mdSession->etc;
    return nss_dbm_db_get_object_attribute(object->handle, object->arena, attribute,
                                           pError, &session->deviceError);
}

static CK_RV
nss_dbm_mdObject_SetAttribute(
    NSSCKMDObject *mdObject,
    NSSCKFWObject *fwObject,
    NSSCKMDSession *mdSession,
    NSSCKFWSession *fwSession,
    NSSCKMDToken *mdToken,
    NSSCKFWToken *fwToken,
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance,
    CK_ATTRIBUTE_TYPE attribute,
    NSSItem *value)
{
    nss_dbm_object_t *object = (nss_dbm_object_t *)mdObject->etc;
    nss_dbm_session_t *session = (nss_dbm_session_t *)mdSession->etc;
    return nss_dbm_db_set_object_attribute(object->handle, attribute, value,
                                           &session->deviceError);
}

NSS_IMPLEMENT NSSCKMDObject *
nss_dbm_mdObject_factory(
    nss_dbm_object_t *object,
    CK_RV *pError)
{
    NSSCKMDObject *rv;

    rv = nss_ZNEW(object->arena, NSSCKMDObject);
    if ((NSSCKMDObject *)NULL == rv) {
        *pError = CKR_HOST_MEMORY;
        return (NSSCKMDObject *)NULL;
    }

    rv->etc = (void *)object;
    rv->Finalize = nss_dbm_mdObject_Finalize;
    rv->Destroy = nss_dbm_mdObject_Destroy;
    /*  IsTokenObject can be deferred */
    rv->GetAttributeCount = nss_dbm_mdObject_GetAttributeCount;
    rv->GetAttributeTypes = nss_dbm_mdObject_GetAttributeTypes;
    rv->GetAttributeSize = nss_dbm_mdObject_GetAttributeSize;
    rv->GetAttribute = nss_dbm_mdObject_GetAttribute;
    rv->SetAttribute = nss_dbm_mdObject_SetAttribute;
    /*  GetObjectSize can be deferred */

    return rv;
}
