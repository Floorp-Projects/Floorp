/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
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

/*
 *********************************************************************
 *
 * Pollable events
 *
 * Pollable events are implemented using layered I/O.  The only
 * I/O methods that are implemented for pollable events are poll
 * and close.  No other methods can be invoked on a pollable
 * event.
 *
 * A pipe or socket pair is created and the pollable event layer
 * is pushed onto the read end.  A pointer to the write end is
 * saved in the PRFilePrivate structure of the pollable event.
 *
 *********************************************************************
 */

#include "prinit.h"
#include "prio.h"
#include "prmem.h"
#include "prerror.h"
#include "prlog.h"

#ifdef VMS

/*
 * On OpenVMS we use an event flag instead of a pipe or a socket since
 * event flags are much more efficient on OpenVMS.
 */
#include "pprio.h"
#include <lib$routines.h>
#include <starlet.h>
#include <stsdef.h>

PR_IMPLEMENT(PRFileDesc *) PR_NewPollableEvent(void)
{
    unsigned int status;
    int flag = -1;
    PRFileDesc *event;

    /*
    ** Allocate an event flag and clear it.
    */
    status = lib$get_ef(&flag);
    if ((!$VMS_STATUS_SUCCESS(status)) || (flag == -1)) {
        PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, status);
        return NULL;
    }
    sys$clref(flag);

    /*
    ** Give NSPR the event flag's negative value. We do this because our
    ** select interprets a negative fd as an event flag rather than a
    ** regular file fd.
    */
    event = PR_CreateSocketPollFd(-flag);
    if (NULL == event) {
        lib$free_ef(&flag);
        return NULL;
    }

    return event;
}

PR_IMPLEMENT(PRStatus) PR_DestroyPollableEvent(PRFileDesc *event)
{
    int flag = -PR_FileDesc2NativeHandle(event);
    PR_DestroySocketPollFd(event);
    lib$free_ef(&flag);
    return PR_SUCCESS;
}

PR_IMPLEMENT(PRStatus) PR_SetPollableEvent(PRFileDesc *event)
{
    /*
    ** Just set the event flag.
    */
    unsigned int status;
    status = sys$setef(-PR_FileDesc2NativeHandle(event));
    if (!$VMS_STATUS_SUCCESS(status)) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, status);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}

PR_IMPLEMENT(PRStatus) PR_WaitForPollableEvent(PRFileDesc *event)
{
    /*
    ** Just clear the event flag.
    */
    unsigned int status;
    status = sys$clref(-PR_FileDesc2NativeHandle(event));
    if (!$VMS_STATUS_SUCCESS(status)) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, status);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}

#elif defined (XP_MAC)

#include "primpl.h"

/*
 * On Mac, local sockets cannot be used, because the networking stack
 * closes them when the machine goes to sleep. Instead, we'll use a simple
 * flag.
 */


/*
 * PRFilePrivate structure for the NSPR pollable events layer
 */
typedef struct PRPollableDesc {
    PRBool      gotEvent;
    PRThread    *pollingThread;
} PRPollableDesc;

static PRStatus PR_CALLBACK _pr_MacPolEvtClose(PRFileDesc *fd);

static PRInt16 PR_CALLBACK _pr_MacPolEvtPoll(
    PRFileDesc *fd, PRInt16 in_flags, PRInt16 *out_flags);

