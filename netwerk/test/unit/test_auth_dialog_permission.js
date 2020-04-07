// This file tests authentication prompt depending on pref
// network.auth.subresource-http-auth-allow:
//   0 - don't allow sub-resources to open HTTP authentication credentials
//       dialogs
//   1 - allow sub-resources to open HTTP authentication credentials dialogs,
//       but don't allow it for cross-origin sub-resources
//   2 - allow the cross-origin authentication as well.

"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

var prefs = Cc["@mozilla.org/preferences-service;1"].getService(
  Ci.nsIPrefBranch
);

// Since this test creates a TYPE_DOCUMENT channel via javascript, it will
// end up using the wrong LoadInfo constructor. Setting this pref will disable
// the ContentPolicyType assertion in the constructor.
prefs.setBoolPref("network.loadinfo.skip_type_assertion", true);

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

var httpserv = new HttpServer();
httpserv.registerPathHandler("/auth", authHandler);
httpserv.start(-1);

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserv.identity.primaryPort;
});

XPCOMUtils.defineLazyGetter(this, "PORT", function() {
  return httpserv.identity.primaryPort;
});

function AuthPrompt(promptExpected) {
  this.promptExpected = promptExpected;
}

AuthPrompt.prototype = {
  user: "guest",
  pass: "guest",

  QueryInterface: ChromeUtils.generateQI(["nsIAuthPrompt"]),

  prompt(title, text, realm, save, defaultText, result) {
    do_throw("unexpected prompt call");
  },

  promptUsernameAndPassword(title, text, realm, savePW, user, pw) {
    Assert.ok(this.promptExpected, "Not expected the authentication prompt.");

    user.value = this.user;
    pw.value = this.pass;
    return true;
  },

  promptPassword(title, text, realm, save, pwd) {
    do_throw("unexpected promptPassword call");
  },
};

function Requestor(promptExpected) {
  this.promptExpected = promptExpected;
}

Requestor.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIInterfaceRequestor"]),

  getInterface(iid) {
    if (iid.equals(Ci.nsIAuthPrompt)) {
      this.prompter = new AuthPrompt(this.promptExpected);
      return this.prompter;
    }

    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  prompter: null,
};

function make_uri(url) {
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  return ios.newURI(url);
}

function makeChan(loadingUrl, url, contentPolicy) {
  var ssm = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(
    Ci.nsIScriptSecurityManager
  );
  var uri = make_uri(loadingUrl);
  var principal = ssm.createContentPrincipal(uri, {});

  return NetUtil.newChannel({
    uri: url,
    loadingPrincipal: principal,
    securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS,
    contentPolicyType: contentPolicy,
  }).QueryInterface(Ci.nsIHttpChannel);
}

function Test(
  subresource_http_auth_allow_pref,
  loadingUri,
  uri,
  contentPolicy,
  expectedCode
) {
  this._subresource_http_auth_allow_pref = subresource_http_auth_allow_pref;
  this._loadingUri = loadingUri;
  this._uri = uri;
  this._contentPolicy = contentPolicy;
  this._expectedCode = expectedCode;
}

Test.prototype = {
  _subresource_http_auth_allow_pref: 1,
  _loadingUri: null,
  _uri: null,
  _contentPolicy: Ci.nsIContentPolicy.TYPE_OTHER,
  _expectedCode: 200,

  onStartRequest(request) {
    try {
      if (!Components.isSuccessCode(request.status)) {
        do_throw("Channel should have a success code!");
      }

      if (!(request instanceof Ci.nsIHttpChannel)) {
        do_throw("Expecting an HTTP channel");
      }

      Assert.equal(request.responseStatus, this._expectedCode);
      // The request should be succeeded iff we expect 200
      Assert.equal(request.requestSucceeded, this._expectedCode == 200);
    } catch (e) {
      do_throw("Unexpected exception: " + e);
    }

    throw Cr.NS_ERROR_ABORT;
  },

  onDataAvailable(request, stream, offset, count) {
    do_throw("Should not get any data!");
  },

  onStopRequest(request, status) {
    Assert.equal(status, Cr.NS_ERROR_ABORT);

    // Clear the auth cache.
    Cc["@mozilla.org/network/http-auth-manager;1"]
      .getService(Ci.nsIHttpAuthManager)
      .clearAll();

    do_timeout(0, run_next_test);
  },

  run() {
    dump(
      "Run test: " +
        this._subresource_http_auth_allow_pref +
        this._loadingUri +
        this._uri +
        this._contentPolicy +
        this._expectedCode +
        " \n"
    );

    prefs.setIntPref(
      "network.auth.subresource-http-auth-allow",
      this._subresource_http_auth_allow_pref
    );
    let chan = makeChan(this._loadingUri, this._uri, this._contentPolicy);
    chan.notificationCallbacks = new Requestor(this._expectedCode == 200);
    chan.asyncOpen(this);
  },
};

var tests = [
  // For the next 3 tests the preference is set to 2 - allow the cross-origin
  // authentication as well.

  // A cross-origin request.
  new Test(
    2,
    "http://example.com",
    URL + "/auth",
    Ci.nsIContentPolicy.TYPE_OTHER,
    200
  ),
  // A non cross-origin sub-resource request.
  new Test(2, URL + "/", URL + "/auth", Ci.nsIContentPolicy.TYPE_OTHER, 200),
  // A top level document.
  new Test(
    2,
    URL + "/auth",
    URL + "/auth",
    Ci.nsIContentPolicy.TYPE_DOCUMENT,
    200
  ),

  // For the next 3 tests the preference is set to 1 - allow sub-resources to
  // open HTTP authentication credentials dialogs, but don't allow it for
  // cross-origin sub-resources

  // A cross-origin request.
  new Test(
    1,
    "http://example.com",
    URL + "/auth",
    Ci.nsIContentPolicy.TYPE_OTHER,
    401
  ),
  // A non cross-origin sub-resource request.
  new Test(1, URL + "/", URL + "/auth", Ci.nsIContentPolicy.TYPE_OTHER, 200),
  // A top level document.
  new Test(
    1,
    URL + "/auth",
    URL + "/auth",
    Ci.nsIContentPolicy.TYPE_DOCUMENT,
    200
  ),

  // For the next 3 tests the preference is set to 0 - don't allow sub-resources
  // to open HTTP authentication credentials dialogs.

  // A cross-origin request.
  new Test(
    0,
    "http://example.com",
    URL + "/auth",
    Ci.nsIContentPolicy.TYPE_OTHER,
    401
  ),
  // A sub-resource request.
  new Test(0, URL + "/", URL + "/auth", Ci.nsIContentPolicy.TYPE_OTHER, 401),
  // A top level request.
  new Test(
    0,
    URL + "/auth",
    URL + "/auth",
    Ci.nsIContentPolicy.TYPE_DOCUMENT,
    200
  ),
];

function run_next_test() {
  var nextTest = tests.shift();
  if (!nextTest) {
    httpserv.stop(do_test_finished);
    return;
  }

  nextTest.run();
}

function run_test() {
  do_test_pending();
  run_next_test();
}
