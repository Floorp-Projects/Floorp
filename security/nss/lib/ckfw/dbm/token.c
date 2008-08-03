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
static const char CVS_ID[] = "@(#) $RCSfile: token.c,v $ $Revision: 1.4 $ $Date: 2006/03/02 22:48:54 $";
#endif /* DEBUG */

#include "ckdbm.h"

static CK_RV
nss_dbm_mdToken_Setup
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  nss_dbm_token_t *token = (nss_dbm_token_t *)mdToken->etc;
  CK_RV rv = CKR_OK;

  token->arena = NSSCKFWToken_GetArena(fwToken, &rv);
  token->session_db = nss_dbm_db_open(token->arena, fwInstance, (char *)NULL, 
                                      O_RDWR|O_CREAT, &rv);
  if( (nss_dbm_db_t *)NULL == token->session_db ) {
    return rv;
  }

  /* Add a label record if there isn't one? */

  return CKR_OK;
}

static void
nss_dbm_mdToken_Invalidate
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  nss_dbm_token_t *token = (nss_dbm_token_t *)mdToken->etc;

  if( (nss_dbm_db_t *)NULL != token->session_db ) {
    nss_dbm_db_close(token->session_db);
    token->session_db = (nss_dbm_db_t *)NULL;
  }
}

static CK_RV
nss_dbm_mdToken_InitToken
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  NSSItem *pin,
  NSSUTF8 *label
)
{
  nss_dbm_token_t *token = (nss_dbm_token_t *)mdToken->etc;
  nss_dbm_instance_t *instance = (nss_dbm_instance_t *)mdInstance->etc;
  CK_RV rv;

  /* Wipe the session object data */
  
  if( (nss_dbm_db_t *)NULL != token->session_db ) {
    nss_dbm_db_close(token->session_db);
  }

  token->session_db = nss_dbm_db_open(token->arena, fwInstance, (char *)NULL, 
                                      O_RDWR|O_CREAT, &rv);
  if( (nss_dbm_db_t *)NULL == token->session_db ) {
    return rv;
  }

  /* Wipe the token object data */

  if( token->slot->flags & O_RDWR ) {
    if( (nss_dbm_db_t *)NULL != token->slot->token_db ) {
      nss_dbm_db_close(token->slot->token_db);
    }

    token->slot->token_db = nss_dbm_db_open(instance->arena, fwInstance, 
                                            token->slot->filename,
                                            token->slot->flags | O_CREAT | O_TRUNC, 
                                            &rv);
    if( (nss_dbm_db_t *)NULL == token->slot->token_db ) {
      return rv;
    }

    /* PIN is irrelevant */

    rv = nss_dbm_db_set_label(token->slot->token_db, label);
    if( CKR_OK != rv ) {
      return rv;
    }
  }

  return CKR_OK;
}

static NSSUTF8 *
nss_dbm_mdToken_GetLabel
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  nss_dbm_token_t *token = (nss_dbm_token_t *)mdToken->etc;

  if( (NSSUTF8 *)NULL == token->label ) {
    token->label = nss_dbm_db_get_label(token->slot->token_db, token->arena, pError);
  }

  /* If no label has been set, return *something* */
  if( (NSSUTF8 *)NULL == token->label ) {
    return token->slot->filename;
  }

  return token->label;
}

static NSSUTF8 *
nss_dbm_mdToken_GetManufacturerID
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return "mozilla.org NSS";
}

static NSSUTF8 *
nss_dbm_mdToken_GetModel
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return "dbm";
}

/* GetSerialNumber is irrelevant */
/* GetHasRNG defaults to CK_FALSE */

static CK_BBOOL
nss_dbm_mdToken_GetIsWriteProtected
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  nss_dbm_token_t *token = (nss_dbm_token_t *)mdToken->etc;

  if( token->slot->flags & O_RDWR ) {
    return CK_FALSE;
  } else {
    return CK_TRUE;
  }
}

