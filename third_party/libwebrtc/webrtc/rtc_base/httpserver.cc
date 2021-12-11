/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>

#include "rtc_base/httpcommon-inl.h"

#include "rtc_base/asyncsocket.h"
#include "rtc_base/checks.h"
#include "rtc_base/httpserver.h"
#include "rtc_base/logging.h"
#include "rtc_base/socketstream.h"
#include "rtc_base/thread.h"

namespace rtc {

///////////////////////////////////////////////////////////////////////////////
// HttpServer
///////////////////////////////////////////////////////////////////////////////

HttpServer::HttpServer() : next_connection_id_(1), closing_(false) {
}

HttpServer::~HttpServer() {
  if (closing_) {
    RTC_LOG(LS_WARNING) << "HttpServer::CloseAll has not completed";
  }
  for (ConnectionMap::iterator it = connections_.begin();
       it != connections_.end();
       ++it) {
    StreamInterface* stream = it->second->EndProcess();
    delete stream;
    delete it->second;
  }
}

int
HttpServer::HandleConnection(StreamInterface* stream) {
  int connection_id = next_connection_id_++;
  RTC_DCHECK(connection_id != HTTP_INVALID_CONNECTION_ID);
  Connection* connection = new Connection(connection_id, this);
  connections_.insert(ConnectionMap::value_type(connection_id, connection));
  connection->BeginProcess(stream);
  return connection_id;
}

void
HttpServer::Respond(HttpServerTransaction* transaction) {
  int connection_id = transaction->connection_id();
  if (Connection* connection = Find(connection_id)) {
    connection->Respond(transaction);
  } else {
    delete transaction;
    // We may be tempted to SignalHttpComplete, but that implies that a
    // connection still exists.
  }
}

void
HttpServer::Close(int connection_id, bool force) {
  if (Connection* connection = Find(connection_id)) {
    connection->InitiateClose(force);
  }
}

void
HttpServer::CloseAll(bool force) {
  if (connections_.empty()) {
    SignalCloseAllComplete(this);
    return;
  }
  closing_ = true;
  std::list<Connection*> connections;
  for (ConnectionMap::const_iterator it = connections_.begin();
       it != connections_.end(); ++it) {
    connections.push_back(it->second);
  }
  for (std::list<Connection*>::const_iterator it = connections.begin();
      it != connections.end(); ++it) {
    (*it)->InitiateClose(force);
  }
}

HttpServer::Connection*
HttpServer::Find(int connection_id) {
  ConnectionMap::iterator it = connections_.find(connection_id);
  if (it == connections_.end())
    return nullptr;
  return it->second;
}

void
HttpServer::Remove(int connection_id) {
  ConnectionMap::iterator it = connections_.find(connection_id);
  if (it == connections_.end()) {
    RTC_NOTREACHED();
    return;
  }
  Connection* connection = it->second;
  connections_.erase(it);
  SignalConnectionClosed(this, connection_id, connection->EndProcess());
  delete connection;
  if (closing_ && connections_.empty()) {
    closing_ = false;
    SignalCloseAllComplete(this);
  }
}

///////////////////////////////////////////////////////////////////////////////
// HttpServer::Connection
///////////////////////////////////////////////////////////////////////////////

HttpServer::Connection::Connection(int connection_id, HttpServer* server)
    : connection_id_(connection_id),
      server_(server),
      current_(nullptr),
      signalling_(false),
      close_(false) {}

HttpServer::Connection::~Connection() {
  // It's possible that an object hosted inside this transaction signalled
  // an event which caused the connection to close.
  Thread::Current()->Dispose(current_);
}

void
HttpServer::Connection::BeginProcess(StreamInterface* stream) {
  base_.notify(this);
  base_.attach(stream);
  current_ = new HttpServerTransaction(connection_id_);
  if (base_.mode() != HM_CONNECT)
    base_.recv(&current_->request);
}

StreamInterface*
HttpServer::Connection::EndProcess() {
  base_.notify(nullptr);
  base_.abort(HE_DISCONNECTED);
  return base_.detach();
}

void
HttpServer::Connection::Respond(HttpServerTransaction* transaction) {
  RTC_DCHECK(current_ == nullptr);
  current_ = transaction;
  if (current_->response.begin() == current_->response.end()) {
    current_->response.set_error(HC_INTERNAL_SERVER_ERROR);
  }
  bool keep_alive = HttpShouldKeepAlive(current_->request);
  current_->response.setHeader(HH_CONNECTION,
                               keep_alive ? "Keep-Alive" : "Close",
                               false);
  close_ = !HttpShouldKeepAlive(current_->response);
  base_.send(&current_->response);
}

void
HttpServer::Connection::InitiateClose(bool force) {
  bool request_in_progress = (HM_SEND == base_.mode()) || (nullptr == current_);
  if (!signalling_ && (force || !request_in_progress)) {
    server_->Remove(connection_id_);
  } else {
    close_ = true;
  }
}

//
// IHttpNotify Implementation
//

HttpError
HttpServer::Connection::onHttpHeaderComplete(bool chunked, size_t& data_size) {
  if (data_size == SIZE_UNKNOWN) {
    data_size = 0;
  }
  RTC_DCHECK(current_ != nullptr);
  bool custom_document = false;
  server_->SignalHttpRequestHeader(server_, current_, &custom_document);
  if (!custom_document) {
    current_->request.document.reset(new MemoryStream);
  }
  return HE_NONE;
}

void
HttpServer::Connection::onHttpComplete(HttpMode mode, HttpError err) {
  if (mode == HM_SEND) {
    RTC_DCHECK(current_ != nullptr);
    signalling_ = true;
    server_->SignalHttpRequestComplete(server_, current_, err);
    signalling_ = false;
    if (close_) {
      // Force a close
      err = HE_DISCONNECTED;
    }
  }
  if (err != HE_NONE) {
    server_->Remove(connection_id_);
  } else if (mode == HM_CONNECT) {
    base_.recv(&current_->request);
  } else if (mode == HM_RECV) {
    RTC_DCHECK(current_ != nullptr);
    // TODO: do we need this?
    //request_.document_->rewind();
    HttpServerTransaction* transaction = current_;
    current_ = nullptr;
    server_->SignalHttpRequest(server_, transaction);
  } else if (mode == HM_SEND) {
    Thread::Current()->Dispose(current_->response.document.release());
    current_->request.clear(true);
    current_->response.clear(true);
    base_.recv(&current_->request);
  } else {
    RTC_NOTREACHED();
  }
}

void
HttpServer::Connection::onHttpClosed(HttpError err) {
  server_->Remove(connection_id_);
}

///////////////////////////////////////////////////////////////////////////////
// HttpListenServer
///////////////////////////////////////////////////////////////////////////////

HttpListenServer::HttpListenServer() {
  SignalConnectionClosed.connect(this, &HttpListenServer::OnConnectionClosed);
}

HttpListenServer::~HttpListenServer() {
}

int HttpListenServer::Listen(const SocketAddress& address) {
  AsyncSocket* sock =
      Thread::Current()->socketserver()->CreateAsyncSocket(address.family(),
                                                           SOCK_STREAM);
  if (!sock) {
    return SOCKET_ERROR;
  }
  listener_.reset(sock);
  listener_->SignalReadEvent.connect(this, &HttpListenServer::OnReadEvent);
  if ((listener_->Bind(address) != SOCKET_ERROR) &&
      (listener_->Listen(5) != SOCKET_ERROR))
    return 0;
  return listener_->GetError();
}

bool HttpListenServer::GetAddress(SocketAddress* address) const {
  if (!listener_) {
    return false;
  }
  *address = listener_->GetLocalAddress();
  return !address->IsNil();
}

void HttpListenServer::StopListening() {
  if (listener_) {
    listener_->Close();
  }
}

void HttpListenServer::OnReadEvent(AsyncSocket* socket) {
  RTC_DCHECK(socket == listener_.get());
  AsyncSocket* incoming = listener_->Accept(nullptr);
  if (incoming) {
    StreamInterface* stream = new SocketStream(incoming);
    //stream = new LoggingAdapter(stream, LS_VERBOSE, "HttpServer", false);
    HandleConnection(stream);
  }
}

void HttpListenServer::OnConnectionClosed(HttpServer* server,
                                          int connection_id,
                                          StreamInterface* stream) {
  Thread::Current()->Dispose(stream);
}

///////////////////////////////////////////////////////////////////////////////

}  // namespace rtc
