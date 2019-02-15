"use strict";

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

// This holds information about the test that is currently running.
// We can only call nsIHttpChannel.crossOriginOpenerPolicyMismatch
// in the main process, so for each channel opened in the child process we need
// to perform the check in the parent. We intercept it during onStartRequest
// with the "http-on-examine-response" observer notification.
let gData = null;

function observer(subject, topic, state) {
  info("observer called with " + topic);
  let chan = subject.QueryInterface(Ci.nsIHttpChannel);
  equal(chan.channelId, gData.channelId);
  equal(chan.hasCrossOriginOpenerPolicyMismatch(), gData.expectedResult, `check for mismatch testing ${gData.policyA}, ${gData.originA}, ${gData.policyB}, ${gData.originB}, ${gData.expectedResult}`);
}

function waitForTest() {
  do_await_remote_message("prepare-test").then((data) => {
    info(`prepare test: ${data}`);
    gData = data;
    do_send_remote_message("test-ready");

    // So we don't hang, the child must send a final message with no data
    // so we can clear the listener. Otherwise we call waitForTest again.
    if (!data) {
      info("parent test finishing");
      return;
    }
    waitForTest();
  });
}

function run_test() {
  Services.obs.addObserver(observer, "http-on-examine-response");
  Services.prefs.setCharPref("network.dns.localDomains", "foo.example.com, example.com, example.org");

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("network.dns.localDomains");
    Services.obs.removeObserver(observer, "http-on-examine-response");
  });

  waitForTest();

  run_test_in_child("../unit/test_crossOriginOpenerPolicy.js");
}
