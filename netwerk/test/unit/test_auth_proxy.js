/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This tests the automatic login to the proxy with password,
 * if the password is stored and the browser is restarted.
 *
 * <copied from="test_authentication.js"/>
 */

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

const FLAG_RETURN_FALSE = 1 << 0;
const FLAG_WRONG_PASSWORD = 1 << 1;
const FLAG_PREVIOUS_FAILED = 1 << 2;

function AuthPrompt2(proxyFlags, hostFlags) {
  this.proxyCred.flags = proxyFlags;
  this.hostCred.flags = hostFlags;
}
AuthPrompt2.prototype = {
  proxyCred: {
    user: "proxy",
    pass: "guest",
    realmExpected: "intern",
    flags: 0,
  },
  hostCred: { user: "host", pass: "guest", realmExpected: "extern", flags: 0 },

  QueryInterface: ChromeUtils.generateQI(["nsIAuthPrompt2"]),

  promptAuth: function ap2_promptAuth(channel, encryptionLevel, authInfo) {
    try {
      // never HOST and PROXY set at the same time in prompt
      Assert.equal(
        (authInfo.flags & Ci.nsIAuthInformation.AUTH_HOST) != 0,
        (authInfo.flags & Ci.nsIAuthInformation.AUTH_PROXY) == 0
      );

      var isProxy = (authInfo.flags & Ci.nsIAuthInformation.AUTH_PROXY) != 0;
      var cred = isProxy ? this.proxyCred : this.hostCred;

      dump(
        "with flags: " +
          ((cred.flags & FLAG_WRONG_PASSWORD) != 0 ? "wrong password" : "") +
          " " +
          ((cred.flags & FLAG_PREVIOUS_FAILED) != 0 ? "previous failed" : "") +
          " " +
          ((cred.flags & FLAG_RETURN_FALSE) != 0 ? "return false" : "") +
          "\n"
      );

      // PROXY properly set by necko (checked using realm)
      Assert.equal(cred.realmExpected, authInfo.realm);

      // PREVIOUS_FAILED properly set by necko
      Assert.equal(
        (cred.flags & FLAG_PREVIOUS_FAILED) != 0,
        (authInfo.flags & Ci.nsIAuthInformation.PREVIOUS_FAILED) != 0
      );

      if (cred.flags & FLAG_RETURN_FALSE) {
        cred.flags |= FLAG_PREVIOUS_FAILED;
        cred.flags &= ~FLAG_RETURN_FALSE;
        return false;
      }

      authInfo.username = cred.user;
      if (cred.flags & FLAG_WRONG_PASSWORD) {
        authInfo.password = cred.pass + ".wrong";
        cred.flags |= FLAG_PREVIOUS_FAILED;
        // Now clear the flag to avoid an infinite loop
        cred.flags &= ~FLAG_WRONG_PASSWORD;
      } else {
        authInfo.password = cred.pass;
        cred.flags &= ~FLAG_PREVIOUS_FAILED;
      }
    } catch (e) {
      do_throw(e);
    }
    return true;
  },

  asyncPromptAuth: function ap2_async(
    channel,
    callback,
    context,
    encryptionLevel,
    authInfo
  ) {
    var me = this;
    var allOverAndDead = false;
    executeSoon(function () {
      try {
        if (allOverAndDead) {
          throw new Error("already canceled");
        }
        var ret = me.promptAuth(channel, encryptionLevel, authInfo);
        if (!ret) {
          callback.onAuthCancelled(context, true);
        } else {
          callback.onAuthAvailable(context, authInfo);
        }
        allOverAndDead = true;
      } catch (e) {
        do_throw(e);
      }
    });
    return new Cancelable(function () {
      if (allOverAndDead) {
        throw new Error("can't cancel, already ran");
      }
      callback.onAuthAvailable(context, authInfo);
      allOverAndDead = true;
    });
  },
};

function Cancelable(onCancelFunc) {
  this.onCancelFunc = onCancelFunc;
}
Cancelable.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsICancelable"]),
  cancel: function cancel() {
    try {
      this.onCancelFunc();
    } catch (e) {
      do_throw(e);
    }
  },
};

