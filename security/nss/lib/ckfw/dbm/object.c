/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: object.c,v $ $Revision: 1.3 $ $Date: 2006/03/02 22:48:54 $";
#endif /* DEBUG */

#include "ckdbm.h"

static void
nss_dbm_mdObject_Finalize
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  ;
}

static CK_RV
nss_dbm_mdObject_Destroy
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  nss_dbm_object_t *object = (nss_dbm_object_t *)mdObject->etc;
  return nss_dbm_db_delete_object(object->handle);
}

static CK_ULONG
nss_dbm_mdObject_GetAttributeCount
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  nss_dbm_object_t *object = (nss_dbm_object_t *)mdObject->etc;
  nss_dbm_session_t *session = (nss_dbm_session_t *)mdSession->etc;
  return nss_dbm_db_get_object_attribute_count(object->handle, pError, 
                                               &session->deviceError);
}

static CK_RV
nss_dbm_mdObject_GetAttributeTypes
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_ATTRIBUTE_TYPE_PTR typeArray,
  CK_ULONG ulCount
)
{
  nss_dbm_object_t *object = (nss_dbm_object_t *)mdObject->etc;
  nss_dbm_session_t *session = (nss_dbm_session_t *)mdSession->etc;
  return nss_dbm_db_get_object_attribute_types(object->handle, typeArray,
                                               ulCount, &session->deviceError);
}

static CK_ULONG
nss_dbm_mdObject_GetAttributeSize
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_ATTRIBUTE_TYPE attribute,
  CK_RV *pError
)
{
  nss_dbm_object_t *object = (nss_dbm_object_t *)mdObject->etc;
  nss_dbm_session_t *session = (nss_dbm_session_t *)mdSession->etc;
  return nss_dbm_db_get_object_attribute_size(object->handle, attribute, pError, 
                                              &session->deviceError);
}

static NSSItem *
nss_dbm_mdObject_GetAttribute
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_ATTRIBUTE_TYPE attribute,
  CK_RV *pError
)
{
  nss_dbm_object_t *object = (nss_dbm_object_t *)mdObject->etc;
  nss_dbm_session_t *session = (nss_dbm_session_t *)mdSession->etc;
  return nss_dbm_db_get_object_attribute(object->handle, object->arena, attribute,
                                         pError, &session->deviceError);
}

static CK_RV
nss_dbm_mdObject_SetAttribute
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_ATTRIBUTE_TYPE attribute,
  NSSItem *value
)
{
  nss_dbm_object_t *object = (nss_dbm_object_t *)mdObject->etc;
  nss_dbm_session_t *session = (nss_dbm_session_t *)mdSession->etc;
  return nss_dbm_db_set_object_attribute(object->handle, attribute, value,
                                         &session->deviceError);
}

NSS_IMPLEMENT NSSCKMDObject *
nss_dbm_mdObject_factory
(
  nss_dbm_object_t *object,
  CK_RV *pError
)
{
  NSSCKMDObject *rv;

  rv = nss_ZNEW(object->arena, NSSCKMDObject);
  if( (NSSCKMDObject *)NULL == rv ) {
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
