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

#ifndef nsSocketTransportService2_h__
#define nsSocketTransportService2_h__

#include "nsISocketTransportService.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "pldhash.h"
#include "prinrval.h"
#include "prlog.h"
#include "prio.h"

//-----------------------------------------------------------------------------

#if defined(PR_LOGGING)
//
// set NSPR_LOG_MODULES=nsSocketTransport:5
//
extern PRLogModuleInfo *gSocketTransportLog;
#define LOG(args) PR_LOG(gSocketTransportLog, PR_LOG_DEBUG, args)
#else
#define LOG(args)
#endif

//-----------------------------------------------------------------------------

#define NS_SOCKET_MAX_COUNT    50
#define NS_SOCKET_POLL_TIMEOUT PR_INTERVAL_NO_TIMEOUT

//-----------------------------------------------------------------------------
// socket handler: methods are only called on the socket thread.

class nsASocketHandler : public nsISupports
{
public:
    nsASocketHandler() : mCondition(NS_OK), mPollFlags(0) {}

    //
    // this condition variable will be checked to determine if the socket
    // handler should be detached.  it must only be accessed on the socket
    // thread.
    //
    nsresult mCondition;

    //
    // these flags can only be modified on the socket transport thread.
    // the socket transport service will check these flags before calling
    // PR_Poll.
    //
    PRUint16 mPollFlags;

    //
    // called to service a socket
    // 
    // params:
    //   socketRef - socket identifier
    //   fd        - socket file descriptor
    //   pollFlags - poll "in-flags"
    //
    virtual void OnSocketReady(PRFileDesc *fd,
                               PRInt16 pollFlags) = 0;

    //
    // called when a socket is no longer under the control of the socket
    // transport service.  the socket handler may close the socket at this
    // point.  after this call returns, the handler will no longer be owned
    // by the socket transport service.
    //
    virtual void OnSocketDetached(PRFileDesc *fd) = 0;
};

//-----------------------------------------------------------------------------

class nsSocketTransportService : public nsISocketTransportService
                               , public nsIRunnable
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISOCKETTRANSPORTSERVICE
    NS_DECL_NSIRUNNABLE

    nsSocketTransportService();

    //
    // add a new socket to the list of controlled sockets.  returns a socket
    // reference for the newly attached socket that can be used with other
    // methods to control the socket.
    //
    // NOTE: this function may only be called from an event dispatch on the
    //       socket thread.
    //
    nsresult AttachSocket(PRFileDesc *fd, nsASocketHandler *);

    //
    // LookupHost checks to see if we've previously resolved the hostname
    // during this session.  We remember all successful connections to prevent
    // ip-address spoofing.  See bug 149943.
    //
    // Returns TRUE if found, and sets |addr| to the cached value.
    //
    nsresult LookupHost(const nsACString &host, PRUint16 port, PRIPv6Addr *addr);

    // 
    // Remember host:port -> IP address mapping.
    //
    nsresult RememberHost(const nsACString &host, PRUint16 port, PRIPv6Addr *addr);

private:

    virtual ~nsSocketTransportService();

    //-------------------------------------------------------------------------
    // misc (any thread)
    //-------------------------------------------------------------------------

    PRBool      mInitialized;
    nsIThread  *mThread;
    PRFileDesc *mThreadEvent;

    // pref to control autodial code
    PRBool      mAutodialEnabled;

    //-------------------------------------------------------------------------
    // event queue (any thread)
    //-------------------------------------------------------------------------

    struct SocketEvent
    {
        SocketEvent(nsISocketEventHandler *handler,
                    PRUint32 type, PRUint32 uparam, void *vparam)
            : mType(type)
            , mUparam(uparam)
            , mVparam(vparam)
            , mNext(nsnull)
            { NS_ADDREF(mHandler = handler); }

       ~SocketEvent() { NS_RELEASE(mHandler); }

        nsISocketEventHandler *mHandler;
        PRUint32               mType;
        PRUint32               mUparam;
        void                  *mVparam;

        struct SocketEvent    *mNext;
    };

    struct SocketEventQ;
    friend struct SocketEventQ;
    struct SocketEventQ
    {
        SocketEventQ() : mHead(nsnull), mTail(nsnull) {}

        SocketEvent *mHead;
        SocketEvent *mTail;
    };

    SocketEventQ  mEventQ;
    PRLock       *mEventQLock;
    PRBool        mServicingEventQ;

    //-------------------------------------------------------------------------
    // socket lists (socket thread only)
    //
    // only "active" sockets are on the poll list.  the active list is kept
    // in sync with the poll list such that the index of a socket on the poll
    // list matches its index on the active socket list.  as a result the first
    // element of the active socket list is always null.
    //-------------------------------------------------------------------------

    struct SocketContext
    {
        PRFileDesc       *mFD;
        nsASocketHandler *mHandler;
    };

    SocketContext mActiveList [ NS_SOCKET_MAX_COUNT + 1 ];
    SocketContext mIdleList   [ NS_SOCKET_MAX_COUNT     ];

    PRUint32 mActiveCount;
    PRUint32 mIdleCount;

    nsresult DetachSocket_Internal(SocketContext *);
    nsresult AddToIdleList(SocketContext *);
    nsresult AddToPollList(SocketContext *);
    void RemoveFromIdleList(SocketContext *);
    void RemoveFromPollList(SocketContext *);

    nsresult MoveToIdleList(SocketContext *sock)
    {
        RemoveFromPollList(sock);
        return AddToIdleList(sock);
    }
    nsresult MoveToPollList(SocketContext *sock)
    {
        RemoveFromIdleList(sock);
        return AddToPollList(sock);
    }
    
    // returns PR_FALSE to stop processing the main loop
    PRBool ServiceEventQ();

    //-------------------------------------------------------------------------
    // poll list (socket thread only)
    //
    // first element of the poll list is the "NSPR pollable event"
    //-------------------------------------------------------------------------

    PRPollDesc mPollList[ NS_SOCKET_MAX_COUNT + 1 ];

    PRUint32 PollCount() { return mActiveCount + 1; }
    PRInt32  Poll(); // calls PR_Poll

    //-------------------------------------------------------------------------
    // mHostDB maps host:port -> nsHostEntry
    //-------------------------------------------------------------------------

    struct nsHostEntry : PLDHashEntryStub
    {
        PRIPv6Addr  addr;
        const char *hostport() const { return (const char *) key; }
    };

    static PLDHashTableOps ops;

    static PRBool PR_CALLBACK MatchEntry(PLDHashTable *, const PLDHashEntryHdr *, const void *);
    static void   PR_CALLBACK ClearEntry(PLDHashTable *, PLDHashEntryHdr *);

    PLDHashTable mHostDB;
};

extern nsSocketTransportService *gSocketTransportService;
extern PRThread                 *gSocketThread;

#endif // !nsSocketTransportService_h__
