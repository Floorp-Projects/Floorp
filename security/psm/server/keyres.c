/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#include "serv.h"
#include "kgenctxt.h"
#include "pk11func.h"

#define SSMRESOURCE(object) (&(object)->super)

SSMStatus
SSMKeyPair_Create(void *arg, SSMControlConnection * connection, 
                  SSMResource **res)
{
    SSMKeyPair          *keyPair    = NULL;
    SSMKeyPairArg       *keyPairArg = (SSMKeyPairArg*)arg;
    SSMStatus            rv;

    keyPair = SSM_ZNEW(SSMKeyPair);
    if (keyPair == NULL) {
        goto loser;
    }
    rv = SSMKeyPair_Init(keyPair, connection, SSM_RESTYPE_KEY_PAIR,
                         keyPairArg->keyGenContext);
    if (rv != PR_SUCCESS) {
        goto loser;
    }
    *res = SSMRESOURCE(keyPair);
    return PR_SUCCESS;
    
 loser:
    if (keyPair != NULL) {
	SSM_FreeResource (SSMRESOURCE(keyPair));
    }
    return PR_FAILURE;
}

SSMStatus
SSMKeyPair_Init(SSMKeyPair       *inKeyPair, SSMControlConnection *connection,
                SSMResourceType   type, SSMKeyGenContext *keyGenContext)
{
    SSMStatus rv = PR_SUCCESS;

    rv = SSMResource_Init(connection, &inKeyPair->super, type);
    inKeyPair->m_KeyGenContext = keyGenContext;
    inKeyPair->m_KeyGenType    = invalidKeyGen;
    SSM_ClientGetResourceReference(SSMRESOURCE(inKeyPair), NULL);
    return rv;
}


SSMStatus
SSMKeyPair_Destroy(SSMResource *res, PRBool doFree)
{
    SSMKeyPair *keyPair = (SSMKeyPair*)res;

    SSM_LockResource(SSMRESOURCE(keyPair));
    if (keyPair->m_PubKey) {
        SECKEY_DestroyPublicKey(keyPair->m_PubKey);
    }
    if (keyPair->m_PrivKey) {
        SECKEY_DestroyPrivateKey(keyPair->m_PrivKey);
    }

    SSM_UnlockResource(SSMRESOURCE(keyPair));
    SSMResource_Destroy(SSMRESOURCE(keyPair), PR_FALSE); 
    if (doFree) {
        PORT_Free(keyPair);
    }
    return PR_SUCCESS;
}

SSMStatus 
SSMKeyPair_SetKeyGenType(SSMKeyPair    *inKeyPair,
                         SSMKeyGenType  inKeyGenType)
{
  if (inKeyPair->m_KeyGenType != invalidKeyGen) {
    return PR_FAILURE;
  }
  inKeyPair->m_KeyGenType = inKeyGenType;
  return PR_SUCCESS;
}

SSMStatus 
SSMKeyPair_SetAttr(SSMResource *res,
                   SSMAttributeID attrID,
                   SSMAttributeValue *value)
{
    SSMKeyPair *keyPair = (SSMKeyPair*)res;
    SSMStatus    rv;

    PR_ASSERT(SSM_IsAKindOf(res, SSM_RESTYPE_KEY_PAIR));
    switch(attrID) {
    case SSM_FID_KEYPAIR_KEY_GEN_TYPE:
      SSM_DEBUG("Setting the key gen type to %d.\n", value->u.numeric);
      if (value->type != SSM_NUMERIC_ATTRIBUTE) {
        rv = PR_FAILURE;
        break;
      }
      rv = SSMKeyPair_SetKeyGenType(keyPair, (SSMKeyGenType)value->u.numeric);
      break;
    default:
      SSM_DEBUG("Got unknown KeyPair Set Attr request %d.\n", attrID);
      rv = PR_FAILURE;
      break;
    }
    return rv;
}

