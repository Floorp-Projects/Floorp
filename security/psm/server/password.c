/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "connect.h"
#include "ssmerrs.h"
#include "ctrlconn.h"
#include "prinrval.h"
#include "crmf.h"
#include "newproto.h"
#include "messages.h"
#include "minihttp.h"
#include "textgen.h"
#include "sechash.h"
#include "pk11func.h"

extern SSMHashTable * tokenList;
extern PRMonitor * tokenLock;

#define SSMRESOURCE(conn) (&(conn)->super)

/* Make these into functions? */
#if 0
#define SSM_PARENT_CONN(x)  ((((SSMConnection *)(x))->m_parent) != NULL)? \
(((SSMControlConnection *)(((SSMConnection *)(x))->m_parent))):\
((SSMControlConnection *)x)
#define SSM_PWD_TABLE(x) (SSM_PARENT_CONN(x))->m_passwdTable
     
#define SSM_OUT_QUEUE(x) (SSM_PARENT_CONN(x))->m_controlOutQ
#endif

#if 1     
SSMControlConnection * SSM_PARENT_CONN (SSMConnection * x) 
{ 
  return (((((SSMConnection *)(x))->m_parent) != NULL)?
          (((SSMControlConnection *)(((SSMConnection *)(x))->m_parent))):
          ((SSMControlConnection *)x));
}

SSMHashTable * SSM_PWD_TABLE(SSMConnection * x) 
{
  return (SSM_PARENT_CONN(x)->m_passwdTable);
}

SSMCollection * SSM_OUT_QUEUE(SSMConnection * x)
{
  return (SSM_PARENT_CONN(x)->m_controlOutQ);
}
#endif

PRInt32 SSM_GetTokenKey(PK11SlotInfo * slot) 
{
  return ((PK11_GetSlotID(slot)<<16) | PK11_GetModuleID(slot));
}

char * SSM_GetPasswdCallback(PK11SlotInfo *slot,  PRBool retry,  void *arg)
{
  return SSM_GetAuthentication(slot, retry, PR_FALSE, (SSMResource*)arg);
}

/* Do a couple of things in this functions: 
 * 1) Get a password from user, and authenticate to token.
 * 2) Save this password encrypted in the global Cartman tokenList 
 *    for future reference. 
 *    Need to save password in case other users will need to authenticate
 *    to the same tokens, so we make sure they're using the correct 
 *    passwords.
 * 3) Save this password encrypted in the control connection table in 
 *    case we will need to use it again. 
 */
