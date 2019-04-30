/* -*- Mode: javascript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");

var baseURL;
const kResponseTimeoutPref = "network.http.response.timeout";
const kResponseTimeout = 1;
const kShortLivedKeepalivePref =
  "network.http.tcp_keepalive.short_lived_connections";
const kLongLivedKeepalivePref =
  "network.http.tcp_keepalive.long_lived_connections";

const prefService = Cc["@mozilla.org/preferences-service;1"]
                       .getService(Ci.nsIPrefBranch);

var server = new HttpServer();

function TimeoutListener(expectResponse) {
  this.expectResponse = expectResponse;
}

TimeoutListener.prototype = {
  onStartRequest: function (request) {
  },

  onDataAvailable: function (request, stream) {
  },

  onStopRequest: function (request, status) {
    if (this.expectResponse) {
      Assert.equal(status, Cr.NS_OK);
    } else {
      Assert.equal(status, Cr.NS_ERROR_NET_TIMEOUT);
    }

    run_next_test();
  },
};

function serverStopListener() {
  do_test_finished();
}

function testTimeout(timeoutEnabled, expectResponse) {
  // Set timeout pref.
  if (timeoutEnabled) {
    prefService.setIntPref(kResponseTimeoutPref, kResponseTimeout);
  } else {
    prefService.setIntPref(kResponseTimeoutPref, 0);
  }

  var chan = NetUtil.newChannel({uri: baseURL, loadUsingSystemPrincipal: true})
                    .QueryInterface(Ci.nsIHttpChannel);
  var listener = new TimeoutListener(expectResponse);
  chan.asyncOpen(listener);
}

function testTimeoutEnabled() {
  // Set a timeout value; expect a timeout and no response.
  testTimeout(true, false);
}

function testTimeoutDisabled() {
  // Set a timeout value of 0; expect a response.
  testTimeout(false, true);
}

function testTimeoutDisabledByShortLivedKeepalives() {
  // Enable TCP Keepalives for short lived HTTP connections.
  prefService.setBoolPref(kShortLivedKeepalivePref, true);
  prefService.setBoolPref(kLongLivedKeepalivePref, false);

  // Try to set a timeout value, but expect a response without timeout.
  testTimeout(true, true);
}

function testTimeoutDisabledByLongLivedKeepalives() {
  // Enable TCP Keepalives for long lived HTTP connections.
  prefService.setBoolPref(kShortLivedKeepalivePref, false);
  prefService.setBoolPref(kLongLivedKeepalivePref, true);

  // Try to set a timeout value, but expect a response without timeout.
  testTimeout(true, true);
}

function testTimeoutDisabledByBothKeepalives() {
  // Enable TCP Keepalives for short and long lived HTTP connections.
  prefService.setBoolPref(kShortLivedKeepalivePref, true);
  prefService.setBoolPref(kLongLivedKeepalivePref, true);

  // Try to set a timeout value, but expect a response without timeout.
  testTimeout(true, true);
}

function setup_tests() {
  // Start tests with timeout enabled, i.e. disable TCP keepalives for HTTP.
  // Reset pref in cleanup.
  if (prefService.getBoolPref(kShortLivedKeepalivePref)) {
    prefService.setBoolPref(kShortLivedKeepalivePref, false);
    registerCleanupFunction(function() {
      prefService.setBoolPref(kShortLivedKeepalivePref, true);
    });
  }
  if (prefService.getBoolPref(kLongLivedKeepalivePref)) {
    prefService.setBoolPref(kLongLivedKeepalivePref, false);
    registerCleanupFunction(function() {
      prefService.setBoolPref(kLongLivedKeepalivePref, true);
    });
  }

  var tests = [
    // Enable with a timeout value >0;
    testTimeoutEnabled,
    // Disable with a timeout value of 0;
    testTimeoutDisabled,
    // Disable by enabling TCP keepalive for short-lived HTTP connections.
    testTimeoutDisabledByShortLivedKeepalives,
    // Disable by enabling TCP keepalive for long-lived HTTP connections.
    testTimeoutDisabledByLongLivedKeepalives,
    // Disable by enabling TCP keepalive for both HTTP connection types.
    testTimeoutDisabledByBothKeepalives
  ];

  for (var i=0; i < tests.length; i++) {
    add_test(tests[i]);
  }
}

function setup_http_server() {
  // Start server; will be stopped at test cleanup time.
  server.start(-1);
  baseURL = "http://localhost:" + server.identity.primaryPort + "/";
  info("Using baseURL: " + baseURL);
  server.registerPathHandler('/', function(metadata, response) {
    // Wait until the timeout should have passed, then respond.
    response.processAsync();

    do_timeout((kResponseTimeout+1)*1000 /* ms */, function() {
      response.setStatusLine(metadata.httpVersion, 200, "OK");
      response.write("Hello world");
      response.finish();
    });
  });
  registerCleanupFunction(function() {
    server.stop(serverStopListener);
  });
}

function run_test() {
  setup_http_server();

  setup_tests();

  run_next_test();
}
