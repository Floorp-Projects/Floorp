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
#include "kgenctxt.h"
#include "ssmerrs.h"
#include "minihttp.h"
#include "textgen.h"
#include "base64.h"
#include "newproto.h"
#include "messages.h"
#include "advisor.h"
#include "oldfunc.h"
#include "secmod.h"

#define SSMRESOURCE(ct) (&(ct)->super)

#define DEFAULT_KEY_GEN_ALLOC 5

void SSMKeyGenContext_ServiceThread(void *arg);

static PRBool
ssmkeygencontext_can_escrow(SSMKeyGenType keyGenType);

#if 1
/*
 * We're cheating for now so that escrowing keys on smart cards 
 * will work.
 */
SECKEYPrivateKey*
pk11_loadPrivKey(PK11SlotInfo *slot,SECKEYPrivateKey *privKey,
                SECKEYPublicKey *pubKey, PRBool token, PRBool sensitive);
#endif

/* Initialize an SSMKeyGenContext object. */
SSMStatus
SSMKeyGenContext_Init(SSMKeyGenContext *ct, 
                      SSMKeyGenContextCreateArg *arg, 
                      SSMResourceType type)
{
    SSMStatus rv;
    SSMControlConnection * parent = arg->parent;

    rv = SSMResource_Init(parent, SSMRESOURCE(ct), type);
    if (rv != PR_SUCCESS) goto loser;

    ct->m_parent = parent;
    ct->m_ctxtype = arg->type;

    if (arg->param) {
        ct->super.m_clientContext.data = arg->param->data;
        ct->super.m_clientContext.len  = arg->param->len;
    }
    /*
     *  Create full keyGenContext for CRMF request and 
     *  keyGenContext-light for old-style keygen.
     *  Differentiate on arg->type.
     */
    switch (arg->type) { 
      case SSM_CRMF_KEYGEN:
        /* Create message queue and key collection. */
        ct->m_incomingQ = SSM_NewCollection();
        if (!ct->m_incomingQ) goto loser;

        ct->m_serviceThread = SSM_CreateThread(SSMRESOURCE(ct),
                                               SSMKeyGenContext_ServiceThread);
        if (!ct->m_serviceThread)
          goto loser;
        break;
      case SSM_OLD_STYLE_KEYGEN:
        /* don't need to do anything */
        ct->m_serviceThread = NULL;
        SSM_DEBUG("Creating a keygen context for old-style keygen.\n");
        break;
      default:
        goto loser;
        break;
    }
      
    /* this is temporary hack, need to send this event in any case -jane */
    rv = SSMControlConnection_SendUIEvent(ct->m_parent,
                                          "get",
                                          "keygen_window",
                                          SSMRESOURCE(ct),
                                          NULL,
                                          &SSMRESOURCE(ct)->m_clientContext,
                                          PR_FALSE);
    /*
     * UI isn't crucial for the keygen to work, don't bail if sending UI
     * didn't work.
     */

    if (arg->type == SSM_CRMF_KEYGEN) {
      rv = SSM_SendQMessage(ct->m_incomingQ, SSM_PRIORITY_SHUTDOWN,
                          SSM_KEYGEN_CXT_MESSAGE_DATA_PROVIDER_OPEN,
                          0, NULL, PR_FALSE);
      if (rv != PR_SUCCESS)
        goto loser;

      ct->m_keyGens = SSM_ZNEW_ARRAY(SSMKeyGenParams*, DEFAULT_KEY_GEN_ALLOC);
      if (ct->m_keyGens == NULL) {
        goto loser;
      }
    }
    ct->m_numKeyGens = 0;
    ct->m_allocKeyGens = DEFAULT_KEY_GEN_ALLOC;
    ct->m_eaCert = NULL;
    ct->m_userCancel = PR_FALSE;
    goto done;
 loser:
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
    ct->m_serviceThread = PR_GetCurrentThread();
 done:
	/* 
		Prevent the control thread from responding if we successfully
		created this object. This is because we want to make sure that
		the Create Resource response is sent before the UI event due
		to total weirdness in the browser. 
	
		See SSMKeyGenContext_ServiceThread below; at the top of that
		routine, the Create Resource request is sent just before the
		UI event.
	
		"Icepicks in my forehead!" 
                (Only needs that for CRMF context)
	*/
	if (rv == PR_SUCCESS && arg->type == SSM_CRMF_KEYGEN) 
          rv = SSM_ERR_DEFER_RESPONSE;
    return rv;
}

SSMStatus
SSMKeyGenContext_Shutdown(SSMResource *res, SSMStatus status)
{
    SSMKeyGenContext *ct = (SSMKeyGenContext *) res;
    SSMStatus rv = PR_SUCCESS, trv;

    SSMKeyGenContext_Invariant(ct);

    SSM_LockResource(res);
    if (res->m_resourceShutdown) {
        rv = SSM_ERR_ALREADY_SHUT_DOWN;
    } else {

        /* If this is the service thread, deregister it. */
        rv = SSMResource_Shutdown(res, status);
        
        if (rv == PR_SUCCESS)
        {
            /* Post shutdown msgs to all applicable queues. */
            
            if (status != PR_SUCCESS && ct->m_incomingQ)
                trv = SSM_SendQMessage(ct->m_incomingQ,
                                       SSM_PRIORITY_SHUTDOWN,
                                       SSM_DATA_PROVIDER_SHUTDOWN,
                                       0, NULL, PR_TRUE);
            
            /* Wake up the keygen thread */
            if (ct->m_serviceThread)
                PR_Interrupt(ct->m_serviceThread);
        }
    }
    SSM_NotifyResource(res);
    res->m_resourceShutdown = PR_TRUE;
    SSM_UnlockResource(res);
    return rv;
}

