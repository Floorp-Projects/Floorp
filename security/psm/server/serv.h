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
#ifndef __SERV_H__
#define __SERV_H__

#include "prio.h"
#include "prtypes.h"
#include "prnetdb.h"
#include "prprf.h"
#include "prinit.h"
#include "prthread.h"
#include "prinrval.h"
#include "prmon.h"
#include "prmem.h"
#include "prerror.h"

#include <stdlib.h>
#include "seccomon.h"
#include "ssmdefs.h"
#include "servimpl.h"
#include "protocol.h"
#include "ssmerrs.h"

/* extern int errno; */

#undef ALLOW_STANDALONE

#ifdef ALLOW_STANDALONE
extern PRBool standalone;
#define SSM_ENV_STANDALONE       "NSM_STANDALONE"
#endif

/* Environment variables we pay attention to */
#ifdef DEBUG
#define SSM_ENV_SUPPRESS_CONSOLE "NSM_SUPPRESS_CONSOLE"
#define SSM_ENV_LOG_FILE         "NSM_LOG_FILE"
/* Password must be base64 encoded. */
#define SSM_ENV_STATIC_PASSWORD  "NSM_PASSWORD"
#endif

#define MAX_CONNECTIONS                         5
#define LINESIZE                               512
#define DATA_CONN_MSG_SIZE                     sizeof(PRFileDesc *)


#define CARTMAN_WAIT_BEFORE_SLEEP           (PR_TicksPerSecond() * 60)
#define CARTMAN_SPINTIME                    CARTMAN_WAIT_BEFORE_SLEEP
#define CARTMAN_KEEP_CONNECTION_ALIVE       (CARTMAN_WAIT_BEFORE_SLEEP * 900)

/* Some macros to cut down typing when doing memory allocation.*/
#define SSM_NEW  PR_NEW 
#define SSM_ZNEW PR_NEWZAP
#define SSM_NEW_ARRAY(type,size)  (type*)PR_Malloc((size)*sizeof(type))
#define SSM_ZNEW_ARRAY(type,size) (type*)PR_Calloc((size),sizeof(type)) 

/* Safe strcmp() which compares only based on the shorter 
   of the two lengths. DO NOT USE if you really want to determine
   string equality -- this is used for sloppy parameter matches. */
#define MIN_STRCMP(x,y) strncmp((x), (y), PR_MIN(strlen(x),strlen(y)))

/* forward declare this type to eliminate circular dependencies */
typedef struct SSMResource SSMResource;
typedef struct SSMControlConnection SSMControlConnection;

extern PRUint32 httpPort;

PRIntn mainLoop(PRIntn argc, char ** argv);
PRFileDesc * SSM_OpenControlPort(void);
PRFileDesc * SSM_OpenPort(void);
PRBool SSM_SocketPeerCheck(PRFileDesc *sock, PRBool isCtrl);

SSMPolicyType SSM_GetPolicy(void);

#define THREAD_NAME_LEN 128

SSMStatus SSM_AddLogSocket(PRFileDesc *sock);
void SSM_DebugP(char *fmt, ...);

#if defined(WIN32)
#define STRNCASECMP(s1,s2,n) _strnicmp((s1),(s2),(n))
#elif defined(XP_UNIX)
#define STRNCASECMP(s1,s2,n) strncasecmp((s1),(s2),(n))
#else
/* Should really figure out how this platform does this */
int SSM_strncasecmp(const char *s1, const char *s2, size_t count);
#define STRNCASECMP(s1,s2,n) SSM_strncasecmp((s1),(s2),(n))
#endif

/*
 * If you want to enable PSM log output, make the following
 * #if 0 block a #if 1 block or add a -DPSM_LOG to the compile
 * line.
 */
#if 0
#define PSM_LOG 1
#endif

#if defined(DEBUG) && defined(PSM_LOG)
#define SSM_DEBUG SSM_DebugP
#else
#define SSM_DEBUG if(0) SSM_DebugP
#endif
void SSM_DumpBuffer(char *buf, PRIntn len);

SECItem * SSM_ConstructMessage(PRUintn size);
void SSM_FreeEvent(SECItem * event);
void SSM_FreeResponse(SSMResponse * response);
void SSM_FreeMessage(SECItem * msg);
int ssm_ReadThisMany(PRFileDesc * sockID, void * buffer, int thisMany);
int ssm_WriteThisMany(PRFileDesc * sockID, void * buffer, int thisMany);

/* Macros used in http and base64 processing */
#undef IS_EOL
#define IS_EOL(c) (((c) == 0x0A) || ((c) == 0x0D))
#undef IS_WHITESPACE
#define IS_WHITESPACE(c) (((c) == '\t') || ((c) == '\n') \
                          || ((c) == '\r') || ((c) == ' '))

extern SSMPolicyType policyType;

SSMStatus SSM_CloseSocketWithLinger(PRFileDesc *sock);

SSMStatus SSM_RequestFilePathFromUser(SSMResource *res,
				      const char *promptKey,
				      const char *fileRegEx,
				      PRBool getExistingFile);

char* SSM_GetCharFromKey(const char *key, const char *locale);

void SSM_HandleFilePathReply(SSMControlConnection *ctrl,
			     SECItem              *msg);
void SSM_HandleUserPromptReply(SSMControlConnection *ctrl,
			       SECItem              *msg);
			       
SSMStatus SSM_CopyCMTItem(CMTItem *dest, CMTItem *source);

SSMStatus
SSM_PrettyPrintDER(PRFileDesc *out, SECItem *derItem, PRBool raw);
PRBool
SSM_IsCRLPresent(SSMControlConnection *ctrl);

#ifdef XP_UNIX
void SSM_ReleaseLockFile();
#endif /*XP_UNIX*/

/* Function used on Mac to register threads for later destruction at app quit time. */
#ifdef XP_MAC
PRThread * SSM_CreateAndRegisterThread(PRThreadType type,
										void (*start)(void *arg),
										void *arg,
										PRThreadPriority priority,
										PRThreadScope scope,
										PRThreadState state,
										PRUint32 stackSize);

void SSM_KillAllThreads(void);
char* SSM_ConvertMacPathToUnix(char* macPath);                 
#else
#define SSM_CreateAndRegisterThread PR_CreateThread
#endif

#endif
