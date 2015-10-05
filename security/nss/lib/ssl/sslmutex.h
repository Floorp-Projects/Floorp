/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef __SSLMUTEX_H_
#define __SSLMUTEX_H_ 1

/* What SSL really wants is portable process-shared unnamed mutexes in 
 * shared memory, that have the property that if the process that holds
 * them dies, they are released automatically, and that (unlike fcntl 
 * record locking) lock to the thread, not to the process.  
 * NSPR doesn't provide that.  
 * Windows has mutexes that meet that description, but they're not portable.  
 * POSIX mutexes are not automatically released when the holder dies, 
 * and other processes/threads cannot release the mutex on behalf of the 
 * dead holder.  
 * POSIX semaphores can be used to accomplish this on systems that implement 
 * process-shared unnamed POSIX semaphores, because a watchdog thread can 
 * discover and release semaphores that were held by a dead process.  
 * On systems that do not support process-shared POSIX unnamed semaphores, 
 * they can be emulated using pipes.  
 * The performance cost of doing that is not yet measured.
 *
 * So, this API looks a lot like POSIX pthread mutexes.
 */

#include "prtypes.h"
#include "prlock.h"

#if defined(NETBSD)
#include <sys/param.h> /* for __NetBSD_Version__ */
#endif

#if defined(WIN32)

#include <wtypes.h>

typedef struct 
{
    PRBool isMultiProcess;
#ifdef WINNT
    /* on WINNT we need both the PRLock and the Win32 mutex for fibers */
    struct {
#else
    union {
#endif
        PRLock* sslLock;
        HANDLE sslMutx;
    } u;
} sslMutex;

typedef int    sslPID;

#elif defined(LINUX) || defined(AIX) || defined(BEOS) || defined(BSDI) || (defined(NETBSD) && __NetBSD_Version__ < 500000000) || defined(OPENBSD)

#include <sys/types.h>
#include "prtypes.h"

typedef struct { 
    PRBool isMultiProcess;
    union {
        PRLock* sslLock;
        struct {
            int      mPipes[3]; 
            PRInt32  nWaiters;
        } pipeStr;
    } u;
} sslMutex;
typedef pid_t sslPID;

#elif defined(XP_UNIX) /* other types of Unix */

#include <sys/types.h>	/* for pid_t */
#include <semaphore.h>  /* for sem_t, and sem_* functions */

typedef struct
{
    PRBool isMultiProcess;
    union {
        PRLock* sslLock;
        sem_t  sem;
    } u;
} sslMutex;

typedef pid_t sslPID;

#else

/* what platform is this ?? */

typedef struct { 
    PRBool isMultiProcess;
    union {
        PRLock* sslLock;
        /* include cross-process locking mechanism here */
    } u;
} sslMutex;

typedef int sslPID;

#endif

#include "seccomon.h"

SEC_BEGIN_PROTOS

extern SECStatus sslMutex_Init(sslMutex *sem, int shared);

/* If processLocal is set to true, then just free resources which are *only* associated
 * with the current process. Leave any shared resources (including the state of 
 * shared memory) intact. */
extern SECStatus sslMutex_Destroy(sslMutex *sem, PRBool processLocal);

extern SECStatus sslMutex_Unlock(sslMutex *sem);

extern SECStatus sslMutex_Lock(sslMutex *sem);

#ifdef WINNT

extern SECStatus sslMutex_2LevelInit(sslMutex *sem);

#endif

SEC_END_PROTOS

#endif
