/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This testcase exercises the Protocol Proxy Service's async filter functionality
// run_filter_*() are entry points for each individual test.

"use strict";

var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
var pps = Cc["@mozilla.org/network/protocol-proxy-service;1"].getService();
var prefs = Cc["@mozilla.org/preferences-service;1"].getService(
  Ci.nsIPrefBranch
);

/**
 * Test nsIProtocolHandler that allows proxying, but doesn't allow HTTP
 * proxying.
 */
function TestProtocolHandler() {}
TestProtocolHandler.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIProtocolHandler]),
  scheme: "moz-test",
  defaultPort: -1,
  protocolFlags:
    Ci.nsIProtocolHandler.URI_NOAUTH |
    Ci.nsIProtocolHandler.URI_NORELATIVE |
    Ci.nsIProtocolHandler.ALLOWS_PROXY |
    Ci.nsIProtocolHandler.URI_DANGEROUS_TO_LOAD,
  newChannel(uri, aLoadInfo) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  },
  allowPort(port, scheme) {
    return true;
  },
};

function TestProtocolHandlerFactory() {}
TestProtocolHandlerFactory.prototype = {
  createInstance(delegate, iid) {
    return new TestProtocolHandler().QueryInterface(iid);
  },
  lockFactory(lock) {},
};

function register_test_protocol_handler() {
  var reg = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  reg.registerFactory(
    Components.ID("{4ea7dd3a-8cae-499c-9f18-e1de773ca25b}"),
    "TestProtocolHandler",
    "@mozilla.org/network/protocol;1?name=moz-test",
    new TestProtocolHandlerFactory()
  );
}

function check_proxy(pi, type, host, port, flags, timeout, hasNext) {
  Assert.notEqual(pi, null);
  Assert.equal(pi.type, type);
  Assert.equal(pi.host, host);
  Assert.equal(pi.port, port);
  if (flags != -1) {
    Assert.equal(pi.flags, flags);
  }
  if (timeout != -1) {
    Assert.equal(pi.failoverTimeout, timeout);
  }
  if (hasNext) {
    Assert.notEqual(pi.failoverProxy, null);
  } else {
    Assert.equal(pi.failoverProxy, null);
  }
}

const SYNC = 0;
const THROW = 1;
const ASYNC = 2;

function TestFilter(type, host, port, flags, timeout, result) {
  this._type = type;
  this._host = host;
  this._port = port;
  this._flags = flags;
  this._timeout = timeout;
  this._result = result;
}
TestFilter.prototype = {
  _type: "",
  _host: "",
  _port: -1,
  _flags: 0,
  _timeout: 0,
  _async: false,
  _throwing: false,

  QueryInterface: ChromeUtils.generateQI([Ci.nsIProtocolProxyFilter]),

  applyFilter(uri, pi, cb) {
    if (this._result == THROW) {
      throw Components.Exception("", Cr.NS_ERROR_FAILURE);
    }

    var pi_tail = pps.newProxyInfo(
      this._type,
      this._host,
      this._port,
      "",
      "",
      this._flags,
      this._timeout,
      null
    );
    if (pi) {
      pi.failoverProxy = pi_tail;
    } else {
      pi = pi_tail;
    }

    if (this._result == ASYNC) {
      executeSoon(() => {
        cb.onProxyFilterResult(pi);
      });
    } else {
      cb.onProxyFilterResult(pi);
    }
  },
};

function resolveCallback() {}
resolveCallback.prototype = {
  nextFunction: null,

  QueryInterface: ChromeUtils.generateQI([Ci.nsIProtocolProxyCallback]),

  onProxyAvailable(req, channel, pi, status) {
    this.nextFunction(pi);
  },
};

// ==============================================================

var filter1;
var filter2;
var filter3;

function run_filter_test1() {
  filter1 = new TestFilter("http", "foo", 8080, 0, 10, ASYNC);
  filter2 = new TestFilter("http", "bar", 8090, 0, 10, ASYNC);
  pps.registerFilter(filter1, 20);
  pps.registerFilter(filter2, 10);

  var cb = new resolveCallback();
  cb.nextFunction = filter_test1_1;
  var channel = NetUtil.newChannel({
    uri: "http://www.mozilla.org/",
    loadUsingSystemPrincipal: true,
  });
  var req = pps.asyncResolve(channel, 0, cb);
}

function filter_test1_1(pi) {
  check_proxy(pi, "http", "bar", 8090, 0, 10, true);
  check_proxy(pi.failoverProxy, "http", "foo", 8080, 0, 10, false);

  pps.unregisterFilter(filter2);

  var cb = new resolveCallback();
  cb.nextFunction = filter_test1_2;
  var channel = NetUtil.newChannel({
    uri: "http://www.mozilla.org/",
    loadUsingSystemPrincipal: true,
  });
  var req = pps.asyncResolve(channel, 0, cb);
}

