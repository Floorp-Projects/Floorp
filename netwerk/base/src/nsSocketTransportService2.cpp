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

#include "nsSocketTransportService2.h"
#include "nsSocketTransport2.h"
#include "nsPrintfCString.h"
#include "nsReadableUtils.h"
#include "nsAutoLock.h"
#include "nsNetError.h"
#include "prlock.h"
#include "prlog.h"
#include "prerror.h"
#include "plstr.h"

#if defined(PR_LOGGING)
PRLogModuleInfo *gSocketTransportLog = nsnull;
#endif

nsSocketTransportService *gSocketTransportService = nsnull;
PRThread                 *gSocketThread           = nsnull;

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
nsSocketTransportService::PostEvent(nsISocketEventHandler *handler,
                                    PRUint32 type, PRUint32 uparam,
                                    void *vparam)
{
    LOG(("nsSocketTransportService::PostEvent [handler=%x type=%u u=%x v=%x]\n",
        handler, type, uparam, vparam));

    NS_ASSERTION(handler, "null handler");

    nsAutoLock lock(mEventQLock);
    NS_ENSURE_TRUE(mInitialized, NS_ERROR_OFFLINE);

    SocketEvent *event = new SocketEvent(handler, type, uparam, vparam);
    if (!event)
        return NS_ERROR_OUT_OF_MEMORY;

    if (mEventQ.mTail)
        mEventQ.mTail->mNext = event;
    mEventQ.mTail = event;
    if (!mEventQ.mHead)
        mEventQ.mHead = event;

    PR_SetPollableEvent(mThreadEvent);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// socket api (socket thread only)

nsresult
nsSocketTransportService::AttachSocket(PRFileDesc *fd, nsASocketHandler *handler)
{
    LOG(("nsSocketTransportService::AttachSocket [handler=%x]\n", handler));

    NS_ENSURE_TRUE(mServicingEventQ, NS_ERROR_UNEXPECTED);
    NS_ENSURE_TRUE(mIdleCount < NS_SOCKET_MAX_COUNT, NS_ERROR_UNEXPECTED);

    SocketContext *sock = &mIdleList[mIdleCount];
    sock->mFD = fd;
    sock->mHandler = handler;
    NS_ADDREF(handler);

    mIdleCount++;
    return NS_OK;
}

nsresult
nsSocketTransportService::DetachSocket_Internal(SocketContext *sock)
{
    LOG(("nsSocketTransportService::DetachSocket_Internal [handler=%x]\n", sock->mHandler));

    // inform the handler that this socket is going away
    sock->mHandler->OnSocketDetached(sock->mFD);

    // cleanup
    sock->mFD = nsnull;
    NS_RELEASE(sock->mHandler);

    // find out what list this is on...
    PRInt32 index = sock - mActiveList;
    if (index > 0 && index <= NS_SOCKET_MAX_COUNT)
        RemoveFromPollList(sock);
    else
        RemoveFromIdleList(sock);

    // NOTE: sock is now an invalid pointer
    return NS_OK;
}

nsresult
nsSocketTransportService::AddToPollList(SocketContext *sock)
{
    LOG(("nsSocketTransportService::AddToPollList [handler=%x]\n", sock->mHandler));

    NS_ASSERTION(mActiveCount < NS_SOCKET_MAX_COUNT, "too many active sockets");

    memcpy(&mActiveList[++mActiveCount], sock, sizeof(SocketContext));
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
    NS_ASSERTION(index > 0 && index <= NS_SOCKET_MAX_COUNT, "invalid index");

    LOG(("  index=%u mActiveCount=%u\n", index, mActiveCount));

    if (index != mActiveCount) {
        memcpy(&mActiveList[index], &mActiveList[mActiveCount], sizeof(SocketContext));
        memcpy(&mPollList[index], &mPollList[mActiveCount], sizeof(PRPollDesc));
    }
    mActiveCount--;

    LOG(("  active=%u idle=%u\n", mActiveCount, mIdleCount));
}

nsresult
nsSocketTransportService::AddToIdleList(SocketContext *sock)
{
    LOG(("nsSocketTransportService::AddToIdleList [handler=%x]\n", sock->mHandler));

    NS_ASSERTION(mIdleCount < NS_SOCKET_MAX_COUNT, "too many idle sockets");
    memcpy(&mIdleList[mIdleCount], sock, sizeof(SocketContext));
    mIdleCount++;

    LOG(("  active=%u idle=%u\n", mActiveCount, mIdleCount));
    return NS_OK;
}

void
nsSocketTransportService::RemoveFromIdleList(SocketContext *sock)
{
    LOG(("nsSocketTransportService::RemoveFromIdleList [handler=%x]\n", sock->mHandler));

    PRUint32 index = sock - &mIdleList[0];
    NS_ASSERTION(index >= 0 && index < NS_SOCKET_MAX_COUNT, "invalid index");

    if (index != mIdleCount - 1)
        memcpy(&mActiveList[index], &mActiveList[mIdleCount - 1], sizeof(SocketContext));
    mIdleCount--;

    LOG(("  active=%u idle=%u\n", mActiveCount, mIdleCount));
}

//-----------------------------------------------------------------------------
// host:port -> ipaddr cache

PLDHashTableOps nsSocketTransportService::ops =
{
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    PL_DHashGetKeyStub,
    PL_DHashStringKey,
    nsSocketTransportService::MatchEntry,
    PL_DHashMoveEntryStub,
    nsSocketTransportService::ClearEntry,
    PL_DHashFinalizeStub,
    nsnull
};

PRBool PR_CALLBACK
nsSocketTransportService::MatchEntry(PLDHashTable *table,
                                     const PLDHashEntryHdr *entry,
                                     const void *key)
{
    const nsSocketTransportService::nsHostEntry *he =
        NS_REINTERPRET_CAST(const nsSocketTransportService::nsHostEntry *, entry);

    return !strcmp(he->hostport(), (const char *) key);
}

void PR_CALLBACK
nsSocketTransportService::ClearEntry(PLDHashTable *table,
                                     PLDHashEntryHdr *entry)
{
    nsSocketTransportService::nsHostEntry *he =
        NS_REINTERPRET_CAST(nsSocketTransportService::nsHostEntry *, entry);

    PL_strfree((char *) he->key);
    he->key = nsnull;
    memset(&he->addr, 0, sizeof(he->addr));
}

nsresult
nsSocketTransportService::LookupHost(const nsACString &host, PRUint16 port, PRIPv6Addr *addr)
{
    NS_ASSERTION(!host.IsEmpty(), "empty host");
    NS_ASSERTION(addr, "null addr");

    PLDHashEntryHdr *hdr;
    nsCAutoString hostport(host + nsPrintfCString(":%d", port));

    hdr = PL_DHashTableOperate(&mHostDB, hostport.get(), PL_DHASH_LOOKUP);
    if (PL_DHASH_ENTRY_IS_BUSY(hdr)) {
        // found match
        nsHostEntry *ent = NS_REINTERPRET_CAST(nsHostEntry *, hdr);
        memcpy(addr, &ent->addr, sizeof(ent->addr));
        return NS_OK;
    }

    return NS_ERROR_UNKNOWN_HOST;
}

nsresult
nsSocketTransportService::RememberHost(const nsACString &host, PRUint16 port, PRIPv6Addr *addr)
{
    // remember hostname

    PLDHashEntryHdr *hdr;
    nsCAutoString hostport(host + nsPrintfCString(":%d", port));

    hdr = PL_DHashTableOperate(&mHostDB, hostport.get(), PL_DHASH_ADD);
    if (!hdr)
        return NS_ERROR_FAILURE;

    NS_ASSERTION(PL_DHASH_ENTRY_IS_BUSY(hdr), "entry not busy");

    nsHostEntry *ent = NS_REINTERPRET_CAST(nsHostEntry *, hdr);
    if (ent->key == nsnull) {
        ent->key = (const void *) ToNewCString(hostport);
        memcpy(&ent->addr, addr, sizeof(ent->addr));
    }
#ifdef DEBUG
    else {
        // verify that the existing entry is in fact a perfect match
        NS_ASSERTION(PL_strcmp(ent->hostport(), hostport.get()) == 0, "bad match");
        NS_ASSERTION(memcmp(&ent->addr, addr, sizeof(ent->addr)) == 0, "bad match");
    }
#endif
    return NS_OK;
}

//-----------------------------------------------------------------------------
// xpcom api

NS_IMPL_THREADSAFE_ISUPPORTS2(nsSocketTransportService,
                              nsISocketTransportService,
                              nsIRunnable)

// called from main thread only
NS_IMETHODIMP
nsSocketTransportService::Init()
{
    NS_ASSERTION(nsIThread::IsMainThread(), "wrong thread");

    if (mInitialized)
        return NS_OK;

    if (!mThreadEvent)
        mThreadEvent = PR_NewPollableEvent();

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

        PR_SetPollableEvent(mThreadEvent);
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
    // Initialize hostname database
    //
    PL_DHashTableInit(&mHostDB, &ops, nsnull, sizeof(nsHostEntry), 0);

    //
    // setup thread
    //
    NS_ASSERTION(mThreadEvent, "no thread event");

    mPollList[0].fd = mThreadEvent;
    mPollList[0].in_flags = PR_POLL_READ;

    PRInt32 i, count;

    //
    // poll loop
    //
    PRBool active = PR_TRUE;
    while (active) {
        // clear out flags for pollable event
        mPollList[0].out_flags = 0;

        //
        // walk active list backwards to see if any sockets should actually be
        // idle, then walk the idle list backwards to see if any idle sockets
        // should become active.  take care to only check idle sockets that
        // were idle to begin with ;-)
        //
        count = mIdleCount;
        if (mActiveCount) {
            for (i=mActiveCount; i>=1; --i) {
                //---
                LOG(("  active [%d] { handler=%x condition=%x pollflags=%hu }\n", i,
                    mActiveList[i].mHandler,
                    mActiveList[i].mHandler->mCondition,
                    mActiveList[i].mHandler->mPollFlags));
                //---
                if (NS_FAILED(mActiveList[i].mHandler->mCondition))
                    DetachSocket_Internal(&mActiveList[i]);
                else {
                    PRUint16 in_flags = mActiveList[i].mHandler->mPollFlags;
                    if (in_flags == 0)
                        MoveToIdleList(&mActiveList[i]);
                    else {
                        // update poll flags
                        mPollList[i].in_flags = in_flags;
                        mPollList[i].out_flags = 0;
                    }
                }
            }
        }
        if (count) {
            for (i=count-1; i>=0; --i) {
                //---
                LOG(("  idle [%d] { handler=%x condition=%x pollflags=%hu }\n", i,
                    mIdleList[i].mHandler,
                    mIdleList[i].mHandler->mCondition,
                    mIdleList[i].mHandler->mPollFlags));
                //---
                if (NS_FAILED(mIdleList[i].mHandler->mCondition))
                    DetachSocket_Internal(&mIdleList[i]);
                else if (mIdleList[i].mHandler->mPollFlags != 0)
                    MoveToPollList(&mIdleList[i]);
            }
        }

        LOG(("  calling PR_Poll [active=%u idle=%u]\n", mActiveCount, mIdleCount));

        PRInt32 n = PR_Poll(mPollList, PollCount(), NS_SOCKET_POLL_TIMEOUT);
        if (n < 0) {
            LOG(("  PR_Poll error [%d]\n", PR_GetError()));
            active = PR_FALSE;
        }
        else if (n == 0) {
            LOG(("  PR_Poll timeout expired\n"));
            // loop around again
        }
        else {
            count = PollCount();

            //
            // service "active" sockets...
            //
            for (i=1; i<count; ++i) {
                if (mPollList[i].out_flags != 0) {
                    nsASocketHandler *handler = mActiveList[i].mHandler;
                    handler->OnSocketReady(mPollList[i].fd,
                                           mPollList[i].out_flags);
                }
            }

            //
            // check for "dead" sockets and remove them (need to do this in
            // reverse order obviously).
            //
            for (i=count-1; i>=1; --i) {
                if (NS_FAILED(mActiveList[i].mHandler->mCondition))
                    DetachSocket_Internal(&mActiveList[i]);
            }

            //
            // service the event queue
            //
            if (mPollList[0].out_flags == PR_POLL_READ) {

                // grab the event queue
                SocketEvent *head = nsnull, *event;
                {
                    nsAutoLock lock(mEventQLock);

                    // wait should not block
                    PR_WaitForPollableEvent(mPollList[0].fd);

                    head = mEventQ.mHead;
                    mEventQ.mHead = nsnull;
                    mEventQ.mTail = nsnull;

                    // check to see if we're supposed to shutdown
                    active = mInitialized;
                }
                // service the event queue
                mServicingEventQ = PR_TRUE;
                while (head) {
                    head->mHandler->OnSocketEvent(head->mType,
                                                  head->mUparam,
                                                  head->mVparam);
                    // delete head of queue
                    event = head->mNext;
                    delete head;
                    head = event;
                }
                mServicingEventQ = PR_FALSE;
            }
        }
    }

    //
    // shutdown thread
    //
    
    LOG(("shutting down socket transport thread...\n"));

    // detach any sockets
    for (i=mActiveCount; i>=1; --i)
        DetachSocket_Internal(&mActiveList[i]);
    for (i=mIdleCount-1; i>=0; --i)
        DetachSocket_Internal(&mIdleList[i]);

    // clear the hostname database
    PL_DHashTableFinish(&mHostDB);

    gSocketThread = nsnull;
    return NS_OK;
}
