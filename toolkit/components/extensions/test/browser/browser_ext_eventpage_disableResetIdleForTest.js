"use strict";

const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);

const { AppUiTestDelegate } = ChromeUtils.importESModule(
  "resource://testing-common/AppUiTestDelegate.sys.mjs"
);

// Ignore error "Actor 'Conduits' destroyed before query 'RunListener' was resolved"
PromiseTestUtils.allowMatchingRejectionsGlobally(
  /Actor 'Conduits' destroyed before query 'RunListener'/
);

async function run_test_disableResetIdleForTest(options) {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 3,
      action: {},
    },
    background() {
      browser.action.onClicked.addListener(async () => {
        browser.test.notifyPass("action-clicked");
        // Deliberately keep this listener active to simulate a still active listener
        // callback, while calling extension.terminateBackground().
        await new Promise(() => {});
      });

      browser.test.sendMessage("background-ready");
    },
  });

  await extension.startup();
  await extension.awaitMessage("background-ready");
  // After startup, the listener should be persistent but not primed.
  assertPersistentListeners(extension, "browserAction", "onClicked", {
    primed: false,
  });

  // Terminating the background should prime the persistent listener.
  await extension.terminateBackground();
  assertPersistentListeners(extension, "browserAction", "onClicked", {
    primed: true,
  });

  // Wake up the background, and verify the listener is no longer primed.
  await AppUiTestDelegate.clickBrowserAction(window, extension.id);
  await extension.awaitFinish("action-clicked");
  await AppUiTestDelegate.closeBrowserAction(window, extension.id);
  await extension.awaitMessage("background-ready");
  assertPersistentListeners(extension, "browserAction", "onClicked", {
    primed: false,
  });

  // Terminate the background again, while the onClicked listener is still
  // being executed.
  // With options.disableResetIdleForTest = true, the termination should NOT
  // be skipped and the listener should become primed again.
  // With options.disableResetIdleForTest = false or unset, the termination
  // should be skipped and the listener should not become primed.
  await extension.terminateBackground(options);
  assertPersistentListeners(extension, "browserAction", "onClicked", {
    primed: !!options?.disableResetIdleForTest,
  });

  await extension.unload();
}

// Verify default behaviour when terminating a background while a
// listener is still running: The background should not be terminated
// and the listener should not become primed. Not specifyiny a value
// for disableResetIdleForTest defauls to disableResetIdleForTest:false.
add_task(async function test_disableResetIdleForTest_default() {
  await run_test_disableResetIdleForTest();
});

// Verify that disableResetIdleForTest:true is honoured and terminating
// a background while a listener is still running is enforced: The
// background should be terminated and the listener should become primed.
add_task(async function test_disableResetIdleForTest_true() {
  await run_test_disableResetIdleForTest({ disableResetIdleForTest: true });
});
