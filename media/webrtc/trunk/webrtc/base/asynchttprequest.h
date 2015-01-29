/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_ASYNCHTTPREQUEST_H_
#define WEBRTC_BASE_ASYNCHTTPREQUEST_H_

#include <string>
#include "webrtc/base/event.h"
#include "webrtc/base/httpclient.h"
#include "webrtc/base/signalthread.h"
#include "webrtc/base/socketpool.h"
#include "webrtc/base/sslsocketfactory.h"

namespace rtc {

class FirewallManager;

///////////////////////////////////////////////////////////////////////////////
// AsyncHttpRequest
// Performs an HTTP request on a background thread.  Notifies on the foreground
// thread once the request is done (successfully or unsuccessfully).
///////////////////////////////////////////////////////////////////////////////

class AsyncHttpRequest : public SignalThread {
 public:
  explicit AsyncHttpRequest(const std::string &user_agent);
  ~AsyncHttpRequest();

  // If start_delay is less than or equal to zero, this starts immediately.
  // Start_delay defaults to zero.
  int start_delay() const { return start_delay_; }
  void set_start_delay(int delay) { start_delay_ = delay; }

  const ProxyInfo& proxy() const { return proxy_; }
  void set_proxy(const ProxyInfo& proxy) {
    proxy_ = proxy;
  }
  void set_firewall(FirewallManager * firewall) {
    firewall_ = firewall;
  }

  // The DNS name of the host to connect to.
  const std::string& host() { return host_; }
  void set_host(const std::string& host) { host_ = host; }

  // The port to connect to on the target host.
  int port() { return port_; }
  void set_port(int port) { port_ = port; }

  // Whether the request should use SSL.
  bool secure() { return secure_; }
  void set_secure(bool secure) { secure_ = secure; }

  // Time to wait on the download, in ms.
  int timeout() { return timeout_; }
  void set_timeout(int timeout) { timeout_ = timeout; }

  // Fail redirects to allow analysis of redirect urls, etc.
  bool fail_redirect() const { return fail_redirect_; }
  void set_fail_redirect(bool redirect) { fail_redirect_ = redirect; }

  // Returns the redirect when redirection occurs
  const std::string& response_redirect() { return response_redirect_; }

  HttpRequestData& request() { return client_.request(); }
  HttpResponseData& response() { return client_.response(); }
  HttpErrorType error() { return error_; }

 protected:
  void set_error(HttpErrorType error) { error_ = error; }
  virtual void OnWorkStart();
  virtual void OnWorkStop();
  void OnComplete(HttpClient* client, HttpErrorType error);
  virtual void OnMessage(Message* message);
  virtual void DoWork();

 private:
  void LaunchRequest();

  int start_delay_;
  ProxyInfo proxy_;
  FirewallManager* firewall_;
  std::string host_;
  int port_;
  bool secure_;
  int timeout_;
  bool fail_redirect_;
  SslSocketFactory factory_;
  ReuseSocketPool pool_;
  HttpClient client_;
  HttpErrorType error_;
  std::string response_redirect_;
};

}  // namespace rtc

#endif  // WEBRTC_BASE_ASYNCHTTPREQUEST_H_
