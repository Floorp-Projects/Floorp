/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 *********************************************************************
 *
 * Pollable events
 *
 *********************************************************************
 */

#include "primpl.h"

typedef struct MyFilePrivate {
    PRFilePrivate copy;
    PRFileDesc *writeEnd;
    PRFilePrivate *oldSecret;
} MyFilePrivate;

#ifndef XP_UNIX
#define USE_TCP_SOCKETPAIR
#endif

PR_IMPLEMENT(PRFileDesc *) PR_NewPollableEvent(void)
{
    PRFileDesc *fd[2]; /* fd[0] is the read end; fd[1] is the write end */
    MyFilePrivate *secret;

    secret = PR_NEW(MyFilePrivate);
    if (secret == NULL) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        goto errorExit;
    }

#ifndef USE_TCP_SOCKETPAIR
    if (PR_CreatePipe(&fd[0], &fd[1]) == PR_FAILURE) {
        goto errorExit;
    }
#else
    if (PR_NewTCPSocketPair(fd) == PR_FAILURE) {
        goto errorExit;
    }
#endif

    secret->copy = *fd[0]->secret;
    secret->oldSecret = fd[0]->secret;
    secret->writeEnd = fd[1];
    fd[0]->secret = (PRFilePrivate *) secret;

    return fd[0];

errorExit:
    PR_DELETE(secret);
    return NULL;
}

PR_IMPLEMENT(PRStatus) PR_DestroyPollableEvent(PRFileDesc *event)
{
    MyFilePrivate *secret;

    secret = (MyFilePrivate *) event->secret;
    event->secret = secret->oldSecret;
    PR_Close(event);
    PR_Close(secret->writeEnd);
    PR_DELETE(secret);
    return PR_SUCCESS;
}

static const char magicChar = '\x38';

PR_IMPLEMENT(PRStatus) PR_SetPollableEvent(PRFileDesc *event)
{
    MyFilePrivate *secret;

    secret = (MyFilePrivate *) event->secret;
    if (PR_Write(secret->writeEnd, &magicChar, 1) != 1) {
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

    nBytes = PR_Read(event, buf, sizeof(buf));
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
