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
#include "resource.h"
#include "kgenctxt.h"
#include "nlsutil.h"
#include "base64.h"
#include "pk11func.h"
#include "textgen.h"
#include "minihttp.h"
#include "connect.h"
#include "messages.h"

/* lock to wait for UI */
typedef struct {
  PRMonitor * lock;
  PRBool UIComplete;
} SSMTokenUI_Monitor;

#define SSM_PARENT_CONN(x) &((x)->m_parent->super)

#if 0 /* XXX not used? */
static SSMTokenUI_Monitor  UIlock;
#endif

PK11SlotListElement * PK11_GetNextSafe(PK11SlotList *list, 
                                       PK11SlotListElement *le, 
                                       PRBool restart);
SSMStatus SSMTokenUI_GetNames(SSMResource * res, PRBool start, char ** name);

SSMStatus SSM_SetUserPasswordCommunicator(PK11SlotInfo * slot, 
					  SSMKeyGenContext *ct);

CK_MECHANISM_TYPE
SSMKeyGenContext_GenMechToAlgMech(CK_MECHANISM_TYPE mechanism)
{
    CK_MECHANISM_TYPE searchMech;

    /* We are interested in slots based on the ability to perform
       a given algorithm, not on their ability to generate keys usable
       by that algorithm. Therefore, map keygen-specific mechanism tags
       to tags for the corresponding crypto algorthm. */
    switch(mechanism)
    {
    case CKM_RSA_PKCS_KEY_PAIR_GEN:
        searchMech = CKM_RSA_PKCS;
        break;
    case CKM_DSA_KEY_PAIR_GEN:
        searchMech = CKM_DSA;
        break;
    case CKM_RC4_KEY_GEN:
        searchMech = CKM_RC4;
        break;
    case CKM_DH_PKCS_KEY_PAIR_GEN:
        searchMech = CKM_DH_PKCS_DERIVE; /* ### mwelch  is this right? */
        break;
    case CKM_DES_KEY_GEN:
        /* What do we do about DES keygen? Right now, we're just using
           DES_KEY_GEN to look for tokens, because otherwise we'll have
           to search the token list three times. */
    default:
        searchMech = mechanism;
        break;
    }
    return searchMech;
}

