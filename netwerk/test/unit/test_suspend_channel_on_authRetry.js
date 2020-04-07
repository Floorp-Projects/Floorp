// This file tests async handling of a channel suspended in DoAuthRetry
// notifying http-on-modify-request and http-on-before-connect observers.
"use strict";

var CC = Components.Constructor;

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

// Turn off the authentication dialog blocking for this test.
var prefs = Cc["@mozilla.org/preferences-service;1"].getService(
  Ci.nsIPrefBranch
);

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserv.identity.primaryPort;
});

XPCOMUtils.defineLazyGetter(this, "PORT", function() {
  return httpserv.identity.primaryPort;
});

var obs = Cc["@mozilla.org/observer-service;1"].getService(
  Ci.nsIObserverService
);

var requestObserver = null;

function AuthPrompt() {}

AuthPrompt.prototype = {
  user: "guest",
  pass: "guest",

  QueryInterface: ChromeUtils.generateQI(["nsIAuthPrompt"]),

  prompt: function ap1_prompt(title, text, realm, save, defaultText, result) {
    do_throw("unexpected prompt call");
  },

  promptUsernameAndPassword: function promptUP(
    title,
    text,
    realm,
    savePW,
    user,
    pw
  ) {
    user.value = this.user;
    pw.value = this.pass;

    obs.addObserver(requestObserver, "http-on-before-connect");
    obs.addObserver(requestObserver, "http-on-modify-request");
    return true;
  },

  promptPassword: function promptPW(title, text, realm, save, pwd) {
    do_throw("unexpected promptPassword call");
  },
};

function requestListenerObserver(
  suspendOnBeforeConnect,
  suspendOnModifyRequest
) {
  this.suspendOnModifyRequest = suspendOnModifyRequest;
  this.suspendOnBeforeConnect = suspendOnBeforeConnect;
}

requestListenerObserver.prototype = {
  suspendOnModifyRequest: false,
  suspendOnBeforeConnect: false,
  gotOnBeforeConnect: false,
  resumeOnBeforeConnect: false,
  gotOnModifyRequest: false,
  resumeOnModifyRequest: false,
  QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),

  observe(subject, topic, data) {
    if (
      topic === "http-on-before-connect" &&
      subject instanceof Ci.nsIHttpChannel
    ) {
      if (this.suspendOnBeforeConnect) {
        var chan = subject.QueryInterface(Ci.nsIHttpChannel);
        executeSoon(() => {
          this.resumeOnBeforeConnect = true;
          chan.resume();
        });
        this.gotOnBeforeConnect = true;
        chan.suspend();
      }
    } else if (
      topic === "http-on-modify-request" &&
      subject instanceof Ci.nsIHttpChannel
    ) {
      if (this.suspendOnModifyRequest) {
        var chan = subject.QueryInterface(Ci.nsIHttpChannel);
        executeSoon(() => {
          this.resumeOnModifyRequest = true;
          chan.resume();
        });
        this.gotOnModifyRequest = true;
        chan.suspend();
      }
    }
  },
};

function Requestor() {}

Requestor.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIInterfaceRequestor"]),

  getInterface: function requestor_gi(iid) {
    if (iid.equals(Ci.nsIAuthPrompt)) {
      // Allow the prompt to store state by caching it here
      if (!this.prompt) {
        this.prompt = new AuthPrompt();
      }
      return this.prompt;
    }

    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  prompt: null,
};

var listener = {
  expectedCode: -1, // Uninitialized

  onStartRequest: function test_onStartR(request) {
    try {
      if (!Components.isSuccessCode(request.status)) {
        do_throw("Channel should have a success code!");
      }

      if (!(request instanceof Ci.nsIHttpChannel)) {
        do_throw("Expecting an HTTP channel");
      }

      Assert.equal(request.responseStatus, this.expectedCode);
      // The request should be succeeded iff we expect 200
      Assert.equal(request.requestSucceeded, this.expectedCode == 200);
    } catch (e) {
      do_throw("Unexpected exception: " + e);
    }
    throw Cr.NS_ERROR_ABORT;
  },

  onDataAvailable: function test_ODA() {
    do_throw("Should not get any data!");
  },

  onStopRequest: function test_onStopR(request, status) {
    Assert.equal(status, Cr.NS_ERROR_ABORT);
    if (requestObserver.suspendOnBeforeConnect) {
      Assert.ok(
        requestObserver.gotOnBeforeConnect &&
          requestObserver.resumeOnBeforeConnect
      );
    }
    if (requestObserver.suspendOnModifyRequest) {
      Assert.ok(
        requestObserver.gotOnModifyRequest &&
          requestObserver.resumeOnModifyRequest
      );
    }
    obs.removeObserver(requestObserver, "http-on-before-connect");
    obs.removeObserver(requestObserver, "http-on-modify-request");
    moveToNextTest();
  },
};

function makeChan(url, loadingUrl) {
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var ssm = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(
    Ci.nsIScriptSecurityManager
  );
  var principal = ssm.createContentPrincipal(ios.newURI(loadingUrl), {});
  return NetUtil.newChannel({
    uri: url,
    loadingPrincipal: principal,
    securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
  });
}

var tests = [
  test_suspend_on_before_connect,
  test_suspend_on_modify_request,
  test_suspend_all,
];

var current_test = 0;

var httpserv = null;

function moveToNextTest() {
  if (current_test < tests.length - 1) {
    // First, gotta clear the auth cache
    Cc["@mozilla.org/network/http-auth-manager;1"]
      .getService(Ci.nsIHttpAuthManager)
      .clearAll();

    current_test++;
    tests[current_test]();
  } else {
    do_test_pending();
    httpserv.stop(do_test_finished);
  }

  do_test_finished();
}

function run_test() {
  httpserv = new HttpServer();

  httpserv.registerPathHandler("/auth", authHandler);

  httpserv.start(-1);

  tests[0]();
}

function test_suspend_on_auth(suspendOnBeforeConnect, suspendOnModifyRequest) {
  var chan = makeChan(URL + "/auth", URL);
  requestObserver = new requestListenerObserver(
    suspendOnBeforeConnect,
    suspendOnModifyRequest
  );
  chan.notificationCallbacks = new Requestor();
  listener.expectedCode = 200; // OK
  chan.asyncOpen(listener);

  do_test_pending();
}

function test_suspend_on_before_connect() {
  test_suspend_on_auth(true, false);
}

function test_suspend_on_modify_request() {
  test_suspend_on_auth(false, true);
}

function test_suspend_all() {
  test_suspend_on_auth(true, true);
}

// PATH HANDLERS

// /auth
function authHandler(metadata, response) {
  // btoa("guest:guest"), but that function is not available here
  var expectedHeader = "Basic Z3Vlc3Q6Z3Vlc3Q=";

  var body;
  if (
    metadata.hasHeader("Authorization") &&
    metadata.getHeader("Authorization") == expectedHeader
  ) {
    response.setStatusLine(metadata.httpVersion, 200, "OK, authorized");
    response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);

    body = "success";
  } else {
    // didn't know guest:guest, failure
    response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
    response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);

    body = "failed";
  }

  response.bodyOutputStream.write(body, body.length);
}