SSMStatus
SSMKeyGenContext_Destroy(SSMResource *res, PRBool doFree)
{
    SSMKeyGenContext *ct = (SSMKeyGenContext *) res;
    int i;
    
    PR_ASSERT(SSM_IsAKindOf(res, SSM_RESTYPE_KEYGEN_CONTEXT));
    PR_ASSERT(res->m_threadCount == 0);

    SSM_LockResource(res);
    if (ct->slot) 
        PK11_FreeSlot(ct->slot);
    
    ct->slot = NULL; 

    if (ct->m_incomingQ)
        ssm_DrainAndDestroyQueue(&ct->m_incomingQ);

    if (ct->m_eaCert != NULL) {
        CERT_DestroyCertificate(ct->m_eaCert);
    }
    ct->m_eaCert = NULL;
    if (ct->m_keyGens) {
        for (i=0; i<ct->m_numKeyGens; i++) {
            SSM_FreeResource(SSMRESOURCE(ct->m_keyGens[i]->kp));
            PR_Free(ct->m_keyGens[i]);
            ct->m_keyGens[i] = NULL;
        }
        PR_Free(ct->m_keyGens);
        ct->m_keyGens = NULL;
    }
    

    SSM_UnlockResource(res); /* we're done, stand aside for superclass */

    SSMResource_Destroy(res, PR_FALSE); /* destroy superclass fields */
    if (doFree)
        PR_DELETE(res);
    return PR_SUCCESS;
}

/* 
   SSMKeyGenContext_Create creates an SSMKeyGenContext object. 
   The value argument is ignored.
*/
SSMStatus
SSMKeyGenContext_Create(void *arg, SSMControlConnection * connection, 
                        SSMResource **res)
{
    SSMStatus rv = PR_SUCCESS;
    SSMKeyGenContext *ct;
    SSMKeyGenContextCreateArg *carg = (SSMKeyGenContextCreateArg *) arg;
    *res = NULL; /* in case we fail */

    if (!arg)
    {
        rv = PR_INVALID_ARGUMENT_ERROR;
        goto loser;
    }

    ct = (SSMKeyGenContext *) PR_CALLOC(sizeof(SSMKeyGenContext));
    if (!ct) 
    {
        rv = PR_OUT_OF_MEMORY_ERROR;
        goto loser;
    }
    SSMRESOURCE(ct)->m_connection = connection;
    rv = SSMKeyGenContext_Init(ct, carg, SSM_RESTYPE_KEYGEN_CONTEXT);
    if ((rv != PR_SUCCESS) && (rv != SSM_ERR_DEFER_RESPONSE)) goto loser;

    SSMKeyGenContext_Invariant(ct);
    
    *res = &ct->super;
    return rv;

 loser:
    if (rv == PR_SUCCESS) rv = PR_FAILURE;

    if (ct) 
    {
        SSM_ShutdownResource(SSMRESOURCE(ct), rv); /* force destroy */
        SSM_FreeResource(&ct->super);
    }

    return rv;
}

SSMStatus
SSMKeyGenContext_FormSubmitHandler(SSMResource * res, HTTPRequest * req)
{
   SSMStatus rv = SSM_FAILURE;
   char * tmpStr = NULL;
	
  /* make sure you got the right baseRef */
  rv = SSM_HTTPParamValue(req, "baseRef", &tmpStr);
  if (rv != SSM_SUCCESS ||
      PL_strcmp(tmpStr, "windowclose_doclose_js") != 0) {
    goto loser;
  }

  if (!res->m_formName)
    goto loser;
  if (PL_strcmp(res->m_formName, "set_db_password") == 0)
    return rv = SSM_SetDBPasswordHandler(req);
  else 
    goto loser;

loser:
  return SSM_HTTPDefaultCommandHandler(req);
}

SSMStatus 
SSMKeyGenContext_Print(SSMResource   *res,
                       char *fmt,
                       PRIntn numParams,
                       char **value,
                       char **resultStr)
{
    SSMKeyGenContext  *ct              = (SSMKeyGenContext*)res;
    char              *escrowCAName    = NULL;
    SSMTextGenContext *textGenCxt      = NULL;
    SSMStatus          rv = PR_FAILURE;
    char              mechStr[48];

    PR_ASSERT(resultStr != NULL);
    if (resultStr == NULL) {
        return PR_FAILURE;
    }
    if (!SSM_IsAKindOf(res, SSM_RESTYPE_KEYGEN_CONTEXT)) {
        goto loser;
    }

    /* 
     * arghh, this function thinks it's always called for key escrow dialog.
     * not so, it is also called for set password dialog, and maybe called for
     * other things too. 
     * Assume, if no param values are supplied through the arguments, it is
     * indeed key escrow case, otherwise use arguments.
     */
    if (!numParams) {
        if (ct->m_eaCert != NULL) {
            escrowCAName = CERT_GetCommonName(&ct->m_eaCert->subject);
        }
        if(escrowCAName == NULL) {
            if (SSMTextGen_NewTopLevelContext(NULL, &textGenCxt) != 
                SSM_SUCCESS) 
                goto loser;
            
            rv = SSM_FindUTF8StringInBundles(textGenCxt, 
                                             "certificate_authority_text",
                                             &escrowCAName); 
            if (rv != PR_SUCCESS)
                goto loser;
        }
        /* The escrow name should be in a format we can print already. */
        /* We supply one parameter to NLS_MessageFormat, the resource ID. */
        PR_snprintf(mechStr, 48, "%d", 
                    SSMKeyGenContext_GenMechToAlgMech(ct->mech));
        *resultStr = PR_smprintf(fmt, res->m_id, mechStr, escrowCAName);
    } else 
        SSMResource_Print(res, fmt, numParams, value, resultStr);
    
    rv = (*resultStr == NULL) ? PR_FAILURE : PR_SUCCESS;
 loser:
    PR_FREEIF(escrowCAName);
    return rv;
}

