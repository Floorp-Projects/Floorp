/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: object.c,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:43:50 $ $Name:  $";
#endif /* DEBUG */

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
builtins_mdObject_Destroy
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
  return CKR_SESSION_READ_ONLY;
}

static CK_BBOOL
builtins_mdObject_IsTokenObject
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
  return CK_TRUE;
}

static CK_ULONG
builtins_mdObject_GetAttributeCount
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
  builtinsInternalObject *io = (builtinsInternalObject *)mdObject->etc;
  return io->n;
}

static CK_RV
builtins_mdObject_GetAttributeTypes
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
  builtinsInternalObject *io = (builtinsInternalObject *)mdObject->etc;
  CK_ULONG i;

  if( io->n != ulCount ) {
    return CKR_BUFFER_TOO_SMALL;
  }

  for( i = 0; i < io->n; i++ ) {
    typeArray[i] = io->types[i];
  }

  return CKR_OK;
}

static CK_ULONG
builtins_mdObject_GetAttributeSize
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
  builtinsInternalObject *io = (builtinsInternalObject *)mdObject->etc;
  CK_ULONG i;

  for( i = 0; i < io->n; i++ ) {
    if( attribute == io->types[i] ) {
      return (CK_ULONG)(io->items[i].size);
    }
  }

  *pError = CKR_ATTRIBUTE_TYPE_INVALID;
  return 0;
}

static NSSItem *
builtins_mdObject_GetAttribute
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
  builtinsInternalObject *io = (builtinsInternalObject *)mdObject->etc;
  CK_ULONG i;

  for( i = 0; i < io->n; i++ ) {
    if( attribute == io->types[i] ) {
      return &io->items[i];
    }
  }

  *pError = CKR_ATTRIBUTE_TYPE_INVALID;
  return (NSSItem *)NULL;
}

static CK_ULONG
builtins_mdObject_GetObjectSize
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
  builtinsInternalObject *io = (builtinsInternalObject *)mdObject->etc;
  CK_ULONG i;
  CK_ULONG rv = sizeof(CK_ULONG);

  for( i = 0; i < io->n; i++ ) {
    rv += sizeof(CK_ATTRIBUTE_TYPE) + sizeof(NSSItem) + io->items[i].size;
  }

  return rv;
}

static NSSCKMDObject
builtins_prototype_mdObject = {
  (void *)NULL, /* etc */
  NULL, /* Finalize */
  builtins_mdObject_Destroy,
  builtins_mdObject_IsTokenObject,
  builtins_mdObject_GetAttributeCount,
  builtins_mdObject_GetAttributeTypes,
  builtins_mdObject_GetAttributeSize,
  builtins_mdObject_GetAttribute,
  NULL, /* SetAttribute */
  builtins_mdObject_GetObjectSize,
  (void *)NULL /* null terminator */
};

NSS_IMPLEMENT NSSCKMDObject *
nss_builtins_CreateMDObject
(
  NSSArena *arena,
  builtinsInternalObject *io,
  CK_RV *pError
)
{
  NSSCKMDObject *rv;

  rv = nss_ZNEW(arena, NSSCKMDObject);
  if( (NSSCKMDObject *)NULL == rv ) {
    *pError = CKR_HOST_MEMORY;
    return (NSSCKMDObject *)NULL;
  }

  *rv = builtins_prototype_mdObject;
  rv->etc = (void *)io;

  return rv;
}
