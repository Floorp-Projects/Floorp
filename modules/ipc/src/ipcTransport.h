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
 * The Original Code is Mozilla IPC.
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

#ifndef ipcTransport_h__
#define ipcTransport_h__

#include "nsIObserver.h"
#include "nsIStreamListener.h"
#include "nsIStreamProvider.h"
#include "nsITransport.h"
#include "nsITimer.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "prio.h"

#include "ipcMessage.h"
#include "ipcMessageQ.h"

class ipcTransport;

//----------------------------------------------------------------------------
// ipcTransportObserver interface
//----------------------------------------------------------------------------

class ipcTransportObserver
{
public:
    virtual void OnConnectionEstablished(PRUint32 clientID) = 0;
    virtual void OnConnectionLost() = 0;
    virtual void OnMessageAvailable(const ipcMessage *) = 0;
};

//----------------------------------------------------------------------------
// ipcSendQueue
//----------------------------------------------------------------------------

class ipcSendQueue : public nsIStreamProvider
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMPROVIDER

    ipcSendQueue(ipcTransport *transport)
        : mTransport(transport)
        { }
    virtual ~ipcSendQueue() { }

    void EnqueueMsg(ipcMessage *msg) { mQueue.Append(msg); }

    PRBool IsEmpty() { return mQueue.IsEmpty(); }

private:
    ipcTransport *mTransport;
    ipcMessageQ   mQueue;
};

//-----------------------------------------------------------------------------
// ipcReceiver
//-----------------------------------------------------------------------------

class ipcReceiver : public nsIStreamListener
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER

    ipcReceiver(ipcTransport *transport)
        : mTransport(transport)
        { }
    virtual ~ipcReceiver() { }

    nsresult ReadSegment(const char *, PRUint32 count, PRUint32 *countRead);

private:
    ipcTransport         *mTransport;
    ipcMessage            mMsg;  // message in progress
};

//-----------------------------------------------------------------------------
// ipcTransport
//-----------------------------------------------------------------------------

class ipcTransport : public nsIObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    ipcTransport()
        : mSendQ(this)
        , mReceiver(this)
        , mObserver(nsnull)
        , mFD(nsnull)
        , mWriteSuspended(PR_FALSE)
        , mSentHello(PR_FALSE)
        , mHaveConnection(PR_FALSE)
        , mSpawnedDaemon(PR_FALSE)
        , mConnectionAttemptCount(0)
        { }
    virtual ~ipcTransport();

    nsresult Init(const nsACString &appName,
                  const nsACString &socketPath,
                  ipcTransportObserver *observer);
    nsresult Shutdown();

    // takes ownership of |msg|
    nsresult SendMsg(ipcMessage *msg);

    PRBool   HaveConnection() const { return mHaveConnection; }

public:
    // internal to implementation 
    void OnMessageAvailable(const ipcMessage *);
    void SetWriteSuspended(PRBool val) { mWriteSuspended = val; }
    void OnStartRequest(nsIRequest *req);
    void OnStopRequest(nsIRequest *req, nsresult status);
    PRFileDesc *FD() { return mFD; }

private:
    //
    // helpers
    //
    nsresult Connect();
    nsresult SendMsg_Internal(ipcMessage *msg);
    nsresult CreateTransport();
    nsresult SpawnDaemon();

    //
    // data
    //
    ipcSendQueue           mSendQ;
    ipcReceiver            mReceiver;
    ipcMessageQ            mDelayedQ;
    ipcTransportObserver  *mObserver;
    nsCOMPtr<nsITransport> mTransport;
    nsCOMPtr<nsIRequest>   mReadRequest;
    nsCOMPtr<nsIRequest>   mWriteRequest;
    nsCOMPtr<nsITimer>     mTimer;
    nsCString              mAppName;
    nsCString              mSocketPath;
    PRFileDesc            *mFD;
    PRPackedBool           mWriteSuspended;
    PRPackedBool           mSentHello;
    PRPackedBool           mHaveConnection;
    PRPackedBool           mSpawnedDaemon;
    PRUint8                mConnectionAttemptCount;
};

#endif // !ipcTransport_h__
