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
#ifndef _KGENCTXT_H_
#define _KGENCTXT_H_
#include "resource.h"
#include "keyres.h"
#include "ctrlconn.h"
#include "secmodti.h"
#include "minihttp.h"

typedef enum {
    SSM_CRMF_KEYGEN = (PRInt32) 1, 
    SSM_OLD_STYLE_KEYGEN
} SSMKeyGenContextType;
    
/* Parameters used to create a key gen context */
typedef struct SSMKeyGenContextCreateArg
{
    SSMControlConnection *parent;
    SSMKeyGenContextType  type;
    CMTItem              *param;
} SSMKeyGenContextCreateArg;

typedef struct SSMKeyGenParams
{
    PRUint32 slotID;
    PRUint32 keyGenMechanism;
    SSMKeyPair *kp;
    void *actualParams;
} SSMKeyGenParams;

/* 
   Key generation context. This object helps transactionalize 
   the generation of one or more key pairs for new certificates. 

   This object has associated with it an incoming queue for key generation
   and flush requests, and a worker thread to do the key generation.
*/
struct SSMKeyGenContext 
{
    SSMResource super;

    /* Control connection which created us (and to whom we send
       completion responses */
    SSMControlConnection *m_parent;
 
    /* Context is used for CRMF or old-style keygen */
    SSMKeyGenContextType m_ctxtype;

    /* Queue for messages from control connection dispatcher */
    SSMCollection *m_incomingQ;

    /* Thread which processes keygen requests. */
    PRThread *m_serviceThread;
 
    /* Slot for a token used to generate keys. */
    PK11SlotInfo * slot;
    char *m_slotName;
    PRBool m_disableEscrowWarning;
    
    /* UI information for slot selection. */
    PRMonitor *slotSelectLock;
    PRBool slotUIWaiting;
    PRBool slotUIComplete;
    
    /* key exchange mechanism */
    PRUint32 mech;

    /* public key string, for use with KEYGEN tag */
    char * keystring;

    /* The number of requests we've completed. */
    PRUint32 m_count;
    SSMKeyGenParams **m_keyGens; /* We will finish these up
                                  * once we get the Finish Key Gen message
                                  */
    CERTCertificate *m_eaCert;
    PRInt32 m_numKeyGens;
    PRInt32 m_allocKeyGens;
    PRBool  m_userCancel;
    CMTItem m_context;
};

typedef enum
{
  SSM_KEYGEN_CXT_MESSAGE_NONE = (PRIntn) 0,
  SSM_KEYGEN_CXT_MESSAGE_DATA_PROVIDER_OPEN = SSM_DATA_PROVIDER_OPEN,
  SSM_KEYGEN_CXT_MESSAGE_DATA_PROVIDER_SHUTDOWN = SSM_DATA_PROVIDER_SHUTDOWN,
  SSM_KEYGEN_CXT_MESSAGE_GEN_KEY = SSM_REQUEST_MESSAGE | 
                                   SSM_PKCS11_ACTION | SSM_CREATE_KEY_PAIR,
  SSM_KEYGEN_CXT_MESSAGE_ESCROW_WARNING
} SSMKeyGenContextQMessages;

/* The parameter passed as data when sending a message
 * for ESCROW_WARNING
 */
typedef struct SSMEscrowWarnParam
{
  CERTCertificate *escrowCA;
} SSMEscrowWarnParam;

SSMStatus SSMKeyGenContext_Create(void *arg, SSMControlConnection * conn, 
                                 SSMResource **res);
SSMStatus SSMKeyGenContext_Init(SSMKeyGenContext *ct, 
                               SSMKeyGenContextCreateArg *arg, 
                               SSMResourceType type);
SSMStatus SSMKeyGenContext_SetAttr(SSMResource *res,
                                  SSMAttributeID attrID,
                                  SSMAttributeValue *value);
SSMStatus SSMKeyGenContext_GetAttr(SSMResource *res,
                                   SSMAttributeID attrID,
                                   SSMResourceAttrType attrType,
                                   SSMAttributeValue *value);
SSMStatus SSMKeyGenContext_Destroy(SSMResource *res, PRBool doFree);
SSMStatus SSMKeyGenContext_Shutdown(SSMResource *res, SSMStatus status);
SSMStatus SSMKeyGenContext_SetStatus(SSMKeyGenContext *ct, SSMStatus s);
SSMStatus SSMKeyGenContext_NewKeyPair(SSMKeyGenContext *ct,
                                     SSMKeyPair **kp,
                                     SECItem *msg);
SSMStatus SSMKeyGenContext_Print(SSMResource   *res,
                                char *fmt,
                                PRIntn numParams,
				char ** value,
                                char **resultStr);
SSMStatus SSMKeyGenContext_FormSubmitHandler(SSMResource * res,
	                                     HTTPRequest * req);

                                     
void SSMKeyGenContext_Invariant(SSMKeyGenContext *ct);

SSMStatus SSMKeyGenContext_FinishGeneratingAllKeyPairs(SSMControlConnection *ctrl,
                                              	      SECItem        *msg);


SSMStatus SSMKeyGenContext_BeginGeneratingKeyPair(SSMControlConnection * ctrl,
                                                 SECItem *msg, 
                                                 SSMResourceID *destID);

void SSMKeyGenContext_CancelKeyGen(SSMKeyGenContext *ct);


/* Get a slot to perform keygen : 
 * returns SSM_SUCCESS if the slot is available, stored in keyctxt, 
 * or SSM_ERR_DEFER_RESPONSE if need to wait for user response, 
 * SSM_FAILURE on failure. 
 */
struct SSMTextGenContext;
struct HTTPRequest;
SSMStatus SSMKeyGenContext_GetSlot(SSMKeyGenContext * keyctxt,
                                   CK_MECHANISM_TYPE mechanism);
SSMStatus SSMTokenUI_KeywordHandler(struct SSMTextGenContext * cx);
SSMStatus SSMTokenUI_CommandHandler(struct HTTPRequest * req);
SSMStatus SSMTokenUI_GetNames(SSMResource * res, PRBool start, char ** name);

CK_MECHANISM_TYPE
SSMKeyGenContext_GenMechToAlgMech(CK_MECHANISM_TYPE mechanism);

#endif /* _KGENCTXT_H_ */