static PRIOMethods _pr_mac_polevt_methods = {
    PR_DESC_LAYERED,
    _pr_MacPolEvtClose,
    (PRReadFN)_PR_InvalidInt,
    (PRWriteFN)_PR_InvalidInt,
    (PRAvailableFN)_PR_InvalidInt,
    (PRAvailable64FN)_PR_InvalidInt64,
    (PRFsyncFN)_PR_InvalidStatus,
    (PRSeekFN)_PR_InvalidInt,
    (PRSeek64FN)_PR_InvalidInt64,
    (PRFileInfoFN)_PR_InvalidStatus,
    (PRFileInfo64FN)_PR_InvalidStatus,
    (PRWritevFN)_PR_InvalidInt,        
    (PRConnectFN)_PR_InvalidStatus,        
    (PRAcceptFN)_PR_InvalidDesc,        
    (PRBindFN)_PR_InvalidStatus,        
    (PRListenFN)_PR_InvalidStatus,        
    (PRShutdownFN)_PR_InvalidStatus,    
    (PRRecvFN)_PR_InvalidInt,        
    (PRSendFN)_PR_InvalidInt,        
    (PRRecvfromFN)_PR_InvalidInt,    
    (PRSendtoFN)_PR_InvalidInt,        
    _pr_MacPolEvtPoll,
    (PRAcceptreadFN)_PR_InvalidInt,   
    (PRTransmitfileFN)_PR_InvalidInt, 
    (PRGetsocknameFN)_PR_InvalidStatus,    
    (PRGetpeernameFN)_PR_InvalidStatus,    
    (PRReservedFN)_PR_InvalidInt,    
    (PRReservedFN)_PR_InvalidInt,    
    (PRGetsocketoptionFN)_PR_InvalidStatus,
    (PRSetsocketoptionFN)_PR_InvalidStatus,
    (PRSendfileFN)_PR_InvalidInt, 
    (PRConnectcontinueFN)_PR_InvalidStatus, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt
};

static PRDescIdentity _pr_mac_polevt_id;
static PRCallOnceType _pr_mac_polevt_once_control;
static PRStatus PR_CALLBACK _pr_MacPolEvtInit(void);

static PRInt16 PR_CALLBACK _pr_MacPolEvtPoll(
    PRFileDesc *fd, PRInt16 in_flags, PRInt16 *out_flags)
{
    PRPollableDesc   *pollDesc = (PRPollableDesc *)fd->secret;
    PR_ASSERT(pollDesc);

    // set the current thread so that we can wake up the poll thread
    pollDesc->pollingThread = PR_GetCurrentThread();

    if ((in_flags & PR_POLL_READ) && pollDesc->gotEvent)
        *out_flags = PR_POLL_READ;
    else
        *out_flags = 0;
    return pollDesc->gotEvent ? 1 : 0;
}

static PRStatus PR_CALLBACK _pr_MacPolEvtInit(void)
{
    _pr_mac_polevt_id = PR_GetUniqueIdentity("NSPR pollable events");
    if (PR_INVALID_IO_LAYER == _pr_mac_polevt_id) {
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}

static PRStatus PR_CALLBACK _pr_MacPolEvtClose(PRFileDesc *fd)
{
    PRPollableDesc   *pollDesc = (PRPollableDesc *)fd->secret;
    PR_ASSERT(NULL == fd->higher && NULL == fd->lower);
    PR_ASSERT(pollDesc);
    PR_DELETE(pollDesc);
    fd->dtor(fd);
    return PR_SUCCESS;
}

PR_IMPLEMENT(PRFileDesc *) PR_NewPollableEvent(void)
{
    PRFileDesc      *event;
    PRPollableDesc   *pollDesc;
    
    if (PR_CallOnce(&_pr_mac_polevt_once_control, _pr_MacPolEvtInit) == PR_FAILURE) {
        return NULL;
    }

    event = PR_CreateIOLayerStub(_pr_mac_polevt_id, &_pr_mac_polevt_methods);
    if (NULL == event) {
        return NULL;
    } 

    /*
    ** Allocate an event flag and clear it.
    */
    pollDesc = PR_NEW(PRPollableDesc);
    if (!pollDesc) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        goto errorExit;
    }

    pollDesc->gotEvent = PR_FALSE;
    pollDesc->pollingThread = NULL;
    
    event->secret = (PRFilePrivate*)pollDesc;
    return event;

errorExit:

    if (event) {
        PR_DELETE(event->secret);
        event->dtor(event);
    }
    return NULL;
}

PR_IMPLEMENT(PRStatus) PR_DestroyPollableEvent(PRFileDesc *event)
{
    return PR_Close(event);
}

/* from macsockotpt.c. I wish there was a cleaner way */
extern void WakeUpNotifiedThread(PRThread *thread, OTResult result);

