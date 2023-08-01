// This file tests bug 250375

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

ChromeUtils.defineLazyGetter(this, "URL", function () {
  return "http://localhost:" + httpserv.identity.primaryPort + "/";
});

function inChildProcess() {
  return Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}

function check_request_header(chan, name, value) {
  var chanValue;
  try {
    chanValue = chan.getRequestHeader(name);
  } catch (e) {
    do_throw(
      "Expected to find header '" +
        name +
        "' but didn't find it, got exception: " +
        e
    );
  }
  dump("Value for header '" + name + "' is '" + chanValue + "'\n");
  Assert.equal(chanValue, value);
}

var cookieVal = "C1=V1";

var listener = {
  onStartRequest: function test_onStartR(request) {
    try {
      var chan = request.QueryInterface(Ci.nsIHttpChannel);
      check_request_header(chan, "Cookie", cookieVal);
    } catch (e) {
      do_throw("Unexpected exception: " + e);
    }

    throw Components.Exception("", Cr.NS_ERROR_ABORT);
  },

  onDataAvailable: function test_ODA() {
    throw Components.Exception("", Cr.NS_ERROR_UNEXPECTED);
  },

  onStopRequest: async function test_onStopR(request, status) {
    if (this._iteration == 1) {
      await run_test_continued();
    } else {
      do_test_pending();
      httpserv.stop(do_test_finished);
    }
    do_test_finished();
  },

  _iteration: 1,
};

function makeChan() {
  return NetUtil.newChannel({
    uri: URL,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
}

var httpserv = null;

function run_test() {
  // Allow all cookies if the pref service is available in this process.
  if (!inChildProcess()) {
    Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
    Services.prefs.setBoolPref(
      "network.cookieJarSettings.unblocked_for_testing",
      true
    );
  }

  httpserv = new HttpServer();
  httpserv.start(-1);

  var chan = makeChan();

  chan.setRequestHeader("Cookie", cookieVal, false);

  chan.asyncOpen(listener);

  do_test_pending();
}

async function run_test_continued() {
  var chan = makeChan();

  var cookie2 = "C2=V2";

  await CookieXPCShellUtils.setCookieToDocument(chan.URI.spec, cookie2);

  chan.setRequestHeader("Cookie", cookieVal, false);

  // We expect that the setRequestHeader overrides the
  // automatically-added one, so insert cookie2 in front
  cookieVal = cookie2 + "; " + cookieVal;

  listener._iteration++;
  chan.asyncOpen(listener);

  do_test_pending();
}
