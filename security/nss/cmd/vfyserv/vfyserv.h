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

#ifndef SSLSAMPLE_H
#define SSLSAMPLE_H

/* Generic header files */

#include <stdio.h>
#include <string.h>

/* NSPR header files */

#include "nspr.h"
#include "prerror.h"
#include "prnetdb.h"

/* NSS header files */

#include "pk11func.h"
#include "secitem.h"
#include "ssl.h"
#include "certt.h"
#include "nss.h"
#include "secder.h"
#include "key.h"
#include "sslproto.h"

/* Custom header files */

/*
#include "sslerror.h"
*/

#define BUFFER_SIZE 10240

/* Declare SSL cipher suites. */

extern int cipherSuites[];
extern int ssl2CipherSuites[];
extern int ssl3CipherSuites[];

/* Data buffer read from a socket. */
typedef struct DataBufferStr {
	char data[BUFFER_SIZE];
	int  index;
	int  remaining;
	int  dataStart;
	int  dataEnd;
} DataBuffer;

/* SSL callback routines. */

char * myPasswd(PK11SlotInfo *info, PRBool retry, void *arg);

SECStatus myAuthCertificate(void *arg, PRFileDesc *socket,
                            PRBool checksig, PRBool isServer);

SECStatus myBadCertHandler(void *arg, PRFileDesc *socket);

SECStatus myHandshakeCallback(PRFileDesc *socket, void *arg);

SECStatus myGetClientAuthData(void *arg, PRFileDesc *socket,
                              struct CERTDistNamesStr *caNames,
                              struct CERTCertificateStr **pRetCert,
                              struct SECKEYPrivateKeyStr **pRetKey);

/* Disable all v2/v3 SSL ciphers. */

void disableAllSSLCiphers(void);


/* Error and information utilities. */

void errWarn(char *function);

void exitErr(char *function);

void printSecurityInfo(FILE *outfile, PRFileDesc *fd);

/* Some simple thread management routines. */

#define MAX_THREADS 32

typedef SECStatus startFn(void *a, int b);

typedef enum { rs_idle = 0, rs_running = 1, rs_zombie = 2 } runState;

typedef struct perThreadStr {
	PRFileDesc *a;
	int         b;
	int         rv;
	startFn    *startFunc;
	PRThread   *prThread;
	PRBool      inUse;
	runState    running;
} perThread;

typedef struct GlobalThreadMgrStr {
	PRLock	  *threadLock;
	PRCondVar *threadStartQ;
	PRCondVar *threadEndQ;
	perThread  threads[MAX_THREADS];
	int        index;
	int        numUsed;
	int        numRunning;
} GlobalThreadMgr;

void thread_wrapper(void * arg);

SECStatus launch_thread(GlobalThreadMgr *threadMGR, 
                        startFn *startFunc, void *a, int b);

SECStatus reap_threads(GlobalThreadMgr *threadMGR);

void destroy_thread_data(GlobalThreadMgr *threadMGR);

/* Management of locked variables. */

struct lockedVarsStr {
	PRLock *    lock;
	int         count;
	int         waiters;
	PRCondVar * condVar;
};

typedef struct lockedVarsStr lockedVars;

void lockedVars_Init(lockedVars *lv);

void lockedVars_Destroy(lockedVars *lv);

void lockedVars_WaitForDone(lockedVars *lv);

int lockedVars_AddToCount(lockedVars *lv, int addend);

/* Buffer stuff. */

static const char stopCmd[] = { "GET /stop " };
static const char defaultHeader[] = {
	"HTTP/1.0 200 OK\r\n"
	"Server: SSL sample server\r\n"
	"Content-type: text/plain\r\n"
	"\r\n"
};

#endif