PR_IMPLEMENT(PRStatus) PR_SetPollableEvent(PRFileDesc *event)
{
    PRPollableDesc   *pollDesc = (PRPollableDesc *)event->secret;
    PR_ASSERT(pollDesc);
    PR_ASSERT(pollDesc->pollingThread->state != _PR_DEAD_STATE);
    
    if (pollDesc->pollingThread->state == _PR_DEAD_STATE)
      return PR_FAILURE;

    pollDesc->gotEvent = PR_TRUE;
    
    if (pollDesc->pollingThread)
        WakeUpNotifiedThread(pollDesc->pollingThread, noErr);
        
    return PR_SUCCESS;
}

PR_IMPLEMENT(PRStatus) PR_WaitForPollableEvent(PRFileDesc *event)
{
    PRPollableDesc   *pollDesc = (PRPollableDesc *)event->secret;
    PRStatus status;    
    PR_ASSERT(pollDesc);
    
    /*
      FIXME: Danger Will Robinson!
      
      The current implementation of PR_WaitForPollableEvent is somewhat
      bogus; it makes the assumption that, in Mozilla, this will only
      ever be called when PR_Poll has returned, telling us that an
      event has been set.
    */
    
    PR_ASSERT(pollDesc->gotEvent);
    
    status = (pollDesc->gotEvent) ? PR_SUCCESS : PR_FAILURE;
    pollDesc->gotEvent = PR_FALSE;
    return status;    
}

#else /* VMS */

/*
 * These internal functions are declared in primpl.h,
 * but we can't include primpl.h because the definition
 * of struct PRFilePrivate in this file (for the pollable
 * event layer) will conflict with the definition of
 * struct PRFilePrivate in primpl.h (for the NSPR layer).
 */
extern PRIntn _PR_InvalidInt(void);
extern PRInt64 _PR_InvalidInt64(void);
extern PRStatus _PR_InvalidStatus(void);
extern PRFileDesc *_PR_InvalidDesc(void);

/*
 * PRFilePrivate structure for the NSPR pollable events layer
 */
struct PRFilePrivate {
    PRFileDesc *writeEnd;  /* the write end of the pipe/socketpair */
};

static PRStatus PR_CALLBACK _pr_PolEvtClose(PRFileDesc *fd);

static PRInt16 PR_CALLBACK _pr_PolEvtPoll(
    PRFileDesc *fd, PRInt16 in_flags, PRInt16 *out_flags);

static PRIOMethods _pr_polevt_methods = {
    PR_DESC_LAYERED,
    _pr_PolEvtClose,
    (PRReadFN)_PR_InvalidInt,
    (PRWriteFN)_PR_InvalidInt,
    (PRAvailableFN)_PR_InvalidInt,
    (PRAvailable64FN)_PR_InvalidInt64,
    (PRFsyncFN)_PR_InvalidStatus,
    (PRSeekFN)_PR_InvalidInt,
    (PRSeek64FN)_PR_InvalidInt64,
    (PRFileInfoFN)_PR_InvalidStatus,
    (PRFileInfo64FN)_PR_InvalidStatus,
    (PRWritevFN)_PR_InvalidInt,        
    (PRConnectFN)_PR_InvalidStatus,        
    (PRAcceptFN)_PR_InvalidDesc,        
    (PRBindFN)_PR_InvalidStatus,        
    (PRListenFN)_PR_InvalidStatus,        
    (PRShutdownFN)_PR_InvalidStatus,    
    (PRRecvFN)_PR_InvalidInt,        
    (PRSendFN)_PR_InvalidInt,        
    (PRRecvfromFN)_PR_InvalidInt,    
    (PRSendtoFN)_PR_InvalidInt,        
    _pr_PolEvtPoll,
    (PRAcceptreadFN)_PR_InvalidInt,   
    (PRTransmitfileFN)_PR_InvalidInt, 
    (PRGetsocknameFN)_PR_InvalidStatus,    
    (PRGetpeernameFN)_PR_InvalidStatus,    
    (PRReservedFN)_PR_InvalidInt,    
    (PRReservedFN)_PR_InvalidInt,    
    (PRGetsocketoptionFN)_PR_InvalidStatus,
    (PRSetsocketoptionFN)_PR_InvalidStatus,
    (PRSendfileFN)_PR_InvalidInt, 
    (PRConnectcontinueFN)_PR_InvalidStatus, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt
};

static PRDescIdentity _pr_polevt_id;
static PRCallOnceType _pr_polevt_once_control;
static PRStatus PR_CALLBACK _pr_PolEvtInit(void);

