/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsSocketTransport_h___
#define nsSocketTransport_h___

#include "prclist.h"
#include "prio.h"
#include "prnetdb.h"
#include "prinrval.h"
#include "nsCOMPtr.h"
#include "nsISocketTransport.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIEventQueueService.h"
#include "nsIStreamListener.h"
#include "nsIStreamProvider.h"
#include "nsIDNSListener.h"
#include "nsIDNSService.h"
#include "nsIPipe.h"
#include "nsIProgressEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIFileStreams.h"

#define NS_SOCKET_TRANSPORT_SEGMENT_SIZE        (2*1024)
#define NS_SOCKET_TRANSPORT_BUFFER_SIZE         (8*1024)

//
// This is the maximum amount of data that will be read into a stream before
// another transport is processed...
//
#define MAX_IO_TRANSFER_SIZE  (8*1024)

enum nsSocketState {
  eSocketState_Created        = 0,
  eSocketState_WaitDNS        = 1,
  eSocketState_Closed         = 2,
  eSocketState_WaitConnect    = 3,
  eSocketState_Connected      = 4,
  eSocketState_WaitReadWrite  = 5,
  eSocketState_Done           = 6,
  eSocketState_Timeout        = 7,
  eSocketState_Error          = 8,
  eSocketState_Max            = 9
};

enum nsSocketOperation {
  eSocketOperation_None       = 0,
  eSocketOperation_Connect    = 1,
  eSocketOperation_ReadWrite  = 2,
  eSocketOperation_Max        = 3
};

//
// The following emun provides information about the currently
// active read and/or write requests...
//
// +-------------------------------+
// | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
// +-------------------------------+
// <-----flag bits----><-type bits->
//         
//  Bits:
//      0-3:  Type (ie. None, Async, Sync)
//        4:  Done flag.
//        5:  Wait flag.
//      6-7:  Unused flags...
//
//
//   
enum nsSocketReadWriteInfo {
  eSocketRead_None            = 0x0000,
  eSocketRead_Async           = 0x0001,
  eSocketRead_Sync            = 0x0002,
  eSocketRead_Done            = 0x0010,
  eSocketRead_Wait            = 0x0020,
  eSocketRead_Type_Mask       = 0x000F,
  eSocketRead_Flag_Mask       = 0x00F0,

  eSocketWrite_None           = 0x0000,
  eSocketWrite_Async          = 0x0100,
  eSocketWrite_Sync           = 0x0200,
  eSocketWrite_Done           = 0x1000,
  eSocketWrite_Wait           = 0x2000,
  eSocketWrite_Type_Mask      = 0x0F00,
  eSocketWrite_Flag_Mask      = 0xF000,

  eSocketDNS_Wait             = 0x2020
};

//
// This is the default timeout value (in milliseconds) for sockets which have
// no activity...
//
#define DEFAULT_SOCKET_CONNECT_TIMEOUT_IN_MS  35*1000

// Forward declarations...
class nsSocketTransportService;
class nsSocketBS;  // base class for blocking streams
class nsSocketBIS; // blocking input stream
class nsSocketBOS; // blocking output stream
class nsSocketIS;  // input stream
class nsSocketOS;  // output stream
class nsSocketRequest;
class nsSocketReadRequest;
class nsSocketWriteRequest;
class nsIProxyInfo;

class nsSocketTransport : public nsISocketTransport,
                          public nsIDNSListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSITRANSPORT
    NS_DECL_NSISOCKETTRANSPORT
    NS_DECL_NSIDNSLISTENER
    
    // nsSocketTransport methods:
    nsSocketTransport();
    virtual ~nsSocketTransport();
  
    nsresult Init(nsSocketTransportService* aService,
                  const char* aHost, 
                  PRInt32 aPort,
	              PRUint32 aSocketTypeCount,
	              const char* *aSocketTypes,
                  nsIProxyInfo* aProxyInfo,
                  PRUint32 bufferSegmentSize,
                  PRUint32 bufferMaxSize);
    
    nsresult Process(PRInt16 aSelectFlags);

    nsresult Cancel(nsresult status);
    
    nsresult CheckForTimeout (PRIntervalTime aCurrentTime);
    
    // Close this socket
    nsresult CloseConnection();
    
    // Access methods used by the socket transport service...
    PRFileDesc* GetSocket(void)      { return mSocketFD;    }
    PRInt16     GetSelectFlags(void) { return mSelectFlags; }
    PRCList*    GetListNode(void)    { return &mListLink;   }
    
    static nsSocketTransport* GetInstance(PRCList* qp) { return (nsSocketTransport*)((char*)qp - offsetof(nsSocketTransport, mListLink)); }
    
    PRBool CanBeReused() { return 
        (mCurrentState != eSocketState_Error) && !mClosePending;}

    //
    // request helpers
    //
    nsresult GetName(PRUnichar **);
    nsresult Dispatch(nsSocketRequest *);
    nsresult OnProgress(nsSocketRequest *, nsISupports *ctxt, PRUint32 offset);
    nsresult OnStatus(nsSocketRequest *, nsISupports *ctxt, nsresult message);

    //
    // blocking stream helpers
    //
    PRFileDesc *GetConnectedSocket(); // do the connection if necessary
    void ReleaseSocket(PRFileDesc *); // allow the socket to be closed if pending
    void ClearSocketBS(nsSocketBS *); // clears weak reference to blocking stream

