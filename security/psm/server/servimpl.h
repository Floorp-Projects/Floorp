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
#ifndef __SERVIMPL_H__
#define __SERVIMPL_H__

#include "ssmdefs.h"
#include "collectn.h"
#include "hashtbl.h"

/* Doc subdirectory */
#ifdef XP_UNIX
#define PSM_DOC_DIR "./psmdata/doc/"
#else
#ifdef XP_MAC
#define PSM_DOC_DIR "psmdata/doc/"
#else
#define PSM_DOC_DIR ".\\psmdata\\doc\\"
#endif
#endif

/* Messages passed between data service threads */
typedef enum 
{
  SSM_DATA_BUFFER = (PRIntn) 0, /* "Here's data to write" */
  SSM_DATA_PROVIDER_OPEN,       /* "My end is open, about to queue data" */
  SSM_DATA_PROVIDER_SHUTDOWN    /* "My socket has closed" */
} SSMDataMessageType;  

typedef struct _SSMResponse SSMResponse;
typedef struct _SSMResponseQueue SSMResponseQueue;

void SSM_InitNLS(char *dataDirectory);
SECItem * SSM_ConstructMessage(PRUintn size);
void SSM_FreeEvent(SECItem * event);
void SSM_FreeResponse(SSMResponse * response);
void SSM_FreeMessage(SECItem * msg);
int SSM_ReadThisMany(PRFileDesc * sockID, void * buffer, int thisMany);
int SSM_WriteThisMany(PRFileDesc * sockID, void * buffer, int thisMany);

SSMStatus SSM_SendQMessage(SSMCollection *c,
			  PRIntn priority,
			  PRIntn type, PRIntn length, char *data,
			  PRBool doBlock);

SSMStatus SSM_RecvQMessage(SSMCollection *c,
			  PRIntn priority,
			  PRIntn *type, PRIntn *length, char **data,
			  PRBool doBlock);

void ssm_DrainAndDestroyQueue(SSMCollection **coll);

#endif

