/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TCPFastOpenLayer_h__
#define TCPFastOpenLayer_h__

#include "prerror.h"

namespace mozilla {
namespace net {

/**
 * This layer must be placed just above PR-tcp socket, i.e. it must be under
 * nss layer.
 * At the beginning of TCPFastOpenLayer.cpp there is explanation what this
 * layer do.
 **/

typedef enum {
  TFO_NOT_SET,   // This is only as a control.
                 // A connection not using TFO will have the TFO state set upon
                 // connection creation (in nsHalfOpenSocket::SetupConn).
                 // A connection using TFO will have the TFO state set after
                 // the connection is established or canceled.
  TFO_UNKNOWN,   // This is before the primary socket is built, i.e. before
                 // TCPFastOpenFinish is called.
  TFO_DISABLED,  // tfo is disabled because of a tfo error on a previous
                 // connection to the host (i.e. !mEnt->mUseFastOpen).
                 // If TFO is not supported by the OS, it is disabled by
                 // the pref or too many consecutive errors occurred, this value
                 // is not reported. This is set before StartFastOpen is called.
  TFO_DISABLED_CONNECT,  // Connection is using CONNECT. This is set before
                         // StartFastOpen is called.
  // The following 3 are set just after TCPFastOpenFinish.
  TFO_NOT_TRIED,  // For some reason TCPFastOpenLayer does not have any data to
                  // send with the syn packet. This should never happen.
  TFO_TRIED,      // TCP has sent a TFO cookie request.
  TFO_DATA_SENT,  // On Linux, TCP has send data as well. (On Linux we do not
                  // know whether data has been accepted).
                  // On Windows, TCP has send data or only a TFO cookie request
                  // and the data or TFO cookie has been accepted by the server.
  // The following value is only used on windows and is set after
  // PR_ConnectContinue. That is the point when we know if TFO data was been
  // accepted.
  TFO_DATA_COOKIE_NOT_ACCEPTED,  // This is only on Windows. TFO data or TFO
                                 // cookie request has not been accepted.
  // The following 3 are set during socket error recover
  // (nsSocketTransport::RecoverFromError).
  TFO_FAILED_CONNECTION_REFUSED,
  TFO_FAILED_NET_TIMEOUT,
  TFO_FAILED_UNKNOW_ERROR,
  // The following 4 are set when backup connection finishes before the primary
  // connection.
  TFO_FAILED_BACKUP_CONNECTION_TFO_NOT_TRIED,
  TFO_FAILED_BACKUP_CONNECTION_TFO_TRIED,
  TFO_FAILED_BACKUP_CONNECTION_TFO_DATA_SENT,
  TFO_FAILED_BACKUP_CONNECTION_TFO_DATA_COOKIE_NOT_ACCEPTED,
  // The following 4 are set when the recovery connection fails as well.
  TFO_FAILED_CONNECTION_REFUSED_NO_TFO_FAILED_TOO,
  TFO_FAILED_NET_TIMEOUT_NO_TFO_FAILED_TOO,
  TFO_FAILED_UNKNOW_ERROR_NO_TFO_FAILED_TOO,
  TFO_FAILED_BACKUP_CONNECTION_NO_TFO_FAILED_TOO,
  TFO_BACKUP_CONN,  // This is a backup conn, for a halfOpenSock that was used
                    // TFO.
  TFO_INIT_FAILED,  // nsHalfOpenSocket::SetupConn failed.
  TFO_UNKNOWN_RESOLVING,  // There is a high number of TFO_UNKNOWN state
                          // reported. Let's split them depending on the socket
                          // transport state: TFO_UNKNOWN_RESOLVING,
                          // TFO_UNKNOWN_RESOLVED, TFO_UNKNOWN_CONNECTING and
                          // TFO_UNKNOWN_CONNECTED..
  TFO_UNKNOWN_RESOLVED,
  TFO_UNKNOWN_CONNECTING,
  TFO_UNKNOWN_CONNECTED,
  TFO_FAILED,
  TFO_HTTP  // TFO is disabled for non-secure connections.
} TFOResult;

nsresult AttachTCPFastOpenIOLayer(PRFileDesc* fd);

// Get the result of TCP Fast Open.
void TCPFastOpenFinish(PRFileDesc* fd, PRErrorCode& err,
                       bool& fastOpenNotSupported, uint8_t& tfoStatus);

int32_t TCPFastOpenGetBufferSizeLeft(PRFileDesc* fd);

bool TCPFastOpenGetCurrentBufferSize(PRFileDesc* fd);
bool TCPFastOpenFlushBuffer(PRFileDesc* fd);
}  // namespace net
}  // namespace mozilla

#endif  // TCPFastOpenLayer_h__
