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
** File:        prlayer.c
** Description: Routines for handling pushable protocol modules on sockets.
*/

#include "primpl.h"
#include "prerror.h"
#include "prmem.h"
#include "prlock.h"
#include "prlog.h"
#include "prio.h"

#include <string.h> /* for memset() */

void PR_CALLBACK pl_FDDestructor(PRFileDesc *fd)
{
    PR_ASSERT(fd != NULL);
    if (NULL != fd->lower) fd->lower->higher = fd->higher;
    if (NULL != fd->higher) fd->higher->lower = fd->lower;
    PR_DELETE(fd);
}

/*
** Default methods that just call down to the next fd.
*/
static PRStatus PR_CALLBACK pl_TopClose (PRFileDesc *fd)
{
    PRStatus status;
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);
    PR_ASSERT(fd->secret == NULL);
    PR_ASSERT(fd->methods->file_type == PR_DESC_LAYERED);

    status = (fd->lower->methods->close)(fd->lower);
    fd->lower = fd->higher = NULL;

    fd->dtor(fd);
    return status;
}

static PRInt32 PR_CALLBACK pl_DefRead (PRFileDesc *fd, void *buf, PRInt32 amount)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->read)(fd->lower, buf, amount);
}

static PRInt32 PR_CALLBACK pl_DefWrite (
    PRFileDesc *fd, const void *buf, PRInt32 amount)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->write)(fd->lower, buf, amount);
}

static PRInt32 PR_CALLBACK pl_DefAvailable (PRFileDesc *fd)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->available)(fd->lower);
}

static PRInt64 PR_CALLBACK pl_DefAvailable64 (PRFileDesc *fd)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->available64)(fd->lower);
}

static PRStatus PR_CALLBACK pl_DefFsync (PRFileDesc *fd)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->fsync)(fd->lower);
}

static PRInt32 PR_CALLBACK pl_DefSeek (
    PRFileDesc *fd, PRInt32 offset, PRSeekWhence how)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->seek)(fd->lower, offset, how);
}

static PRInt64 PR_CALLBACK pl_DefSeek64 (
    PRFileDesc *fd, PRInt64 offset, PRSeekWhence how)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->seek64)(fd->lower, offset, how);
}

static PRStatus PR_CALLBACK pl_DefFileInfo (PRFileDesc *fd, PRFileInfo *info)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->fileInfo)(fd->lower, info);
}

static PRStatus PR_CALLBACK pl_DefFileInfo64 (PRFileDesc *fd, PRFileInfo64 *info)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->fileInfo64)(fd->lower, info);
}

static PRInt32 PR_CALLBACK pl_DefWritev (PRFileDesc *fd, const PRIOVec *iov,
    PRInt32 size, PRIntervalTime timeout)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->writev)(fd->lower, iov, size, timeout);
}

static PRStatus PR_CALLBACK pl_DefConnect (
    PRFileDesc *fd, const PRNetAddr *addr, PRIntervalTime timeout)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->connect)(fd->lower, addr, timeout);
}

static PRFileDesc* PR_CALLBACK pl_TopAccept (
    PRFileDesc *fd, PRNetAddr *addr, PRIntervalTime timeout)
{
    PRStatus rv;
    PRFileDesc *newfd;
    PRFileDesc *newstack;

    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    newstack = PR_NEW(PRFileDesc);
    if (NULL == newstack)
    {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        return NULL;
    }
    *newstack = *fd;  /* make a copy of the accepting layer */

    newfd = (fd->lower->methods->accept)(fd->lower, addr, timeout);
    if (NULL == newfd)
    {
        PR_DELETE(newstack);
        return NULL;
    }

    /* this PR_PushIOLayer call cannot fail */
    rv = PR_PushIOLayer(newfd, PR_TOP_IO_LAYER, newstack);
    PR_ASSERT(PR_SUCCESS == rv);
    return newfd;  /* that's it */
}

static PRStatus PR_CALLBACK pl_DefBind (PRFileDesc *fd, const PRNetAddr *addr)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->bind)(fd->lower, addr);
}