function filter_test1_2(pi) {
  check_proxy(pi, "http", "foo", 8080, 0, 10, false);

  pps.unregisterFilter(filter1);

  var cb = new resolveCallback();
  cb.nextFunction = filter_test1_3;
  var channel = NetUtil.newChannel({
    uri: "http://www.mozilla.org/",
    loadUsingSystemPrincipal: true,
  });
  var req = pps.asyncResolve(channel, 0, cb);
}

function filter_test1_3(pi) {
  Assert.equal(pi, null);
  run_filter2_sync_async();
}

function run_filter2_sync_async() {
  filter1 = new TestFilter("http", "foo", 8080, 0, 10, SYNC);
  filter2 = new TestFilter("http", "bar", 8090, 0, 10, ASYNC);
  pps.registerFilter(filter1, 20);
  pps.registerFilter(filter2, 10);

  var cb = new resolveCallback();
  cb.nextFunction = filter_test2_1;
  var channel = NetUtil.newChannel({
    uri: "http://www.mozilla.org/",
    loadUsingSystemPrincipal: true,
  });
  var req = pps.asyncResolve(channel, 0, cb);
}

function filter_test2_1(pi) {
  check_proxy(pi, "http", "bar", 8090, 0, 10, true);
  check_proxy(pi.failoverProxy, "http", "foo", 8080, 0, 10, false);

  pps.unregisterFilter(filter1);
  pps.unregisterFilter(filter2);

  run_filter3_async_sync();
}

function run_filter3_async_sync() {
  filter1 = new TestFilter("http", "foo", 8080, 0, 10, ASYNC);
  filter2 = new TestFilter("http", "bar", 8090, 0, 10, SYNC);
  pps.registerFilter(filter1, 20);
  pps.registerFilter(filter2, 10);

  var cb = new resolveCallback();
  cb.nextFunction = filter_test3_1;
  var channel = NetUtil.newChannel({
    uri: "http://www.mozilla.org/",
    loadUsingSystemPrincipal: true,
  });
  var req = pps.asyncResolve(channel, 0, cb);
}

function filter_test3_1(pi) {
  check_proxy(pi, "http", "bar", 8090, 0, 10, true);
  check_proxy(pi.failoverProxy, "http", "foo", 8080, 0, 10, false);

  pps.unregisterFilter(filter1);
  pps.unregisterFilter(filter2);

  run_filter4_throwing_sync_sync();
}

function run_filter4_throwing_sync_sync() {
  filter1 = new TestFilter("", "", 0, 0, 0, THROW);
  filter2 = new TestFilter("http", "foo", 8080, 0, 10, SYNC);
  filter3 = new TestFilter("http", "bar", 8090, 0, 10, SYNC);
  pps.registerFilter(filter1, 20);
  pps.registerFilter(filter2, 10);
  pps.registerFilter(filter3, 5);

  var cb = new resolveCallback();
  cb.nextFunction = filter_test4_1;
  var channel = NetUtil.newChannel({
    uri: "http://www.mozilla2.org/",
    loadUsingSystemPrincipal: true,
  });
  var req = pps.asyncResolve(channel, 0, cb);
}

function filter_test4_1(pi) {
  check_proxy(pi, "http", "bar", 8090, 0, 10, true);
  check_proxy(pi.failoverProxy, "http", "foo", 8080, 0, 10, false);

  pps.unregisterFilter(filter1);
  pps.unregisterFilter(filter2);
  pps.unregisterFilter(filter3);

  run_filter5_sync_sync_throwing();
}

function run_filter5_sync_sync_throwing() {
  filter1 = new TestFilter("http", "foo", 8080, 0, 10, SYNC);
  filter2 = new TestFilter("http", "bar", 8090, 0, 10, SYNC);
  filter3 = new TestFilter("", "", 0, 0, 0, THROW);
  pps.registerFilter(filter1, 20);
  pps.registerFilter(filter2, 10);
  pps.registerFilter(filter3, 5);

  var cb = new resolveCallback();
  cb.nextFunction = filter_test5_1;
  var channel = NetUtil.newChannel({
    uri: "http://www.mozilla.org/",
    loadUsingSystemPrincipal: true,
  });
  var req = pps.asyncResolve(channel, 0, cb);
}

function filter_test5_1(pi) {
  check_proxy(pi, "http", "bar", 8090, 0, 10, true);
  check_proxy(pi.failoverProxy, "http", "foo", 8080, 0, 10, false);

  pps.unregisterFilter(filter1);
  pps.unregisterFilter(filter2);
  pps.unregisterFilter(filter3);

  run_filter5_2_throwing_async_async();
}

