"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

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

function makeChan(url, inIsolatedMozBrowser, userContextId) {
  var chan = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadInfo.originAttributes = { inIsolatedMozBrowser, userContextId };
  return chan;
}

// [inIsolatedMozBrowser, userContextId, expected_handlers_called]
var firstTests = [
  [false, 0, 1],
  [true, 0, 1],
  [false, 1, 1],
  [true, 1, 1],
];
var secondTests = [
  [false, 0, 0],
  [true, 0, 0],
  [false, 1, 1],
  [true, 1, 0],
];

async function run_all_tests() {
  for (let test of firstTests) {
    handlers_called = 0;
    await test_channel(...test);
  }

  // We can't easily cause webapp data to be cleared from the child process, so skip
  // the rest of these tests.
  let procType = Cc["@mozilla.org/xre/runtime;1"].getService(Ci.nsIXULRuntime)
    .processType;
  if (procType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
    return;
  }

  Services.clearData.deleteDataFromOriginAttributesPattern({
    userContextId: 1,
  });

  for (let test of secondTests) {
    handlers_called = 0;
    await test_channel(...test);
  }
}

function run_test() {
  do_get_profile();

  do_test_pending();

  Services.prefs.setBoolPref("network.http.rcwn.enabled", false);

  httpserv = new HttpServer();
  httpserv.registerPathHandler("/cached", cached_handler);
  httpserv.start(-1);
  run_all_tests().then(() => {
    do_test_finished();
  });
}

function test_channel(inIsolatedMozBrowser, userContextId, expected) {
  return new Promise(resolve => {
    var chan = makeChan(URL, inIsolatedMozBrowser, userContextId);
    chan.asyncOpen(
      new ChannelListener(doneFirstLoad.bind(null, resolve), expected)
    );
  });
}

function doneFirstLoad(resolve, req, buffer, expected) {
  // Load it again, make sure it hits the cache
  var oa = req.loadInfo.originAttributes;
  var chan = makeChan(URL, oa.isInIsolatedMozBrowserElement, oa.userContextId);
  chan.asyncOpen(
    new ChannelListener(doneSecondLoad.bind(null, resolve), expected)
  );
}

function doneSecondLoad(resolve, req, buffer, expected) {
  Assert.equal(handlers_called, expected);
  resolve();
}