function Requestor(proxyFlags, hostFlags) {
  this.proxyFlags = proxyFlags;
  this.hostFlags = hostFlags;
}
Requestor.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIInterfaceRequestor"]),

  getInterface: function requestor_gi(iid) {
    if (iid.equals(Ci.nsIAuthPrompt)) {
      dump("authprompt1 not implemented\n");
      throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
    }
    if (iid.equals(Ci.nsIAuthPrompt2)) {
      try {
        // Allow the prompt to store state by caching it here
        if (!this.prompt2) {
          this.prompt2 = new AuthPrompt2(this.proxyFlags, this.hostFlags);
        }
        return this.prompt2;
      } catch (e) {
        do_throw(e);
      }
    }
    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
  },

  prompt2: null,
};

var listener = {
  expectedCode: -1, // uninitialized

  onStartRequest: function test_onStartR(request) {
    try {
      // Proxy auth cancellation return failures to avoid spoofing
      if (
        !Components.isSuccessCode(request.status) &&
        this.expectedCode != 407
      ) {
        do_throw("Channel should have a success code!");
      }

      if (!(request instanceof Ci.nsIHttpChannel)) {
        do_throw("Expecting an HTTP channel");
      }

      Assert.equal(this.expectedCode, request.responseStatus);
      // If we expect 200, the request should have succeeded
      Assert.equal(this.expectedCode == 200, request.requestSucceeded);

      var cookie = "";
      try {
        cookie = request.getRequestHeader("Cookie");
      } catch (e) {}
      Assert.equal(cookie, "");
    } catch (e) {
      do_throw("Unexpected exception: " + e);
    }

    throw Components.Exception("", Cr.NS_ERROR_ABORT);
  },

  onDataAvailable: function test_ODA() {
    do_throw("Should not get any data!");
  },

  onStopRequest: function test_onStopR(request, status) {
    Assert.equal(status, Cr.NS_ERROR_ABORT);

    if (current_test < tests.length - 1) {
      // First, need to clear the auth cache
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
  },
};

function makeChan(url) {
  if (!url) {
    url = "http://somesite/";
  }

  return NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
}

var current_test = 0;
var httpserv = null;

function run_test() {
  httpserv = new HttpServer();
  httpserv.registerPathHandler("/", proxyAuthHandler);
  httpserv.identity.add("http", "somesite", 80);
  httpserv.start(-1);

  Services.prefs.setCharPref("network.proxy.http", "localhost");
  Services.prefs.setIntPref(
    "network.proxy.http_port",
    httpserv.identity.primaryPort
  );
  Services.prefs.setCharPref("network.proxy.no_proxies_on", "");
  Services.prefs.setIntPref("network.proxy.type", 1);

  // Turn off the authentication dialog blocking for this test.
  Services.prefs.setIntPref("network.auth.subresource-http-auth-allow", 2);
  Services.prefs.setBoolPref(
    "network.auth.non-web-content-triggered-resources-http-auth-allow",
    true
  );

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("network.proxy.http");
    Services.prefs.clearUserPref("network.proxy.http_port");
    Services.prefs.clearUserPref("network.proxy.no_proxies_on");
    Services.prefs.clearUserPref("network.proxy.type");
    Services.prefs.clearUserPref("network.auth.subresource-http-auth-allow");
    Services.prefs.clearUserPref(
      "network.auth.non-web-content-triggered-resources-http-auth-allow"
    );
  });

  tests[current_test]();
}

function test_proxy_returnfalse() {
  dump("\ntest: proxy returnfalse\n");
  var chan = makeChan();
  chan.notificationCallbacks = new Requestor(FLAG_RETURN_FALSE, 0);
  listener.expectedCode = 407; // Proxy Unauthorized
  chan.asyncOpen(listener);

  do_test_pending();
}

function test_proxy_wrongpw() {
  dump("\ntest: proxy wrongpw\n");
  var chan = makeChan();
  chan.notificationCallbacks = new Requestor(FLAG_WRONG_PASSWORD, 0);
  listener.expectedCode = 200; // Eventually OK
  chan.asyncOpen(listener);
  do_test_pending();
}

function test_all_ok() {
  dump("\ntest: all ok\n");
  var chan = makeChan();
  chan.notificationCallbacks = new Requestor(0, 0);
  listener.expectedCode = 200; // OK
  chan.asyncOpen(listener);
  do_test_pending();
}

function test_proxy_407_cookie() {
  var chan = makeChan();
  chan.notificationCallbacks = new Requestor(FLAG_RETURN_FALSE, 0);
  chan.setRequestHeader("X-Set-407-Cookie", "1", false);
  listener.expectedCode = 407; // Proxy Unauthorized
  chan.asyncOpen(listener);

  do_test_pending();
}

