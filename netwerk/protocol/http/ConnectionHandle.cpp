/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

// Log on level :5, instead of default :4.
#undef LOG
#define LOG(args) LOG5(args)
#undef LOG_ENABLED
#define LOG_ENABLED() LOG5_ENABLED()

#include "ConnectionHandle.h"

namespace mozilla {
namespace net {

ConnectionHandle::~ConnectionHandle() {
  if (mConn) {
    nsresult rv = gHttpHandler->ReclaimConnection(mConn);
    if (NS_FAILED(rv)) {
      LOG(
          ("ConnectionHandle::~ConnectionHandle\n"
           "    failed to reclaim connection\n"));
    }
  }
}

NS_IMPL_ISUPPORTS0(ConnectionHandle)

nsresult ConnectionHandle::OnHeadersAvailable(nsAHttpTransaction* trans,
                                              nsHttpRequestHead* req,
                                              nsHttpResponseHead* resp,
                                              bool* reset) {
  return mConn->OnHeadersAvailable(trans, req, resp, reset);
}

void ConnectionHandle::CloseTransaction(nsAHttpTransaction* trans,
                                        nsresult reason) {
  mConn->CloseTransaction(trans, reason);
}

nsresult ConnectionHandle::TakeTransport(nsISocketTransport** aTransport,
                                         nsIAsyncInputStream** aInputStream,
                                         nsIAsyncOutputStream** aOutputStream) {
  return mConn->TakeTransport(aTransport, aInputStream, aOutputStream);
}

bool ConnectionHandle::IsPersistent() {
  MOZ_ASSERT(OnSocketThread());
  return mConn->IsPersistent();
}

bool ConnectionHandle::IsReused() {
  MOZ_ASSERT(OnSocketThread());
  return mConn->IsReused();
}

void ConnectionHandle::DontReuse() {
  MOZ_ASSERT(OnSocketThread());
  mConn->DontReuse();
}

nsresult ConnectionHandle::PushBack(const char* buf, uint32_t bufLen) {
  return mConn->PushBack(buf, bufLen);
}

already_AddRefed<HttpConnectionBase> ConnectionHandle::TakeHttpConnection() {
  // return our connection object to the caller and clear it internally
  // do not drop our reference - the caller now owns it.
  MOZ_ASSERT(mConn);
  return mConn.forget();
}

already_AddRefed<HttpConnectionBase> ConnectionHandle::HttpConnection() {
  RefPtr<HttpConnectionBase> rv(mConn);
  return rv.forget();
}

void ConnectionHandle::TopBrowsingContextIdChanged(uint64_t id) {
  // Do nothing.
}

}  // namespace net
}  // namespace mozilla
