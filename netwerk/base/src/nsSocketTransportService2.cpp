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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
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

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif

#include "nsSocketTransportService2.h"
#include "nsSocketTransport2.h"
#include "nsReadableUtils.h"
#include "nsAutoLock.h"
#include "nsNetError.h"
#include "prnetdb.h"
#include "prlock.h"
#include "prerror.h"
#include "plstr.h"

#if defined(PR_LOGGING)
PRLogModuleInfo *gSocketTransportLog = nsnull;
#endif

nsSocketTransportService *gSocketTransportService = nsnull;
PRThread                 *gSocketThread           = nsnull;

#define PLEVENT_FROM_LINK(_link) \
    ((PLEvent*) ((char*) (_link) - offsetof(PLEvent, link)))

static inline void
MoveCList(PRCList &from, PRCList &to)
{
    if (!PR_CLIST_IS_EMPTY(&from)) {
        to.next = from.next;
        to.prev = from.prev;
        to.next->prev = &to;
        to.prev->next = &to;
        PR_INIT_CLIST(&from);
    }             
}

//-----------------------------------------------------------------------------
// ctor/dtor (called on the main/UI thread by the service manager)

nsSocketTransportService::nsSocketTransportService()
    : mInitialized(PR_FALSE)
    , mThread(nsnull)
    , mThreadEvent(nsnull)
    , mAutodialEnabled(PR_FALSE)
    , mEventQLock(PR_NewLock())
    , mActiveCount(0)
    , mIdleCount(0)
{
#if defined(PR_LOGGING)
    gSocketTransportLog = PR_NewLogModule("nsSocketTransport");
#endif

    NS_ASSERTION(nsIThread::IsMainThread(), "wrong thread");

    PR_INIT_CLIST(&mEventQ);
    PR_INIT_CLIST(&mPendingSocketQ);

    NS_ASSERTION(!gSocketTransportService, "must not instantiate twice");
    gSocketTransportService = this;
}

nsSocketTransportService::~nsSocketTransportService()
{
    NS_ASSERTION(nsIThread::IsMainThread(), "wrong thread");
    NS_ASSERTION(!mInitialized, "not shutdown properly");

    PR_DestroyLock(mEventQLock);
    
    if (mThreadEvent)
        PR_DestroyPollableEvent(mThreadEvent);

    gSocketTransportService = nsnull;
}

//-----------------------------------------------------------------------------
// event queue (any thread)

