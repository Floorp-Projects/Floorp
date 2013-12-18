Cu.import("resource://testing-common/httpd.js");

const SERVER_PORT = 8080;
const baseURL = "http://localhost:" + SERVER_PORT + "/";
const kResponseTimeoutPref = "network.http.response.timeout";
const kResponseTimeout = 1;

const prefService = Cc["@mozilla.org/preferences-service;1"]
                       .getService(Ci.nsIPrefBranch);

var server = new HttpServer();

function TimeoutListener(enableTimeout, nextTest) {
  this.enableTimeout = enableTimeout;
  this.nextTest = nextTest;
}

TimeoutListener.prototype = {
  onStartRequest: function (request, ctx) {
  },

  onDataAvailable: function (request, ctx, stream) {
  },

  onStopRequest: function (request, ctx, status) {
    if (this.enableTimeout) {
      do_check_eq(status, Cr.NS_ERROR_NET_TIMEOUT);
    } else {
      do_check_eq(status, Cr.NS_OK);
    }

    if (this.nextTest) {
      this.nextTest();
    } else {
      server.stop(serverStopListener);
    }
  },
};

function serverStopListener() {
  do_test_finished();
}

function testTimeout(timeoutEnabled, nextTest) {
  // Set timeout pref.
  if (timeoutEnabled) {
    prefService.setIntPref(kResponseTimeoutPref, kResponseTimeout);
  } else {
    prefService.setIntPref(kResponseTimeoutPref, 0);
  }

  var ios = Cc["@mozilla.org/network/io-service;1"]
  .getService(Ci.nsIIOService);
  var chan = ios.newChannel(baseURL, null, null)
  .QueryInterface(Ci.nsIHttpChannel);
  var listener = new TimeoutListener(timeoutEnabled, nextTest);
  chan.asyncOpen(listener, null);
}

function testTimeoutEnabled() {
  testTimeout(true, testTimeoutDisabled);
}

function testTimeoutDisabled() {
  testTimeout(false, null);
}

function run_test() {
  // Start server; will be stopped after second test.
  server.start(SERVER_PORT);
  server.registerPathHandler('/', function(metadata, response) {
    // Wait until the timeout should have passed, then respond.
    response.processAsync();
    timer = Cc["@mozilla.org/timer;1"]
    .createInstance(Components.interfaces.nsITimer);
    timer.initWithCallback(function() {
      response.setStatusLine(metadata.httpVersion, 200, "OK");
      response.write("Hello world");
      response.finish();
    }, (kResponseTimeout+1)*1000 /* ms */, Ci.nsITimer.TYPE_ONE_SHOT);
  });

  // First test checks timeout enabled.
  // Second test (in callback) checks timeout disabled.
  testTimeoutEnabled();

  do_test_pending();
}

