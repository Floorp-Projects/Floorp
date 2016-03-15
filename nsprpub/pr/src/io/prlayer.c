/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
static PRStatus _PR_DestroyIOLayer(PRFileDesc *stack);

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
    PRFileDesc *top, *lower;
	PRStatus rv;

    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);
    PR_ASSERT(fd->secret == NULL);
    PR_ASSERT(fd->methods->file_type == PR_DESC_LAYERED);

	if (PR_IO_LAYER_HEAD == fd->identity) {
		/*
		 * new style stack; close all the layers, before deleting the
		 * stack head
		 */
		rv = fd->lower->methods->close(fd->lower);
		_PR_DestroyIOLayer(fd);
		return rv;
	} else if ((fd->higher) && (PR_IO_LAYER_HEAD == fd->higher->identity)) {
		/*
		 * lower layers of new style stack
		 */
		lower = fd->lower;
		/*
		 * pop and cleanup current layer
		 */
    	top = PR_PopIOLayer(fd->higher, PR_TOP_IO_LAYER);
		top->dtor(top);
		/*
		 * then call lower layer
		 */
		return (lower->methods->close(lower));
	} else {
		/* old style stack */
    	top = PR_PopIOLayer(fd, PR_TOP_IO_LAYER);
		top->dtor(top);
		return (fd->methods->close)(fd);
	}
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

static PRStatus PR_CALLBACK pl_DefConnectcontinue (
    PRFileDesc *fd, PRInt16 out_flags)
{
    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

    return (fd->lower->methods->connectcontinue)(fd->lower, out_flags);
}

