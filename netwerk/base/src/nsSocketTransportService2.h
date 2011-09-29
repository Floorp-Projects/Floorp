/* vim:set ts=4 sw=4 sts=4 ci et: */
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

#include "nsPISocketTransportService.h"
#include "nsIThreadInternal.h"
#include "nsThreadUtils.h"
#include "nsEventQueue.h"
#include "nsCOMPtr.h"
#include "pldhash.h"
#include "prinrval.h"
#include "prlog.h"
#include "prinit.h"
#include "prio.h"
#include "nsASocketHandler.h"
#include "nsIObserver.h"
#include "mozilla/Mutex.h"

//-----------------------------------------------------------------------------

#if defined(PR_LOGGING)
//
// set NSPR_LOG_MODULES=nsSocketTransport:5
//
extern PRLogModuleInfo *gSocketTransportLog;
#endif
#define SOCKET_LOG(args)     PR_LOG(gSocketTransportLog, PR_LOG_DEBUG, args)
#define SOCKET_LOG_ENABLED() PR_LOG_TEST(gSocketTransportLog, PR_LOG_DEBUG)

//-----------------------------------------------------------------------------

#define NS_SOCKET_POLL_TIMEOUT PR_INTERVAL_NO_TIMEOUT

//-----------------------------------------------------------------------------

class nsSocketTransportService : public nsPISocketTransportService
                               , public nsIEventTarget
                               , public nsIThreadObserver
                               , public nsIRunnable
                               , public nsIObserver
{
    typedef mozilla::Mutex Mutex;

public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSPISOCKETTRANSPORTSERVICE
    NS_DECL_NSISOCKETTRANSPORTSERVICE
    NS_DECL_NSIEVENTTARGET
    NS_DECL_NSITHREADOBSERVER
    NS_DECL_NSIRUNNABLE
    NS_DECL_NSIOBSERVER 

    nsSocketTransportService();

    // Max Socket count may need to get initialized/used by nsHttpHandler
    // before this class is initialized.
    static PRUint32 gMaxCount;
    static PRCallOnceType gMaxCountInitOnce;
    static PRStatus DiscoverMaxCount();

    //
    // the number of sockets that can be attached at any given time is
    // limited.  this is done because some operating systems (e.g., Win9x)
    // limit the number of sockets that can be created by an application.
    // AttachSocket will fail if the limit is exceeded.  consumers should
    // call CanAttachSocket and check the result before creating a socket.
    //
    bool CanAttachSocket() {
        return mActiveCount + mIdleCount < gMaxCount;
    }

protected:

    virtual ~nsSocketTransportService();

private:

    //-------------------------------------------------------------------------
    // misc (any thread)
    //-------------------------------------------------------------------------

    nsCOMPtr<nsIThread> mThread;    // protected by mLock
    PRFileDesc *mThreadEvent;
                            // protected by mLock.  mThreadEvent may change
                            // if the old pollable event is broken.  only
                            // the socket thread may change mThreadEvent;
                            // it needs to lock mLock only when it changes
                            // mThreadEvent.  other threads don't change
                            // mThreadEvent; they need to lock mLock
                            // whenever they access mThreadEvent.
    bool        mAutodialEnabled;
                            // pref to control autodial code

    // Returns mThread, protecting the get-and-addref with mLock
    already_AddRefed<nsIThread> GetThreadSafely();

    //-------------------------------------------------------------------------
    // initialization and shutdown (any thread)
    //-------------------------------------------------------------------------

    Mutex         mLock;
    bool          mInitialized;
    bool          mShuttingDown;
                            // indicates whether we are currently in the
                            // process of shutting down

    //-------------------------------------------------------------------------
    // socket lists (socket thread only)
    //
    // only "active" sockets are on the poll list.  the active list is kept
    // in sync with the poll list such that:
    //
    //   mActiveList[k].mFD == mPollList[k+1].fd
    //
    // where k=0,1,2,...
    //-------------------------------------------------------------------------

    struct SocketContext
    {
        PRFileDesc       *mFD;
        nsASocketHandler *mHandler;
        PRUint16          mElapsedTime;  // time elapsed w/o activity
    };

    SocketContext *mActiveList;                   /* mListSize entries */
    SocketContext *mIdleList;                     /* mListSize entries */

    PRUint32 mActiveListSize;
    PRUint32 mIdleListSize;
    PRUint32 mActiveCount;
    PRUint32 mIdleCount;

    nsresult DetachSocket(SocketContext *, SocketContext *);
    nsresult AddToIdleList(SocketContext *);
    nsresult AddToPollList(SocketContext *);
    void RemoveFromIdleList(SocketContext *);
    void RemoveFromPollList(SocketContext *);
    void MoveToIdleList(SocketContext *sock);
    void MoveToPollList(SocketContext *sock);

    bool GrowActiveList();
    bool GrowIdleList();
    void   InitMaxCount();
    
    //-------------------------------------------------------------------------
    // poll list (socket thread only)
    //
    // first element of the poll list is mThreadEvent (or null if the pollable
    // event cannot be created).
    //-------------------------------------------------------------------------

    PRPollDesc *mPollList;                        /* mListSize + 1 entries */

    PRIntervalTime PollTimeout();            // computes ideal poll timeout
    nsresult       DoPollIteration(bool wait);
                                             // perfoms a single poll iteration
    PRInt32        Poll(bool wait, PRUint32 *interval);
                                             // calls PR_Poll.  the out param
                                             // interval indicates the poll
                                             // duration in seconds.

    //-------------------------------------------------------------------------
    // pending socket queue - see NotifyWhenCanAttachSocket
    //-------------------------------------------------------------------------

    nsEventQueue mPendingSocketQ; // queue of nsIRunnable objects

    // Preference Monitor for SendBufferSize
    nsresult    UpdatePrefs();
    PRInt32     mSendBufferSize;
};

extern nsSocketTransportService *gSocketTransportService;
extern PRThread                 *gSocketThread;

#endif // !nsSocketTransportService_h__