protected:
    nsresult doConnection(PRInt16 aSelectFlags);
    nsresult doBlockingConnection();
    nsresult doReadWrite(PRInt16 aSelectFlags);
    nsresult doResolveHost();

    nsresult OnStatus(nsresult message); // with either request

    void CompleteAsyncRead();
    void CompleteAsyncWrite();

    PRIntervalTime mSocketTimeout;
    PRIntervalTime mSocketConnectTimeout;
    
    // Access methods for manipulating the ReadWriteInfo...
    inline void SetReadType(nsSocketReadWriteInfo aType) {
        mReadWriteState = (mReadWriteState & ~eSocketRead_Type_Mask) | aType; 
    }
    inline PRUint32 GetReadType(void) {
        return mReadWriteState & eSocketRead_Type_Mask;
    }
    inline void SetWriteType(nsSocketReadWriteInfo aType) {
        mReadWriteState = (mReadWriteState & ~eSocketWrite_Type_Mask) | aType; 
    }
    inline PRUint32 GetWriteType(void) {
        return mReadWriteState & eSocketWrite_Type_Mask;
    }
    inline void SetFlag(nsSocketReadWriteInfo aFlag) { 
        mReadWriteState |= aFlag;
    }
    inline PRUint32 GetFlag(nsSocketReadWriteInfo aFlag) {
        return mReadWriteState & aFlag;
    }
    
    inline void ClearFlag(nsSocketReadWriteInfo aFlag) {
        mReadWriteState &= ~aFlag;
    } 
    
protected:
    
    nsSocketState                   mCurrentState;
    nsCOMPtr<nsIRequest>            mDNSRequest;
    nsCOMPtr<nsIProgressEventSink>  mProgressSink;
    nsCOMPtr<nsIInterfaceRequestor> mNotificationCallbacks;
    char*                           mHostName;
    PRInt32                         mPort;
    PRIntervalTime                  mLastActiveTime;
    PRCList                         mListLink;
    PRMonitor*                      mMonitor;
    PRNetAddr                       mNetAddress;
    nsSocketOperation               mOperation;
    nsCOMPtr<nsISupports>           mSecurityInfo;

    PRInt32                         mProxyPort;
    char*                           mProxyHost;
    PRPackedBool                    mProxyTransparent;

    /* put all the packed bools together so we save space */
    PRPackedBool                    mClosePending;
    PRPackedBool                    mWasConnected;

    nsSocketTransportService*       mService;

    PRUint32                        mReadWriteState;
    PRInt16                         mSelectFlags;
    nsresult                        mStatus;

    // last status reported via OnStatus (eg. NS_NET_STATUS_RESOLVING_HOST)
    nsresult                        mLastOnStatusMsg;

    PRFileDesc*                     mSocketFD;
    PRUint32                        mSocketRef;  // if non-zero, keep the socket open unless there is an error
    PRUint32                        mSocketLock; // if non-zero, do not close the socket even if there is an error
    PRUint32                        mSocketTypeCount;
    char*                          *mSocketTypes;

    PRInt32                         mBytesExpected;

    PRUint32                        mBufferSegmentSize;
    PRUint32                        mBufferMaxSize;
    
    PRUint32                        mIdleTimeoutInSeconds;

    nsSocketBIS                    *mBIS;  // weak reference
    nsSocketBOS                    *mBOS;  // weak reference
    nsSocketReadRequest            *mReadRequest;
    nsSocketWriteRequest           *mWriteRequest;
};

/**
 * base blocking stream ...
 */
class nsSocketBS
{
public:
    nsSocketBS();
    virtual ~nsSocketBS();

    nsresult Init();

    void SetTransport(nsSocketTransport *);

protected: 
    PRFileDesc *GetSocket();
    void ReleaseSocket(PRFileDesc *);

