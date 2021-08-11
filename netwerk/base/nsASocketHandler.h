/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsASocketHandler_h__
#define nsASocketHandler_h__

#include "nsError.h"
#include "nsINetAddr.h"
#include "nsISupports.h"
#include "prio.h"

// socket handler used by nsISocketTransportService.
// methods are only called on the socket thread.

class nsASocketHandler : public nsISupports {
 public:
  nsASocketHandler() = default;

  //
  // this condition variable will be checked to determine if the socket
  // handler should be detached.  it must only be accessed on the socket
  // thread.
  //
  nsresult mCondition{NS_OK};

  //
  // these flags can only be modified on the socket transport thread.
  // the socket transport service will check these flags before calling
  // PR_Poll.
  //
  uint16_t mPollFlags{0};

  //
  // this value specifies the maximum amount of time in seconds that may be
  // spent waiting for activity on this socket.  if this timeout is reached,
  // then OnSocketReady will be called with outFlags = -1.
  //
  // the default value for this member is UINT16_MAX, which disables the
  // timeout error checking.  (i.e., a timeout value of UINT16_MAX is
  // never reached.)
  //
  uint16_t mPollTimeout{UINT16_MAX};

  bool mIsPrivate{false};

  //
  // called to service a socket
  //
  // params:
  //   socketRef - socket identifier
  //   fd        - socket file descriptor
  //   outFlags  - value of PR_PollDesc::out_flags after PR_Poll returns
  //               or -1 if a timeout occurred
  //
  virtual void OnSocketReady(PRFileDesc* fd, int16_t outFlags) = 0;

  //
  // called when a socket is no longer under the control of the socket
  // transport service.  the socket handler may close the socket at this
  // point.  after this call returns, the handler will no longer be owned
  // by the socket transport service.
  //
  virtual void OnSocketDetached(PRFileDesc* fd) = 0;

  //
  // called to determine if the socket is for a local peer.
  // when used for server sockets, indicates if it only accepts local
  // connections.
  //
  virtual void IsLocal(bool* aIsLocal) = 0;

  //
  // called to determine if this socket should not be terminated when Gecko
  // is turned offline. This is mostly useful for the debugging server
  // socket.
  //
  virtual void KeepWhenOffline(bool* aKeepWhenOffline) {
    *aKeepWhenOffline = false;
  }

  //
  // called when global pref for keepalive has changed.
  //
  virtual void OnKeepaliveEnabledPrefChange(bool aEnabled) {}

  //
  // called to determine the NetAddr. addr can only be assumed to be initialized
  // when NS_OK is returned
  //
  virtual nsresult GetRemoteAddr(mozilla::net::NetAddr* addr) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  //
  // returns the number of bytes sent/transmitted over the socket
  //
  virtual uint64_t ByteCountSent() = 0;
  virtual uint64_t ByteCountReceived() = 0;
};

#endif  // !nsASocketHandler_h__
