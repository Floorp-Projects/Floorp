/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the authorization header can get deleted e.g. by
// extensions if they are observing "http-on-modify-request". In a first step
// the auth cache is filled with credentials which then get added to the
// following request. On "http-on-modify-request" it is tested whether the
// authorization header got added at all and if so it gets removed. This test
// passes iff both succeeds.

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

Components.utils.import("resource://testing-common/httpd.js");

var notification = "http-on-modify-request";

var httpServer = null;

var authCredentials = "guest:guest";
var authPath = "/authTest";
var authCredsURL = "http://" + authCredentials + "@localhost:8888" + authPath;
var authURL = "http://localhost:8888" + authPath;

function authHandler(metadata, response) {
  if (metadata.hasHeader("Test")) {
    // Lets see if the auth header got deleted.
    var noAuthHeader = false;
    if (!metadata.hasHeader("Authorization")) {
      noAuthHeader = true;
    }
    do_check_true(noAuthHeader);
  } else {
    // Not our test request yet.
    if (!metadata.hasHeader("Authorization")) {
      response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
      response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);
    }
  }
}

function RequestObserver() {
  this.register();
}

RequestObserver.prototype = {
  register: function() {
    do_print("Registering " + notification);
    Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService).
      addObserver(this, notification, true);
  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIObserver) || iid.equals(Ci.nsISupportsWeakReference) ||
        iid.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  observe: function(subject, topic, data) {
    if (topic == notification) {
      if (!(subject instanceof Ci.nsIHttpChannel)) {
        do_throw(notification + " observed a non-HTTP channel.");
      }
      try {
        let authHeader = subject.getRequestHeader("Authorization");
      } catch (e) {
        // Throw if there is no header to delete. We should get one iff caching
        // the auth credentials is working and the header gets added _before_
        // "http-on-modify-request" gets called.
        httpServer.stop(do_test_finished);
        do_throw("No authorization header found, aborting!");
      }
      // We are still here. Let's remove the authorization header now.
      subject.setRequestHeader("Authorization", null, false);
    }
  }
}

var listener = {
  onStartRequest: function test_onStartR(request, ctx) {},

  onDataAvailable: function test_ODA() {
    do_throw("Should not get any data!");
  },

  onStopRequest: function test_onStopR(request, ctx, status) {
    if (current_test < (tests.length - 1)) {
      current_test++;
      tests[current_test]();
    } else {
      do_test_pending();
      httpServer.stop(do_test_finished);
    }
    do_test_finished();
  }
};

function makeChan(url) {
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var chan = ios.newChannel(url, null, null).QueryInterface(Ci.nsIHttpChannel);
  return chan;
}

var tests = [startAuthHeaderTest, removeAuthHeaderTest];

var current_test = 0;

var requestObserver = null;

function run_test() {
  httpServer = new HttpServer();
  httpServer.registerPathHandler(authPath, authHandler);
  httpServer.start(8888);

  tests[0]();
}

function startAuthHeaderTest() {
  var chan = makeChan(authCredsURL);
  chan.asyncOpen(listener, null);

  do_test_pending();
}

function removeAuthHeaderTest() {
  // After caching the auth credentials in the first test, lets try to remove
  // the authorization header now...
  requestObserver = new RequestObserver();
  var chan = makeChan(authURL);
  // Indicating that the request is coming from the second test.
  chan.setRequestHeader("Test", "1", false);
  chan.asyncOpen(listener, null);

  do_test_pending();
}