SSMStatus SSMKeyGenContext_SetEscrowAuthority(SSMKeyGenContext *ct,
                                             char             *base64Cert)
{
    SECItem   derCert = { siBuffer, NULL, 0 };
    SECStatus rv;

    if (base64Cert    == NULL ||
        ct->m_eaCert != NULL) {
        return PR_FAILURE;
    }
    rv = ATOB_ConvertAsciiToItem(&derCert, base64Cert);
    if (rv != SECSuccess) {
        goto loser;
    }
    ct->m_eaCert = CERT_NewTempCertificate(ct->super.m_connection->m_certdb,
                                           &derCert, NULL, PR_FALSE, PR_TRUE);
    if (ct->m_eaCert == NULL) {
        goto loser;
    }
    return PR_SUCCESS;
 loser:
    if (ct->m_eaCert != NULL) {
        CERT_DestroyCertificate(ct->m_eaCert);
    }
    ct->m_eaCert = NULL;
    return PR_FAILURE;
}

/*
 * This function is very similar to SECMOD_FindSlotByName.
 * The main difference is that we want to look for the slot
 * name and then see if a token exists.  The NSS version looks
 * at the token name if the token is present when doing this.
 */
PK11SlotInfo*
SSM_FindSlotByNameFromModule(SECMODModule *module, char *slotName)
{
    int i;
    PK11SlotInfo *currSlot = NULL;
    char *currSlotName = NULL;
    
    for (i=0; i<module->slotCount; i++) {
        currSlot = module->slots[i];
        if (currSlot == NULL) {
            continue;
        }
        currSlotName = PK11_GetSlotName(currSlot);
        if (currSlotName == NULL) {
            continue;
        }
        if (PL_strcmp(currSlotName, slotName) == 0 &&
            PK11_IsPresent(currSlot)) {
            return currSlot;
        }
    }
    return NULL;
}

PK11SlotInfo*
SSM_FindSlotByName(SSMControlConnection *conn, char *slotName)
{
    SECMODModuleList *modList, *currMod;
    PK11SlotInfo *slot = NULL;

    modList = SECMOD_GetDefaultModuleList();
    currMod = modList;
    /*
     * Iterate through the modules looking for the correct slot.
     */
    while (currMod != NULL && currMod->module != NULL) {
        slot = SSM_FindSlotByNameFromModule(currMod->module, slotName);
        if (slot != NULL){
            return PK11_ReferenceSlot(slot);
        }
        currMod = currMod->next;
    }
    return NULL;
}

SSMStatus SSMKeyGenContext_SetDefaultToken(SSMKeyGenContext *ct, 
                                           CMTItem          *string,
                                           PRBool            bySlotName)
{
    PK11SlotInfo *oldSlot;

    oldSlot = ct->slot;
    ct->slot = NULL;
    ct->m_slotName = SSM_NEW_ARRAY(char, (string->len+1));
    if (ct->m_slotName == NULL) {
        goto loser;
    }
    memcpy(ct->m_slotName, string->data, string->len);
    ct->m_slotName[string->len] = '\0';
    if (bySlotName) {
        ct->slot = SSM_FindSlotByName(ct->super.m_connection, ct->m_slotName);
    } else {
        ct->slot = PK11_FindSlotByName(ct->m_slotName);
    }
    if (ct->slot == NULL) {
        goto loser;
    }
    if (oldSlot != NULL) {
        PK11_FreeSlot(oldSlot);
    }
    return SSM_SUCCESS;
 loser:
    if (ct->slot) {
        PK11_FreeSlot(ct->slot);
    }
    ct->slot = oldSlot;
    if (ct->m_slotName) {
        PR_Free(ct->m_slotName);
        ct->m_slotName = NULL;
    }
    return SSM_FAILURE;
}

SSMStatus SSMKeyGenContext_SetAttr(SSMResource *res,
                                  SSMAttributeID attrID,
                                  SSMAttributeValue *value)
{
    SSMKeyGenContext *ct = (SSMKeyGenContext*)res;
    SSMStatus rv = PR_FAILURE;

    PR_ASSERT(SSM_IsAKindOf(res, SSM_RESTYPE_KEYGEN_CONTEXT));
    switch(attrID) {
    case SSM_FID_KEYGEN_ESCROW_AUTHORITY:
      SSM_DEBUG("Setting the Escrow Authority to \n%s\n", value->u.string.data);
      if (value->type != SSM_STRING_ATTRIBUTE) {
          goto loser;
      }
      rv = SSMKeyGenContext_SetEscrowAuthority(ct, (char *) value->u.string.data);
      break;
    case SSM_FID_CLIENT_CONTEXT:
      SSM_DEBUG("Setting the Key Gen UI context\n");
      if (value->type != SSM_STRING_ATTRIBUTE) {
          goto loser;
      }
      if (!(res->m_clientContext.data = (unsigned char *) PR_Malloc(value->u.string.len))) {
          goto loser;
      }
      memcpy(res->m_clientContext.data, value->u.string.data, value->u.string.len);
      res->m_clientContext.len = value->u.string.len;
      rv = SSM_SUCCESS;
      break;
    case SSM_FID_KEYGEN_SLOT_NAME:
        rv = SSMKeyGenContext_SetDefaultToken(ct, &value->u.string, PR_TRUE);
        break;
    case SSM_FID_KEYGEN_TOKEN_NAME:
        rv = SSMKeyGenContext_SetDefaultToken(ct, &value->u.string, PR_FALSE);
        break;
    case SSM_FID_DISABLE_ESCROW_WARN:
      ct->m_disableEscrowWarning = PR_TRUE;
      rv = SSM_SUCCESS;
      break;
    default:
      rv = SSMResource_SetAttr(res, attrID, value);
      break;
    }
    return rv;
loser:
    return PR_FAILURE;
}