SSMStatus SSMKeyGenContext_GetSlot(SSMKeyGenContext * keyctxt,
                                   CK_MECHANISM_TYPE mechanism)
{
    SSMStatus rv = SSM_SUCCESS;
    PK11SlotList * slotList = NULL;
    CK_MECHANISM_TYPE searchMech;
    PRBool reLock;

    searchMech = SSMKeyGenContext_GenMechToAlgMech(mechanism);

    if (!keyctxt->slot || 
	!PK11_DoesMechanism(keyctxt->slot, searchMech)) {
      slotList = PK11_GetAllTokens(searchMech, PR_TRUE, PR_TRUE, 
				   keyctxt->super.m_connection);
  
      if (!keyctxt) 
        goto loser; 
      if (!slotList || !slotList->head) 
        goto loser;
   
      if (!slotList->head->next) {
        /* only one slot available, just return it */
        keyctxt->slot = slotList->head->slot;
	keyctxt->m_slotName = strdup(PK11_GetSlotName(keyctxt->slot));
      } else { 
        char * mech = PR_smprintf("mech=%d&task=keygen&unused=unused", searchMech);
	if (keyctxt->m_ctxtype == SSM_OLD_STYLE_KEYGEN) {
	  CMTItem msg;
	  GenKeyOldStyleTokenRequest request;
	  char ** tokenNames = NULL;
	  int numtokens = 0;
	  PK11SlotListElement * slotElement;
	  
	  slotElement = PK11_GetFirstSafe(slotList);
	  while(slotElement) {
	    numtokens++;
	    tokenNames = (char **) PR_REALLOC(tokenNames, numtokens*sizeof(*tokenNames));
	    tokenNames[numtokens - 1] = strdup(PK11_GetTokenName(slotElement->slot));
	    
	    slotElement = PK11_GetNextSafe(slotList, slotElement, PR_FALSE);
	  }
	  
	  /* send message to plugin: use native UI for select token dialog */
	  SSM_LockUIEvent(&keyctxt->super); 
	  request.rid = keyctxt->super.m_id;
	  request.numtokens = numtokens;
	  request.tokenNames = tokenNames;
	  
	  msg.type = SSM_REPLY_OK_MESSAGE | SSM_KEYGEN_TAG | SSM_KEYGEN_TOKEN;
	  if (CMT_EncodeMessage(GenKeyOldStyleTokenRequestTemplate, &msg, 
				&request) !=  CMTSuccess) 
	    goto loser;  
	  
	  SSM_SendQMessage(keyctxt->super.m_connection->m_controlOutQ, 
			   SSM_PRIORITY_NORMAL, 
			   msg.type, msg.len, (char *)msg.data, PR_TRUE);
	  
	  SSM_WaitUIEvent(&keyctxt->super, PR_INTERVAL_NO_TIMEOUT);
	  SSM_UnlockUIEvent(&keyctxt->super);
	  keyctxt->slot = (PK11SlotInfo*)keyctxt->super.m_uiData;
	} else /* post UI event */ {
	  /* post a UI event to ask user for the slot */
	  SSM_LockUIEvent(&keyctxt->super);
	  
	  /* Release the lock on the resource temporarily while
	   * the UI happens.
	   */
	  rv = SSM_UnlockResource(&keyctxt->super);
	  reLock = (rv == PR_SUCCESS) ? PR_TRUE : PR_FALSE;
	  rv = SSMControlConnection_SendUIEvent(keyctxt->super.m_connection,
						"get",
						"select_token",
						(SSMResource *)keyctxt,
						mech,
						&((SSMResource*)keyctxt)->m_clientContext,
						PR_TRUE);
	  if (rv != SSM_SUCCESS) {
	    if (keyctxt->super.m_UILock != NULL) {
	      PR_ExitMonitor(keyctxt->super.m_UILock);
	    }
	    goto loser;
	  }
	  /* wait until the UI dialog box is done */
	  SSM_WaitUIEvent(&keyctxt->super, PR_INTERVAL_NO_TIMEOUT);
	  if (reLock) {
	  /* Get the lock back. */
	    SSM_LockResource(&keyctxt->super);
	  }
	  SSM_UnlockUIEvent(&keyctxt->super);
	  PR_Free(mech);
	  /* ok, the return from UI is processed in command handler */
	  /* Make sure password prompt doesn't get sucked away by
	   * the disappearing dialog
	   */
	  PR_Sleep(PR_TicksPerSecond());
	  keyctxt->slot = (PK11SlotInfo*)keyctxt->super.m_uiData;
	  if (keyctxt->slot != NULL) {
	    keyctxt->m_slotName = strdup(PK11_GetSlotName(keyctxt->slot));
	  }
	} /* end of post UI event */	
      } /* end of multiple tokens available */
      /* Get a slot reference so it doesn't disappear */
      if (keyctxt->slot) {
	PK11_ReferenceSlot(keyctxt->slot);
      }
    }
    /* we should have a slot now */
    if (keyctxt->slot == NULL) 
      goto loser;
    
    /* Before we authenticate, make sure the user initialized the DB. */
    if (PK11_NeedUserInit(keyctxt->slot)) 
      rv = SSM_SetUserPasswordCommunicator(keyctxt->slot, keyctxt);

    rv = PK11_Authenticate(keyctxt->slot, PR_FALSE, keyctxt);
    if (rv != SECSuccess) {
        goto loser;
    }
    goto done;
loser:
    
    if (rv == SSM_SUCCESS)
        rv = SSM_FAILURE;
    keyctxt->slot = NULL; 
done: 
    if (slotList) 
        PK11_FreeSlotList(slotList);
   
    return rv;
}

/* send a message to communicator to display UI for setting slot password */
SSMStatus
SSM_SetUserPasswordCommunicator(PK11SlotInfo * slot, 
                                SSMKeyGenContext *ct)
{
  CMTItem msg;
  GenKeyOldStylePasswordRequest passwdmsg;
  SSMStatus rv = SSM_FAILURE;
  PRBool reLock = PR_FALSE;
  
  PR_ASSERT(SSM_IsAKindOf(&ct->super, SSM_RESTYPE_KEYGEN_CONTEXT));


  switch (ct->m_ctxtype) {
  case (SSM_CRMF_KEYGEN):
    rv = SSM_SetUserPassword(slot, &ct->super);   
    break;
  case (SSM_OLD_STYLE_KEYGEN):
    SSM_LockUIEvent(&ct->super);
    passwdmsg.rid = ct->super.m_id;
    passwdmsg.tokenName = PK11_GetTokenName(slot);
    passwdmsg.internal = (CMBool) PK11_IsInternal(slot);
    passwdmsg.minpwdlen = PR_MAX(SSM_MIN_PWD_LEN,PK11_GetMinimumPwdLength(slot));
    passwdmsg.maxpwdlen = slot->maxPassword;
    msg.type = SSM_REPLY_OK_MESSAGE | SSM_KEYGEN_TAG | SSM_KEYGEN_PASSWORD;
    if (CMT_EncodeMessage(GenKeyOldStylePasswordRequestTemplate, &msg, 
			  &passwdmsg) != CMTSuccess)
      goto loser;
    rv = SSM_SendQMessage(ct->super.m_connection->m_controlOutQ,
			  SSM_PRIORITY_NORMAL,
			  msg.type, msg.len, (char *)msg.data, PR_TRUE);
    if (rv != SSM_SUCCESS) 
      SSM_UnlockUIEvent(&ct->super);
    else 
      SSM_WaitUIEvent(&ct->super, PR_INTERVAL_NO_TIMEOUT); 
   break;
  default:
    SSM_DEBUG("Unknown keygen type!\n");
    goto loser;
  }
  
loser:
  if (rv != SSM_SUCCESS)
    SSM_DEBUG("SetUserPasswordComm: can't send password request msg!\n");
  return rv;
}


