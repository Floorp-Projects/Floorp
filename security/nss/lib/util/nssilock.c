/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nssilock.c - NSS lock instrumentation wrapper functions
 *
 * NOTE - These are not public interfaces
 *
 * Implementation Notes:
 * I've tried to make the instrumentation relatively non-intrusive.
 * To do this, I have used a single PR_LOG() call in each
 * instrumented function. There's room for improvement.
 *
 *
 */

#include "prinit.h"
#include "prerror.h"
#include "prlock.h"
#include "prmem.h"
#include "prenv.h"
#include "prcvar.h"
#include "prio.h"

#if defined(NEED_NSS_ILOCK)
#include "prlog.h"
#include "nssilock.h"

/*
** Declare the instrumented PZLock
*/
struct pzlock_s {
    PRLock *lock;        /* the PZLock to be instrumented */
    PRIntervalTime time; /* timestamp when the lock was aquired */
    nssILockType ltype;
};

/*
** Declare the instrumented PZMonitor
*/
struct pzmonitor_s {
    PRMonitor *mon;      /* the PZMonitor to be instrumented */
    PRIntervalTime time; /* timestamp when the monitor was aquired */
    nssILockType ltype;
};

/*
** Declare the instrumented PZCondVar
*/
struct pzcondvar_s {
    PRCondVar *cvar; /* the PZCondVar to be instrumented */
    nssILockType ltype;
};

/*
** Define a CallOnce type to ensure serialized self-initialization
*/
static PRCallOnceType coNssILock;  /* CallOnce type */
static PRIntn nssILockInitialized; /* initialization done when 1 */
static PRLogModuleInfo *nssILog;   /* Log instrumentation to this handle */

#define NUM_TT_ENTRIES 6000000
static PRInt32 traceIndex = -1; /* index into trace table */
static struct pzTrace_s *tt;    /* pointer to trace table */
static PRInt32 ttBufSize = (NUM_TT_ENTRIES * sizeof(struct pzTrace_s));
static PRCondVar *ttCVar;
static PRLock *ttLock;
static PRFileDesc *ttfd; /* trace table file */

/*
** Vtrace() -- Trace events, write events to external media
**
** Vtrace() records traced events in an in-memory trace table
** when the trace table fills, Vtrace writes the entire table
** to a file.
**
** data can be lost!
**
*/
static void
Vtrace(
    nssILockOp op,
    nssILockType ltype,
    PRIntervalTime callTime,
    PRIntervalTime heldTime,
    void *lock,
    PRIntn line,
    char *file)
{
    PRInt32 idx;
    struct pzTrace_s *tp;

RetryTrace:
    idx = PR_ATOMIC_INCREMENT(&traceIndex);
    while (NUM_TT_ENTRIES <= idx || op == FlushTT) {
        if (NUM_TT_ENTRIES == idx || op == FlushTT) {
            int writeSize = idx * sizeof(struct pzTrace_s);
            PR_Lock(ttLock);
            PR_Write(ttfd, tt, writeSize);
            traceIndex = -1;
            PR_NotifyAllCondVar(ttCVar);
            PR_Unlock(ttLock);
            goto RetryTrace;
        } else {
            PR_Lock(ttLock);
            while (NUM_TT_ENTRIES < idx)
                PR_WaitCondVar(ttCVar, PR_INTERVAL_NO_WAIT);
            PR_Unlock(ttLock);
            goto RetryTrace;
        }
    } /* end while() */

    /* create the trace entry */
    tp = tt + idx;
    tp->threadID = PR_GetThreadID(PR_GetCurrentThread());
    tp->op = op;
    tp->ltype = ltype;
    tp->callTime = callTime;
    tp->heldTime = heldTime;
    tp->lock = lock;
    tp->line = line;
    strcpy(tp->file, file);
    return;
} /* --- end Vtrace() --- */

/*
** pz_TraceFlush() -- Force trace table write to file
**
*/
extern void
pz_TraceFlush(void)
{
    Vtrace(FlushTT, nssILockSelfServ, 0, 0, NULL, 0, "");
    return;
} /* --- end pz_TraceFlush() --- */

/*
** nssILockInit() -- Initialization for nssilock
**
** This function is called from the CallOnce mechanism.
*/
static PRStatus
nssILockInit(void)
{
    int i;
    nssILockInitialized = 1;

    /* new log module */
    nssILog = PR_NewLogModule("nssilock");
    if (NULL == nssILog) {
        return (PR_FAILURE);
    }

    tt = PR_Calloc(NUM_TT_ENTRIES, sizeof(struct pzTrace_s));
    if (NULL == tt) {
        fprintf(stderr, "nssilock: can't allocate trace table\n");
        exit(1);
    }

    ttfd = PR_Open("xxxTTLog", PR_CREATE_FILE | PR_WRONLY, 0666);
    if (NULL == ttfd) {
        fprintf(stderr, "Oh Drat! Can't open 'xxxTTLog'\n");
        exit(1);
    }

    ttLock = PR_NewLock();
    ttCVar = PR_NewCondVar(ttLock);

    return (PR_SUCCESS);
} /* --- end nssILockInit() --- */

