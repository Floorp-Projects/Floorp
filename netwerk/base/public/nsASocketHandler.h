/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsASocketHandler_h__
#define nsASocketHandler_h__

// socket handler used by nsISocketTransportService.
// methods are only called on the socket thread.

class nsASocketHandler : public nsISupports
{
public:
    nsASocketHandler()
        : mCondition(NS_OK)
        , mPollFlags(0)
        , mPollTimeout(PR_UINT16_MAX)
        {}

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
    // this value specifies the maximum amount of time in seconds that may be
    // spent waiting for activity on this socket.  if this timeout is reached,
    // then OnSocketReady will be called with outFlags = -1.
    //
    // the default value for this member is PR_UINT16_MAX, which disables the
    // timeout error checking.  (i.e., a timeout value of PR_UINT16_MAX is
    // never reached.)
    //
    PRUint16 mPollTimeout;

    //
    // called to service a socket
    // 
    // params:
    //   socketRef - socket identifier
    //   fd        - socket file descriptor
    //   outFlags  - value of PR_PollDesc::out_flags after PR_Poll returns
    //               or -1 if a timeout occurred
    //
    virtual void OnSocketReady(PRFileDesc *fd, PRInt16 outFlags) = 0;

    //
    // called when a socket is no longer under the control of the socket
    // transport service.  the socket handler may close the socket at this
    // point.  after this call returns, the handler will no longer be owned
    // by the socket transport service.
    //
    virtual void OnSocketDetached(PRFileDesc *fd) = 0;
};

#endif // !nsASocketHandler_h__