/* GetLoginRequired defaults to CK_FALSE */
/* GetUserPinInitialized defaults to CK_FALSE */
/* GetRestoreKeyNotNeeded is irrelevant */
/* GetHasClockOnToken defaults to CK_FALSE */
/* GetHasProtectedAuthenticationPath defaults to CK_FALSE */
/* GetSupportsDualCryptoOperations is irrelevant */

static CK_ULONG
nss_dbm_mdToken_effectively_infinite
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  return CK_EFFECTIVELY_INFINITE;
}

static CK_VERSION
nss_dbm_mdToken_GetHardwareVersion
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  nss_dbm_token_t *token = (nss_dbm_token_t *)mdToken->etc;
  return nss_dbm_db_get_format_version(token->slot->token_db);
}

/* GetFirmwareVersion is irrelevant */
/* GetUTCTime is irrelevant */

static NSSCKMDSession *
nss_dbm_mdToken_OpenSession
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  NSSCKFWSession *fwSession,
  CK_BBOOL rw,
  CK_RV *pError
)
{
  nss_dbm_token_t *token = (nss_dbm_token_t *)mdToken->etc;
  return nss_dbm_mdSession_factory(token, fwSession, fwInstance, rw, pError);
}

/* GetMechanismCount defaults to zero */
/* GetMechanismTypes is irrelevant */
/* GetMechanism is irrelevant */

NSS_IMPLEMENT NSSCKMDToken *
nss_dbm_mdToken_factory
(
  nss_dbm_slot_t *slot,
  CK_RV *pError
)
{
  nss_dbm_token_t *token;
  NSSCKMDToken *rv;

  token = nss_ZNEW(slot->instance->arena, nss_dbm_token_t);
  if( (nss_dbm_token_t *)NULL == token ) {
    *pError = CKR_HOST_MEMORY;
    return (NSSCKMDToken *)NULL;
  }

  rv = nss_ZNEW(slot->instance->arena, NSSCKMDToken);
  if( (NSSCKMDToken *)NULL == rv ) {
    *pError = CKR_HOST_MEMORY;
    return (NSSCKMDToken *)NULL;
  }

  token->slot = slot;

  rv->etc = (void *)token;
  rv->Setup = nss_dbm_mdToken_Setup;
  rv->Invalidate = nss_dbm_mdToken_Invalidate;
  rv->InitToken = nss_dbm_mdToken_InitToken;
  rv->GetLabel = nss_dbm_mdToken_GetLabel;
  rv->GetManufacturerID = nss_dbm_mdToken_GetManufacturerID;
  rv->GetModel = nss_dbm_mdToken_GetModel;
  /*  GetSerialNumber is irrelevant */
  /*  GetHasRNG defaults to CK_FALSE */
  rv->GetIsWriteProtected = nss_dbm_mdToken_GetIsWriteProtected;
  /*  GetLoginRequired defaults to CK_FALSE */
  /*  GetUserPinInitialized defaults to CK_FALSE */
  /*  GetRestoreKeyNotNeeded is irrelevant */
  /*  GetHasClockOnToken defaults to CK_FALSE */
  /*  GetHasProtectedAuthenticationPath defaults to CK_FALSE */
  /*  GetSupportsDualCryptoOperations is irrelevant */
  rv->GetMaxSessionCount = nss_dbm_mdToken_effectively_infinite;
  rv->GetMaxRwSessionCount = nss_dbm_mdToken_effectively_infinite;
  /*  GetMaxPinLen is irrelevant */
  /*  GetMinPinLen is irrelevant */
  /*  GetTotalPublicMemory defaults to CK_UNAVAILABLE_INFORMATION */
  /*  GetFreePublicMemory defaults to CK_UNAVAILABLE_INFORMATION */
  /*  GetTotalPrivateMemory defaults to CK_UNAVAILABLE_INFORMATION */
  /*  GetFreePrivateMemory defaults to CK_UNAVAILABLE_INFORMATION */
  rv->GetHardwareVersion = nss_dbm_mdToken_GetHardwareVersion;
  /*  GetFirmwareVersion is irrelevant */
  /*  GetUTCTime is irrelevant */
  rv->OpenSession = nss_dbm_mdToken_OpenSession;
  rv->null = NULL;

  return rv;
}