function test_proxy_200_cookie() {
  var chan = makeChan();
  chan.notificationCallbacks = new Requestor(0, 0);
  chan.setRequestHeader("X-Set-407-Cookie", "1", false);
  listener.expectedCode = 200; // OK
  chan.asyncOpen(listener);
  do_test_pending();
}

function test_host_returnfalse() {
  dump("\ntest: host returnfalse\n");
  var chan = makeChan();
  chan.notificationCallbacks = new Requestor(0, FLAG_RETURN_FALSE);
  listener.expectedCode = 401; // Host Unauthorized
  chan.asyncOpen(listener);

  do_test_pending();
}

function test_host_wrongpw() {
  dump("\ntest: host wrongpw\n");
  var chan = makeChan();
  chan.notificationCallbacks = new Requestor(0, FLAG_WRONG_PASSWORD);
  listener.expectedCode = 200; // Eventually OK
  chan.asyncOpen(listener);
  do_test_pending();
}

function test_proxy_wrongpw_host_wrongpw() {
  dump("\ntest: proxy wrongpw, host wrongpw\n");
  var chan = makeChan();
  chan.notificationCallbacks = new Requestor(
    FLAG_WRONG_PASSWORD,
    FLAG_WRONG_PASSWORD
  );
  listener.expectedCode = 200; // OK
  chan.asyncOpen(listener);
  do_test_pending();
}

function test_proxy_wrongpw_host_returnfalse() {
  dump("\ntest: proxy wrongpw, host return false\n");
  var chan = makeChan();
  chan.notificationCallbacks = new Requestor(
    FLAG_WRONG_PASSWORD,
    FLAG_RETURN_FALSE
  );
  listener.expectedCode = 401; // Host Unauthorized
  chan.asyncOpen(listener);
  do_test_pending();
}

var tests = [
  test_proxy_returnfalse,
  test_proxy_wrongpw,
  test_all_ok,
  test_proxy_407_cookie,
  test_proxy_200_cookie,
  test_host_returnfalse,
  test_host_wrongpw,
  test_proxy_wrongpw_host_wrongpw,
  test_proxy_wrongpw_host_returnfalse,
];

// PATH HANDLERS

// Proxy
function proxyAuthHandler(metadata, response) {
  try {
    var realm = "intern";
    // btoa("proxy:guest"), but that function is not available here
    var expectedHeader = "Basic cHJveHk6Z3Vlc3Q=";

    var body;
    if (
      metadata.hasHeader("Proxy-Authorization") &&
      metadata.getHeader("Proxy-Authorization") == expectedHeader
    ) {
      dump("proxy password ok\n");
      response.setHeader(
        "Proxy-Authenticate",
        'Basic realm="' + realm + '"',
        false
      );

      hostAuthHandler(metadata, response);
    } else {
      dump("proxy password required\n");
      response.setStatusLine(
        metadata.httpVersion,
        407,
        "Unauthorized by HTTP proxy"
      );
      response.setHeader(
        "Proxy-Authenticate",
        'Basic realm="' + realm + '"',
        false
      );
      if (metadata.hasHeader("X-Set-407-Cookie")) {
        response.setHeader("Set-Cookie", "chewy", false);
      }
      body = "failed";
      response.bodyOutputStream.write(body, body.length);
    }
  } catch (e) {
    do_throw(e);
  }
}

// Host /auth
function hostAuthHandler(metadata, response) {
  try {
    var realm = "extern";
    // btoa("host:guest"), but that function is not available here
    var expectedHeader = "Basic aG9zdDpndWVzdA==";

    var body;
    if (
      metadata.hasHeader("Authorization") &&
      metadata.getHeader("Authorization") == expectedHeader
    ) {
      dump("host password ok\n");
      response.setStatusLine(
        metadata.httpVersion,
        200,
        "OK, authorized for host"
      );
      response.setHeader(
        "WWW-Authenticate",
        'Basic realm="' + realm + '"',
        false
      );
      body = "success";
    } else {
      dump("host password required\n");
      response.setStatusLine(
        metadata.httpVersion,
        401,
        "Unauthorized by HTTP server host"
      );
      response.setHeader(
        "WWW-Authenticate",
        'Basic realm="' + realm + '"',
        false
      );
      body = "failed";
    }
    response.bodyOutputStream.write(body, body.length);
  } catch (e) {
    do_throw(e);
  }
}