char * SSM_GetAuthentication(PK11SlotInfo * slot, PRBool retry, PRBool init, 
                             SSMResource * res)
{
  PRInt32 tokenKey;
  SSMStatus rv = PR_SUCCESS;
  char * passwd = NULL, * tmp = NULL; 
  PRBool first  = PR_FALSE;
  SSM_TokenInfo * info = NULL, * infoLocal = NULL;
  SSMConnection *conn = &(res->m_connection->super);
  
  tokenKey = SSM_GetTokenKey(slot);

  /* register as interested in a password */
 SSM_PARENT_CONN(conn)->m_waiting++;
  
  /* Get passwd table lock. */
  SSM_LockPasswdTable(conn);
  /* Look for entry for moduleID/slotID. */
  rv = SSM_HashFind(SSM_PWD_TABLE(conn), tokenKey, (void **)&passwd);
  SSM_UnlockPasswdTable(conn);
  if (rv != PR_SUCCESS) {
    first = PR_TRUE;
    /* no entry found, we are the first to authenticate to this slot */
    SSM_DEBUG("%ld: creating passwd table entry for %s \n", 
              conn, PK11_GetSlotName(slot));
    SSM_LockPasswdTable(conn);
    rv = SSM_HashInsert(SSM_PWD_TABLE(conn),tokenKey,(void *)SSM_NO_PASSWORD);
    SSM_UnlockPasswdTable(conn);
    if (rv != PR_SUCCESS) {
      SSM_DEBUG("%ld: could not create entry in password table\n", conn);
      goto loser;
    }
    rv = SSM_AskUserPassword(res, slot, retry, init);
    if (rv != PR_SUCCESS) {
      SSM_DEBUG("%ld: error sending password request event\n", conn);
      goto loser;
    }
  } /* end of the we-are-first-to-request-this-passwd-clause */
  
  /* If no password found, wait for it */
  if (!passwd || passwd == (char *)SSM_NO_PASSWORD) {
    rv = SSMControlConnection_WaitPassword(conn, tokenKey, &passwd);
    if (rv != PR_SUCCESS) 
      goto loser;
  }

  if (((int) passwd) == SSM_CANCEL_PASSWORD) {
    /* no password was provided or user hit "Cancel" */
    SSM_LockPasswdTable(conn);
    rv = SSM_HashRemove(SSM_PWD_TABLE(conn), tokenKey, (void **)&passwd);  
    SSM_UnlockPasswdTable(conn);
    if (rv != SSM_SUCCESS) 
      SSM_DEBUG("SSM_GetAuthentication: user hit Cancel, can't remove password from connection table\n");
    passwd = NULL;
    goto done;
  }
  
  if (first) {
    /* We were the first to request the password, 
     * so we need to enter it in to the token list.
     */
    /* encrypt the password */
    if (SSM_EncryptPasswd(slot, passwd, &info) != SSM_SUCCESS) {
      SSM_DEBUG("%ld: could not encrypt password\n.", conn);
      goto loser;
    }
    
    /* Place encrypted passwd in Cartman-wide tokenList */
    PR_EnterMonitor(tokenLock);
    /* Remove from tokenList if already on the list - must be stale */
    rv = SSM_HashRemove(tokenList, tokenKey, (void **)&tmp);
    if (rv == SSM_SUCCESS && tmp && tmp != (char *)SSM_NO_PASSWORD 
        && tmp != (char *)SSM_CANCEL_PASSWORD) { /* free stale data */
      PR_Free(tmp);
      tmp = NULL;
    }
    rv = SSM_HashInsert(tokenList, tokenKey, info);
    PR_ExitMonitor(tokenLock);
    if (rv != PR_SUCCESS) {
      SSM_DEBUG("%ld: can't create encr passwd entry\n", conn, tokenKey);
      goto loser;
    }
    
    /* Store encrypted password in control connection table */ 
    infoLocal = (SSM_TokenInfo *) PORT_ZAlloc(sizeof(SSM_TokenInfo));
    if (!infoLocal) 
      goto loser;
    infoLocal->slot = info->slot;
    infoLocal->tokenID = info->tokenID;
    infoLocal->encryptedLen = info->encryptedLen;
    infoLocal->symKey = info->symKey;
    infoLocal->encrypted = (char *) PORT_ZAlloc(info->encryptedLen);
    if (!infoLocal->encrypted)
      goto loser;
    memcpy(infoLocal->encrypted, info->encrypted, info->encryptedLen);
    PR_EnterMonitor(SSM_PARENT_CONN(conn)->m_encrPasswdLock);
    rv = SSM_HashRemove(SSM_PARENT_CONN(conn)->m_encrPasswdTable, tokenKey, 
                   (void **)&tmp);
    if (rv == SSM_SUCCESS && tmp && tmp != (char *)SSM_NO_PASSWORD && 
        tmp != (char *)SSM_CANCEL_PASSWORD )  {/* free stale data */
      PR_Free(tmp); 
      tmp = NULL;
    }
    rv = SSM_HashInsert(SSM_PARENT_CONN(conn)->m_encrPasswdTable, tokenKey,
                        infoLocal);
    PR_ExitMonitor(SSM_PARENT_CONN(conn)->m_encrPasswdLock);
    if (rv != PR_SUCCESS) {
      SSM_DEBUG("%ld: cannot insert token %d entry in encrPasswdTable\n",
                conn, tokenKey);
      goto loser;
    }
    
    SSM_DEBUG("%ld: wait untill others are done with passwd, remove it\n",
              conn);
    /* while ((SSM_PARENT_CONN(conn))->m_waiting > 1)
     *  PR_Sleep(SSM_PASSWORD_WAIT_TIME);
     */

    SSM_LockPasswdTable(conn);
    rv = SSM_HashRemove(SSM_PWD_TABLE(conn), tokenKey, (void **)&passwd);
    if (rv != PR_SUCCESS) { 
      SSM_DEBUG("%ld: could not remove passwd.\n", conn);
      goto loser;
    }
      SSM_UnlockPasswdTable(conn);
    } /* end of if-first clause */
    
    goto done;
    
 loser:
    /* cleanup */
    PR_FREEIF(passwd);
    passwd = NULL;
    if (info) {
      PR_FREEIF(info->encrypted);
      PR_Free(info);
    }
    if (infoLocal) {
      PR_FREEIF(infoLocal->encrypted);
      PR_Free(infoLocal);
    }
 done:
    /* We are done receiving passwd */
    (SSM_PARENT_CONN(conn))->m_waiting--;
    return passwd ? strdup (passwd) : NULL;
}
/* 
 * We get this callback if the slot is already logged-in. 
 * Need to check client-supplied password against the password stored in 
 *      tokenList.
 */