extern PZLock *
pz_NewLock(
    nssILockType ltype,
    char *file,
    PRIntn line)
{
    PRStatus rc;
    PZLock *lock;

    /* Self Initialize the nssILock feature */
    if (!nssILockInitialized) {
        rc = PR_CallOnce(&coNssILock, nssILockInit);
        if (PR_FAILURE == rc) {
            PR_SetError(PR_UNKNOWN_ERROR, 0);
            return (NULL);
        }
    }

    lock = PR_NEWZAP(PZLock);
    if (NULL != lock) {
        lock->ltype = ltype;
        lock->lock = PR_NewLock();
        if (NULL == lock->lock) {
            PR_DELETE(lock);
            PORT_SetError(SEC_ERROR_NO_MEMORY);
        }
    } else {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
    }

    Vtrace(NewLock, ltype, 0, 0, lock, line, file);
    return (lock);
} /* --- end pz_NewLock() --- */

extern void
pz_Lock(
    PZLock *lock,
    char *file,
    PRIntn line)
{
    PRIntervalTime callTime;

    callTime = PR_IntervalNow();
    PR_Lock(lock->lock);
    lock->time = PR_IntervalNow();
    callTime = lock->time - callTime;

    Vtrace(Lock, lock->ltype, callTime, 0, lock, line, file);
    return;
} /* --- end  pz_Lock() --- */

extern PRStatus
pz_Unlock(
    PZLock *lock,
    char *file,
    PRIntn line)
{
    PRStatus rc;
    PRIntervalTime callTime, now, heldTime;

    callTime = PR_IntervalNow();
    rc = PR_Unlock(lock->lock);
    now = PR_IntervalNow();
    callTime = now - callTime;
    heldTime = now - lock->time;
    Vtrace(Unlock, lock->ltype, callTime, heldTime, lock, line, file);
    return (rc);
} /* --- end  pz_Unlock() --- */

extern void
pz_DestroyLock(
    PZLock *lock,
    char *file,
    PRIntn line)
{
    Vtrace(DestroyLock, lock->ltype, 0, 0, lock, line, file);
    PR_DestroyLock(lock->lock);
    PR_DELETE(lock);
    return;
} /* --- end  pz_DestroyLock() --- */

extern PZCondVar *
pz_NewCondVar(
    PZLock *lock,
    char *file,
    PRIntn line)
{
    PZCondVar *cvar;

    cvar = PR_NEWZAP(PZCondVar);
    if (NULL == cvar) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
    } else {
        cvar->ltype = lock->ltype;
        cvar->cvar = PR_NewCondVar(lock->lock);
        if (NULL == cvar->cvar) {
            PR_DELETE(cvar);
            PORT_SetError(SEC_ERROR_NO_MEMORY);
        }
    }
    Vtrace(NewCondVar, lock->ltype, 0, 0, cvar, line, file);
    return (cvar);
} /* --- end  pz_NewCondVar() --- */

extern void
pz_DestroyCondVar(
    PZCondVar *cvar,
    char *file,
    PRIntn line)
{
    Vtrace(DestroyCondVar, cvar->ltype, 0, 0, cvar, line, file);
    PR_DestroyCondVar(cvar->cvar);
    PR_DELETE(cvar);
} /* --- end  pz_DestroyCondVar() --- */

extern PRStatus
pz_WaitCondVar(
    PZCondVar *cvar,
    PRIntervalTime timeout,
    char *file,
    PRIntn line)
{
    PRStatus rc;
    PRIntervalTime callTime;

    callTime = PR_IntervalNow();
    rc = PR_WaitCondVar(cvar->cvar, timeout);
    callTime = PR_IntervalNow() - callTime;

    Vtrace(WaitCondVar, cvar->ltype, callTime, 0, cvar, line, file);
    return (rc);
} /* --- end  pz_WaitCondVar() --- */

extern PRStatus
pz_NotifyCondVar(
    PZCondVar *cvar,
    char *file,
    PRIntn line)
{
    PRStatus rc;

    rc = PR_NotifyCondVar(cvar->cvar);

    Vtrace(NotifyCondVar, cvar->ltype, 0, 0, cvar, line, file);
    return (rc);
} /* --- end  pz_NotifyCondVar() --- */

