"use strict";
let testURL = chromeRoot + "browser_focus.html";
let newTab = null;

function test() {
  waitForExplicitFinish();

  registerCleanupFunction(function() {
    try {
      messageManager.sendAsyncMessage("Test:E10SFocusTestFinished", {});
      Browser.closeTab(newTab);
    } finally {
      newTab = null;
    }
  });

  messageManager.addMessageListener("pageshow", function listener(aMessage) {
    if (newTab && newTab.browser.currentURI.spec != "about:blank") {
      messageManager.removeMessageListener("pageshow", listener);
      setTimeout(onTabLoaded, 0);
    }
  });

  newTab = Browser.addTab(testURL, true);
}

function onTabLoaded() {
  // ensure that the <browser> is not already focused
  newTab.browser.blur();
  messageManager.loadFrameScript(chromeRoot + "remote_focus.js", false);
  testFocus();
}

function testFocus() {
  onMessageOnce(messageManager, "Test:E10SFocusReceived", function() {
    ok("Focus in <browser remote> triggered activateRemoteFrame as expected");
    testBlur();
  });
  newTab.browser.focus();
}

function testBlur() {
  onMessageOnce(messageManager, "Test:E10SBlurReceived", function() {
    ok("Blur in <browser remote> triggerered deactivateRemoteFrame as expected");
    endTest();
  });
  newTab.browser.blur();
}

function endTest() {
  finish();
}