PRBool SSM_VerifyPasswdCallback(PK11SlotInfo * slot, void * arg)
{
  char * passwd = NULL;
  PRInt32 tokenKey;
  SSM_TokenInfo * info = NULL;
  SSM_TokenInfo * tokenInfo = NULL;
  SSMStatus rv;
  PRBool result = PR_FALSE;
  SSMResource * res = (SSMResource*)arg;
  SSMConnection * conn = &(res->m_connection->super);
  PRInt32 doTry = 0;
  void * tmp = NULL;
  
  if (!slot || !arg) 
    goto loser;
  
  tokenKey = PK11_GetSlotID(slot) ^ PK11_GetModuleID(slot);
  info = (SSM_TokenInfo *) PORT_ZAlloc(sizeof(SSM_TokenInfo));
  if (!info) {
    SSM_DEBUG("Could not allocate memory in VerifyPasswdCallback.\n");
    goto loser;
  }
  
  /* Get the password for this token. 
   * We might have to find it in the local encrypted password table or 
   * to request it from the client. 
   */
  rv = SSM_HashFind(SSM_PARENT_CONN(conn)->m_encrPasswdTable, tokenKey, 
                    (void **)&info);
  if (rv != PR_SUCCESS || info == (void *)SSM_NO_PASSWORD) { 
  askpassword:
    /* ask user for a password and store it in local passwd table */
    rv = SSM_AskUserPassword(res, slot, doTry?PR_TRUE:PR_FALSE, PR_FALSE);
    if (rv != PR_SUCCESS)
      goto loser;
    doTry++;
    rv = SSMControlConnection_WaitPassword(conn, tokenKey, &passwd);
    if (rv != PR_SUCCESS || !passwd)
      goto loser;
    rv = SSM_EncryptPasswd(slot, passwd, &info);
    if (rv != SSM_SUCCESS) 
      goto loser;
  } /* end of no password, ask user */
  
  /* Now get the stored password for this token */
  if (!tokenInfo) {
    PR_EnterMonitor(tokenLock);
    rv = SSM_HashFind(tokenList, tokenKey, (void **)&tokenInfo);
    PR_ExitMonitor(tokenLock);
    if (rv != PR_SUCCESS || !tokenInfo) {
      SSM_DEBUG("Can't find token info in VerifyPasswd.\n");
      goto loser;
    }
  }
  /* Check password against tokenList entry   */
  if (memcmp(tokenInfo->encrypted, info->encrypted, tokenInfo->encryptedLen)
      != 0 || tokenInfo->encryptedLen != info->encryptedLen) {
    /* Failed compare, bad password */
    /* first clean up */
    if (info) PR_Free(info);
    info = NULL;
    /* If not retry, ask client for password. */
    if (doTry < 2 ) {
      /* ask user again */
      PR_Free(passwd);
      passwd = NULL;
      goto askpassword; 
    }
    else 
      goto loser;
  } /* end of password not verified */
  
  SSM_DEBUG("Password verified OK.\n");

  /* store password for local use */
  PR_EnterMonitor(SSM_PARENT_CONN(conn)->m_encrPasswdLock);
  SSM_HashRemove(SSM_PARENT_CONN(conn)->m_encrPasswdTable, tokenKey, 
                 (void **)&tmp);
  if (tmp && tmp != (char *)SSM_NO_PASSWORD )  {/* free stale data */
    PR_Free(tmp); 
    tmp = NULL;
  }
  rv = SSM_HashInsert(SSM_PARENT_CONN(conn)->m_encrPasswdTable, tokenKey,
                      info);
  PR_ExitMonitor(SSM_PARENT_CONN(conn)->m_encrPasswdLock);
  if (rv != PR_SUCCESS) {
    SSM_DEBUG("%ld: cannot insert token %d entry in encrPasswdTable\n",
              conn, tokenKey);
    goto loser;
  }
  
  result = PR_TRUE;
  goto done;
 loser:
  SSM_DEBUG("Password not verified. \n");
  /* log out this slot?? */
  if (info) {
    if (info->encrypted)
      PR_Free(info->encrypted);
    PR_Free(info);
  }
  result = PR_FALSE;
 done:
  if (passwd)
    PR_Free(passwd);
  return result;
}

/* Encrypt password for storage */
#define SSM_PAD_BLOCK_SIZE(x, y) ((((x) + ((y)-1))/(y))*(y)) 

SSMStatus SSM_EncryptPasswd(PK11SlotInfo * slot, char * passwd, 
                            SSM_TokenInfo ** tokenInfo)
{
  int resultLen;
  char *hashResult = NULL;
  SSMStatus rv = SSM_SUCCESS;
  SECStatus srv;
  SSM_TokenInfo * info;

  /* Hash the password. */
  resultLen = HASH_ResultLen(HASH_AlgSHA1);
  hashResult = (char *) PORT_ZAlloc(resultLen); /* because the original PORT_ZAlloc'd */
  if (!hashResult)
    goto loser;
  
  srv = HASH_HashBuf(HASH_AlgSHA1, (unsigned char *) hashResult, (unsigned char *) passwd, strlen(passwd));
  if (srv != SECSuccess)
    goto loser;
  
  /* fill in the tokenInfo structure */
  info = (SSM_TokenInfo *) PORT_ZAlloc(sizeof(SSM_TokenInfo));
  if (!info) {
    SSM_DEBUG("EncryptPwd: could not allocate memory to token list entry.\n");
    PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    goto loser;
  }
  info->encrypted = hashResult;
  info->encryptedLen = resultLen;
  info->slot = slot;
  info->tokenID = SSM_GetTokenKey(slot);
  *tokenInfo = info;
  goto done;
 loser:
  if (rv == SSM_SUCCESS) rv = SSM_FAILURE;
  PR_FREEIF(hashResult);
 done:
  return rv;
}

