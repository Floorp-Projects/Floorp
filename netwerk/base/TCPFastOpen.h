/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TCPFASTOPEN_H__
#define TCPFASTOPEN_H__

/**
  * This is an abstract class for TCP Fast Open - TFO (RFC7413).
  * It is not always safe to use Fast Open. It can be use for requests that
  * are replayable.
  * Middle boxes can block or reset connections that use TFO, therefore a
  * backup connection will be prepared with a delay.
  * In case of blocking such a connection tcp socket will terminate only after
  * a timeout, therefore a backup connection is needed. If connection is refuse
  * the same socketTransport will retry.
  *
  * This is implemented by nsHalfopenSocket.
  **/

namespace mozilla {
namespace net {

class TCPFastOpen
{
public:

  // Check if we have a transaction that is safe to be used with TFO.
  // Connections over TLS are always safe and some http requests (e.g. GET).
  virtual bool FastOpenEnabled() = 0;
  // To use TFO we need to have a transaction prepared, e.g. also have
  // nsHttpConnection ready. This functions is call by nsSocketTransport to
  // setup a connection.
  virtual nsresult StartFastOpen() = 0;
  // Inform nsHalfopenSocket whether a connection using TFO succeeded or not.
  // This will cancel the backup connection and in case of a failure rewind
  // the transaction.
  virtual void SetFastOpenConnected(nsresult error, bool aWillRetry) = 0;
  virtual void FastOpenNotSupported() = 0;
  virtual void SetFastOpenStatus(uint8_t tfoStatus) = 0;
};

}
}

#endif //TCPFASTOPEN_H__