function run_filter5_2_throwing_async_async() {
  filter1 = new TestFilter("", "", 0, 0, 0, THROW);
  filter2 = new TestFilter("http", "foo", 8080, 0, 10, ASYNC);
  filter3 = new TestFilter("http", "bar", 8090, 0, 10, ASYNC);
  pps.registerFilter(filter1, 20);
  pps.registerFilter(filter2, 10);
  pps.registerFilter(filter3, 5);

  var cb = new resolveCallback();
  cb.nextFunction = filter_test5_2;
  var channel = NetUtil.newChannel({
    uri: "http://www.mozilla.org/",
    loadUsingSystemPrincipal: true,
  });
  var req = pps.asyncResolve(channel, 0, cb);
}

function filter_test5_2(pi) {
  check_proxy(pi, "http", "bar", 8090, 0, 10, true);
  check_proxy(pi.failoverProxy, "http", "foo", 8080, 0, 10, false);

  pps.unregisterFilter(filter1);
  pps.unregisterFilter(filter2);
  pps.unregisterFilter(filter3);

  run_filter6_async_async_throwing();
}

function run_filter6_async_async_throwing() {
  filter1 = new TestFilter("http", "foo", 8080, 0, 10, ASYNC);
  filter2 = new TestFilter("http", "bar", 8090, 0, 10, ASYNC);
  filter3 = new TestFilter("", "", 0, 0, 0, THROW);
  pps.registerFilter(filter1, 20);
  pps.registerFilter(filter2, 10);
  pps.registerFilter(filter3, 5);

  var cb = new resolveCallback();
  cb.nextFunction = filter_test6_1;
  var channel = NetUtil.newChannel({
    uri: "http://www.mozilla.org/",
    loadUsingSystemPrincipal: true,
  });
  var req = pps.asyncResolve(channel, 0, cb);
}

function filter_test6_1(pi) {
  check_proxy(pi, "http", "bar", 8090, 0, 10, true);
  check_proxy(pi.failoverProxy, "http", "foo", 8080, 0, 10, false);

  pps.unregisterFilter(filter1);
  pps.unregisterFilter(filter2);
  pps.unregisterFilter(filter3);

  run_filter7_async_throwing_async();
}

function run_filter7_async_throwing_async() {
  filter1 = new TestFilter("http", "foo", 8080, 0, 10, ASYNC);
  filter2 = new TestFilter("", "", 0, 0, 0, THROW);
  filter3 = new TestFilter("http", "bar", 8090, 0, 10, ASYNC);
  pps.registerFilter(filter1, 20);
  pps.registerFilter(filter2, 10);
  pps.registerFilter(filter3, 5);

  var cb = new resolveCallback();
  cb.nextFunction = filter_test7_1;
  var channel = NetUtil.newChannel({
    uri: "http://www.mozilla.org/",
    loadUsingSystemPrincipal: true,
  });
  var req = pps.asyncResolve(channel, 0, cb);
}

function filter_test7_1(pi) {
  check_proxy(pi, "http", "bar", 8090, 0, 10, true);
  check_proxy(pi.failoverProxy, "http", "foo", 8080, 0, 10, false);

  pps.unregisterFilter(filter1);
  pps.unregisterFilter(filter2);
  pps.unregisterFilter(filter3);

  run_filter8_sync_throwing_sync();
}

function run_filter8_sync_throwing_sync() {
  filter1 = new TestFilter("http", "foo", 8080, 0, 10, SYNC);
  filter2 = new TestFilter("", "", 0, 0, 0, THROW);
  filter3 = new TestFilter("http", "bar", 8090, 0, 10, SYNC);
  pps.registerFilter(filter1, 20);
  pps.registerFilter(filter2, 10);
  pps.registerFilter(filter3, 5);

  var cb = new resolveCallback();
  cb.nextFunction = filter_test8_1;
  var channel = NetUtil.newChannel({
    uri: "http://www.mozilla.org/",
    loadUsingSystemPrincipal: true,
  });
  var req = pps.asyncResolve(channel, 0, cb);
}

function filter_test8_1(pi) {
  check_proxy(pi, "http", "bar", 8090, 0, 10, true);
  check_proxy(pi.failoverProxy, "http", "foo", 8080, 0, 10, false);

  pps.unregisterFilter(filter1);
  pps.unregisterFilter(filter2);
  pps.unregisterFilter(filter3);

  run_filter9_throwing();
}

function run_filter9_throwing() {
  filter1 = new TestFilter("", "", 0, 0, 0, THROW);
  pps.registerFilter(filter1, 20);

  var cb = new resolveCallback();
  cb.nextFunction = filter_test9_1;
  var channel = NetUtil.newChannel({
    uri: "http://www.mozilla.org/",
    loadUsingSystemPrincipal: true,
  });
  var req = pps.asyncResolve(channel, 0, cb);
}

function filter_test9_1(pi) {
  Assert.equal(pi, null);
  do_test_finished();
}

// =========================================

function run_test() {
  register_test_protocol_handler();

  // start of asynchronous test chain
  run_filter_test1();
  do_test_pending();
}