static PRStatus PR_CALLBACK pl_DefListen (PRFileDesc *fd, PRIntn backlog)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->listen)(fd->lower, backlog);
}

static PRStatus PR_CALLBACK pl_DefShutdown (PRFileDesc *fd, PRIntn how)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->shutdown)(fd->lower, how);
}

static PRInt32 PR_CALLBACK pl_DefRecv (
    PRFileDesc *fd, void *buf, PRInt32 amount,
    PRIntn flags, PRIntervalTime timeout)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->recv)(
        fd->lower, buf, amount, flags, timeout);
}

static PRInt32 PR_CALLBACK pl_DefSend (
    PRFileDesc *fd, const void *buf,
    PRInt32 amount, PRIntn flags, PRIntervalTime timeout)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->send)(fd->lower, buf, amount, flags, timeout);
}

static PRInt32 PR_CALLBACK pl_DefRecvfrom (
    PRFileDesc *fd, void *buf, PRInt32 amount,
    PRIntn flags, PRNetAddr *addr, PRIntervalTime timeout)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->recvfrom)(
        fd->lower, buf, amount, flags, addr, timeout);
}

static PRInt32 PR_CALLBACK pl_DefSendto (
    PRFileDesc *fd, const void *buf, PRInt32 amount, PRIntn flags,
    const PRNetAddr *addr, PRIntervalTime timeout)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->sendto)(
        fd->lower, buf, amount, flags, addr, timeout);
}

static PRInt16 PR_CALLBACK pl_DefPoll (
    PRFileDesc *fd, PRInt16 in_flags, PRInt16 *out_flags)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->poll)(fd->lower, in_flags, out_flags);
}

static PRInt32 PR_CALLBACK pl_DefAcceptread (
    PRFileDesc *sd, PRFileDesc **nd, PRNetAddr **raddr, void *buf,
    PRInt32 amount, PRIntervalTime t)
{
    PRInt32 nbytes;
    PRStatus rv;
    PRFileDesc *newstack;

    PR_ASSERT(sd != NULL);
    PR_ASSERT(sd->lower != NULL);

    newstack = PR_NEW(PRFileDesc);
    if (NULL == newstack)
    {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        return -1;
    }
    *newstack = *sd;  /* make a copy of the accepting layer */

    nbytes = sd->lower->methods->acceptread(
        sd->lower, nd, raddr, buf, amount, t);
    if (-1 == nbytes)
    {
        PR_DELETE(newstack);
        return nbytes;
    }

    /* this PR_PushIOLayer call cannot fail */
    rv = PR_PushIOLayer(*nd, PR_TOP_IO_LAYER, newstack);
    PR_ASSERT(PR_SUCCESS == rv);
    return nbytes;
}

static PRInt32 PR_CALLBACK pl_DefTransmitfile (
    PRFileDesc *sd, PRFileDesc *fd, const void *headers, PRInt32 hlen,
    PRTransmitFileFlags flags, PRIntervalTime t)
{
    PR_ASSERT(sd != NULL);
    PR_ASSERT(sd->lower != NULL);

    return sd->lower->methods->transmitfile(
        sd->lower, fd, headers, hlen, flags, t);
}

static PRStatus PR_CALLBACK pl_DefGetsockname (PRFileDesc *fd, PRNetAddr *addr)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->getsockname)(fd->lower, addr);
}

static PRStatus PR_CALLBACK pl_DefGetpeername (PRFileDesc *fd, PRNetAddr *addr)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->getpeername)(fd->lower, addr);
}

static PRStatus PR_CALLBACK pl_DefGetsockopt (
    PRFileDesc *fd, PRSockOption optname, void* optval, PRInt32* optlen)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->getsockopt)(fd->lower, optname, optval, optlen);
}

static PRStatus PR_CALLBACK pl_DefSetsockopt (
    PRFileDesc *fd, PRSockOption optname, const void* optval, PRInt32 optlen)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->setsockopt)(fd->lower, optname, optval, optlen);
}