SSMStatus SSM_NotEncryptPasswd(PK11SlotInfo * slot, char * passwd, 
                               SSM_TokenInfo * info)
{
  CK_MECHANISM_TYPE mechanism;
  /* CK_SESSION_HANDLE session = CK_INVALID_SESSION; */
  PRInt32 keyLength, blockSize, outlen;
  PRUint32 encryptedLength;
  PK11SymKey * symKey;
  SECStatus rv;
  char * encrypted = NULL;
  PK11Context * context=NULL;
  SECItem *params; 
  
  
  if (!slot || !passwd || !info) {
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    goto loser;
  }
  mechanism = CRMF_GetBestWrapPadMechanism(slot);
  keyLength = PK11_GetBestKeyLength(slot, mechanism);
  /* session = PK11_GetRWSession(slot);
     if (session == CK_INVALID_SESSION)
     goto loser;*/
  blockSize = PK11_GetBlockSize(mechanism, NULL);
  
  /* 
   * A password is encrypted when we first authenticate to token. 
   * In this case, generate a symmetric Key on the slot. 
   * If the key is already present, it means that the password for this 
   *       slot has already been encrypted and stored, need to encrypt 
   *       new password with the same key to compare against the stored
   *       password. 
   */ 
  /* If no symKey found, generate one */
  if (!info->symKey) {
    symKey = PK11_KeyGen(slot, mechanism, NULL, keyLength, NULL);
    if (!symKey) { 
      SSM_DEBUG("Failed to generate symKey to encrypt passwd.\n");
      goto loser;
    }
  } else 
    symKey = info->symKey;
  
  encryptedLength = SSM_PAD_BLOCK_SIZE(strlen(passwd)+1, blockSize);
  encrypted = (char *) PORT_ZAlloc(encryptedLength);
  if (!encrypted) {
    SSM_DEBUG("Could not allocate space for encrypted password. \n");
    goto loser;
  }
  params = CRMF_GetIVFromMechanism(mechanism);
  context=PK11_CreateContextBySymKey(mechanism, CKA_ENCRYPT, 
                                     symKey, params);
  if (params != NULL) {
    SECITEM_FreeItem(params, PR_TRUE);
  } 
  if (!context) {
    SSM_DEBUG("Can't create context to encrypt password: %d.\n", 
              PR_GetError());
    goto loser;
  }
  rv = PK11_CipherOp(context, (unsigned char *) encrypted, &outlen, 
                     (int) encryptedLength, 
                     (unsigned char *) passwd, strlen(passwd));
  if (rv != PR_SUCCESS) {
    SSM_DEBUG("Error encrypting password: %d\n", PR_GetError());
    goto loser;
  }
  rv = PK11_DigestFinal(context, (unsigned char *) &encrypted[outlen], 
                        (unsigned int *) &outlen, (unsigned int) blockSize);
  if (rv != PR_SUCCESS) {
    SSM_DEBUG("Error encrypting password: %d\n", PR_GetError());
    goto loser;
  }
  PK11_DestroyContext(context, PR_TRUE);
  /*if (session != CK_INVALID_SESSION)
    PK11_RestoreROSession(slot, session);*/
  
  /* fill in the tokenInfo structure */
  info->encrypted = encrypted;
  info->encryptedLen = encryptedLength;
  info->slot = slot;
  return SSM_SUCCESS;
 loser:
  SSM_DEBUG("Failed to encrypt password.\n");
  if (context != NULL)
    PK11_DestroyContext(context, PR_TRUE);
  /*if (session != CK_INVALID_SESSION)
    PK11_RestoreROSession(slot, session);*/
  if (encrypted && *encrypted) 
    PR_Free(encrypted);
  return SSM_FAILURE;
}

/* Needs to be fixed using NLS lib and proper string storage. -jane */
char * SSM_GetPrompt(PK11SlotInfo *slot, PRBool retry, PRBool init)
{
  char * prompt = NULL, * tmp = NULL, * key;
  SSMTextGenContext * cx;
  SSMStatus rv;

  PR_ASSERT(init != PR_TRUE);
 
  rv = SSMTextGen_NewTopLevelContext(NULL, &cx);
  if (rv != SSM_SUCCESS || !cx) 
    goto loser;
  if (retry) 
    key = "retry_token_password";
  else 
    key = "ask_token_password";
  
  rv = SSM_GetAndExpandTextKeyedByString(cx, key, &tmp);
  if (rv != SSM_SUCCESS || !tmp)
    goto loser;
  prompt = PR_smprintf(tmp, PK11_GetTokenName(slot));
  
 loser:
  PR_FREEIF(tmp);
  return prompt;
}


