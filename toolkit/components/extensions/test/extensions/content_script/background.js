browser.runtime.onMessage.addListener(([msg, expectedState, readyState], sender) => {
  browser.test.assertEq(msg, "script-run", "message type is correct");
  browser.test.assertEq(readyState, expectedState, "readyState is correct");
  browser.test.sendMessage("script-run-" + expectedState);
});
