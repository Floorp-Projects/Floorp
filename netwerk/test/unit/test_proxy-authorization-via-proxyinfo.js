/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test checks that the proxy-auth header is propagated to the CONNECT request when
// set on a proxy-info object via a proxy filter

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
const pps = Cc["@mozilla.org/network/protocol-proxy-service;1"].getService();

class TestFilter {
  constructor(type, host, port, flags, auth) {
    this._type = type;
    this._host = host;
    this._port = port;
    this._flags = flags;
    this._auth = auth;
    this.QueryInterface = ChromeUtils.generateQI([Ci.nsIProtocolProxyFilter]);
  }

  applyFilter(pps, uri, pi, cb) {
    cb.onProxyFilterResult(pps.newProxyInfo(
      this._type, this._host, this._port,
      this._auth, "", this._flags, 1000, null));
  }
};

let httpServer = null;
let port;
let connectProcessesed = false;
const proxyAuthHeader = 'proxy-auth-header-value';

function make_channel(url) {
  return NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true
  });
}

function connect_handler(request, response)
{
  Assert.equal(request.method, "CONNECT");
  Assert.ok(request.hasHeader("Proxy-Authorization"));
  Assert.equal(request.getHeader("Proxy-Authorization"), proxyAuthHeader);

  // Just refuse to connect, we have what we need now.
  response.setStatusLine(request.httpVersion, 500, "STOP");
  connectProcessesed = true;
}

function finish_test()
{
  Assert.ok(connectProcessesed);
  httpServer.stop(do_test_finished);
}

function run_test()
{
  httpServer = new HttpServer();
  httpServer.identity.add("https", "mozilla.org", 443);
  httpServer.registerPathHandler("CONNECT", connect_handler);
  httpServer.start(-1);
  port = httpServer.identity.primaryPort;

  pps.registerFilter(new TestFilter("http", "localhost", port, 0, proxyAuthHeader), 10);

  let chan = make_channel("https://mozilla.org/");
  chan.asyncOpen(new ChannelListener(finish_test, null, CL_EXPECT_FAILURE));
  do_test_pending();
}