/* Send a password request for the client */
SSMStatus SSM_AskUserPassword(SSMResource * res, 
                             PK11SlotInfo * slot, PRInt32 retry, PRBool init)
{
  SECItem message;
  char * prompt = NULL;
  PRInt32 tokenKey = SSM_GetTokenKey(slot);
  SSMStatus rv = PR_FAILURE;
  SSMConnection *conn = (SSMConnection *)res->m_connection;
  PasswordRequest request;
  
  prompt = SSM_GetPrompt(slot, retry, init);
  retry++;
  if (!prompt) {
    SSM_DEBUG("%ld: error getting prompt for password request.\n", conn);
    goto loser;
  }
  request.tokenKey = tokenKey;
  request.prompt = prompt;
  request.clientContext = res->m_clientContext;
  if (CMT_EncodeMessage(PasswordRequestTemplate, (CMTItem*)&message, &request) != CMTSuccess) {
      goto loser;
  }
  if (message.len == 0 || !message.data) {
    SSM_DEBUG("%ld: could not create password request message.\n", conn);
    goto loser;
  }
  message.type = (SECItemType) (SSM_EVENT_MESSAGE | SSM_AUTH_EVENT);
  rv = SSM_SendQMessage(SSM_OUT_QUEUE(conn), SSM_PRIORITY_UI, message.type, 
                        message.len, (char *)message.data, PR_TRUE);
  if (rv != PR_SUCCESS) { 
    SSM_DEBUG("%ld: Can't enqueue password request. \n", conn);
    goto loser;
  }
 loser:
  if (prompt)
    PR_Free(prompt);
  if (message.data)
    PR_Free(message.data);
  return rv;
}

SSMStatus SSMControlConnection_WaitPassword(SSMConnection * conn, 
                                           PRInt32 key, char ** str)
{
  char * passwd;
  PRIntervalTime before;
  SSMStatus rv = PR_FAILURE;
  
  *str = NULL;
  /* Wait no longer than our time-out period. */
  before = PR_IntervalNow();
  SSM_LockPasswdTable(conn);    
 wait:
  SSM_DEBUG("%ld : waiting on password table for the password\n", conn); 
  SSM_WaitPasswdTable(conn);
  /* Returned from wait.
   * Look for password.
   */
  rv = SSM_HashFind(SSM_PWD_TABLE(conn), key, (void **)&passwd);
  if (rv!=PR_SUCCESS || !passwd || passwd ==(char *)SSM_NO_PASSWORD) {
    /* password not found, check for timeout */
    if (PR_IntervalNow() - before > SSM_PASSWORD_WAIT_TIME) {
      SSM_DEBUG("%ld:Timed out waiting for password.Bailing out.\n", 
                conn);
      SSM_UnlockPasswdTable(conn);
      return PR_FAILURE;
    } 
    else 
      goto wait; /* continue waiting */
  } /* end of no password found */
  SSM_UnlockPasswdTable(conn);
  *str = passwd;
  return rv;
}

extern PK11SlotListElement * 
PK11_GetNextSafe(PK11SlotList * list, PK11SlotListElement * element,PRBool start);
                                       
PK11SlotListElement *
ssm_GetSlotWithPwd(PK11SlotList * slotlist, PK11SlotListElement * current,
                        PRBool start)
{
  PK11SlotListElement * next = NULL;
  PR_ASSERT(slotlist);
  if (!current || start)
    next = PK11_GetFirstSafe(slotlist);
  else
    next = PK11_GetNextSafe(slotlist, current, PR_FALSE);

  while (next                          && 
         PK11_NeedUserInit(next->slot) && 
         !PK11_NeedLogin(next->slot)     )
    next = PK11_GetNextSafe(slotlist, next, PR_FALSE);

  return next;
}

PRIntn
ssm_NumSlotsWithPassword(PK11SlotList * slotList)
{
  PRIntn numslots = 0;
  PK11SlotListElement * element = PK11_GetFirstSafe(slotList);
  while (element) {
    if (PK11_NeedLogin(element->slot) || !PK11_NeedUserInit(element->slot))
      numslots++;
    element = PK11_GetNextSafe(slotList, element,PR_FALSE);
  }
  return numslots;
}