static PRFileDesc* PR_CALLBACK pl_TopAccept (
    PRFileDesc *fd, PRNetAddr *addr, PRIntervalTime timeout)
{
    PRStatus rv;
    PRFileDesc *newfd, *layer = fd;
    PRFileDesc *newstack;
	PRBool newstyle_stack = PR_FALSE;

    PR_ASSERT(fd != NULL);
    PR_ASSERT(fd->lower != NULL);

	/* test for new style stack */
	while (NULL != layer->higher)
		layer = layer->higher;
	newstyle_stack = (PR_IO_LAYER_HEAD == layer->identity) ? PR_TRUE : PR_FALSE;
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

    if (newstyle_stack) {
		newstack->lower = newfd;
		newfd->higher = newstack;
		return newstack;
	} else {
		/* this PR_PushIOLayer call cannot fail */
		rv = PR_PushIOLayer(newfd, PR_TOP_IO_LAYER, newstack);
		PR_ASSERT(PR_SUCCESS == rv);
    	return newfd;  /* that's it */
	}
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
    PRFileDesc *layer = sd;
	PRBool newstyle_stack = PR_FALSE;

    PR_ASSERT(sd != NULL);
    PR_ASSERT(sd->lower != NULL);

	/* test for new style stack */
	while (NULL != layer->higher)
		layer = layer->higher;
	newstyle_stack = (PR_IO_LAYER_HEAD == layer->identity) ? PR_TRUE : PR_FALSE;
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
    if (newstyle_stack) {
		newstack->lower = *nd;
		(*nd)->higher = newstack;
		*nd = newstack;
		return nbytes;
	} else {
		/* this PR_PushIOLayer call cannot fail */
		rv = PR_PushIOLayer(*nd, PR_TOP_IO_LAYER, newstack);
		PR_ASSERT(PR_SUCCESS == rv);
		return nbytes;
	}
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

static PRInt32 PR_CALLBACK pl_DefSendfile (
	PRFileDesc *sd, PRSendFileData *sfd,
	PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    PR_ASSERT(sd != NULL);
    PR_ASSERT(sd->lower != NULL);

    return sd->lower->methods->sendfile(
        sd->lower, sfd, flags, timeout);
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
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt,
    pl_DefGetsocketoption,
    pl_DefSetsocketoption,
    pl_DefSendfile,
    pl_DefConnectcontinue,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt
};

PR_IMPLEMENT(const PRIOMethods*) PR_GetDefaultIOMethods(void)
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

/*
 * PR_CreateIOLayer
 *		Create a new style stack, where the stack top is a dummy header.
 *		Unlike the old style stacks, the contents of the stack head
 *		are not modified when a layer is pushed onto or popped from a new
 *		style stack.
 */

PR_IMPLEMENT(PRFileDesc*) PR_CreateIOLayer(PRFileDesc *top)
{
    PRFileDesc *fd = NULL;

	fd = PR_NEWZAP(PRFileDesc);
	if (NULL == fd)
		PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
	else
	{
		fd->methods = &pl_methods;
		fd->dtor = pl_FDDestructor;
		fd->identity = PR_IO_LAYER_HEAD;
		fd->higher = NULL;
		fd->lower = top;
		top->higher = fd;
		top->lower = NULL;
	}
    return fd;
}  /* PR_CreateIOLayer */

/*
 * _PR_DestroyIOLayer
 *		Delete the stack head of a new style stack.
 */

static PRStatus _PR_DestroyIOLayer(PRFileDesc *stack)
{
    if (NULL == stack)
        return PR_FAILURE;
    else {
        PR_DELETE(stack);
    	return PR_SUCCESS;
    }
}  /* _PR_DestroyIOLayer */

PR_IMPLEMENT(PRStatus) PR_PushIOLayer(
    PRFileDesc *stack, PRDescIdentity id, PRFileDesc *fd)
{
    PRFileDesc *insert = PR_GetIdentitiesLayer(stack, id);

    PR_ASSERT(fd != NULL);
    PR_ASSERT(stack != NULL);
    PR_ASSERT(insert != NULL);
    PR_ASSERT(PR_IO_LAYER_HEAD != id);
    if ((NULL == stack) || (NULL == fd) || (NULL == insert))
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }

    if (stack == insert)
    {
		/* going on top of the stack */
		/* old-style stack */	
		PRFileDesc copy = *stack;
		*stack = *fd;
		*fd = copy;
		fd->higher = stack;
		if (fd->lower)
		{
			PR_ASSERT(fd->lower->higher == stack);
			fd->lower->higher = fd;
		}
		stack->lower = fd;
		stack->higher = NULL;
	} else {
        /*
		 * going somewhere in the middle of the stack for both old and new
		 * style stacks, or going on top of stack for new style stack
		 */
        fd->lower = insert;
        fd->higher = insert->higher;

        insert->higher->lower = fd;
        insert->higher = fd;
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

    if (extract == stack) {
        /* popping top layer of the stack */
		/* old style stack */
        PRFileDesc copy = *stack;
        extract = stack->lower;
        *stack = *extract;
        *extract = copy;
        stack->higher = NULL;
        if (stack->lower) {
            PR_ASSERT(stack->lower->higher == extract);
            stack->lower->higher = stack;
        }
	} else if ((PR_IO_LAYER_HEAD == stack->identity) &&
					(extract == stack->lower) && (extract->lower == NULL)) {
			/*
			 * new style stack
			 * popping the only layer in the stack; delete the stack too
			 */
			stack->lower = NULL;
			_PR_DestroyIOLayer(stack);
	} else {
		/* for both kinds of stacks */
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
    /*
     * In the initial round, both identity_cache.ident and
     * identity_cache.length are 0, so (identity_cache.ident + 1) is greater
     * than length.  In later rounds, identity_cache.ident is always less
     * than length, so (identity_cache.ident + 1) can be equal to but cannot
     * be greater than length.
     */
    length = identity_cache.length;
    if ((identity_cache.ident + 1) >= length)
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
    PR_ASSERT(identity_cache.length == 0 ||
              identity_cache.ident < identity_cache.length);
    identity = identity_cache.ident + 1;
    if (identity >= identity_cache.length)  /* there's no room */
    {
        /* we have to do something - hopefully it's already done */
        if ((NULL != names) && (identity < length))
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
    PR_ASSERT(identity_cache.ident < identity_cache.length);
    PR_Unlock(identity_cache.ml);

    if (NULL != old) PR_DELETE(old);
    if (NULL != names) PR_DELETE(names);

    return identity;
}  /* PR_GetUniqueIdentity */

PR_IMPLEMENT(const char*) PR_GetNameForIdentity(PRDescIdentity ident)
{
    const char *rv = NULL;
    if (!_pr_initialized) _PR_ImplicitInitialization();

    if ((PR_TOP_IO_LAYER != ident) && (ident >= 0)) {
      PR_Lock(identity_cache.ml);
      PR_ASSERT(ident <= identity_cache.ident);
      rv = (ident > identity_cache.ident) ? NULL : identity_cache.name[ident];
      PR_Unlock(identity_cache.ml);
    }

    return rv;
}  /* PR_GetNameForIdentity */

PR_IMPLEMENT(PRDescIdentity) PR_GetLayersIdentity(PRFileDesc* fd)
{
    PR_ASSERT(NULL != fd);
    if (PR_IO_LAYER_HEAD == fd->identity) {
    	PR_ASSERT(NULL != fd->lower);
    	return fd->lower->identity;
	} else
    	return fd->identity;
}  /* PR_GetLayersIdentity */

PR_IMPLEMENT(PRFileDesc*) PR_GetIdentitiesLayer(PRFileDesc* fd, PRDescIdentity id)
{
    PRFileDesc *layer = fd;

    if (PR_TOP_IO_LAYER == id) {
    	if (PR_IO_LAYER_HEAD == fd->identity)
			return fd->lower;
		else 
			return fd;
	}

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

void _PR_InitLayerCache(void)
{
    memset(&identity_cache, 0, sizeof(identity_cache));
    identity_cache.ml = PR_NewLock();
    PR_ASSERT(NULL != identity_cache.ml);
}  /* _PR_InitLayerCache */

void _PR_CleanupLayerCache(void)
{
    if (identity_cache.ml)
    {
        PR_DestroyLock(identity_cache.ml);
        identity_cache.ml = NULL;
    }

    if (identity_cache.name)
    {
        PRDescIdentity ident;

        for (ident = 0; ident <= identity_cache.ident; ident++)
            PR_DELETE(identity_cache.name[ident]);

        PR_DELETE(identity_cache.name);
    }
}  /* _PR_CleanupLayerCache */

/* prlayer.c */