static PRInt16 PR_CALLBACK _pr_PolEvtPoll(
    PRFileDesc *fd, PRInt16 in_flags, PRInt16 *out_flags)
{
    return (fd->lower->methods->poll)(fd->lower, in_flags, out_flags);
}

static PRStatus PR_CALLBACK _pr_PolEvtInit(void)
{
    _pr_polevt_id = PR_GetUniqueIdentity("NSPR pollable events");
    if (PR_INVALID_IO_LAYER == _pr_polevt_id) {
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}

#if !defined(XP_UNIX)
#define USE_TCP_SOCKETPAIR
#endif

PR_IMPLEMENT(PRFileDesc *) PR_NewPollableEvent(void)
{
    PRFileDesc *event;
    PRFileDesc *fd[2]; /* fd[0] is the read end; fd[1] is the write end */
#ifdef USE_TCP_SOCKETPAIR
    PRSocketOptionData socket_opt;
    PRStatus rv;
#endif

    fd[0] = fd[1] = NULL;

    if (PR_CallOnce(&_pr_polevt_once_control, _pr_PolEvtInit) == PR_FAILURE) {
        return NULL;
    }

    event = PR_CreateIOLayerStub(_pr_polevt_id, &_pr_polevt_methods);
    if (NULL == event) {
        goto errorExit;
    } 
    event->secret = PR_NEW(PRFilePrivate);
    if (event->secret == NULL) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        goto errorExit;
    }

#ifndef USE_TCP_SOCKETPAIR
    if (PR_CreatePipe(&fd[0], &fd[1]) == PR_FAILURE) {
        fd[0] = fd[1] = NULL;
        goto errorExit;
    }
#else
    if (PR_NewTCPSocketPair(fd) == PR_FAILURE) {
        fd[0] = fd[1] = NULL;
        goto errorExit;
    }
	/*
	 * set the TCP_NODELAY option to reduce notification latency
	 */
    socket_opt.option = PR_SockOpt_NoDelay;
    socket_opt.value.no_delay = PR_TRUE;
    rv = PR_SetSocketOption(fd[1], &socket_opt);
    PR_ASSERT(PR_SUCCESS == rv);
#endif

    event->secret->writeEnd = fd[1];
    if (PR_PushIOLayer(fd[0], PR_TOP_IO_LAYER, event) == PR_FAILURE) {
        goto errorExit;
    }

    return fd[0];

errorExit:
    if (fd[0]) {
        PR_Close(fd[0]);
        PR_Close(fd[1]);
    }
    if (event) {
        PR_DELETE(event->secret);
        event->dtor(event);
    }
    return NULL;
}

static PRStatus PR_CALLBACK _pr_PolEvtClose(PRFileDesc *fd)
{
    PRFileDesc *event;

    event = PR_PopIOLayer(fd, PR_TOP_IO_LAYER);
    PR_ASSERT(NULL == event->higher && NULL == event->lower);
    PR_Close(fd);
    PR_Close(event->secret->writeEnd);
    PR_DELETE(event->secret);
    event->dtor(event);
    return PR_SUCCESS;
}

PR_IMPLEMENT(PRStatus) PR_DestroyPollableEvent(PRFileDesc *event)
{
    return PR_Close(event);
}

static const char magicChar = '\x38';

PR_IMPLEMENT(PRStatus) PR_SetPollableEvent(PRFileDesc *event)
{
    if (PR_Write(event->secret->writeEnd, &magicChar, 1) != 1) {
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}

PR_IMPLEMENT(PRStatus) PR_WaitForPollableEvent(PRFileDesc *event)
{
    char buf[1024];
    PRInt32 nBytes;
#ifdef DEBUG
    PRIntn i;
#endif

    nBytes = PR_Read(event->lower, buf, sizeof(buf));
    if (nBytes == -1) {
        return PR_FAILURE;
    }

#ifdef DEBUG
    /*
     * Make sure people do not write to the pollable event fd
     * directly.
     */
    for (i = 0; i < nBytes; i++) {
        PR_ASSERT(buf[i] == magicChar);
    }
#endif

    return PR_SUCCESS;
}

#endif /* VMS */