char*
SSM_GetSlotNameForPasswordChange(HTTPRequest * req)
{
    PK11SlotList *slotList = NULL;
    PK11SlotInfo *slot=NULL;
    PK11SlotListElement *listElem = NULL;
    SSMResource *target;
    char *slotName=NULL;
    SSMStatus rv;
    
    slotName = NULL;
    target = REQ_TARGET(req);
    slotList = PK11_GetAllTokens(CKM_INVALID_MECHANISM, PR_TRUE, 
                                 PR_TRUE, target);
    if (!slotList || !slotList->head)
      goto loser;
    if (ssm_NumSlotsWithPassword(slotList)>1) {
      char * mech = PR_smprintf("mech=%d&unused1=unused1&unused2=unused2",
                                CKM_INVALID_MECHANISM);
      SSM_LockUIEvent(target);
      rv = SSMControlConnection_SendUIEvent(req->ctrlconn,
                                            "get", "select_token",
                                            target,mech,
                                            &target->m_clientContext, 
                                              PR_TRUE);
      SSM_WaitUIEvent(target, PR_INTERVAL_NO_TIMEOUT);
      slot = (PK11SlotInfo *) target->m_uiData;
      if (!slot) 
        goto loser;
    } else {
      listElem = ssm_GetSlotWithPwd(slotList, NULL, PR_TRUE);
      slot = listElem->slot;
    }

    if (!slot) {
      goto loser;
    }
    slotName = PK11_GetTokenName(slot);
    PK11_FreeSlot(slot);
    PK11_FreeSlotList(slotList);
    return PL_strdup(slotName);
 loser:
    if (slot)
      PK11_FreeSlot(slot);
    if (slotList)
      PK11_FreeSlotList(slotList);
    return NULL;
}

SSMStatus SSM_ReSetPasswordKeywordHandler(SSMTextGenContext * cx)
{  
  char * slotname = NULL;
  PK11SlotInfo * slot; 
  char * tmp = NULL;
  SSMStatus rv;
  SSMResource * target = cx->m_request->target;
  PK11SlotList * slotList = NULL;
  PK11SlotListElement * el = NULL;

  PR_ASSERT(cx != NULL);
  PR_ASSERT(cx->m_request != NULL);
  PR_ASSERT(&cx->m_result != NULL);

  rv = SSM_HTTPParamValue(cx->m_request, "action", &slotname);
  
  if (!slotname || strcmp(slotname, "")== 0) 
    slot = PK11_GetInternalKeySlot();
  else if (strcmp(slotname, "all") == 0) {
      char *userSlotName = NULL;

      /* ask user */
      userSlotName = SSM_GetSlotNameForPasswordChange(cx->m_request);
      if (!userSlotName) 
        goto cancel;
      slot = PK11_FindSlotByName(userSlotName);
      PR_Free(userSlotName);
  } 
  else 
    slot = PK11_FindSlotByName(slotname);
  if (!slot) {
    SSM_DEBUG("ReSetPasswordKeywordHandler: bad slotname %s\n", slotname);
    goto loser;
  }
  
  slotname = PK11_GetTokenName(slot);
  if (PK11_NeedPWInitForSlot(slot))
    rv = SSM_GetAndExpandTextKeyedByString(cx, "set_new_password", &tmp);
  else 
    rv = SSM_GetAndExpandTextKeyedByString(cx, "reset_password", &tmp);
  if (rv != SSM_SUCCESS) 
    goto loser;
  PR_FREEIF(cx->m_result);
  cx->m_result = PR_smprintf(tmp, slotname);
  return rv;
  
 loser:
  if (cx->m_result) 
    PR_Free(cx->m_result);
  cx->m_result = NULL;
  return PR_FAILURE;
 cancel:
  SSM_HTTPCloseWindow(cx->m_request);
  goto loser;
}

PRBool 
ssm_VerifyPwdLength(char * password)
{
  if (!password)
    return (!SSM_MIN_PWD_LEN);

  if (strlen(password) < SSM_MIN_PWD_LEN)
    return PR_FALSE;
  if (strlen(password) > SSM_MAX_PWD_LEN)
    return PR_FALSE;
  return PR_TRUE;
}

SSMStatus SSM_PasswordPrefKeywordHandler(SSMTextGenContext * cx)
{
  char * fmt = NULL, * checked = NULL;
  char * markchecked[] = { "", "", ""};
  SSMStatus rv;
  PRIntn askpw, timeout;

  PR_ASSERT(cx != NULL);
  PR_ASSERT(cx->m_request != NULL);
  PR_ASSERT(cx->m_result != NULL);
  
  /* need to get the table and fill it with current preferences */
  rv = SSM_GetAndExpandTextKeyedByString(cx, "password_lifetime", &fmt);
  if (rv != SSM_SUCCESS || !fmt) 
    goto done;
  rv = SSM_GetAndExpandTextKeyedByString(cx, "text_checked", &checked);
  if (rv != SSM_SUCCESS || !checked) 
    goto done;
  rv = PREF_GetIntPref(cx->m_request->ctrlconn->m_prefs, 
                       "security.ask_for_password", &askpw);
  if (rv != SSM_SUCCESS)
    goto done;
  rv = PREF_GetIntPref(cx->m_request->ctrlconn->m_prefs, 
                       "security.password_lifetime", &timeout);
  if (rv != SSM_SUCCESS)
    goto done;

  markchecked[askpw] = checked;
  PR_FREEIF(cx->m_result);
  cx->m_result = PR_smprintf(fmt, markchecked[0], markchecked[1], 
                             markchecked[2], timeout);
  
done: 
  return rv;
}

