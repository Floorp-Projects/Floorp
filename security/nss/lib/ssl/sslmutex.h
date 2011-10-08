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
 * Portions created by the Initial Developer are Copyright (C) 2001
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
/* $Id: sslmutex.h,v 1.13 2011/09/30 23:27:08 rrelyea%redhat.com Exp $ */
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
