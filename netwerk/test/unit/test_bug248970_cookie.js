/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

var httpserver;

function inChildProcess() {
  return Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}
function makeChan(path) {
  return NetUtil.newChannel({
    uri: "http://localhost:" + httpserver.identity.primaryPort + "/" + path,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
}

function setup_chan(path, isPrivate, callback) {
  var chan = makeChan(path);
  chan.QueryInterface(Ci.nsIPrivateBrowsingChannel).setPrivate(isPrivate);
  chan.asyncOpen(new ChannelListener(callback));
}

function set_cookie(value, callback) {
  return setup_chan("set?cookie=" + value, false, callback);
}

function set_private_cookie(value, callback) {
  return setup_chan("set?cookie=" + value, true, callback);
}

function check_cookie_presence(value, isPrivate, expected, callback) {
  setup_chan(
    "present?cookie=" + value.replace("=", "|"),
    isPrivate,
    function (req) {
      req.QueryInterface(Ci.nsIHttpChannel);
      Assert.equal(req.responseStatus, expected ? 200 : 404);
      callback(req);
    }
  );
}

function presentHandler(metadata, response) {
  var present = false;
  var match = /cookie=([^&]*)/.exec(metadata.queryString);
  if (match) {
    try {
      present = metadata
        .getHeader("Cookie")
        .includes(match[1].replace("|", "="));
    } catch (x) {}
  }
  response.setStatusLine("1.0", present ? 200 : 404, "");
}

function setHandler(metadata, response) {
  response.setStatusLine("1.0", 200, "Cookie set");
  var match = /cookie=([^&]*)/.exec(metadata.queryString);
  if (match) {
    response.setHeader("Set-Cookie", match[1]);
  }
}

function run_test() {
  // Allow all cookies if the pref service is available in this process.
  if (!inChildProcess()) {
    Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
    Services.prefs.setBoolPref(
      "network.cookieJarSettings.unblocked_for_testing",
      true
    );
  }

  httpserver = new HttpServer();
  httpserver.registerPathHandler("/set", setHandler);
  httpserver.registerPathHandler("/present", presentHandler);
  httpserver.start(-1);

  do_test_pending();

  function check_cookie(req) {
    req.QueryInterface(Ci.nsIHttpChannel);
    Assert.equal(req.responseStatus, 200);
    try {
      Assert.ok(
        req.getResponseHeader("Set-Cookie") != "",
        "expected a Set-Cookie header"
      );
    } catch (x) {
      do_throw("missing Set-Cookie header");
    }

    runNextTest();
  }

  let tests = [];

  function runNextTest() {
    executeSoon(tests.shift());
  }

  tests.push(function () {
    set_cookie("C1=V1", check_cookie);
  });
  tests.push(function () {
    set_private_cookie("C2=V2", check_cookie);
  });
  tests.push(function () {
    // Check that the first cookie is present in a non-private request
    check_cookie_presence("C1=V1", false, true, runNextTest);
  });
  tests.push(function () {
    // Check that the second cookie is present in a private request
    check_cookie_presence("C2=V2", true, true, runNextTest);
  });
  tests.push(function () {
    // Check that the first cookie is not present in a private request
    check_cookie_presence("C1=V1", true, false, runNextTest);
  });
  tests.push(function () {
    // Check that the second cookie is not present in a non-private request
    check_cookie_presence("C2=V2", false, false, runNextTest);
  });

  // The following test only works in a non-e10s situation at the moment,
  // since the notification needs to run in the parent process but there is
  // no existing mechanism to make that happen.
  if (!inChildProcess()) {
    tests.push(function () {
      // Simulate all private browsing instances being closed
      Services.obs.notifyObservers(null, "last-pb-context-exited");
      // Check that all private cookies are now unavailable in new private requests
      check_cookie_presence("C2=V2", true, false, runNextTest);
    });
  }

  tests.push(function () {
    httpserver.stop(do_test_finished);
  });

  runNextTest();
}