SSMStatus SSM_SetDBPasswordHandler(HTTPRequest * req)
{
  SSMStatus rv = SSM_FAILURE;
  char * oldpassword, * newpassword, *repeatpassword, * action;
  PK11SlotInfo * slot;
  char * responseKey = NULL;
  char * result = NULL;
  char * slotname = NULL, * askpwdoption, * pwdlifetime;
  PRIntn askpw, timeout;

  rv = SSM_HTTPParamValue(req, "baseRef", &action);
  if (rv != SSM_SUCCESS || strcmp(action, "windowclose_doclose_js")!= 0)
    SSM_DEBUG("SetDBPasswordHandler: bad action %s\n", action);
  
  rv = SSM_HTTPParamValue(req, "slot", &slotname);
  if (rv != SSM_SUCCESS || !slotname || 
      !(slot = PK11_FindSlotByName(slotname)))
    goto loser;

  /* process password preferences */
  rv = SSM_HTTPParamValue(req, "passwordlife", &askpwdoption);
  if (rv != SSM_SUCCESS || !askpwdoption) 
    goto loser;
  rv = SSM_HTTPParamValue(req, "passwordwillexpire", &pwdlifetime);
  if (rv != SSM_SUCCESS || !pwdlifetime) 
    goto loser;
  if (strcmp(askpwdoption, "firsttime") == 0)
    askpw = 0;
  else if (strcmp(askpwdoption, "everytime") == 0)
    askpw = 1;
  else if (strcmp(askpwdoption, "expiretime")==0) {
    askpw = 2;
  }
  else {
    SSM_DEBUG("SetDBPasswordHandler: bad password lifetime parameter %s\n", 
              askpwdoption);
    goto loser;
  }
  timeout = atoi(pwdlifetime);
  if (askpw == 2 && !timeout) 
    goto loser;

  PK11_SetSlotPWValues(slot, askpw, timeout);
  rv = SSMControlConnection_SaveIntPref(req->ctrlconn, 
                                        "security.ask_for_password", askpw);
  if (rv != PR_SUCCESS)
    goto loser;
  rv = SSMControlConnection_SaveIntPref(req->ctrlconn,
                                        "security.password_lifetime", timeout);
  if (rv != SSM_SUCCESS)
    goto loser;

  rv = SSM_HTTPParamValue(req, "newpassword", &newpassword);
  if (rv != SSM_SUCCESS) 
    goto loser;
  rv = SSM_HTTPParamValue(req, "repeatpassword", &repeatpassword);
  if (rv != SSM_SUCCESS) 
    goto loser;

  if (!PK11_NeedPWInitForSlot(slot)) {
      /* oldpassword doesn't make sense for password initialization dialog */
      rv = SSM_HTTPParamValue(req, "oldpassword", &oldpassword);
      if (rv != SSM_SUCCESS) {
          goto loser;
      }

      /* we do this check to find the case where the user changed only password
       * settings, not the password itself
       */
      if ((oldpassword[0] == '\0') && (newpassword[0] == '\0') &&
          (repeatpassword[0] == '\0')) {
          rv = SSM_HTTPDefaultCommandHandler(req);
          goto done;
      }
  }

  if (!ssm_VerifyPwdLength(newpassword))
    goto loser;

  if (strcmp(newpassword, repeatpassword) != 0)
    goto loser;

  if (!PK11_NeedPWInitForSlot(slot)) { /* there is some password on the DB */
    if (!oldpassword) 
      goto loser;
    if (PK11_CheckUserPassword(slot, oldpassword) != 
        SECSuccess)
      goto loser;
    if (PK11_ChangePW(slot, oldpassword, newpassword) != 
      SECSuccess)
      goto loser;
  }
  else 
    {
      if (PK11_NeedUserInit(slot)) {
        if (PK11_InitPin(slot, NULL, newpassword) != SECSuccess)
          goto loser;
        }
      else {
        if (PK11_ChangePW(slot, NULL, newpassword) != SECSuccess) 
          goto loser;
      }
    }

  result = PR_smprintf("result=password_success");
 loser:
  if (!result)
    result = PR_smprintf("result=password_failure");
  
  rv = SSM_HTTPCloseAndSleep(req);
  if (rv != SSM_SUCCESS) 
    SSM_DEBUG("SetDBPasswordHandler: failure in DefaultCommandHandler\n");

  /* post status if password dialog was invoked from the SecurityAdvisor */
  if (SSM_IsA(req->target, SSM_RESTYPE_SECADVISOR_CONTEXT))
    SSMControlConnection_SendUIEvent(req->ctrlconn, "get", 
                                     "show_followup", NULL, 
                                     result, 
                                     &((SSMResource *)req->ctrlconn)->m_clientContext,
                                     PR_TRUE);
  
  PR_FREEIF(responseKey);
done:
  if (req->target && req->target->m_UILock)
    SSM_NotifyUIEvent(req->target);
  return rv;
}