SSMStatus SSMKeyGenContext_GetAttr(SSMResource *res,
                                   SSMAttributeID attrID,
                                   SSMResourceAttrType attrType,
                                   SSMAttributeValue *value)
{
    SSMKeyGenContext *cxt;
    SSMStatus rv;

    PR_ASSERT(SSM_IsAKindOf(res, SSM_RESTYPE_KEYGEN_CONTEXT));
    cxt = (SSMKeyGenContext*)res;
    switch(attrID) {
    case SSM_FID_CLIENT_CONTEXT:
      SSM_DEBUG("Getting the Key Gen UI context");
      value->type = SSM_STRING_ATTRIBUTE;
      if (!(value->u.string.data = (unsigned char *) PR_Malloc(res->m_clientContext.len))) {
          goto loser;
      }
      memcpy(value->u.string.data, res->m_clientContext.data, res->m_clientContext.len);
      value->u.string.len = res->m_clientContext.len;
      break;
    case SSM_FID_CHOOSE_TOKEN_URL:
      {
        PRUint32 width, height;
        char * mech, *url;

        mech = PR_smprintf("mech=%d&task=keygen&unused=unused",
                           SSMKeyGenContext_GenMechToAlgMech(cxt->mech));
        rv = SSM_GenerateURL(res->m_connection,"get", "select_token", res, 
                             mech, &width, &height, &url);
        PR_FREEIF(mech);
        if (rv != SSM_SUCCESS) {
            goto loser;
        }
        value->u.string.data = (unsigned char*)url;
        value->u.string.len  = PL_strlen(url);
        value->type = SSM_STRING_ATTRIBUTE;
      }
      break;
    case SSM_FID_INIT_DB_URL:
      {
          char *url;
          
          url = SSM_GenerateChangePasswordURL(cxt->slot, res);
          if (url == NULL){
              goto loser;
          }
          value->u.string.data = (unsigned char*)url;
          value->u.string.len  = PL_strlen(url);
          value->type = SSM_STRING_ATTRIBUTE;
      }
      break;
    default:
      SSM_DEBUG("Got unkown KeyGenContext Get Attribute Request %d\n", attrID);
      goto loser;
      break;
    }
    return PR_SUCCESS;
loser:
    value->type = SSM_NO_ATTRIBUTE;
    return PR_FAILURE;
}

/* As a sanity check, make sure we have data structures consistent
   with our type. */
void SSMKeyGenContext_Invariant(SSMKeyGenContext *ct)
{
#ifdef DEBUG
    if (ct)
    {
        SSMResource_Invariant(&(ct->super));
        SSM_LockResource(SSMRESOURCE(ct));
        PR_ASSERT(SSM_IsAKindOf(SSMRESOURCE(ct), SSM_RESTYPE_KEYGEN_CONTEXT));
        PR_ASSERT(ct->m_ctxtype == SSM_CRMF_KEYGEN || 
                  ct->m_ctxtype == SSM_OLD_STYLE_KEYGEN);
        if (ct->m_ctxtype == SSM_CRMF_KEYGEN) {
          PR_ASSERT(ct->m_incomingQ != NULL);
#if 0
          PR_ASSERT(ct->m_serviceThread != NULL); /* context == service thread */
#endif /* If the user canceled, then this thread will be NULL. */
        }
        SSM_UnlockResource(SSMRESOURCE(ct));
    }
#endif
}

static SSMStatus
ssm_process_next_pqg_param(SECItem *dest, unsigned char *curParam)
{
    PRUint32 tmpLong;

    tmpLong = PR_ntohl(*(PRUint32*)curParam);
    dest->len = tmpLong;
    curParam += sizeof (PRUint32);
    dest->data = PORT_ZNewArray(unsigned char, tmpLong);
    PORT_Memcpy(dest->data, curParam, tmpLong);
    return PR_SUCCESS;
}