static PRStatus PR_CALLBACK pl_DefGetsocketoption (
    PRFileDesc *fd, PRSocketOptionData *data)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->getsocketoption)(fd->lower, data);
}

static PRStatus PR_CALLBACK pl_DefSetsocketoption (
    PRFileDesc *fd, const PRSocketOptionData *data)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->setsocketoption)(fd->lower, data);
}

/* Methods for the top of the stack.  Just call down to the next fd. */
static PRIOMethods pl_methods = {
    PR_DESC_LAYERED,
    pl_TopClose,
    pl_DefRead,
    pl_DefWrite,
    pl_DefAvailable,
    pl_DefAvailable64,
    pl_DefFsync,
    pl_DefSeek,
    pl_DefSeek64,
    pl_DefFileInfo,
    pl_DefFileInfo64,
    pl_DefWritev,
    pl_DefConnect,
    pl_TopAccept,
    pl_DefBind,
    pl_DefListen,
    pl_DefShutdown,
    pl_DefRecv,
    pl_DefSend,
    pl_DefRecvfrom,
    pl_DefSendto,
    pl_DefPoll,
    pl_DefAcceptread,
    pl_DefTransmitfile,
    pl_DefGetsockname,
    pl_DefGetpeername,
    pl_DefGetsockopt,
    pl_DefSetsockopt,
    pl_DefGetsocketoption,
    pl_DefSetsocketoption
};

PR_IMPLEMENT(const PRIOMethods*) PR_GetDefaultIOMethods()
{
    return &pl_methods;
}  /* PR_GetDefaultIOMethods */

PR_IMPLEMENT(PRFileDesc*) PR_CreateIOLayerStub(
    PRDescIdentity ident, const PRIOMethods *methods)
{
    PRFileDesc *fd = NULL;
    PR_ASSERT((PR_NSPR_IO_LAYER != ident) && (PR_TOP_IO_LAYER != ident));
    if ((PR_NSPR_IO_LAYER == ident) || (PR_TOP_IO_LAYER == ident))
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    else
    {
        fd = PR_NEWZAP(PRFileDesc);
        if (NULL == fd)
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        else
        {
            fd->methods = methods;
            fd->dtor = pl_FDDestructor;
            fd->identity = ident;
        }
    }
    return fd;
}  /* PR_CreateIOLayerStub */

PR_IMPLEMENT(PRStatus) PR_PushIOLayer(
    PRFileDesc *stack, PRDescIdentity id, PRFileDesc *fd)
{
    PRFileDesc *insert = PR_GetIdentitiesLayer(stack, id);

    PR_ASSERT(fd != NULL);
    PR_ASSERT(stack != NULL);
    PR_ASSERT(insert != NULL);
    if ((NULL == stack) || (NULL == fd) || (NULL == insert))
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }

    if (stack == insert)
    {
        /* going on top of the stack */
        PRFileDesc copy = *stack;
        *stack = *fd;
        *fd = copy;
        fd->higher = stack;
        stack->lower = fd;
        stack->higher = NULL;
    }
    else
    {
        /* going somewhere in the middle of the stack */
        fd->lower = insert;
        fd->higher = insert->higher;

        insert->higher = fd;
        insert->higher->lower = fd;
    }

    return PR_SUCCESS;
}

PR_IMPLEMENT(PRFileDesc*) PR_PopIOLayer(PRFileDesc *stack, PRDescIdentity id)
{
    PRFileDesc *extract = PR_GetIdentitiesLayer(stack, id);

    PR_ASSERT(0 != id);
    PR_ASSERT(NULL != stack);
    PR_ASSERT(NULL != extract);
    if ((NULL == stack) || (0 == id) || (NULL == extract))
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return NULL;
    }

    if (extract == stack)
    {
        /* popping top layer of the stack */
        PRFileDesc copy = *stack;
        extract = stack->lower;
        *stack = *extract;
        *extract = copy;
        stack->higher = NULL;
    }
    else
    {
        extract->lower->higher = extract->higher;
        extract->higher->lower = extract->lower;
    }
    extract->higher = extract->lower = NULL;
    return extract;
}  /* PR_PopIOLayer */

