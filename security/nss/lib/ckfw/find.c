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
static const char CVS_ID[] = "@(#) $RCSfile: find.c,v $ $Revision: 1.9 $ $Date: 2009/02/09 07:55:52 $";
#endif /* DEBUG */

/*
 * find.c
 *
 * This file implements the nssCKFWFindObjects type and methods.
 */

#ifndef CK_H
#include "ck.h"
#endif /* CK_H */

/*
 * NSSCKFWFindObjects
 *
 *  -- create/destroy --
 *  nssCKFWFindObjects_Create
 *  nssCKFWFindObjects_Destroy
 *
 *  -- public accessors --
 *  NSSCKFWFindObjects_GetMDFindObjects
 * 
 *  -- implement public accessors --
 *  nssCKFWFindObjects_GetMDFindObjects
 *
 *  -- private accessors --
 *
 *  -- module fronts --
 *  nssCKFWFindObjects_Next
 */

struct NSSCKFWFindObjectsStr {
  NSSCKFWMutex *mutex; /* merely to serialise the MDObject calls */
  NSSCKMDFindObjects *mdfo1;
  NSSCKMDFindObjects *mdfo2;
  NSSCKFWSession *fwSession;
  NSSCKMDSession *mdSession;
  NSSCKFWToken *fwToken;
  NSSCKMDToken *mdToken;
  NSSCKFWInstance *fwInstance;
  NSSCKMDInstance *mdInstance;

