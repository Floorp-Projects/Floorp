// This file tests bug 250375

const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserv.identity.primaryPort + "/";
});

function inChildProcess() {
  return Cc["@mozilla.org/xre/app-info;1"]
           .getService(Ci.nsIXULRuntime)
           .processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;  
}

function check_request_header(chan, name, value) {
  var chanValue;
  try {
    chanValue = chan.getRequestHeader(name);
  } catch (e) {
    do_throw("Expected to find header '" + name + "' but didn't find it, got exception: " + e);
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

    throw Cr.NS_ERROR_ABORT;
  },

  onDataAvailable: function test_ODA() {
    throw Cr.NS_ERROR_UNEXPECTED;
  },

  onStopRequest: function test_onStopR(request, status) {
    if (this._iteration == 1) {
      run_test_continued();
    } else {
      do_test_pending();
      httpserv.stop(do_test_finished);
    }
    do_test_finished();
  },

  _iteration: 1
};

function makeChan() {
  return NetUtil.newChannel({uri: URL, loadUsingSystemPrincipal: true})
                .QueryInterface(Ci.nsIHttpChannel);
}

var httpserv = null;

function run_test() {
  // Allow all cookies if the pref service is available in this process.
  if (!inChildProcess()) {
    Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
    Services.prefs.setBoolPref("network.cookieSettings.unblocked_for_testing", true);
  }

  httpserv = new HttpServer();
  httpserv.start(-1);

  var chan = makeChan();

  chan.setRequestHeader("Cookie", cookieVal, false);

  chan.asyncOpen(listener);

  do_test_pending();
}

function run_test_continued() {
  var chan = makeChan();

  var cookServ = Cc["@mozilla.org/cookieService;1"]
                   .getService(Ci.nsICookieService);
  var cookie2 = "C2=V2";
  cookServ.setCookieString(chan.URI, null, cookie2, chan);
  chan.setRequestHeader("Cookie", cookieVal, false);

  // We expect that the setRequestHeader overrides the
  // automatically-added one, so insert cookie2 in front
  cookieVal = cookie2 + "; " + cookieVal;

  listener._iteration++;
  chan.asyncOpen(listener);

  do_test_pending();
}