void*
ssm_ConvertToActualKeyGenParams(PRUint32 keyGenMech, unsigned char *params,
				PRUint32 paramLen, PRUint32 keySize)
{
    void          *returnParams = NULL;
    unsigned char *curPtr;
    PRUint32       tmpLong;

    if (params != NULL && paramLen > 0) {
        curPtr = params;
        switch (keyGenMech) {
	case CKM_RSA_PKCS_KEY_PAIR_GEN:
	  {
	      PK11RSAGenParams *rsaParams;
	      
	      rsaParams = PORT_New(PK11RSAGenParams);
	      if (rsaParams == NULL) {
		  return NULL;
	      }
	      rsaParams->keySizeInBits = keySize;
	      tmpLong = PR_ntohl(*(PRUint32*)curPtr);
	      rsaParams->pe = (unsigned long) tmpLong;
	      returnParams = rsaParams;
	      break;
	  }
	case CKM_DSA_KEY_PAIR_GEN:
	  {
	      PQGParams *pqgParams;

	      pqgParams = PORT_ZNew(PQGParams);
	      if (pqgParams == NULL) {
		  return NULL;
	      }
	      ssm_process_next_pqg_param(&pqgParams->prime, curPtr);
	      curPtr += sizeof(PRUint32) + pqgParams->prime.len;
	      ssm_process_next_pqg_param(&pqgParams->subPrime, curPtr);
	      curPtr += sizeof(PRUint32) + pqgParams->subPrime.len;
	      ssm_process_next_pqg_param(&pqgParams->base, curPtr);
	      returnParams = pqgParams;
	      break;
	  }
	default:
	    returnParams = NULL;
	}
    } else {
        /* In this case we provide the parameters ourselves. */
        switch (keyGenMech) {
	case CKM_RSA_PKCS_KEY_PAIR_GEN:
	  {
	      PK11RSAGenParams *rsaParams;

	      rsaParams = PORT_New(PK11RSAGenParams);
	      if (rsaParams == NULL) {
		  return NULL;
	      }
	      /* I'm just taking the same parameters used in 
	       * certdlgs.c:GenKey
	       */
	      if (keySize > 0) {
		  rsaParams->keySizeInBits = keySize;
	      } else {
		  rsaParams->keySizeInBits = 1024;
	      }
	      rsaParams->pe = 65537L;
	      returnParams = rsaParams;
	      break;
	  }
	case CKM_DSA_KEY_PAIR_GEN:
	  {
	      PQGParams *pqgParams = NULL;
              PQGVerify *vfy = NULL;
	      SECStatus  rv;
	      int        index;
	      
	      index = PQG_PBITS_TO_INDEX(keySize);
	      if (index == -1) {
		returnParams = NULL;
		break;
	      }
	      rv = PQG_ParamGen(0, &pqgParams, &vfy);
              if (vfy) {
                  PQG_DestroyVerify(vfy);
              }
	      if (rv != SECSuccess) {
		  if (pqgParams) {
		      PQG_DestroyParams(pqgParams);
		  }
		  return NULL;
	      }
	      returnParams = pqgParams;
	      break;
	  }
	default:
	  returnParams = NULL;
	}
    }
    return returnParams;
}

static void
ssm_FreeKeyGenParams(CK_MECHANISM_TYPE keyGenMechanism, void *params)
{
    switch (keyGenMechanism) {
    case CKM_RSA_PKCS_KEY_PAIR_GEN:
        PORT_Free(params);
	break;
    case CKM_DSA_KEY_PAIR_GEN:
	PQG_DestroyParams((PQGParams*) params);
	break;
    }
}