SSMStatus
ssmpkcs11_convert_slot(SSMTextGenContext *cx, 
                       PRInt32 slotIndex,
                       PK11SlotInfo *slot,
                       char *fmt,
                       PRBool accumulate);

SSMStatus SSMTokenUI_KeywordHandler(SSMTextGenContext * cx)
{
    SSMResource *ctxt = NULL;
    SSMStatus rv;
    char * tempUStr = NULL;
    char * wrapper = NULL;
    CK_MECHANISM_TYPE searchMech;
    char *searchMechStr;
    static PK11SlotList * slotList;
    static PK11SlotListElement * slotElement;
    PRIntn i = 0;
 
    if (!cx || !cx->m_request || !cx->m_params || !cx->m_result)
        goto loser;

    ctxt = SSMTextGen_GetTargetObject(cx);
    
    /* Get the wrapper string from the properties file. */
    wrapper = (char *) SSM_At(cx->m_params, 0);
    rv = SSM_GetAndExpandTextKeyedByString(cx, wrapper, &tempUStr);
    if (rv != PR_SUCCESS)
        goto loser;

    rv = SSM_HTTPParamValue(cx->m_request, "action", &searchMechStr);
    if (rv != SSM_SUCCESS) 
        goto loser;

    searchMech = atoi(searchMechStr);
    slotList = PK11_GetAllTokens(searchMech, PR_TRUE, PR_TRUE, 
				 ctxt->m_connection);
    if (!slotList) 
        goto loser;

    slotElement = PK11_GetFirstSafe(slotList);
    while(slotElement)
    {
        rv = ssmpkcs11_convert_slot(cx, i++, slotElement->slot, tempUStr, 
				    PR_TRUE);
        if (rv != SSM_SUCCESS)
            goto loser;
        slotElement = PK11_GetNextSafe(slotList, slotElement, PR_FALSE); 
    }

    rv = SSM_SUCCESS;
    goto done;
loser: 
    SSM_DEBUG("Errors creating token list!\n");
    SSMTextGen_UTF8StringClear(&cx->m_result);
    if (rv==SSM_SUCCESS) 
        rv = SSM_FAILURE;
done:
    PR_FREEIF(tempUStr);
    return rv;
}
PK11SlotInfo *
SSMPKCS11_FindSlotByID(SECMODModuleID modID,
                       CK_SLOT_ID slotID);

SSMStatus
figure_out_token_selection(char *str, 
                           SECMODModuleID *modID,
                           CK_SLOT_ID *slotID)
{
    char *walk;

    walk = PL_strchr(str, '+');
    if (!walk)
        return SSM_FAILURE;
    *walk++ = '\0';
    *modID = atoi(str);
    *slotID = atoi(walk);
    return SSM_SUCCESS;
}

SSMStatus 
SSMTokenUI_CommandHandler(HTTPRequest * req)
{
    SSMStatus rv;
    char * tokenName = NULL;
    SSMResource *res;
    SECMODModuleID modID;
    CK_SLOT_ID slotID;
    PK11SlotInfo *slot;

    if (!req->target) 
        goto loser;
    res = req->target;

    res->m_uiData = NULL;
    /* check the parameter values */
    if (res->m_buttonType == SSM_BUTTON_CANCEL)
    {
	slot = NULL;
        goto done; /* return the closing javascript */
    }

    /* Determine what token the user selected. */
    rv = SSM_HTTPParamValue(req, "token", &tokenName);
    if (rv != SSM_SUCCESS)
    {
        req->httprv = HTTP_BAD_REQUEST;
        goto loser;
    }

    rv = figure_out_token_selection(tokenName, &modID, &slotID);
    if (rv != SSM_SUCCESS)
        goto loser;
    slot = SSMPKCS11_FindSlotByID(modID, slotID);
    if (!slot)
        goto loser;

done:
    /* Tell the keygen context what token was selected. */
    res->m_uiData = slot;
    SSM_NotifyUIEvent(res);
    /* Done with our part, so pass back whatever the dialog wants in
       order to close the window. */
    return SSM_HTTPCloseAndSleep(req);

loser:
    res->m_uiData = NULL;
    SSM_NotifyUIEvent(res);
    SSM_DEBUG("Can't get input from token_select dialog!\n");
    return SSM_FAILURE;
}