SSMStatus SSM_ShowFollowupKeywordHandler(SSMTextGenContext * cx)
{
  char * resultvalue;
  SSMStatus rv;

  PR_ASSERT(cx != NULL);
  PR_ASSERT(cx->m_request != NULL);
  PR_ASSERT(cx->m_result != NULL);
  
  rv = SSM_HTTPParamValue(cx->m_request, "result", &resultvalue);
  if (rv != SSM_SUCCESS || !resultvalue)
    goto loser;
  if (!strcmp(resultvalue, "password_success"))
    rv = SSM_GetAndExpandTextKeyedByString(cx, "set_password_success",
                                           &cx->m_result);
  else if (!strcmp(resultvalue,"password_failure"))
    rv = SSM_GetAndExpandTextKeyedByString(cx, "set_password_failure",
                                           &cx->m_result);
  else if (!strcmp(resultvalue, "no_ldap_setup"))
    rv = SSM_GetAndExpandTextKeyedByString(cx, "no_ldap_server_set",
                                           &cx->m_result);
 loser:
  return rv;
}

char *
SSM_SetPasswordHTMLParamsFromTokenName(char *tokenName)
{
    char *url = NULL;
    tokenName = SSM_ConvertStringToHTMLString(tokenName);
    url = PR_smprintf("slot=%s&mechanism=%d", tokenName, 
                      CKM_INVALID_MECHANISM);
    PR_Free(tokenName);
    return url;
}

char *
SSM_SetPasswordHTMLParams(PK11SlotInfo *slot) {
  return SSM_SetPasswordHTMLParamsFromTokenName(PK11_GetTokenName(slot));
}

char * SSM_GenerateChangePasswordURL(PK11SlotInfo *slot, SSMResource *target)
{
    PRUint32 width, height;
    char *slotHTMLName = NULL, *url = NULL, *extraParams = NULL;
    SSMStatus rv;

    if (slot) {
        slotHTMLName = 
          SSM_ConvertStringToHTMLString(PK11_GetTokenName(slot));
    } else {
        slotHTMLName = PL_strdup("all");
    }
    extraParams = SSM_SetPasswordHTMLParams(slot);
    rv = SSM_GenerateURL(target->m_connection,"get", "set_password", target, 
                         extraParams, &width, &height, &url);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    PR_Free(slotHTMLName);
    PR_Free(extraParams);
    return url;
 loser:
    PR_FREEIF(slotHTMLName);
    PR_FREEIF(extraParams);
   
    return NULL;
}

SSMStatus SSM_SetUserPassword(PK11SlotInfo * slot, SSMResource * ct)
{
  SSMStatus rv;
  char *params = SSM_SetPasswordHTMLParams(slot);

  SSM_LockUIEvent(ct);
  rv = SSMControlConnection_SendUIEvent(ct->m_connection,
                                        "get", "set_password",
                                        ct, params,
                                        &ct->m_clientContext,
                                        PR_TRUE);
  if (rv != SSM_SUCCESS) 
    goto loser;
  SSM_WaitUIEvent(ct, PR_INTERVAL_NO_TIMEOUT);
  return rv;
loser:
  SSM_UnlockUIEvent(ct);
  return rv;
}

SSMStatus SSM_ProcessPasswordWindow(HTTPRequest * req)
{
  SSMStatus rv = SSM_FAILURE;
  SSMResource * target = NULL;
  char *slotName = NULL, *slotHTMLName = NULL;
  char *extraParams = NULL;
  
  if (!req || !req->ctrlconn) 
    goto loser;
  /*
   * The window contents aren't going to change, so just send back
   * a NO_CONTENT error which causes leave its content as is.
   */
  rv = SSM_HTTPReportError(req, HTTP_NO_CONTENT);
  target = REQ_TARGET(req);
  /* First let's figure out if there are more than one token installed,
   * if so ask the user which one to change the password on
   */
  slotName = SSM_GetSlotNameForPasswordChange(req);
  if (slotName == NULL) {
    goto loser;
  }

  extraParams = SSM_SetPasswordHTMLParamsFromTokenName(slotName);
  SSM_LockUIEvent(target);
  /* send UI event to bring up the dialog */
  rv = SSMControlConnection_SendUIEvent(req->ctrlconn, "get", 
                                        "set_password", target, 
                                        extraParams, 
                                        &target->m_clientContext,
                                        PR_TRUE);
  PR_FREEIF(extraParams);
  if (rv != SSM_SUCCESS) { 
    SSM_UnlockUIEvent(&req->ctrlconn->super.super);
    goto loser;
  }
  SSM_WaitUIEvent(target, PR_INTERVAL_NO_TIMEOUT);
 loser:
  return rv;
}


