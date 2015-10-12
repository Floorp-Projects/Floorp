Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserv.identity.primaryPort + "/cached";
});

var httpserv = null;
var handlers_called = 0;

function cached_handler(metadata, response) {
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Cache-Control", "max-age=10000", false);
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  var body = "0123456789";
  response.bodyOutputStream.write(body, body.length);
  handlers_called++;
}

function makeChan(url, appId, inBrowser) {
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var chan = ios.newChannel2(url,
                             null,
                             null,
                             null,      // aLoadingNode
                             Services.scriptSecurityManager.getSystemPrincipal(),
                             null,      // aTriggeringPrincipal
                             Ci.nsILoadInfo.SEC_NORMAL,
                             Ci.nsIContentPolicy.TYPE_OTHER).QueryInterface(Ci.nsIHttpChannel);
  chan.notificationCallbacks = {
    appId: appId,
    isInBrowserElement: inBrowser,
    originAttributes: {
      appId: appId,
      inBrowser: inBrowser,
    },
    QueryInterface: function(iid) {
      if (iid.equals(Ci.nsILoadContext))
        return this;
      throw Cr.NS_ERROR_NO_INTERFACE;
    },
    getInterface: function(iid) { return this.QueryInterface(iid); }
  };
  return chan;
}

var firstTests = [[0, false, 1], [0, true, 1], [1, false, 1], [1, true, 1]];
var secondTests = [[0, false, 0], [0, true, 0], [1, false, 0], [1, true, 1]];
var thirdTests = [[0, false, 0], [0, true, 0], [1, false, 1], [1, true, 1]];

function run_all_tests() {
  for (let test of firstTests) {
    handlers_called = 0;
    var chan = makeChan(URL, test[0], test[1]);
    chan.asyncOpen(new ChannelListener(doneFirstLoad, test[2]), null);
    yield undefined;
  }

  // We can't easily cause webapp data to be cleared from the child process, so skip
  // the rest of these tests.
  let procType = Cc["@mozilla.org/xre/runtime;1"].getService(Ci.nsIXULRuntime).processType;
  if (procType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT)
    return;

  let attrs_inBrowser = JSON.stringify({ appId:1, inBrowser:true });
  let attrs_notInBrowser = JSON.stringify({ appId:1 });

  Services.obs.notifyObservers(null, "clear-origin-data", attrs_inBrowser);

  for (let test of secondTests) {
    handlers_called = 0;
    var chan = makeChan(URL, test[0], test[1]);
    chan.asyncOpen(new ChannelListener(doneFirstLoad, test[2]), null);
    yield undefined;
  }

  Services.obs.notifyObservers(null, "clear-origin-data", attrs_notInBrowser);
  Services.obs.notifyObservers(null, "clear-origin-data", attrs_inBrowser);

  for (let test of thirdTests) {
    handlers_called = 0;
    var chan = makeChan(URL, test[0], test[1]);
    chan.asyncOpen(new ChannelListener(doneFirstLoad, test[2]), null);
    yield undefined;
  }
}

var gTests;
function run_test() {
  do_get_profile();
  do_test_pending();
  httpserv = new HttpServer();
  httpserv.registerPathHandler("/cached", cached_handler);
  httpserv.start(-1);
  gTests = run_all_tests();
  gTests.next();
}

function doneFirstLoad(req, buffer, expected) {
  // Load it again, make sure it hits the cache
  var chan = makeChan(URL, 0, false);
  chan.asyncOpen(new ChannelListener(doneSecondLoad, expected), null);
}

function doneSecondLoad(req, buffer, expected) {
  do_check_eq(handlers_called, expected);
  try {
    gTests.next();
  } catch (x) {
    do_test_finished();
  }
}
