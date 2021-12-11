/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PromptTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromptTestUtils.jsm"
);

function whenBrowserLoaded(browser, callback) {
  return BrowserTestUtils.browserLoaded(browser).then(callback);
}

function waitForOnBeforeUnloadDialog(browser, callback) {
  PromptTestUtils.waitForPrompt(browser, {
    modalType: Services.prompt.MODAL_TYPE_CONTENT,
    promptType: "confirmEx",
  }).then(dialog => {
    SimpleTest.waitForCondition(
      () => Services.focus.activeWindow == browser.ownerGlobal,
      function() {
        let { button0, button1 } = dialog.ui;
        callback(button0, button1);
      },
      "Waited too long for window with dialog to focus"
    );
  });
}