  NSSCKMDFindObjects *mdFindObjects; /* varies */
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
 * just do these routines as no-ops.
 */

static CK_RV
findObjects_add_pointer
(
  const NSSCKFWFindObjects *fwFindObjects
)
{
  return CKR_OK;
}

static CK_RV
findObjects_remove_pointer
(
  const NSSCKFWFindObjects *fwFindObjects
)
{
  return CKR_OK;
}

NSS_IMPLEMENT CK_RV
nssCKFWFindObjects_verifyPointer
(
  const NSSCKFWFindObjects *fwFindObjects
)
{
  return CKR_OK;
}

#endif /* DEBUG */

/*
 * nssCKFWFindObjects_Create
 *
 */
NSS_EXTERN NSSCKFWFindObjects *
nssCKFWFindObjects_Create
(
  NSSCKFWSession *fwSession,
  NSSCKFWToken *fwToken,
  NSSCKFWInstance *fwInstance,
  NSSCKMDFindObjects *mdFindObjects1,
  NSSCKMDFindObjects *mdFindObjects2,
  CK_RV *pError
)
{
  NSSCKFWFindObjects *fwFindObjects = NULL;
  NSSCKMDSession *mdSession;
  NSSCKMDToken *mdToken;
  NSSCKMDInstance *mdInstance;

  mdSession = nssCKFWSession_GetMDSession(fwSession);
  mdToken = nssCKFWToken_GetMDToken(fwToken);
  mdInstance = nssCKFWInstance_GetMDInstance(fwInstance);

  fwFindObjects = nss_ZNEW(NULL, NSSCKFWFindObjects);
  if (!fwFindObjects) {
    *pError = CKR_HOST_MEMORY;
    goto loser;
  }

  fwFindObjects->mdfo1 = mdFindObjects1;
  fwFindObjects->mdfo2 = mdFindObjects2;
  fwFindObjects->fwSession = fwSession;
  fwFindObjects->mdSession = mdSession;
  fwFindObjects->fwToken = fwToken;
  fwFindObjects->mdToken = mdToken;
  fwFindObjects->fwInstance = fwInstance;
  fwFindObjects->mdInstance = mdInstance;

  fwFindObjects->mutex = nssCKFWInstance_CreateMutex(fwInstance, NULL, pError);
  if (!fwFindObjects->mutex) {
    goto loser;
  }

#ifdef DEBUG
  *pError = findObjects_add_pointer(fwFindObjects);
  if( CKR_OK != *pError ) {
    goto loser;
  }
#endif /* DEBUG */

  return fwFindObjects;

 loser:
  if( fwFindObjects ) {
    if( NULL != mdFindObjects1 ) {
      if( NULL != mdFindObjects1->Final ) {
        fwFindObjects->mdFindObjects = mdFindObjects1;
        mdFindObjects1->Final(mdFindObjects1, fwFindObjects, mdSession, 
          fwSession, mdToken, fwToken, mdInstance, fwInstance);
      }
    }

    if( NULL != mdFindObjects2 ) {
      if( NULL != mdFindObjects2->Final ) {
        fwFindObjects->mdFindObjects = mdFindObjects2;
        mdFindObjects2->Final(mdFindObjects2, fwFindObjects, mdSession, 
          fwSession, mdToken, fwToken, mdInstance, fwInstance);
      }
    }

    nss_ZFreeIf(fwFindObjects);
  }

  if( CKR_OK == *pError ) {
    *pError = CKR_GENERAL_ERROR;
  }

  return (NSSCKFWFindObjects *)NULL;
}


/*
 * nssCKFWFindObjects_Destroy
 *
 */
NSS_EXTERN void
nssCKFWFindObjects_Destroy
(
  NSSCKFWFindObjects *fwFindObjects
)
{
#ifdef NSSDEBUG
  if( CKR_OK != nssCKFWFindObjects_verifyPointer(fwFindObjects) ) {
    return;
  }
#endif /* NSSDEBUG */

  (void)nssCKFWMutex_Destroy(fwFindObjects->mutex);

  if (fwFindObjects->mdfo1) {
    if (fwFindObjects->mdfo1->Final) {
      fwFindObjects->mdFindObjects = fwFindObjects->mdfo1;
      fwFindObjects->mdfo1->Final(fwFindObjects->mdfo1, fwFindObjects,
        fwFindObjects->mdSession, fwFindObjects->fwSession, 
        fwFindObjects->mdToken, fwFindObjects->fwToken,
        fwFindObjects->mdInstance, fwFindObjects->fwInstance);
    }
  }

  if (fwFindObjects->mdfo2) {
    if (fwFindObjects->mdfo2->Final) {
      fwFindObjects->mdFindObjects = fwFindObjects->mdfo2;
      fwFindObjects->mdfo2->Final(fwFindObjects->mdfo2, fwFindObjects,
        fwFindObjects->mdSession, fwFindObjects->fwSession, 
        fwFindObjects->mdToken, fwFindObjects->fwToken,
        fwFindObjects->mdInstance, fwFindObjects->fwInstance);
    }
  }

  nss_ZFreeIf(fwFindObjects);

#ifdef DEBUG
  (void)findObjects_remove_pointer(fwFindObjects);
#endif /* DEBUG */

  return;
}

/*
 * nssCKFWFindObjects_GetMDFindObjects
 *
 */
NSS_EXTERN NSSCKMDFindObjects *
nssCKFWFindObjects_GetMDFindObjects
(
  NSSCKFWFindObjects *fwFindObjects
)
{
#ifdef NSSDEBUG
  if( CKR_OK != nssCKFWFindObjects_verifyPointer(fwFindObjects) ) {
    return (NSSCKMDFindObjects *)NULL;
  }
#endif /* NSSDEBUG */

  return fwFindObjects->mdFindObjects;
}

/*
 * nssCKFWFindObjects_Next
 *
 */
NSS_EXTERN NSSCKFWObject *
nssCKFWFindObjects_Next
(
  NSSCKFWFindObjects *fwFindObjects,
  NSSArena *arenaOpt,
  CK_RV *pError
)
{
  NSSCKMDObject *mdObject;
  NSSCKFWObject *fwObject = (NSSCKFWObject *)NULL;
  NSSArena *objArena;

#ifdef NSSDEBUG
  if (!pError) {
    return (NSSCKFWObject *)NULL;
  }

  *pError = nssCKFWFindObjects_verifyPointer(fwFindObjects);
  if( CKR_OK != *pError ) {
    return (NSSCKFWObject *)NULL;
  }
#endif /* NSSDEBUG */

  *pError = nssCKFWMutex_Lock(fwFindObjects->mutex);
  if( CKR_OK != *pError ) {
    return (NSSCKFWObject *)NULL;
  }

  if (fwFindObjects->mdfo1) {
    if (fwFindObjects->mdfo1->Next) {
      fwFindObjects->mdFindObjects = fwFindObjects->mdfo1;
      mdObject = fwFindObjects->mdfo1->Next(fwFindObjects->mdfo1,
        fwFindObjects, fwFindObjects->mdSession, fwFindObjects->fwSession,
        fwFindObjects->mdToken, fwFindObjects->fwToken, 
        fwFindObjects->mdInstance, fwFindObjects->fwInstance,
        arenaOpt, pError);
      if (!mdObject) {
        if( CKR_OK != *pError ) {
          goto done;
        }

        /* All done. */
        fwFindObjects->mdfo1->Final(fwFindObjects->mdfo1, fwFindObjects,
          fwFindObjects->mdSession, fwFindObjects->fwSession,
          fwFindObjects->mdToken, fwFindObjects->fwToken, 
          fwFindObjects->mdInstance, fwFindObjects->fwInstance);
        fwFindObjects->mdfo1 = (NSSCKMDFindObjects *)NULL;
      } else {
        goto wrap;
      }
    }
  }

  if (fwFindObjects->mdfo2) {
    if (fwFindObjects->mdfo2->Next) {
      fwFindObjects->mdFindObjects = fwFindObjects->mdfo2;
      mdObject = fwFindObjects->mdfo2->Next(fwFindObjects->mdfo2,
        fwFindObjects, fwFindObjects->mdSession, fwFindObjects->fwSession,
        fwFindObjects->mdToken, fwFindObjects->fwToken, 
        fwFindObjects->mdInstance, fwFindObjects->fwInstance,
        arenaOpt, pError);
      if (!mdObject) {
        if( CKR_OK != *pError ) {
          goto done;
        }

        /* All done. */
        fwFindObjects->mdfo2->Final(fwFindObjects->mdfo2, fwFindObjects,
          fwFindObjects->mdSession, fwFindObjects->fwSession,
          fwFindObjects->mdToken, fwFindObjects->fwToken, 
          fwFindObjects->mdInstance, fwFindObjects->fwInstance);
        fwFindObjects->mdfo2 = (NSSCKMDFindObjects *)NULL;
      } else {
        goto wrap;
      }
    }
  }
  
  /* No more objects */
  *pError = CKR_OK;
  goto done;

 wrap:
  /*
   * This seems is less than ideal-- we should determine if it's a token
   * object or a session object, and use the appropriate arena.
   * But that duplicates logic in nssCKFWObject_IsTokenObject.
   * Also we should lookup the real session the object was created on
   * if the object was a session object... however this code is actually
   * correct because nssCKFWObject_Create will return a cached version of
   * the object from it's hash. This is necessary because 1) we don't want
   * to create an arena style leak (where our arena grows with every search),
   * and 2) we want the same object to always have the same ID. This means
   * the only case the nssCKFWObject_Create() will need the objArena and the
   * Session is in the case of token objects (session objects should already
   * exist in the cache from their initial creation). So this code is correct,
   * but it depends on nssCKFWObject_Create caching all objects.
   */
  objArena = nssCKFWToken_GetArena(fwFindObjects->fwToken, pError);
  if (!objArena) {
    if( CKR_OK == *pError ) {
      *pError = CKR_HOST_MEMORY;
    }
    goto done;
  }

  fwObject = nssCKFWObject_Create(objArena, mdObject,
               NULL, fwFindObjects->fwToken, 
               fwFindObjects->fwInstance, pError);
  if (!fwObject) {
    if( CKR_OK == *pError ) {
      *pError = CKR_GENERAL_ERROR;
    }
  }

 done:
  (void)nssCKFWMutex_Unlock(fwFindObjects->mutex);
  return fwObject;
}

/*
 * NSSCKFWFindObjects_GetMDFindObjects
 *
 */

NSS_EXTERN NSSCKMDFindObjects *
NSSCKFWFindObjects_GetMDFindObjects
(
  NSSCKFWFindObjects *fwFindObjects
)
{
#ifdef DEBUG
  if( CKR_OK != nssCKFWFindObjects_verifyPointer(fwFindObjects) ) {
    return (NSSCKMDFindObjects *)NULL;
  }
#endif /* DEBUG */

  return nssCKFWFindObjects_GetMDFindObjects(fwFindObjects);
}
