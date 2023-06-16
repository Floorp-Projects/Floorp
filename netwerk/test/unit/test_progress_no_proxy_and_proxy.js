/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// This test can be merged with test_progress.js once HTTP/3 tests are
// enabled on all plaforms.

var last = 0;
var max = 0;
var using_proxy = false;

const RESPONSE_LENGTH = 3000000;
const STATUS_RECEIVING_FROM = 0x4b0006;

const TYPE_ONSTATUS = 1;
const TYPE_ONPROGRESS = 2;
const TYPE_ONSTARTREQUEST = 3;
const TYPE_ONDATAAVAILABLE = 4;
const TYPE_ONSTOPREQUEST = 5;

var ProgressCallback = function () {};

ProgressCallback.prototype = {
  _listener: null,
  _got_onstartrequest: false,
  _got_onstatus_after_onstartrequest: false,
  _last_callback_handled: null,
  statusArg: "",
  finish: null,

  QueryInterface: ChromeUtils.generateQI([
    "nsIProgressEventSink",
    "nsIStreamListener",
    "nsIRequestObserver",
  ]),

  getInterface(iid) {
    if (
      iid.equals(Ci.nsIProgressEventSink) ||
      iid.equals(Ci.nsIStreamListener) ||
      iid.equals(Ci.nsIRequestObserver)
    ) {
      return this;
    }
    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
  },

  onStartRequest(request) {
    Assert.equal(this._last_callback_handled, TYPE_ONSTATUS);
    this._got_onstartrequest = true;
    this._last_callback_handled = TYPE_ONSTARTREQUEST;

    this._listener = new ChannelListener(checkRequest, request);
    this._listener.onStartRequest(request);
  },

  onDataAvailable(request, data, offset, count) {
    Assert.equal(this._last_callback_handled, TYPE_ONPROGRESS);
    this._last_callback_handled = TYPE_ONDATAAVAILABLE;

    this._listener.onDataAvailable(request, data, offset, count);
  },

  onStopRequest(request, status) {
    Assert.equal(this._last_callback_handled, TYPE_ONDATAAVAILABLE);
    Assert.ok(this._got_onstatus_after_onstartrequest);
    this._last_callback_handled = TYPE_ONSTOPREQUEST;

    this._listener.onStopRequest(request, status);
    delete this._listener;

    check_http_info(request, this.expected_httpVersion, using_proxy);

    this.finish();
  },

  onProgress(request, progress, progressMax) {
    Assert.equal(this._last_callback_handled, TYPE_ONSTATUS);
    this._last_callback_handled = TYPE_ONPROGRESS;

    Assert.equal(this.mStatus, STATUS_RECEIVING_FROM);
    last = progress;
    max = progressMax;
  },

  onStatus(request, status, statusArg) {
    if (!this._got_onstartrequest) {
      // Ensure that all messages before onStartRequest are onStatus
      if (this._last_callback_handled) {
        Assert.equal(this._last_callback_handled, TYPE_ONSTATUS);
      }
    } else if (this._last_callback_handled == TYPE_ONSTARTREQUEST) {
      this._got_onstatus_after_onstartrequest = true;
    } else {
      Assert.equal(this._last_callback_handled, TYPE_ONDATAAVAILABLE);
    }
    this._last_callback_handled = TYPE_ONSTATUS;

    Assert.equal(statusArg, this.statusArg);
    this.mStatus = status;
  },

  mStatus: 0,
};

function chanPromise(uri, statusArg, version) {
  return new Promise(resolve => {
    var chan = makeHTTPChannel(uri, using_proxy);
    chan.requestMethod = "GET";
    let listener = new ProgressCallback();
    listener.statusArg = statusArg;
    chan.notificationCallbacks = listener;
    listener.expected_httpVersion = version;
    listener.finish = resolve;
    chan.asyncOpen(listener);
  });
}

function checkRequest(request, data, context) {
  Assert.equal(last, RESPONSE_LENGTH);
  Assert.equal(max, RESPONSE_LENGTH);
}

async function check_progress(server) {
  info(`Testing ${server.constructor.name}`);
  await server.registerPathHandler("/test", (req, resp) => {
    // Generate a post.
    function generateContent(size) {
      return "0".repeat(size);
    }

    resp.writeHead(200, {
      "content-type": "application/json",
      "content-length": "3000000",
    });
    resp.end(generateContent(3000000));
  });
  await chanPromise(
    `${server.origin()}/test`,
    `${server.domain()}`,
    server.version()
  );
}

add_task(async function setup() {
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");
  addCertFromFile(certdb, "proxy-ca.pem", "CTu,u,u");
});

add_task(async function test_http_1_and_2() {
  await with_node_servers(
    [NodeHTTPServer, NodeHTTPSServer, NodeHTTP2Server],
    check_progress
  );
});

add_task(async function test_http_proxy() {
  using_proxy = true;
  let proxy = new NodeHTTPProxyServer();
  await proxy.start();
  await with_node_servers(
    [NodeHTTPServer, NodeHTTPSServer, NodeHTTP2Server],
    check_progress
  );
  await proxy.stop();
  using_proxy = false;
});

add_task(async function test_https_proxy() {
  using_proxy = true;
  let proxy = new NodeHTTPSProxyServer();
  await proxy.start();
  await with_node_servers(
    [NodeHTTPServer, NodeHTTPSServer, NodeHTTP2Server],
    check_progress
  );
  await proxy.stop();
  using_proxy = false;
});

add_task(async function test_http2_proxy() {
  using_proxy = true;
  let proxy = new NodeHTTP2ProxyServer();
  await proxy.start();
  await with_node_servers(
    [NodeHTTPServer, NodeHTTPSServer, NodeHTTP2Server],
    check_progress
  );
  await proxy.stop();
  using_proxy = false;
});

add_task(async function test_http3() {
  await http3_setup_tests("h3-29");
  await chanPromise(
    "https://foo.example.com/" + RESPONSE_LENGTH,
    "foo.example.com",
    "h3-29"
  );
  http3_clear_prefs();
});
