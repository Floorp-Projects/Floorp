/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