NS_IMETHODIMP
nsSocketTransportService::PostEvent(PLEvent *event)
{
    LOG(("nsSocketTransportService::PostEvent [event=%p]\n", event));

    NS_ASSERTION(event, "null event");

    nsAutoLock lock(mEventQLock);
    if (!mInitialized)
        return NS_ERROR_OFFLINE;

    PR_APPEND_LINK(&event->link, &mEventQ);

    if (mThreadEvent)
        PR_SetPollableEvent(mThreadEvent);
    // else wait for Poll timeout
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::IsOnCurrentThread(PRBool *result)
{
    *result = (PR_GetCurrentThread() == gSocketThread);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// socket api (socket thread only)

nsresult
nsSocketTransportService::NotifyWhenCanAttachSocket(PLEvent *event)
{
    LOG(("nsSocketTransportService::NotifyWhenCanAttachSocket\n"));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    if (CanAttachSocket()) {
        NS_WARNING("should have called CanAttachSocket");
        return PostEvent(event);
    }

    PR_APPEND_LINK(&event->link, &mPendingSocketQ);
    return NS_OK;
}

nsresult
nsSocketTransportService::AttachSocket(PRFileDesc *fd, nsASocketHandler *handler)
{
    LOG(("nsSocketTransportService::AttachSocket [handler=%x]\n", handler));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    SocketContext sock;
    sock.mFD = fd;
    sock.mHandler = handler;

    nsresult rv = AddToIdleList(&sock);
    if (NS_SUCCEEDED(rv))
        NS_ADDREF(handler);
    return rv;
}

nsresult
nsSocketTransportService::DetachSocket(SocketContext *sock)
{
    LOG(("nsSocketTransportService::DetachSocket [handler=%x]\n", sock->mHandler));

    // inform the handler that this socket is going away
    sock->mHandler->OnSocketDetached(sock->mFD);

    // cleanup
    sock->mFD = nsnull;
    NS_RELEASE(sock->mHandler);

    // find out what list this is on.
    PRUint32 index = sock - mActiveList;
    if (index < NS_SOCKET_MAX_COUNT)
        RemoveFromPollList(sock);
    else
        RemoveFromIdleList(sock);

    // NOTE: sock is now an invalid pointer
    
    //
    // notify the first element on the pending socket queue...
    //
    if (!PR_CLIST_IS_EMPTY(&mPendingSocketQ)) {
        // move event from pending queue to event queue
        PLEvent *event = PLEVENT_FROM_LINK(PR_LIST_HEAD(&mPendingSocketQ));
        PR_REMOVE_AND_INIT_LINK(&event->link);
        PostEvent(event);
    }
    return NS_OK;
}

nsresult
nsSocketTransportService::AddToPollList(SocketContext *sock)
{
    LOG(("nsSocketTransportService::AddToPollList [handler=%x]\n", sock->mHandler));

    if (mActiveCount == NS_SOCKET_MAX_COUNT) {
        NS_ERROR("too many active sockets");
        return NS_ERROR_UNEXPECTED;
    }

    mActiveList[mActiveCount] = *sock;
    mActiveCount++;

    mPollList[mActiveCount].fd = sock->mFD;
    mPollList[mActiveCount].in_flags = sock->mHandler->mPollFlags;
    mPollList[mActiveCount].out_flags = 0;

    LOG(("  active=%u idle=%u\n", mActiveCount, mIdleCount));
    return NS_OK;
}

void
nsSocketTransportService::RemoveFromPollList(SocketContext *sock)
{
    LOG(("nsSocketTransportService::RemoveFromPollList [handler=%x]\n", sock->mHandler));

    PRUint32 index = sock - mActiveList;
    NS_ASSERTION(index < NS_SOCKET_MAX_COUNT, "invalid index");

    LOG(("  index=%u mActiveCount=%u\n", index, mActiveCount));

    if (index != mActiveCount-1) {
        mActiveList[index] = mActiveList[mActiveCount-1];
        mPollList[index+1] = mPollList[mActiveCount];
    }
    mActiveCount--;

    LOG(("  active=%u idle=%u\n", mActiveCount, mIdleCount));
}

nsresult
nsSocketTransportService::AddToIdleList(SocketContext *sock)
{
    LOG(("nsSocketTransportService::AddToIdleList [handler=%x]\n", sock->mHandler));

    if (mIdleCount == NS_SOCKET_MAX_COUNT) {
        NS_ERROR("too many idle sockets");
        return NS_ERROR_UNEXPECTED;
    }

    mIdleList[mIdleCount] = *sock;
    mIdleCount++;

    LOG(("  active=%u idle=%u\n", mActiveCount, mIdleCount));
    return NS_OK;
}

void
nsSocketTransportService::RemoveFromIdleList(SocketContext *sock)
{
    LOG(("nsSocketTransportService::RemoveFromIdleList [handler=%x]\n", sock->mHandler));

    PRUint32 index = sock - &mIdleList[0];
    NS_ASSERTION(index < NS_SOCKET_MAX_COUNT, "invalid index");

    if (index != mIdleCount-1)
        mIdleList[index] = mIdleList[mIdleCount-1];
    mIdleCount--;

    LOG(("  active=%u idle=%u\n", mActiveCount, mIdleCount));
}

void
nsSocketTransportService::MoveToIdleList(SocketContext *sock)
{
    nsresult rv = AddToIdleList(sock);
    if (NS_FAILED(rv))
        DetachSocket(sock);
    else
        RemoveFromPollList(sock);
}

void
nsSocketTransportService::MoveToPollList(SocketContext *sock)
{
    nsresult rv = AddToPollList(sock);
    if (NS_FAILED(rv))
        DetachSocket(sock);
    else
        RemoveFromIdleList(sock);
}

PRInt32
nsSocketTransportService::Poll()
{
    PRPollDesc *pollList;
    PRUint32 pollCount;
    PRIntervalTime pollTimeout;

    if (mPollList[0].fd) {
        mPollList[0].out_flags = 0;
        pollList = mPollList;
        pollCount = mActiveCount + 1;
        pollTimeout = NS_SOCKET_POLL_TIMEOUT;
    }
    else {
        // no pollable event, so busy wait...
        pollCount = mActiveCount;
        if (pollCount)
            pollList = &mPollList[1];
        else
            pollList = nsnull;
        pollTimeout = PR_MillisecondsToInterval(25);
    }

    return PR_Poll(pollList, pollCount, pollTimeout);
}

PRBool
nsSocketTransportService::ServiceEventQ()
{
    PRBool keepGoing;

    // grab the event queue
    PRCList eq;
    PR_INIT_CLIST(&eq);
    {
        nsAutoLock lock(mEventQLock);

        MoveCList(mEventQ, eq);

        // check to see if we're supposed to shutdown
        keepGoing = mInitialized;
    }
    // service the event queue
    PLEvent *event;
    while (!PR_CLIST_IS_EMPTY(&eq)) {
        event = PLEVENT_FROM_LINK(PR_LIST_HEAD(&eq));
        PR_REMOVE_AND_INIT_LINK(&event->link);

        PL_HandleEvent(event);
    }
    return keepGoing;
}


//-----------------------------------------------------------------------------
// xpcom api

NS_IMPL_THREADSAFE_ISUPPORTS3(nsSocketTransportService,
                              nsISocketTransportService,
                              nsIEventTarget,
                              nsIRunnable)

// called from main thread only
NS_IMETHODIMP
nsSocketTransportService::Init()
{
    NS_ASSERTION(nsIThread::IsMainThread(), "wrong thread");

    if (mInitialized)
        return NS_OK;

    if (!mThreadEvent) {
        mThreadEvent = PR_NewPollableEvent();
        //
        // NOTE: per bug 190000, this failure could be caused by Zone-Alarm
        // or similar software.
        //
        // NOTE: per bug 191739, this failure could also be caused by lack
        // of a loopback device on Windows and OS/2 platforms (NSPR creates
        // a loopback socket pair on these platforms to implement a pollable
        // event object).  if we can't create a pollable event, then we'll
        // have to "busy wait" to implement the socket event queue :-(
        //
        NS_WARN_IF_FALSE(mThreadEvent,
                "running socket transport thread without a pollable event");
    }

    nsresult rv = NS_NewThread(&mThread, this, 0, PR_JOINABLE_THREAD);
    if (NS_FAILED(rv)) return rv;

    mInitialized = PR_TRUE;
    return NS_OK;
}

// called from main thread only
NS_IMETHODIMP
nsSocketTransportService::Shutdown()
{
    LOG(("nsSocketTransportService::Shutdown\n"));

    NS_ASSERTION(nsIThread::IsMainThread(), "wrong thread");

    if (!mInitialized)
        return NS_OK;

    {
        nsAutoLock lock(mEventQLock);

        // signal uninitialized to block further events
        mInitialized = PR_FALSE;

        if (mThreadEvent)
            PR_SetPollableEvent(mThreadEvent);
        // else wait for Poll timeout
    }

    // join with thread
    mThread->Join();
    NS_RELEASE(mThread);

    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::CreateTransport(const char **types,
                                          PRUint32 typeCount,
                                          const nsACString &host,
                                          PRInt32 port,
                                          nsIProxyInfo *proxyInfo,
                                          nsISocketTransport **result)
{
    NS_ENSURE_TRUE(mInitialized, NS_ERROR_OFFLINE);
    NS_ENSURE_TRUE(port >= 0 && port <= 0xFFFF, NS_ERROR_ILLEGAL_VALUE);

    nsSocketTransport *trans = new nsSocketTransport();
    if (!trans)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(trans);

    nsresult rv = trans->Init(types, typeCount, host, port, proxyInfo);
    if (NS_FAILED(rv)) {
        NS_RELEASE(trans);
        return rv;
    }

    *result = trans;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::GetAutodialEnabled(PRBool *value)
{
    *value = mAutodialEnabled;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::SetAutodialEnabled(PRBool value)
{
    mAutodialEnabled = value;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::Run()
{
    LOG(("nsSocketTransportService::Run"));

    gSocketThread = PR_GetCurrentThread();

    //
    // add thread event to poll list (mThreadEvent may be NULL)
    //
    mPollList[0].fd = mThreadEvent;
    mPollList[0].in_flags = PR_POLL_READ;

    PRInt32 i, count;

    //
    // poll loop
    //
    PRBool active = PR_TRUE;
    while (active) {
        //
        // walk active list backwards to see if any sockets should actually be
        // idle, then walk the idle list backwards to see if any idle sockets
        // should become active.  take care to check only idle sockets that
        // were idle to begin with ;-)
        //
        count = mIdleCount;
        for (i=mActiveCount-1; i>=0; --i) {
            //---
            LOG(("  active [%u] { handler=%x condition=%x pollflags=%hu }\n", i,
                mActiveList[i].mHandler,
                mActiveList[i].mHandler->mCondition,
                mActiveList[i].mHandler->mPollFlags));
            //---
            if (NS_FAILED(mActiveList[i].mHandler->mCondition))
                DetachSocket(&mActiveList[i]);
            else {
                PRUint16 in_flags = mActiveList[i].mHandler->mPollFlags;
                if (in_flags == 0)
                    MoveToIdleList(&mActiveList[i]);
                else {
                    // update poll flags
                    mPollList[i+1].in_flags = in_flags;
                    mPollList[i+1].out_flags = 0;
                }
            }
        }
        for (i=count-1; i>=0; --i) {
            //---
            LOG(("  idle [%u] { handler=%x condition=%x pollflags=%hu }\n", i,
                mIdleList[i].mHandler,
                mIdleList[i].mHandler->mCondition,
                mIdleList[i].mHandler->mPollFlags));
            //---
            if (NS_FAILED(mIdleList[i].mHandler->mCondition))
                DetachSocket(&mIdleList[i]);
            else if (mIdleList[i].mHandler->mPollFlags != 0)
                MoveToPollList(&mIdleList[i]);
        }

        LOG(("  calling PR_Poll [active=%u idle=%u]\n", mActiveCount, mIdleCount));

        PRInt32 n = Poll();
        if (n < 0) {
            LOG(("  PR_Poll error [%d]\n", PR_GetError()));
            active = PR_FALSE;
        }
        else if (n > 0) {
            //
            // service "active" sockets...
            //
            for (i=0; i<PRInt32(mActiveCount); ++i) {
                PRPollDesc &desc = mPollList[i+1];
                if (desc.out_flags != 0) {
                    nsASocketHandler *handler = mActiveList[i].mHandler;
                    handler->OnSocketReady(desc.fd, desc.out_flags);
                }
            }

            //
            // check for "dead" sockets and remove them (need to do this in
            // reverse order obviously).
            //
            for (i=mActiveCount-1; i>=0; --i) {
                if (NS_FAILED(mActiveList[i].mHandler->mCondition))
                    DetachSocket(&mActiveList[i]);
            }

            //
            // service the event queue (mPollList[0].fd == mThreadEvent)
            //
            if (mPollList[0].out_flags == PR_POLL_READ) {
                // acknowledge pollable event (wait should not block)
                PR_WaitForPollableEvent(mThreadEvent);
                active = ServiceEventQ();
            }
        }
        else {
            LOG(("  PR_Poll timed out\n"));
            //
            // service event queue whenever PR_Poll times out.
            //
            active = ServiceEventQ();
        }
    }

    //
    // shutdown thread
    //
    
    LOG(("shutting down socket transport thread...\n"));

    // detach any sockets
    for (i=mActiveCount-1; i>=0; --i)
        DetachSocket(&mActiveList[i]);
    for (i=mIdleCount-1; i>=0; --i)
        DetachSocket(&mIdleList[i]);

    gSocketThread = nsnull;
    return NS_OK;
}
