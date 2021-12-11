/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ConnectionHandle_h__
#define ConnectionHandle_h__

#include "nsAHttpConnection.h"
#include "HttpConnectionBase.h"

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// ConnectionHandle
//
// thin wrapper around a real connection, used to keep track of references
// to the connection to determine when the connection may be reused.  the
// transaction owns a reference to this handle.  this extra
// layer of indirection greatly simplifies consumer code, avoiding the
// need for consumer code to know when to give the connection back to the
// connection manager.
//
class ConnectionHandle : public nsAHttpConnection {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSAHTTPCONNECTION(mConn)

  explicit ConnectionHandle(HttpConnectionBase* conn) : mConn(conn) {}
  void Reset() { mConn = nullptr; }

 private:
  virtual ~ConnectionHandle();
  RefPtr<HttpConnectionBase> mConn;
};

}  // namespace net
}  // namespace mozilla

#endif  // ConnectionHandle_h__
