Cu.import("resource://gre/modules/NetUtil.jsm");

function inChildProcess() {
  return Cc["@mozilla.org/xre/app-info;1"]
           .getService(Ci.nsIXULRuntime)
           .processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}

function makeChan(url, appId, inIsolatedMozBrowser) {
  var chan = NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true})
                    .QueryInterface(Ci.nsIHttpChannel);
  chan.loadInfo.originAttributes = { appId: appId,
                                     inIsolatedMozBrowser: inIsolatedMozBrowser
                                   };
  return chan;
}

// Simple online load
function run_test() {
  do_test_pending();
  var chan = makeChan("http://localhost:12345/first", 14, false);
  chan.asyncOpen2(new ChannelListener(checkResponse, "response0"));
}

// Should return cached result
function test1() {
  do_test_pending();
  var chan = makeChan("http://localhost:12345/first", 14, false);
  chan.asyncOpen2(new ChannelListener(checkResponse, "response0"));
}

// This request should fail
function test2() {
  do_test_pending();
  var chan = makeChan("http://localhost:12345/second", 14, false);
  chan.asyncOpen2(new ChannelListener(checkResponse, "", CL_EXPECT_FAILURE));
}

// This request should succeed
function test3() {
  do_test_pending();
  var chan = makeChan("http://localhost:12345/second", 14, false);
  chan.asyncOpen2(new ChannelListener(checkResponse, "response3"));
}

function checkResponse(req, buffer, expected) {
  do_check_eq(buffer, expected);
  do_test_finished();
}