extern PRStatus
pz_NotifyAllCondVar(
    PZCondVar *cvar,
    char *file,
    PRIntn line)
{
    PRStatus rc;

    rc = PR_NotifyAllCondVar(cvar->cvar);

    Vtrace(NotifyAllCondVar, cvar->ltype, 0, 0, cvar, line, file);
    return (rc);
} /* --- end  pz_NotifyAllCondVar() --- */

extern PZMonitor *
pz_NewMonitor(
    nssILockType ltype,
    char *file,
    PRIntn line)
{
    PRStatus rc;
    PZMonitor *mon;

    /* Self Initialize the nssILock feature */
    if (!nssILockInitialized) {
        rc = PR_CallOnce(&coNssILock, nssILockInit);
        if (PR_FAILURE == rc) {
            PR_SetError(PR_UNKNOWN_ERROR, 0);
            return (NULL);
        }
    }

    mon = PR_NEWZAP(PZMonitor);
    if (NULL != mon) {
        mon->ltype = ltype;
        mon->mon = PR_NewMonitor();
        if (NULL == mon->mon) {
            PR_DELETE(mon);
            PORT_SetError(SEC_ERROR_NO_MEMORY);
        }
    } else {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
    }

    Vtrace(NewMonitor, ltype, 0, 0, mon, line, file);
    return (mon);
} /* --- end  pz_NewMonitor() --- */

extern void
pz_DestroyMonitor(
    PZMonitor *mon,
    char *file,
    PRIntn line)
{
    Vtrace(DestroyMonitor, mon->ltype, 0, 0, mon, line, file);
    PR_DestroyMonitor(mon->mon);
    PR_DELETE(mon);
    return;
} /* --- end  pz_DestroyMonitor() --- */

extern void
pz_EnterMonitor(
    PZMonitor *mon,
    char *file,
    PRIntn line)
{
    PRIntervalTime callTime, now;

    callTime = PR_IntervalNow();
    PR_EnterMonitor(mon->mon);
    now = PR_IntervalNow();
    callTime = now - callTime;
    if (PR_GetMonitorEntryCount(mon->mon) == 1) {
        mon->time = now;
    }
    Vtrace(EnterMonitor, mon->ltype, callTime, 0, mon, line, file);
    return;
} /* --- end  pz_EnterMonitor() --- */

extern PRStatus
pz_ExitMonitor(
    PZMonitor *mon,
    char *file,
    PRIntn line)
{
    PRStatus rc;
    PRIntervalTime callTime, now, heldTime;
    PRIntn mec = PR_GetMonitorEntryCount(mon->mon);

    heldTime = (PRIntervalTime)-1;
    callTime = PR_IntervalNow();
    rc = PR_ExitMonitor(mon->mon);
    now = PR_IntervalNow();
    callTime = now - callTime;
    if (mec == 1)
        heldTime = now - mon->time;
    Vtrace(ExitMonitor, mon->ltype, callTime, heldTime, mon, line, file);
    return (rc);
} /* --- end  pz_ExitMonitor() --- */

extern PRIntn
pz_GetMonitorEntryCount(
    PZMonitor *mon,
    char *file,
    PRIntn line)
{
    return (PR_GetMonitorEntryCount(mon->mon));
} /* --- end pz_GetMonitorEntryCount() --- */

extern PRStatus
pz_Wait(
    PZMonitor *mon,
    PRIntervalTime ticks,
    char *file,
    PRIntn line)
{
    PRStatus rc;
    PRIntervalTime callTime;

    callTime = PR_IntervalNow();
    rc = PR_Wait(mon->mon, ticks);
    callTime = PR_IntervalNow() - callTime;
    Vtrace(Wait, mon->ltype, callTime, 0, mon, line, file);
    return (rc);
} /* --- end  pz_Wait() --- */

extern PRStatus
pz_Notify(
    PZMonitor *mon,
    char *file,
    PRIntn line)
{
    PRStatus rc;
    PRIntervalTime callTime;

    callTime = PR_IntervalNow();
    rc = PR_Notify(mon->mon);
    callTime = PR_IntervalNow() - callTime;
    Vtrace(Notify, mon->ltype, callTime, 0, mon, line, file);
    return (rc);
} /* --- end  pz_Notify() --- */

extern PRStatus
pz_NotifyAll(
    PZMonitor *mon,
    char *file,
    PRIntn line)
{
    PRStatus rc;
    PRIntervalTime callTime;

    callTime = PR_IntervalNow();
    rc = PR_NotifyAll(mon->mon);
    callTime = PR_IntervalNow() - callTime;
    Vtrace(NotifyAll, mon->ltype, callTime, 0, mon, line, file);
    return (rc);
} /* --- end  pz_NotifyAll() --- */

#endif /* NEED_NSS_ILOCK */
/* --- end nssilock.c --------------------------------- */
