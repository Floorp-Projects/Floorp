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
#ifndef __SSM_CONNECT_H__
#define __SSM_CONNECT_H__

#include "ssmdefs.h"
#include "collectn.h"
#include "hashtbl.h"
#include "resource.h"
#include "pkcs11t.h"
#include "plarena.h"
#include "mcom_db.h"
#include "seccomon.h"
#include "secmodt.h"
#include "prinrval.h"

/* Cartman password policy */
#define SSM_MIN_PWD_LEN   0
#define SSM_MAX_PWD_LEN   64

typedef struct SSMConnection SSMConnection;
typedef SSMStatus (*SSMConnectionAuthenticateFunc)(SSMConnection *conn,
												   char *nonce);

struct SSMConnection
{
    SSMResource super;
    /*
      ---------------------------------------------
      Generic fields, applicable to all connections
      ---------------------------------------------
    */
   
    /* ### mwelch - children/parent should perhaps be put in subclasses,
       but we may use these in multiple ways in the future */

    /* Parent connection object. If this is a data object, m_parent points
       to the control connection which spawned it. */
    SSMConnection * m_parent;

    /* Child connection objects. If this is a control object, the collection
       is filled with data connection objects. */
    SSMCollection *m_children;

	/* The function to call when we want to validate an incoming connection */
	SSMConnectionAuthenticateFunc m_auth_func;
};

/* 
 * Information for authenticated tokens. 
 */
#define SSM_MINUTES_WAIT_PASSWORD 10
#define SSM_PASSWORD_WAIT_TIME  (SSM_MINUTES_WAIT_PASSWORD*PR_TicksPerSecond()*60)
#define SSM_NO_PASSWORD         0x99999999
#define SSM_CANCEL_PASSWORD     0x99999998

struct _SSM_TokenInfoStr {
    PK11SlotInfo * slot;   
    PRInt32 tokenID;
    char * encrypted;
    PRInt32 encryptedLen;
    PK11SymKey * symKey;
};
typedef struct _SSM_TokenInfoStr SSM_TokenInfo;


/* Password callback */
char * SSM_GetPasswdCallback(PK11SlotInfo *slot, PRBool retry,  void *arg);
PRBool SSM_VerifyPasswdCallback(PK11SlotInfo * slot, void * arg);

#ifdef ALLOW_STANDALONE
/* For standalone mode only */
char * SSM_StandaloneGetPasswdCallback(PK11SlotInfo *slot, PRBool retry,  
                                       void *arg);
PRBool SSM_StandaloneVerifyPasswdCallback(PK11SlotInfo * slot, void * arg);
#endif

char * SSM_GetAuthentication(PK11SlotInfo * slot, PRBool retry, PRBool init, 
				SSMResource * conn);
SSMStatus SSM_SetUserPassword(PK11SlotInfo * slot, SSMResource * ct);

SSMStatus SSMConnection_Create(void *arg, SSMControlConnection * conn, 
                              SSMResource **res);
SSMStatus SSMConnection_Init(SSMControlConnection *ctrl, 
			    SSMConnection *conn, SSMResourceType type);
SSMStatus SSMConnection_Shutdown(SSMResource *conn, SSMStatus status);
SSMStatus SSMConnection_Destroy(SSMResource *conn, PRBool doFree);
void SSMConnection_Invariant(SSMConnection *conn);

SSMStatus SSMConnection_AttachChild(SSMConnection *parent, SSMConnection *child);
SSMStatus SSMConnection_DetachChild(SSMConnection *parent, SSMConnection *child);

SSMStatus SSMConnection_GetAttrIDs(SSMResource *res,
                                  SSMAttributeID **ids,
                                  PRIntn *count);

SSMStatus SSMConnection_SetAttr(SSMResource *res,
                               SSMAttributeID attrID,
                               SSMAttributeValue *value);

SSMStatus SSMConnection_GetAttr(SSMResource *res,
                                SSMAttributeID attrID,
                                SSMResourceAttrType attrType,
                                SSMAttributeValue *value);

/* Given a RID and a proposed nonce, see if the connection exists
   and if the nonce matches. */
SSMStatus SSMConnection_Authenticate(SSMConnection *conn, char *nonce);
/* Function to encrypt password to storing it in Cartman */
SSMStatus SSM_EncryptPasswd(PK11SlotInfo * slot, char * passwd, 
                            SSM_TokenInfo ** info);
SSMStatus SSMControlConnection_WaitPassword(SSMConnection * conn, 
                                           PRInt32 key, char ** str);
SSMStatus SSM_AskUserPassword(SSMResource * conn, 
                             PK11SlotInfo * slot, PRInt32 retry, PRBool init);
/* Function returning user prompt */
char * SSM_GetPrompt(PK11SlotInfo *slot, PRBool retry, PRBool init);

#define XP_SEC_ERROR_PWD "Successive login failures may disable this card or database. Password is invalid. Retry?\n %s\n"

#define XP_SEC_ENTER_PWD "Please enter the password or the pin for\n%s."
#define XP_SEC_SET_PWD "Please enter the password or the pin for\n%s. Remember this password, you will need \nit to access your certificates."
#endif
