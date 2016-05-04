/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
extern int ssl3CipherSuites[];

/* Data buffer read from a socket. */
typedef struct DataBufferStr {
    char data[BUFFER_SIZE];
    int index;
    int remaining;
    int dataStart;
    int dataEnd;
} DataBuffer;

/* SSL callback routines. */

char *myPasswd(PK11SlotInfo *info, PRBool retry, void *arg);

SECStatus myAuthCertificate(void *arg, PRFileDesc *socket,
                            PRBool checksig, PRBool isServer);

SECStatus myBadCertHandler(void *arg, PRFileDesc *socket);

void myHandshakeCallback(PRFileDesc *socket, void *arg);

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

typedef enum { rs_idle = 0,
               rs_running = 1,
               rs_zombie = 2 } runState;

typedef struct perThreadStr {
    PRFileDesc *a;
    int b;
    int rv;
    startFn *startFunc;
    PRThread *prThread;
    PRBool inUse;
    runState running;
} perThread;

typedef struct GlobalThreadMgrStr {
    PRLock *threadLock;
    PRCondVar *threadStartQ;
    PRCondVar *threadEndQ;
    perThread threads[MAX_THREADS];
    int index;
    int numUsed;
    int numRunning;
} GlobalThreadMgr;

void thread_wrapper(void *arg);

SECStatus launch_thread(GlobalThreadMgr *threadMGR,
                        startFn *startFunc, void *a, int b);

SECStatus reap_threads(GlobalThreadMgr *threadMGR);

void destroy_thread_data(GlobalThreadMgr *threadMGR);

/* Management of locked variables. */

struct lockedVarsStr {
    PRLock *lock;
    int count;
    int waiters;
    PRCondVar *condVar;
};

typedef struct lockedVarsStr lockedVars;

void lockedVars_Init(lockedVars *lv);

void lockedVars_Destroy(lockedVars *lv);

void lockedVars_WaitForDone(lockedVars *lv);

int lockedVars_AddToCount(lockedVars *lv, int addend);

#endif
