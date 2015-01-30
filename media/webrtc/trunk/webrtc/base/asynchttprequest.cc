/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/asynchttprequest.h"

namespace rtc {

enum {
  MSG_TIMEOUT = SignalThread::ST_MSG_FIRST_AVAILABLE,
  MSG_LAUNCH_REQUEST
};
static const int kDefaultHTTPTimeout = 30 * 1000;  // 30 sec

///////////////////////////////////////////////////////////////////////////////
// AsyncHttpRequest
///////////////////////////////////////////////////////////////////////////////

AsyncHttpRequest::AsyncHttpRequest(const std::string &user_agent)
    : start_delay_(0),
      firewall_(NULL),
      port_(80),
      secure_(false),
      timeout_(kDefaultHTTPTimeout),
      fail_redirect_(false),
      factory_(Thread::Current()->socketserver(), user_agent),
      pool_(&factory_),
      client_(user_agent.c_str(), &pool_),
      error_(HE_NONE) {
  client_.SignalHttpClientComplete.connect(this,
      &AsyncHttpRequest::OnComplete);
}

AsyncHttpRequest::~AsyncHttpRequest() {
}

void AsyncHttpRequest::OnWorkStart() {
  if (start_delay_ <= 0) {
    LaunchRequest();
  } else {
    Thread::Current()->PostDelayed(start_delay_, this, MSG_LAUNCH_REQUEST);
  }
}

void AsyncHttpRequest::OnWorkStop() {
  // worker is already quitting, no need to explicitly quit
  LOG(LS_INFO) << "HttpRequest cancelled";
}

void AsyncHttpRequest::OnComplete(HttpClient* client, HttpErrorType error) {
  Thread::Current()->Clear(this, MSG_TIMEOUT);

  set_error(error);
  if (!error) {
    LOG(LS_INFO) << "HttpRequest completed successfully";

    std::string value;
    if (client_.response().hasHeader(HH_LOCATION, &value)) {
      response_redirect_ = value.c_str();
    }
  } else {
    LOG(LS_INFO) << "HttpRequest completed with error: " << error;
  }

  worker()->Quit();
}

void AsyncHttpRequest::OnMessage(Message* message) {
  switch (message->message_id) {
   case MSG_TIMEOUT:
    LOG(LS_INFO) << "HttpRequest timed out";
    client_.reset();
    worker()->Quit();
    break;
   case MSG_LAUNCH_REQUEST:
    LaunchRequest();
    break;
   default:
    SignalThread::OnMessage(message);
    break;
  }
}

void AsyncHttpRequest::DoWork() {
  // Do nothing while we wait for the request to finish. We only do this so
  // that we can be a SignalThread; in the future this class should not be
  // a SignalThread, since it does not need to spawn a new thread.
  Thread::Current()->ProcessMessages(kForever);
}

void AsyncHttpRequest::LaunchRequest() {
  factory_.SetProxy(proxy_);
  if (secure_)
    factory_.UseSSL(host_.c_str());

  bool transparent_proxy = (port_ == 80) &&
           ((proxy_.type == PROXY_HTTPS) || (proxy_.type == PROXY_UNKNOWN));
  if (transparent_proxy) {
    client_.set_proxy(proxy_);
  }
  client_.set_fail_redirect(fail_redirect_);
  client_.set_server(SocketAddress(host_, port_));

  LOG(LS_INFO) << "HttpRequest start: " << host_ + client_.request().path;

  Thread::Current()->PostDelayed(timeout_, this, MSG_TIMEOUT);
  client_.start();
}

}  // namespace rtc
