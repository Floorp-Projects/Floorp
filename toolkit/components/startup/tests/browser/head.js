/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function whenBrowserLoaded(browser, callback) {
  return BrowserTestUtils.browserLoaded(browser).then(callback);
}

function waitForOnBeforeUnloadDialog(browser, callback) {
  browser.addEventListener(
    "DOMWillOpenModalDialog",
    function onModalDialog(event) {
      if (Cu.isCrossProcessWrapper(event.target)) {
        // This event fires in both the content and chrome processes. We
        // want to ignore the one in the content process.
        return;
      }

      browser.removeEventListener(
        "DOMWillOpenModalDialog",
        onModalDialog,
        true
      );

      SimpleTest.waitForCondition(
        () => Services.focus.activeWindow == browser.ownerGlobal,
        function() {
          let prompt = browser.tabModalPromptBox.listPrompts()[0];
          let { button0, button1 } = prompt.ui;
          callback(button0, button1);
        },
        "Waited too long for window with dialog to focus"
      );
    },
    true
  );
}
