/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This test checks following expectations when using HTTP/1 proxy:
 *
 * - check we are seeing expected nsresult error codes on channels
 *   (nsIChannel.status) corresponding to different proxy status code
 *   responses (502, 504, 407, ...)
 * - check we don't try to ask for credentials or otherwise authenticate to
 *   the proxy when 407 is returned and there is no Proxy-Authenticate
 *   response header sent
 */

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

const pps = Cc["@mozilla.org/network/protocol-proxy-service;1"].getService();

let server_port;
let http_server;

class ProxyFilter {
  constructor(type, host, port, flags) {
    this._type = type;
    this._host = host;
    this._port = port;
    this._flags = flags;
    this.QueryInterface = ChromeUtils.generateQI(["nsIProtocolProxyFilter"]);
  }
  applyFilter(uri, pi, cb) {
    if (uri.spec.match(/(\/proxy-session-counter)/)) {
      cb.onProxyFilterResult(pi);
      return;
    }
    cb.onProxyFilterResult(
      pps.newProxyInfo(
        this._type,
        this._host,
        this._port,
        "",
        "",
        this._flags,
        1000,
        null
      )
    );
  }
}

class UnxpectedAuthPrompt2 {
  constructor(signal) {
    this.signal = signal;
    this.QueryInterface = ChromeUtils.generateQI(["nsIAuthPrompt2"]);
  }
  asyncPromptAuth() {
    this.signal.triggered = true;
    throw Components.Exception("", Cr.ERROR_UNEXPECTED);
  }
}

class AuthRequestor {
  constructor(prompt) {
    this.prompt = prompt;
    this.QueryInterface = ChromeUtils.generateQI(["nsIInterfaceRequestor"]);
  }
  getInterface(iid) {
    if (iid.equals(Ci.nsIAuthPrompt2)) {
      return this.prompt();
    }
    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
  }
}

function make_channel(url) {
  return NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
    // Using TYPE_DOCUMENT for the authentication dialog test, it'd be blocked for other types
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
  });
}

function get_response(channel, flags = CL_ALLOW_UNKNOWN_CL) {
  return new Promise(resolve => {
    channel.asyncOpen(
      new ChannelListener(
        (request, data) => {
          request.QueryInterface(Ci.nsIHttpChannel);
          const status = request.status;
          const http_code = status ? undefined : request.responseStatus;
          request.QueryInterface(Ci.nsIProxiedChannel);
          const proxy_connect_response_code =
            request.httpProxyConnectResponseCode;
          resolve({ status, http_code, data, proxy_connect_response_code });
        },
        null,
        flags
      )
    );
  });
}

function connect_handler(request, response) {
  Assert.equal(request.method, "CONNECT");

  switch (request.host) {
    case "404.example.com":
      response.setStatusLine(request.httpVersion, 404, "Not found");
      break;
    case "407.example.com":
      response.setStatusLine(request.httpVersion, 407, "Authenticate");
      // And deliberately no Proxy-Authenticate header
      break;
    case "429.example.com":
      response.setStatusLine(request.httpVersion, 429, "Too Many Requests");
      break;
    case "502.example.com":
      response.setStatusLine(request.httpVersion, 502, "Bad Gateway");
      break;
    case "504.example.com":
      response.setStatusLine(request.httpVersion, 504, "Gateway timeout");
      break;
    default:
      response.setStatusLine(request.httpVersion, 500, "I am dumb");
  }
}

add_task(async function setup() {
  http_server = new HttpServer();
  http_server.identity.add("https", "404.example.com", 443);
  http_server.identity.add("https", "407.example.com", 443);
  http_server.identity.add("https", "429.example.com", 443);
  http_server.identity.add("https", "502.example.com", 443);
  http_server.identity.add("https", "504.example.com", 443);
  http_server.registerPathHandler("CONNECT", connect_handler);
  http_server.start(-1);
  server_port = http_server.identity.primaryPort;

  // make all native resolve calls "secretly" resolve localhost instead
  Services.prefs.setBoolPref("network.dns.native-is-localhost", true);

  pps.registerFilter(new ProxyFilter("http", "localhost", server_port, 0), 10);
});

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("network.dns.native-is-localhost");
});

/**
 * Test series beginning.
 */

// The proxy responses with 407 instead of 200 Connected, make sure we get a proper error
// code from the channel and not try to ask for any credentials.
add_task(async function proxy_auth_failure() {
  const chan = make_channel(`https://407.example.com/`);
  const auth_prompt = { triggered: false };
  chan.notificationCallbacks = new AuthRequestor(
    () => new UnxpectedAuthPrompt2(auth_prompt)
  );
  const { status, http_code, proxy_connect_response_code } = await get_response(
    chan,
    CL_EXPECT_FAILURE
  );

  Assert.equal(status, Cr.NS_ERROR_PROXY_AUTHENTICATION_FAILED);
  Assert.equal(proxy_connect_response_code, 407);
  Assert.equal(http_code, undefined);
  Assert.equal(auth_prompt.triggered, false, "Auth prompt didn't trigger");
});

// 502 Bad gateway code returned by the proxy.
add_task(async function proxy_bad_gateway_failure() {
  const { status, http_code, proxy_connect_response_code } = await get_response(
    make_channel(`https://502.example.com/`),
    CL_EXPECT_FAILURE
  );

  Assert.equal(status, Cr.NS_ERROR_PROXY_BAD_GATEWAY);
  Assert.equal(proxy_connect_response_code, 502);
  Assert.equal(http_code, undefined);
});

// 504 Gateway timeout code returned by the proxy.
add_task(async function proxy_gateway_timeout_failure() {
  const { status, http_code, proxy_connect_response_code } = await get_response(
    make_channel(`https://504.example.com/`),
    CL_EXPECT_FAILURE
  );

  Assert.equal(status, Cr.NS_ERROR_PROXY_GATEWAY_TIMEOUT);
  Assert.equal(proxy_connect_response_code, 504);
  Assert.equal(http_code, undefined);
});

// 404 Not Found means the proxy could not resolve the host.
add_task(async function proxy_host_not_found_failure() {
  const { status, http_code, proxy_connect_response_code } = await get_response(
    make_channel(`https://404.example.com/`),
    CL_EXPECT_FAILURE
  );

  Assert.equal(status, Cr.NS_ERROR_UNKNOWN_HOST);
  Assert.equal(proxy_connect_response_code, 404);
  Assert.equal(http_code, undefined);
});

// 429 Too Many Requests means we sent too many requests.
add_task(async function proxy_too_many_requests_failure() {
  const { status, http_code, proxy_connect_response_code } = await get_response(
    make_channel(`https://429.example.com/`),
    CL_EXPECT_FAILURE
  );

  Assert.equal(status, Cr.NS_ERROR_PROXY_TOO_MANY_REQUESTS);
  Assert.equal(proxy_connect_response_code, 429);
  Assert.equal(http_code, undefined);
});

add_task(async function shutdown() {
  await new Promise(resolve => {
    http_server.stop(resolve);
  });
});
