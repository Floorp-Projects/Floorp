Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://testing-common/UrlClassifierTestUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://testing-common/httpd.js");

// This test supports both e10s and non-e10s mode. In non-e10s mode, this test
// drives itself by creating a profile directory, setting up the URL classifier
// test tables and adjusting the prefs which are necessary to do the testing.
// In e10s mode however, some of these operations such as creating a profile
// directory, setting up the URL classifier test tables and setting prefs
// aren't supported in the content process, so we split the test into two
// parts, the part testing the normal priority case by setting both prefs to
// false (test_trackingProtection_annotateChannels_wrap1.js), and the part
// testing the lowest priority case by setting both prefs to true
// (test_trackingProtection_annotateChannels_wrap2.js).  These wrapper scripts
// are also in charge of creating the profile directory and setting up the URL
// classifier test tables.
//
// Below where we need to take different actions based on the process type we're
// in, we use runtime.processType to take the correct actions.

const runtime = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime);
if (runtime.processType == runtime.PROCESS_TYPE_DEFAULT) {
  do_get_profile();
}

const topWindowURI = NetUtil.newURI("http://www.itisatrap.org/");

var Ci = Components.interfaces;

function listener(tracking, priority, nextTest) {
  this._tracking = tracking;
  this._priority = priority;
  this._nextTest = nextTest;
}
listener.prototype = {
  onStartRequest: function(request, context) {
    do_check_eq(request.QueryInterface(Ci.nsIHttpChannel).isTrackingResource,
                this._tracking);
    do_check_eq(request.QueryInterface(Ci.nsISupportsPriority).priority,
                this._priority);
    if (runtime.processType == runtime.PROCESS_TYPE_DEFAULT && this._tracking) {
      do_check_true(request.QueryInterface(Ci.nsIClassOfService).classFlags &
                    Ci.nsIClassOfService.Throttleable);
    }
    request.cancel(Components.results.NS_ERROR_ABORT);
    this._nextTest();
  },
  onDataAvailable: function(request, context, stream, offset, count) {
  },
  onStopRequest: function(request, context, status) {
  }
};

var httpServer;
var normalOrigin, trackingOrigin;
var testPriorityMap;
var currentTest;
// When this test is running in e10s mode, the parent process is in charge of
// setting the prefs for us, so here we merely read our prefs, and if they have
// been set we skip the normal priority test and only test the lowest priority
// case, and if it they have not been set we skip the lowest priority test and
// only test the normal priority case.
// In non-e10s mode, both of these will remain false and we adjust the prefs
// ourselves and test both of the cases in one go.
var skipNormalPriority = false, skipLowestPriority = false;

function setup_test() {
  httpServer = new HttpServer();
  httpServer.start(-1);
  httpServer.identity.setPrimary("http", "tracking.example.com", httpServer.identity.primaryPort);
  httpServer.identity.add("http", "example.com", httpServer.identity.primaryPort);
  normalOrigin = "http://localhost:" + httpServer.identity.primaryPort;
  trackingOrigin = "http://tracking.example.com:" + httpServer.identity.primaryPort;

  if (runtime.processType == runtime.PROCESS_TYPE_CONTENT) {
    if (Services.prefs.getBoolPref("privacy.trackingprotection.annotate_channels") &&
        Services.prefs.getBoolPref("privacy.trackingprotection.lower_network_priority")) {
      skipNormalPriority = true;
    } else {
      skipLowestPriority = true;
    }
  }

  runTests();
}

function doPriorityTest() {
  if (!testPriorityMap.length) {
    runTests();
    return;
  }

  currentTest = testPriorityMap.shift();
  var channel = makeChannel(currentTest.path);
  channel.asyncOpen2(new listener(currentTest.expectedTracking,
                                  currentTest.expectedPriority,
                                  doPriorityTest));
}

function makeChannel(path) {
  var chan = NetUtil.newChannel({
    uri: path,
    loadUsingSystemPrincipal: true
  });
  chan.QueryInterface(Ci.nsIHttpChannel);
  chan.requestMethod = "GET";
  chan.loadFlags |= Ci.nsIChannel.LOAD_CLASSIFY_URI;
  chan.QueryInterface(Ci.nsIHttpChannelInternal).setTopWindowURIIfUnknown(topWindowURI);
  return chan;
}

var tests =[
  // Create the HTTP server.
  setup_test,

  // Add the test table into tracking protection table.
  function addTestTrackers() {
    if (runtime.processType == runtime.PROCESS_TYPE_DEFAULT) {
      UrlClassifierTestUtils.addTestTrackers().then(() => {
        runTests();
      });
    } else {
      runTests();
    }
  },

  // With the pref off, the priority of channel should be normal.
  function setupNormalPriority() {
    if (skipNormalPriority) {
      runTests();
      return;
    }
    if (runtime.processType == runtime.PROCESS_TYPE_DEFAULT) {
      Services.prefs.setBoolPref("privacy.trackingprotection.annotate_channels", false);
      Services.prefs.setBoolPref("privacy.trackingprotection.lower_network_priority", false);
    }
    testPriorityMap = [
      {
        path: normalOrigin + "/innocent.css",
        expectedTracking: false,
        expectedPriority: Ci.nsISupportsPriority.PRIORITY_NORMAL
      },
      {
        path: normalOrigin + "/innocent.js",
        expectedTracking: false,
        expectedPriority: Ci.nsISupportsPriority.PRIORITY_NORMAL
      },
      {
        path: trackingOrigin + "/evil.css",
        expectedTracking: false,
        expectedPriority: Ci.nsISupportsPriority.PRIORITY_NORMAL
      },
      {
        path: trackingOrigin + "/evil.js",
        expectedTracking: false,
        expectedPriority: Ci.nsISupportsPriority.PRIORITY_NORMAL
      },
    ];
    // We add the doPriorityTest test here so that it only gets injected in the
    // test list if we're not skipping over this test.
    tests.unshift(doPriorityTest);
    runTests();
  },

  // With the pref on, the priority of channel should be lowest.
  function setupLowestPriority() {
    if (skipLowestPriority) {
      runTests();
      return;
    }
    if (runtime.processType == runtime.PROCESS_TYPE_DEFAULT) {
      Services.prefs.setBoolPref("privacy.trackingprotection.annotate_channels", true);
      Services.prefs.setBoolPref("privacy.trackingprotection.lower_network_priority", true);
    }
    testPriorityMap = [
      {
        path: normalOrigin + "/innocent.css",
        expectedTracking: false,
        expectedPriority: Ci.nsISupportsPriority.PRIORITY_NORMAL
      },
      {
        path: normalOrigin + "/innocent.js",
        expectedTracking: false,
        expectedPriority: Ci.nsISupportsPriority.PRIORITY_NORMAL
      },
      {
        path: trackingOrigin + "/evil.css",
        expectedTracking: true,
        expectedPriority: Ci.nsISupportsPriority.PRIORITY_LOWEST
      },
      {
        path: trackingOrigin + "/evil.js",
        expectedTracking: true,
        expectedPriority: Ci.nsISupportsPriority.PRIORITY_LOWEST
      },
    ];
    // We add the doPriorityTest test here so that it only gets injected in the
    // test list if we're not skipping over this test.
    tests.unshift(doPriorityTest);
    runTests();
  },

  function cleanUp() {
    httpServer.stop(do_test_finished);
    if (runtime.processType == runtime.PROCESS_TYPE_DEFAULT) {
      UrlClassifierTestUtils.cleanupTestTrackers();
    }
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
