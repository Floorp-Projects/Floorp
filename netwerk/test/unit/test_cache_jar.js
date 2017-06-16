Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

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

function makeChan(url, appId, inIsolatedMozBrowser, userContextId) {
  var chan = NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true})
                    .QueryInterface(Ci.nsIHttpChannel);
  chan.loadInfo.originAttributes = { appId: appId,
                                     inIsolatedMozBrowser: inIsolatedMozBrowser,
                                     userContextId: userContextId,
                                   };
  return chan;
}

// [appId, inIsolatedMozBrowser, userContextId, expected_handlers_called]
var firstTests = [
  [0, false, 0, 1], [0, true, 0, 1], [1, false, 0, 1], [1, true, 0, 1],
  [0, false, 1, 1], [0, true, 1, 1], [1, false, 1, 1], [1, true, 1, 1]
];
var secondTests = [
  [0, false, 0, 0], [0, true, 0, 0], [1, false, 0, 0], [1, true, 0, 1],
  [0, false, 1, 0], [0, true, 1, 0], [1, false, 1, 0], [1, true, 1, 0]
];
var thirdTests = [
  [0, false, 0, 0], [0, true, 0, 0], [1, false, 0, 1], [1, true, 0, 1],
  [0, false, 1, 0], [0, true, 1, 0], [1, false, 1, 0], [1, true, 1, 0]
];
var fourthTests = [
  [0, false, 0, 0], [0, true, 0, 0], [1, false, 0, 0], [1, true, 0, 0],
  [0, false, 1, 1], [0, true, 1, 0], [1, false, 1, 0], [1, true, 1, 0]
];

async function run_all_tests() {
  for (let test of firstTests) {
    handlers_called = 0;
    await test_channel(...test);
  }

  // We can't easily cause webapp data to be cleared from the child process, so skip
  // the rest of these tests.
  let procType = Cc["@mozilla.org/xre/runtime;1"].getService(Ci.nsIXULRuntime).processType;
  if (procType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT)
    return;

  let attrs_inBrowser = JSON.stringify({ appId:1, inIsolatedMozBrowser:true });
  let attrs_notInBrowser = JSON.stringify({ appId:1 });

  Services.obs.notifyObservers(null, "clear-origin-attributes-data", attrs_inBrowser);

  for (let test of secondTests) {
    handlers_called = 0;
    await test_channel(...test);
  }

  Services.obs.notifyObservers(null, "clear-origin-attributes-data", attrs_notInBrowser);
  Services.obs.notifyObservers(null, "clear-origin-attributes-data", attrs_inBrowser);

  for (let test of thirdTests) {
    handlers_called = 0;
    await test_channel(...test);
  }

  let attrs_userContextId = JSON.stringify({ userContextId: 1 });
  Services.obs.notifyObservers(null, "clear-origin-attributes-data", attrs_userContextId);

  for (let test of fourthTests) {
    handlers_called = 0;
    await test_channel(...test);
  }
}

function run_test() {
  do_get_profile();
  if (!newCacheBackEndUsed()) {
    do_check_true(true, "This test checks only cache2 specific behavior.");
    return;
  }
  do_test_pending();

  Services.prefs.setBoolPref("network.http.rcwn.enabled", false);

  httpserv = new HttpServer();
  httpserv.registerPathHandler("/cached", cached_handler);
  httpserv.start(-1);
  run_all_tests().then(() => {
    do_test_finished();
  });
}

function test_channel(appId, inIsolatedMozBrowser, userContextId, expected) {
  return new Promise(resolve => {
    var chan = makeChan(URL, appId, inIsolatedMozBrowser, userContextId);
    chan.asyncOpen2(new ChannelListener(doneFirstLoad.bind(null, resolve), expected));
  });
}

function doneFirstLoad(resolve, req, buffer, expected) {
  // Load it again, make sure it hits the cache
  var oa = req.loadInfo.originAttributes;
  var chan = makeChan(URL, oa.appId, oa.isInIsolatedMozBrowserElement, oa.userContextId);
  chan.asyncOpen2(new ChannelListener(doneSecondLoad.bind(null, resolve), expected));
}

function doneSecondLoad(resolve, req, buffer, expected) {
  do_check_eq(handlers_called, expected);
  resolve();
}