SSMStatus 
SSMKeyGenContext_BeginGeneratingKeyPair(SSMControlConnection * ctrl,
                                        SECItem *msg, SSMResourceID *destID)
{
    SSMKeyGenContext    *ct=NULL;
    SSMKeyGenParams        *kg=NULL;
    SSMKeyPair          *kp=NULL; 
    void                *actualParams=NULL;
    SSMStatus             rv = PR_SUCCESS;
    SSMKeyPairArg        keyPairArg;
    KeyPairGenRequest request;

    if (msg == NULL || msg->data == NULL || destID == NULL) 
        return PR_INVALID_ARGUMENT_ERROR;

    if (CMT_DecodeMessage(KeyPairGenRequestTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }

    /* Find the requested key gen context. */
    rv = SSMControlConnection_GetResource(ctrl, request.keyGenCtxtID,
                                          (SSMResource **) &ct);
    if (rv != PR_SUCCESS) 
		goto loser;
    if ((!ct) || 
        (!SSM_IsAKindOf(SSMRESOURCE(ct), SSM_RESTYPE_KEYGEN_CONTEXT)))
    {
        rv = PR_INVALID_ARGUMENT_ERROR;
        goto loser;
    }

    if (ct->m_userCancel) {
        rv = (SSMStatus)SSM_ERR_USER_CANCEL;
        goto loser;
    }

    if (!SSM_KeyGenAllowedForSize(request.keySize)) {
        goto loser;
    }
    /* Convert to actual key generation params. */
    actualParams = ssm_ConvertToActualKeyGenParams(request.genMechanism, 
                                                   request.params.data, request.params.len,
                                                   request.keySize);
    if (actualParams == NULL)  {
		goto loser;
	}
    /* Create a key pair resource so that we can return its ID. */
    keyPairArg.keyGenContext = ct;
    if ((rv = SSMKeyPair_Create(&keyPairArg, SSMRESOURCE(ct)->m_connection, 
                                (SSMResource **) &kp)) != PR_SUCCESS)
        goto loser;

    /* Create a parameter lump with which we'll generate the key
       later. */
    if (!(kg = (SSMKeyGenParams *) PR_CALLOC(sizeof(SSMKeyGenParams)))) {
		goto loser;
	}
    kg->keyGenMechanism = request.genMechanism;
    kg->kp = kp;
    kg->actualParams = actualParams;

    SSM_LockResource(SSMRESOURCE(ct));
    if (ct->m_numKeyGens == ct->m_allocKeyGens) {
        int newSize = ct->m_allocKeyGens * 2;
        SSMKeyGenParams **tmp = (SSMKeyGenParams **) 
            PR_Realloc(ct->m_keyGens,
                       sizeof(SSMKeyGenParams*)*newSize);
        if (tmp == NULL) {
            rv = PR_FAILURE;
            SSM_UnlockResource(SSMRESOURCE(ct));
            goto loser;
        }
        ct->m_keyGens = tmp;
        ct->m_allocKeyGens = newSize;
    }
    ct->m_keyGens[ct->m_numKeyGens] = kg;
    ct->m_numKeyGens++;
    SSM_UnlockResource(SSMRESOURCE(ct));
    SSM_FreeResource(&kp->super);
    *destID = kp->super.m_id;
    goto done;
 loser:
    if (rv == PR_SUCCESS) rv = PR_FAILURE;

    PR_FREEIF(kg);

    /*
     * Something went wrong, so we should get rid of the key gen context
     * as well as locally allocated data.
     */
    SSM_ShutdownResource(SSMRESOURCE(ct), PR_FAILURE);
 done:
    SSM_FreeResource(&ct->super);
    return rv;
}
#define SSM_PARENT_CONN(x) &((x)->m_parent->super)

SSMStatus
SSMKeyGenContext_FinishGeneratingKeyPair(SSMKeyGenContext *ct,
                                         SSMKeyGenParams *kg)
{
    SSMKeyPair          *kp = NULL;
    void                *actualParams = NULL;
    PK11SlotInfo        *slot = NULL, *intSlot = NULL, *origSlot = NULL;
    SSMStatus             rv = PR_SUCCESS;
    PRUint32             keyGenMechanism;
    SECKEYPublicKey      *pubKey = NULL;
    SECKEYPrivateKey     *privKey = NULL;
    PRBool                isPerm;

    SSM_DEBUG("Inside FinishGeneratingKeyPair.\n");
    PR_ASSERT((kg != NULL) && (ct != NULL));

    /* If the context is already exiting due to an error,
       abort the key generation. */
    rv = SSMRESOURCE(ct)->m_status;
    if (rv != PR_SUCCESS) goto loser;

    /* Restore parameters from the message. */
    keyGenMechanism = kg->keyGenMechanism;
    kp = kg->kp;
    actualParams = kg->actualParams;

    /* Finish key generation. */
    if (ct->mech != kg->keyGenMechanism) {
        goto loser;
    }
   
    slot = ct->slot;
    if (!slot) 
      goto loser;
    if (ssmkeygencontext_can_escrow(kp->m_KeyGenType) &&
        ct->m_eaCert != NULL && !PK11_IsInternal(slot)) {
        intSlot = PK11_GetInternalSlot();
        origSlot = slot;
        slot = intSlot;
        isPerm = PR_FALSE;
    } else {
        isPerm = PR_TRUE;
    }

    SSM_DEBUG("Creating key pair.\n");
    privKey = PK11_GenerateKeyPair(slot, keyGenMechanism, 
                                   actualParams,
                                   &pubKey, isPerm,
                                   isPerm, NULL);
    if ((!privKey) || (!pubKey)) 
    {
        rv = PR_FAILURE;
        goto loser;
    }
    /*
     * If we generated the key pair on the internal slot because the
     * keys were going to be escrowed, move the keys over right now.
     */
    if (intSlot != NULL) {
        SECKEYPrivateKey *newPrivKey;

        newPrivKey = pk11_loadPrivKey(origSlot, privKey, pubKey, 
                                      PR_TRUE, PR_TRUE);
        if (newPrivKey == NULL) {
            rv = PR_FAILURE;
            goto loser;
        }
        /*
         * The key is stored away on the final slot now, and the
         * the copy we keep around is on the internal slot so we
         * can escrow it if we have to.
         */
        SECKEY_DestroyPrivateKey(newPrivKey);
        PK11_FreeSlot(intSlot);
        intSlot = NULL;
    }
    /* Put the newly generated keys into the key pair object. */
    kp->m_PubKey = pubKey;
    kp->m_PrivKey = privKey;
    /* Set the wincx of the private key so that crypto operations 
     * that use the signing key as the source for the wincx pass
     * a non NULL value to the authentication functions.
     */
    kp->m_PrivKey->wincx = kp->super.m_connection;

    goto done;

 loser:
    if (rv == PR_SUCCESS) rv = PR_FAILURE;

    /* Destroy keys if we've generated them. */
    if (pubKey)
        SECKEY_DestroyPublicKey(pubKey);
    if (privKey)
        SECKEY_DestroyPrivateKey(privKey);

 done:
    /* Free the PKCS11 key gen params. */
    if (actualParams != NULL)
        ssm_FreeKeyGenParams(keyGenMechanism, actualParams);

    SSM_DEBUG("FinishGeneratingKeyPair returned rv = %d.\n", rv);
    return rv;
}

void
SSMKeyGenContext_SendCompleteEvent(SSMKeyGenContext *ct)
{
    SSMStatus rv = PR_SUCCESS;
    PR_ASSERT(ct->m_parent);
    if (ct->m_parent)
    {
        SECItem msg;
        TaskCompletedEvent event;

        /* Assemble the event. */
        SSM_DEBUG("Task completed event: id %ld, count %ld, status %d\n",
                  SSMRESOURCE(ct)->m_id, ct->m_count, 
                  SSMRESOURCE(ct)->m_status);
        event.resourceID = SSMRESOURCE(ct)->m_id;
        event.numTasks = ct->m_count;
        event.result = SSMRESOURCE(ct)->m_status;

        if (CMT_EncodeMessage(TaskCompletedEventTemplate, (CMTItem*)&msg, (CMTItem*)&event) != CMTSuccess) {
            return;
        }
        if (msg.data)
        {
            /* Send the event to the control queue. */
            rv = SSM_SendQMessage(ct->m_parent->m_controlOutQ,
                                  SSM_PRIORITY_NORMAL,
                                  SSM_EVENT_MESSAGE | SSM_TASK_COMPLETED_EVENT,
                                  (int) msg.len, (char *) msg.data, PR_FALSE);
            SSM_DEBUG("Sent message, rv = %d.\n", rv);
        }
    }
}

SSMStatus
SSMKeyGenContext_SendEscrowWarning(SSMKeyGenContext   *ct,
                                   SSMEscrowWarnParam *escrowParam)
{
    SSMStatus rv;

    rv = SSMControlConnection_SendUIEvent(SSMRESOURCE(ct)->m_connection,
                                          "get", "escrow_warning",
                                          SSMRESOURCE(ct), NULL, 
                                          &SSMRESOURCE(ct)->m_clientContext,
                                          PR_FALSE);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    PR_ASSERT(SSMRESOURCE(ct)->m_buttonType == SSM_BUTTON_NONE);
    while (SSMRESOURCE(ct)->m_buttonType == SSM_BUTTON_NONE) {
      SSM_WaitForOKCancelEvent(SSMRESOURCE(ct), PR_INTERVAL_NO_TIMEOUT);
    }
    /* ### sjlee: this unfortunate hack is due to the fact that bad things
     *            happen if we do not finish closing the window before
     *            another window pops up: this will be supplanted by a
     *            more complete fix later
     */
    PR_Sleep(PR_TicksPerSecond()*2);
    return (SSMRESOURCE(ct)->m_buttonType == SSM_BUTTON_OK) ? PR_SUCCESS :
                                                              PR_FAILURE;
 loser:
    return PR_FAILURE;
}

static PRBool
ssmkeygencontext_can_escrow(SSMKeyGenType keyGenType)
{
    /* For now, we only escrow rsa-encryption keys. */
    return (PRBool)(keyGenType == rsaEnc);
}

static SSMStatus
ssmkg_DoKeyEscrowWarning(SSMKeyGenContext *ct)
{
  int i;
  SSMStatus rv = PR_SUCCESS;

  if (ct->m_eaCert == NULL || ct->m_disableEscrowWarning) {
    /* No Escrow Authority Certificate, return Success so that
     * key generation continues.
     */
    return PR_SUCCESS;
  }
  for (i=0; i < ct->m_numKeyGens; i++) {
      if (ssmkeygencontext_can_escrow(ct->m_keyGens[i]->kp->m_KeyGenType)) {
          rv = SSMKeyGenContext_SendEscrowWarning(ct, NULL);
          break;
      }
  }
  return rv;
}

SSMStatus
SSMKeyGenContext_GenerateAllKeys(SSMKeyGenContext *ct)
{
    SSMStatus rv = PR_FAILURE;
    int i;


    /* Make sure the UI is up before we start. */
    PR_Sleep(PR_TicksPerSecond()*2);    
    /* First figure out if we are going to try and escrow,
     * pop up UI if we are
     */
    rv = ssmkg_DoKeyEscrowWarning(ct);
    if (rv != PR_SUCCESS) {
        /* Tell the service thread to shut itself down. */
        SSM_SendQMessage(ct->m_incomingQ,
                         SSM_PRIORITY_SHUTDOWN,
                         SSM_DATA_PROVIDER_SHUTDOWN,
                         0, NULL, PR_TRUE);
        goto loser;
    }
    /*
     * We'll force all key gens to use the same mechanism so that when
     * we create dual keys, we won't prompt the user multiple times for
     * a slot to create the keys on.
     */
    PR_ASSERT(ct->m_numKeyGens > 0);
    ct->mech = ct->m_keyGens[0]->keyGenMechanism;
    rv = SSMKeyGenContext_GetSlot(ct, ct->mech); 
    if (rv != SSM_SUCCESS) 
        goto loser;
    for (i=0; i<ct->m_numKeyGens; i++) {
        /* Lock so that we can finish a keygen without the state
           changing underneath us. */
        SSM_DEBUG("Waiting for lock in order to start new keygen.\n", rv);
        SSM_LockResource(SSMRESOURCE(ct));
        /* Do key generation. */
        rv = SSMKeyGenContext_FinishGeneratingKeyPair(ct, ct->m_keyGens[i]);
        if (ct->super.m_status == PR_SUCCESS) {
            ct->super.m_status = rv;
        }
        /* No matter what, update the completed count. */
        ct->m_count++;
        
        /* Having finished the request, send back a Context Completed
           event describing our results. */
        SSM_DEBUG("Sending Task Completed event to client.\n", rv);
        SSMKeyGenContext_SendCompleteEvent(ct);
        /* Unlock the object so that the canceller can work. */
        SSM_UnlockResource(SSMRESOURCE(ct));
        if (rv != PR_SUCCESS) {
            
            break;
        }
    }
 loser:
    if (ct != NULL) {
        if (SSMRESOURCE(ct)->m_status != PR_SUCCESS) {
            rv = SSMRESOURCE(ct)->m_status;
        }
    }
    return rv;
}

void
SSMKeyGenContext_ServiceThread(void *arg)
{
    SSMKeyGenContext *ct = (SSMKeyGenContext *) arg;
    SSMResource      *res = SSMRESOURCE(ct);
    PRInt32 type, len;
    char *url = NULL;
    char *data = NULL;
    char *tmp = NULL;
    SSMStatus rv = PR_SUCCESS;
    PRBool processingKeys = PR_FALSE;
    SECItem msg;
    CreateResourceReply reply;

    PR_ASSERT(ct);
    PR_ASSERT(SSM_IsAKindOf(SSMRESOURCE(ct), SSM_RESTYPE_KEYGEN_CONTEXT));
    SSM_RegisterNewThread("keygen", (SSMResource*)arg);
	SSM_DEBUG("Sending Create Resource response to client.\n");
    reply.result = PR_SUCCESS;
    reply.resID = SSMRESOURCE(ct)->m_id;
    if (CMT_EncodeMessage(CreateResourceReplyTemplate, (CMTItem*)&msg, &reply) != CMTSuccess) {
        goto loser;
    }
    if (!msg.data || !msg.len) 
    {
        rv = PR_OUT_OF_MEMORY_ERROR;
        goto loser;
    }

    /* Post the message on the outgoing control channel. */
	PR_ASSERT(SSM_IsAKindOf(&(ct->m_parent->super.super), SSM_RESTYPE_CONTROL_CONNECTION));
    rv = SSM_SendQMessage(ct->m_parent->m_controlOutQ, SSM_PRIORITY_NORMAL, 
                          SSM_REPLY_OK_MESSAGE 
						  | SSM_RESOURCE_ACTION
						  | SSM_CREATE_RESOURCE,
                          (int) msg.len, (char *) msg.data, PR_FALSE);
    if (rv != PR_SUCCESS) goto loser;

    /* Start serving key gen requests. */
    while(1)
    {
        /* Get the next request from the queue.
           If we've been cancelled or if there's an error,
           drain what we have but don't wait around. */
        SSM_DEBUG("Reading message from queue.\n");
#if 1
        if (processingKeys)
        {
#endif
            SSM_DEBUG("Looking for keygen requests (and anything else).\n");
            rv = SSM_RecvQMessage(ct->m_incomingQ, SSM_PRIORITY_ANY,
                                  &type, &len, &data, 
                                  (SSMRESOURCE(ct)->m_status == PR_SUCCESS));
#if 1
        }
        else
	    {
            SSM_DEBUG("Looking only for open/shutdown messages.\n");
            rv = SSM_RecvQMessage(ct->m_incomingQ, SSM_PRIORITY_SHUTDOWN,
                                  &type, &len, &data,
                                  PR_TRUE);
        }
#endif
        SSM_DEBUG("rv = %d reading from queue.\n", rv);
        if (rv != SSM_SUCCESS) goto loser;
        
        /* If it's a keygen request, then attend to it. */
        switch (type) {
        case SSM_KEYGEN_CXT_MESSAGE_DATA_PROVIDER_OPEN:
            processingKeys = PR_TRUE;
            break;
        case SSM_KEYGEN_CXT_MESSAGE_DATA_PROVIDER_SHUTDOWN:
            goto loser; /*destroy object */
        case SSM_KEYGEN_CXT_MESSAGE_GEN_KEY:
            if (!processingKeys)
            {
                SSM_DEBUG("**** Dropping keygen request on the floor ****\n");
                break;
            }
            rv = SSMKeyGenContext_GenerateAllKeys(ct);
            /*
             * We're done generating keys, so let's bail out of this thread.
             */
            goto loser;
        default:
            SSM_DEBUG("Don't understand this request.\n");
            /* ### mwelch Don't know what to do with this request */
            rv = (SSMStatus) SSM_ERR_BAD_REQUEST;
            break;
        }
    }    


 loser:
    SSM_LockResource(res);
    /* De-register ourselves from the resource thread count. */
    if (PR_GetCurrentThread() == ct->m_serviceThread)
    {
        ct->m_serviceThread = NULL;
    }
    res->m_threadCount--;
    SSM_UnlockResource(res);
    PR_FREEIF(tmp);
    PR_FREEIF(url);
	SSM_DEBUG("Shutting down keygen context.\n");
    SSM_ShutdownResource(SSMRESOURCE(ct), rv);
    SSM_FreeResource(SSMRESOURCE(ct));
    SSM_DEBUG("Exiting, status %d.\n", rv);
}

void
SSMKeyGenContext_CancelKeyGen(SSMKeyGenContext *ct)
{
    /* set the status so that subsequent keygen attempts will drop out */
    SSM_ShutdownResource(SSMRESOURCE(ct), SSM_ERR_USER_CANCEL);
    ct->m_userCancel = PR_TRUE;
    SSMRESOURCE(ct)->m_status = SSM_ERR_USER_CANCEL;
}

SSMStatus
SSMKeyGenContext_FinishGeneratingAllKeyPairs(SSMControlConnection *ctrl,
                                             SECItem              *msg)
{
    SSMStatus rv;
    SSMKeyGenContext *ct;
    SingleNumMessage request;

    SSM_DEBUG("SSMKeyGenContext_FinishGeneratingAllKeyPairs...\n");
    if (CMT_DecodeMessage(SingleNumMessageTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }

    rv = SSMControlConnection_GetResource(ctrl, request.value, (SSMResource **) &ct);
    if (rv != PR_SUCCESS) {
        goto loser;
    }
    if (!ct ||
        !SSM_IsAKindOf(SSMRESOURCE(ct), SSM_RESTYPE_KEYGEN_CONTEXT)) {
        rv = PR_INVALID_ARGUMENT_ERROR;
        goto loser;
    }
    if (ct->m_userCancel) {
        rv = (SSMStatus)SSM_ERR_USER_CANCEL;
        goto loser;
    }

    rv = SSM_SendQMessage(ct->m_incomingQ, SSM_PRIORITY_NORMAL, 
                          SSM_KEYGEN_CXT_MESSAGE_GEN_KEY,
                          0, NULL, PR_FALSE);
 loser:
    if (ct != NULL) {
        SSM_FreeResource(&ct->super);
    }
    return rv;
}
