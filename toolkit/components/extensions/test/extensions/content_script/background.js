browser.runtime.onMessage.addListener(([msg, expectedState, readyState], sender) => {
  if (msg == "chrome-namespace-ok") {
    browser.test.sendMessage(msg);
    return;
  }

  browser.test.assertEq(msg, "script-run", "message type is correct");
  browser.test.assertEq(readyState, expectedState, "readyState is correct");
  browser.test.sendMessage("script-run-" + expectedState);
});