    nsresult GetTransport(nsSocketTransport **); // get an owning reference to the transport
    nsresult Poll(PRFileDesc *sock, PRInt16 event);

protected:
    nsSocketTransport *mTransport;     // strong reference
    PRLock            *mTransportLock; // protects access to mTransport
};

/**
 * blocking input stream, returned by nsITransport::OpenInputStream()
 */
class nsSocketBIS : public nsSocketBS
                  , public nsIInputStream
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM

    nsSocketBIS();
    virtual ~nsSocketBIS();
};

/**
 * blocking output stream, returned by nsITransport::OpenOutputStream()
 */
class nsSocketBOS : public nsSocketBS
                  , public nsIOutputStream
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOUTPUTSTREAM

    nsSocketBOS();
    virtual ~nsSocketBOS();
};

/**
 * input stream, passed to nsIStreamListener::OnDataAvailable()
 */
class nsSocketIS : public nsIInputStream
                 , public nsISeekableStream
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM
    NS_DECL_NSISEEKABLESTREAM

    nsSocketIS();
    virtual ~nsSocketIS() { }

    void        SetSocket(PRFileDesc *aSock) { mSock = aSock; }
    PRUint32    GetOffset() { return mOffset; }
    void        SetOffset(PRUint32 o) { mOffset = o; }
    PRBool      GotWouldBlock() { return mError == PR_WOULD_BLOCK_ERROR; }
    PRBool      GotError() { return mError != 0; }
    PRErrorCode GetError() { return mError; }

private:
    PRUint32    mOffset;
    PRFileDesc *mSock;
    PRErrorCode mError;
};

/**
 * output stream, passed to nsIStreamProvider::OnDataWritable()
 */
class nsSocketOS : public nsIOutputStream
                 , public nsISeekableStream
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOUTPUTSTREAM
    NS_DECL_NSISEEKABLESTREAM

    nsSocketOS();
    virtual ~nsSocketOS() { }

    void        SetSocket(PRFileDesc *aSock) { mSock = aSock; }
    PRUint32    GetOffset() { return mOffset; }
    void        SetOffset(PRUint32 o) { mOffset = o; }
    PRBool      GotWouldBlock() { return mError == PR_WOULD_BLOCK_ERROR; }
    PRBool      GotError() { return mError != 0; }
    PRErrorCode GetError() { return mError; }

private:
    static NS_METHOD WriteFromSegments(nsIInputStream *, void *, const char *,
                                       PRUint32, PRUint32, PRUint32 *);

    PRUint32    mOffset;
    PRFileDesc *mSock;
    PRErrorCode mError;
};

/**
 * base request
 */
class nsSocketRequest : public nsITransportRequest
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSITRANSPORTREQUEST

    nsSocketRequest();
    virtual ~nsSocketRequest();

    PRBool IsInitialized() { return mStartFired; }
    PRBool IsSuspended() { return mSuspendCount > 0; }
    PRBool IsCanceled() { return mCanceled; }

    void SetTransport(nsSocketTransport *);
    void SetObserver(nsIRequestObserver *obs) { mObserver = obs; }
    void SetContext(nsISupports *ctx) { mContext = ctx; }
    void SetStatus(nsresult status) { mStatus = status; }

    nsISupports *Context() { return mContext; }
    
    virtual nsresult OnStart();
    virtual nsresult OnStop();

protected:
    nsSocketTransport           *mTransport;
    nsCOMPtr<nsIRequestObserver> mObserver;
    nsCOMPtr<nsISupports>        mContext;
    nsresult                     mStatus;
    PRIntn                       mSuspendCount;
    PRPackedBool                 mCanceled;
    PRPackedBool                 mStartFired;
    PRPackedBool                 mStopFired;
};

/**
 * read request, returned by nsITransport::AsyncRead()
 */
class nsSocketReadRequest : public nsSocketRequest
{
public:
    nsSocketReadRequest();
    virtual ~nsSocketReadRequest();

    void SetSocket(PRFileDesc *);
    void SetListener(nsIStreamListener *l) { mListener = l; }

    nsresult OnStop();
    nsresult OnRead();

private:
    nsSocketIS                 *mInputStream;
    nsCOMPtr<nsIStreamListener> mListener;
};

/**
 * write request, returned by nsITransport::AsyncWrite()
 */
class nsSocketWriteRequest : public nsSocketRequest
{
public:
    nsSocketWriteRequest();
    virtual ~nsSocketWriteRequest();

    void SetSocket(PRFileDesc *);
    void SetProvider(nsIStreamProvider *p) { mProvider = p; }

    nsresult OnStop();
    nsresult OnWrite();

private:
    nsSocketOS                 *mOutputStream;
    nsCOMPtr<nsIStreamProvider> mProvider;
};

#endif /* nsSocketTransport_h___ */