#define ID_CACHE_INCREMENT 16
typedef struct _PRIdentity_cache
{
    PRLock *ml;
    char **name;
    PRIntn length;
    PRDescIdentity ident;
} _PRIdentity_cache;

static _PRIdentity_cache identity_cache;

PR_IMPLEMENT(PRDescIdentity) PR_GetUniqueIdentity(const char *layer_name)
{
    PRDescIdentity identity, length;
    char **names = NULL, *name = NULL, **old = NULL;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    PR_ASSERT((PRDescIdentity)0x7fff > identity_cache.ident);

    if (NULL != layer_name)
    {
        name = (char*)PR_Malloc(strlen(layer_name) + 1);
        if (NULL == name)
        {
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
            return PR_INVALID_IO_LAYER;
        }
        strcpy(name, layer_name);
    }

    /* this initial code runs unsafe */
retry:
    PR_ASSERT(NULL == names);
    length = identity_cache.length;
    if (length < (identity_cache.ident + 1))
    {
        length += ID_CACHE_INCREMENT;
        names = (char**)PR_CALLOC(length * sizeof(char*));
        if (NULL == names)
        {
            if (NULL != name) PR_DELETE(name);
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
            return PR_INVALID_IO_LAYER;
        }
    }

    /* now we get serious about thread safety */
    PR_Lock(identity_cache.ml);
    PR_ASSERT(identity_cache.ident <= identity_cache.length);
    identity = identity_cache.ident + 1;
    if (identity > identity_cache.length)  /* there's no room */
    {
        /* we have to do something - hopefully it's already done */
        if ((NULL != names) && (length >= identity))
        {
            /* what we did is still okay */
            memcpy(
                names, identity_cache.name,
                identity_cache.length * sizeof(char*));
            old = identity_cache.name;
            identity_cache.name = names;
            identity_cache.length = length;
            names = NULL;
        }
        else
        {
            PR_ASSERT(identity_cache.ident <= identity_cache.length);
            PR_Unlock(identity_cache.ml);
            if (NULL != names) PR_DELETE(names);
            goto retry;
        }
    }
    if (NULL != name) /* there's a name to be stored */
    {
        identity_cache.name[identity] = name;
    }
    identity_cache.ident = identity;
    PR_ASSERT(identity_cache.ident <= identity_cache.length);
    PR_Unlock(identity_cache.ml);

    if (NULL != old) PR_DELETE(old);
    if (NULL != names) PR_DELETE(names);

    return identity;
}  /* PR_GetUniqueIdentity */

PR_IMPLEMENT(const char*) PR_GetNameForIdentity(PRDescIdentity ident)
{
    if (!_pr_initialized) _PR_ImplicitInitialization();

    if (PR_TOP_IO_LAYER == ident) return NULL;

    PR_ASSERT(ident <= identity_cache.ident);
    return (ident > identity_cache.ident) ? NULL : identity_cache.name[ident];
}  /* PR_GetNameForIdentity */

PR_IMPLEMENT(PRDescIdentity) PR_GetLayersIdentity(PRFileDesc* fd)
{
    PR_ASSERT(NULL != fd);
    return fd->identity;
}  /* PR_GetLayersIdentity */

PR_IMPLEMENT(PRFileDesc*) PR_GetIdentitiesLayer(PRFileDesc* fd, PRDescIdentity id)
{
    PRFileDesc *layer = fd;

    if (PR_TOP_IO_LAYER == id) return fd;

    for (layer = fd; layer != NULL; layer = layer->lower)
    {
        if (id == layer->identity) return layer;
    }
    for (layer = fd; layer != NULL; layer = layer->higher)
    {
        if (id == layer->identity) return layer;
    }
    return NULL;
}  /* PR_GetIdentitiesLayer */

void _PR_InitLayerCache()
{
    memset(&identity_cache, 0, sizeof(identity_cache));
    identity_cache.ml = PR_NewLock();
    PR_ASSERT(NULL != identity_cache.ml);
}  /* _PR_InitLayerCache */

/* prlayer.c */
