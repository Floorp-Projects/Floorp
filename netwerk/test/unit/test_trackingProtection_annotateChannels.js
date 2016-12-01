Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://testing-common/UrlClassifierTestUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://testing-common/httpd.js");

do_get_profile();

var Ci = Components.interfaces;

function listener(priority, nextTest) {
  this._priority = priority;
  this._nextTest = nextTest;
}
listener.prototype = {
  onStartRequest: function(request, context) {
    do_check_eq(request.QueryInterface(Ci.nsISupportsPriority).priority,
                this._priority);
    this._nextTest();
  },
  onDataAvailable: function(request, context, stream, offset, count) {
  },
  onStopRequest: function(request, context, status) {
  }
};

var httpServer;
var origin;
var testPriorityMap;
var currentTest;

function setup_test() {
  httpServer = new HttpServer();
  httpServer.start(-1);
  httpServer.identity.setPrimary("http", "tracking.example.com", httpServer.identity.primaryPort);
  origin = "http://tracking.example.com:" + httpServer.identity.primaryPort;

  runTests();
}

function doPriorityTest() {
  if (!testPriorityMap.length) {
    runTests();
    return;
  }

  currentTest = testPriorityMap.shift();
  var channel = makeChannel(currentTest.path);
  channel.asyncOpen2(new listener(currentTest.expectedPriority, doPriorityTest));
}

function makeChannel(path) {
  var chan = NetUtil.newChannel({
    uri: path,
    loadUsingSystemPrincipal: true
  });
  chan.QueryInterface(Ci.nsIHttpChannel);
  chan.requestMethod = "GET";
  chan.loadFlags |= Ci.nsIChannel.LOAD_CLASSIFY_URI;
  return chan;
}

var tests =[
  // Create the HTTP server.
  setup_test,

  // Add the test table into tracking protection table.
  function addTestTrackers() {
    UrlClassifierTestUtils.addTestTrackers().then(() => {
      runTests();
    });
  },

  // With the pref off, the priority of channel should be normal.
  function setupNormalPriority() {
    Services.prefs.setBoolPref("privacy.trackingprotection.annotate_channels", false);
    testPriorityMap = [
      {
        path: origin + "/evil.css",
        expectedPriority: Ci.nsISupportsPriority.PRIORITY_NORMAL
      },
      {
        path: origin + "/evil.js",
        expectedPriority: Ci.nsISupportsPriority.PRIORITY_NORMAL
      },
    ];
    runTests();
  },
  doPriorityTest,

  // With the pref on, the priority of channel should be lowest.
  function setupLowestPriority() {
    Services.prefs.setBoolPref("privacy.trackingprotection.annotate_channels", true);
    testPriorityMap = [
      {
        path: origin + "/evil.css",
        expectedPriority: Ci.nsISupportsPriority.PRIORITY_LOWEST
      },
      {
        path: origin + "/evil.js",
        expectedPriority: Ci.nsISupportsPriority.PRIORITY_LOWEST
      },
    ];
    runTests();
  },
  doPriorityTest,

  function cleanUp() {
    httpServer.stop(do_test_finished);
    UrlClassifierTestUtils.cleanupTestTrackers();
    runTests();
  }
];

function runTests()
{
  if (!tests.length) {
    do_test_finished();
    return;
  }

  var test = tests.shift();
  test();
}

function run_test() {
  runTests();
  do_test_pending();
}
