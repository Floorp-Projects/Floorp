// This file ensures that canceling a channel early does not
// send the request to the server (bug 350790)
//
// I've also shoehorned in a test that ENSURE_CALLED_BEFORE_CONNECT works as
// expected: see comments that start with ENSURE_CALLED_BEFORE_CONNECT:

const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");

var ios = Cc["@mozilla.org/network/io-service;1"]
            .getService(Ci.nsIIOService);
var ReferrerInfo = Components.Constructor("@mozilla.org/referrer-info;1",
                                          "nsIReferrerInfo",
                                          "init");
var observer = {
  QueryInterface: function eventsink_qi(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsIObserver))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  observe: function(subject, topic, data) {
    subject = subject.QueryInterface(Ci.nsIRequest);
    subject.cancel(Cr.NS_BINDING_ABORTED);

    // ENSURE_CALLED_BEFORE_CONNECT: setting values should still work
    try {
      subject.QueryInterface(Ci.nsIHttpChannel);
      currentReferrer = subject.getRequestHeader("Referer");
      Assert.equal(currentReferrer, "http://site1.com/");
      var uri = ios.newURI("http://site2.com");
      subject.referrerInfo = new ReferrerInfo(Ci.nsIHttpChannel.REFERRER_POLICY_UNSET, true, uri);
    } catch (ex) {
      do_throw("Exception: " + ex);
    }

    var obs = Cc["@mozilla.org/observer-service;1"].getService();
    obs = obs.QueryInterface(Ci.nsIObserverService);
    obs.removeObserver(observer, "http-on-modify-request");
  },
};

var listener = {
  onStartRequest: function test_onStartR(request) {
    Assert.equal(request.status, Cr.NS_BINDING_ABORTED);

    // ENSURE_CALLED_BEFORE_CONNECT: setting referrer should now fail
    try {
      request.QueryInterface(Ci.nsIHttpChannel);
      currentReferrer = request.getRequestHeader("Referer");
      Assert.equal(currentReferrer, "http://site2.com/");
      var uri = ios.newURI("http://site3.com/");

      // Need to set NECKO_ERRORS_ARE_FATAL=0 else we'll abort process
      var env = Cc["@mozilla.org/process/environment;1"].
                  getService(Ci.nsIEnvironment);
      env.set("NECKO_ERRORS_ARE_FATAL", "0");
      // we expect setting referrer to fail
      try {
        request.referrerInfo = new ReferrerInfo(Ci.nsIHttpChannel.REFERRER_POLICY_UNSET, true, uri);
        do_throw("Error should have been thrown before getting here");
      } catch (ex) { }
    } catch (ex) {
      do_throw("Exception: " + ex);
    }
  },

  onDataAvailable: function test_ODA() {
    do_throw("Should not get any data!");
  },

  onStopRequest: function test_onStopR(request, status) {
    httpserv.stop(do_test_finished);
  },
};

function makeChan(url) {
  var chan = NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true})
                    .QueryInterface(Ci.nsIHttpChannel);

  // ENSURE_CALLED_BEFORE_CONNECT: set original value
  var uri = ios.newURI("http://site1.com");
  chan.referrerInfo = new ReferrerInfo(Ci.nsIHttpChannel.REFERRER_POLICY_UNSET, true, uri);
  return chan;
}

var httpserv = null;

function execute_test() {
  var chan = makeChan("http://localhost:" +
                      httpserv.identity.primaryPort + "/failtest");

  var obs = Cc["@mozilla.org/observer-service;1"].getService();
  obs = obs.QueryInterface(Ci.nsIObserverService);
  obs.addObserver(observer, "http-on-modify-request");

  chan.asyncOpen(listener);
}

function run_test() {
  httpserv = new HttpServer();
  httpserv.registerPathHandler("/failtest", failtest);
  httpserv.start(-1);

  execute_test();

  do_test_pending();
}

// PATHS

// /failtest
function failtest(metadata, response) {
  do_throw("This should not be reached");
}
