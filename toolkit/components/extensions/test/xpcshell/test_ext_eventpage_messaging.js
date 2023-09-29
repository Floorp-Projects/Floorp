"use strict";

// This test checks that the suspension of the event page is delayed when the
// runtime.onConnect / runtime.onMessage events are involved.
//
// Another test (test_ext_eventpage_messaging_wakeup.js) verifies that the event
// page wakes up when these events are to be triggered.

const { ExtensionTestCommon } = ChromeUtils.importESModule(
  "resource://testing-common/ExtensionTestCommon.sys.mjs"
);

add_setup(() => {
  // In this test, we want to verify that an idle timeout is reset when
  // extension messages are set. To avoid waiting for too long, reduce the
  // default timeout to a short value. To avoid premature test termination,
  // this timeout should be sufficiently large to run the relevant logic that
  // is supposed to postpone event page termination:
  // - The idle timer starts when the background has loaded.
  // - The idle timer should reset when expected by tests.
  Services.prefs.setIntPref("extensions.background.idle.timeout", 1000);
});

async function loadEventPageAndExtensionPage({
  backgroundScript,
  extensionPageScript,
}) {
  let extension = ExtensionTestUtils.loadExtension({
    // Delay startup, to ensure that the event page does not suspend until we
    // have started the extension page that runs extensionPageScript.
    startupReason: "APP_STARTUP",
    // APP_STARTUP is not enough, delayedStartup is needed (bug 1756225).
    delayedStartup: true,
    manifest: {
      background: { persistent: false },
    },
    background: backgroundScript,
    files: {
      "page.html": `<!DOCTYPE html><script src="page.js"></script>`,
      "page.js": extensionPageScript,
    },
  });

  // Delay event page startup until notifyEarlyStartup+notifyLateStartup below.
  await ExtensionTestCommon.resetStartupPromises();
  await extension.startup();

  // Start extension page first, so that it can register runtime.onSuspend
  // before there is any chance of encountering a suspended event page.
  let contentPage = await ExtensionTestUtils.loadContentPage(
    `moz-extension://${extension.uuid}/page.html`
  );

  info("Extension page loaded, permitting event page to start up");
  await ExtensionTestCommon.notifyEarlyStartup();
  await ExtensionTestCommon.notifyLateStartup();

  await extension.awaitMessage("FINAL_SUSPEND");
  info("Received FINAL_SUSPEND, awaiting full event page shutdown.");
  await promiseExtensionEvent(extension, "shutdown-background-script");
  await contentPage.close();
  await extension.unload();
}

add_task(async function test_runtime_onMessage_cancels_suspend() {
  await loadEventPageAndExtensionPage({
    backgroundScript() {
      // This background script registers listeners without calling any other
      // extension API. This ensures that if the event page suspend is canceled,
      // that it was intentionally done by the listener, and not as a side
      // effect of an unrelated extension API call.
      browser.runtime.onMessage.addListener(msg => {
        return Promise.resolve(`bg_pong:${msg}`);
      });
    },
    extensionPageScript() {
      let cancelCount = 0;
      browser.runtime.onSuspendCanceled.addListener(() => {
        browser.test.assertEq(1, ++cancelCount, "onSuspendCanceled 1x");
      });
      let suspendCount = 0;
      let firstSuspendTestCompleted = false;
      browser.runtime.onSuspend.addListener(async () => {
        // We expect 2x suspend: first one we cancel, second one is final.
        if (++suspendCount === 1) {
          // First suspend attempt.
          browser.test.assertEq(0, cancelCount, "Not suspended yet");
          let res = await browser.runtime.sendMessage("ping");
          browser.test.assertEq(1, cancelCount, "onMessage cancels suspend");
          browser.test.assertEq("bg_pong:ping", res, "onMessage result");
          firstSuspendTestCompleted = true;
        } else {
          browser.test.assertTrue(firstSuspendTestCompleted, "First test done");
          browser.test.assertEq(2, suspendCount, "Second onSuspend");
          browser.test.sendMessage("FINAL_SUSPEND");
        }
      });
      browser.test.log("Waiting for background to be suspended");
    },
  });
});

add_task(async function test_runtime_onConnect_cancels_suspend() {
  await loadEventPageAndExtensionPage({
    backgroundScript() {
      // This background script registers listeners without calling any other
      // extension API. This ensures that if the event page suspend is canceled,
      // that it was intentionally done by the listener, and not as a side
      // effect of an unrelated extension API call.
      browser.runtime.onConnect.addListener(port => {
        // Set by extensionPageScript before runtime.connect():
        globalThis.notify_extensionPage_got_onConnect();
      });
    },
    extensionPageScript() {
      let cancelCount = 0;
      browser.runtime.onSuspendCanceled.addListener(() => {
        browser.test.assertEq(1, ++cancelCount, "onSuspendCanceled 1x");
      });
      let suspendCount = 0;
      let firstSuspendTestCompleted = false;
      let port; // Prevent port from being gc'd during test.
      browser.runtime.onSuspend.addListener(async () => {
        // We expect 2x suspend: first one we cancel, second one is final.
        if (++suspendCount === 1) {
          // First suspend attempt.
          browser.test.assertEq(0, cancelCount, "Not suspended yet");
          // Call runtime.connect() twice:
          // 1. First connect() should be triggering the reset.
          // 2. We are immediately notified when runtime.onConnect is called.
          // 2. We call connect() again to have another page->parent->background
          //    roundtrip. This ensures that enough time to have been passed to
          //    allow the first runtime.onConnect handling to have finished,
          //    and to have triggeres onSuspendCanceled as desired.
          for (let i = 0; i < 2; ++i) {
            await new Promise(resolve => {
              let bgGlobal = browser.extension.getBackgroundPage();
              browser.test.assertTrue(!!bgGlobal, "Event page still running");
              bgGlobal.notify_extensionPage_got_onConnect = resolve;
              port = browser.runtime.connect({});
            });
          }
          browser.test.assertEq(1, cancelCount, "onConnect cancels suspend");
          firstSuspendTestCompleted = true;
        } else {
          browser.test.assertTrue(firstSuspendTestCompleted, "First test done");
          browser.test.assertEq(2, suspendCount, "Second onSuspend");
          browser.test.assertEq(null, port.error, "port has no error");
          browser.test.sendMessage("FINAL_SUSPEND");
        }
      });
      browser.test.log("Waiting for background to be suspended");
    },
  });
});

add_task(async function test_runtime_Port_onMessage_cancels_suspend() {
  await loadEventPageAndExtensionPage({
    backgroundScript() {
      // This background script registers listeners without calling any other
      // extension API. This ensures that if the event page suspend is canceled,
      // that it was intentionally done by the listener, and not as a side
      // effect of an unrelated extension API call.
      browser.runtime.onConnect.addListener(port => {
        port.onMessage.addListener(msg => {
          // Set by extensionPageScript before runtime.connect():
          globalThis.notify_extensionPage_got_port_onMessage();
        });
      });
    },
    extensionPageScript() {
      let cancelCount = 0;
      browser.runtime.onSuspendCanceled.addListener(() => {
        browser.test.assertEq(1, ++cancelCount, "onSuspendCanceled 1x");
      });
      let suspendCount = 0;
      let firstSuspendTestCompleted = false;
      let port;
      browser.runtime.onSuspend.addListener(async () => {
        // We expect 2x suspend: first one we cancel, second one is final.
        if (++suspendCount === 1) {
          // First suspend attempt.
          browser.test.assertEq(0, cancelCount, "Not suspended yet");
          browser.test.assertTrue(!!port, "Should run after we opened a port");
          // Call port.postMessage() twice:
          // 1. First port.postMessage() should be triggering the reset.
          // 2. We are immediately notified when runtime.onMessage is called.
          // 2. We postMessage() again to have another page->parent->background
          //    roundtrip. This ensures that enough time to have been passed to
          //    allow the first port.onMessage handling to have finished,
          //    and to have triggeres onSuspendCanceled as desired.
          for (let i = 0; i < 2; ++i) {
            await new Promise(resolve => {
              let bgGlobal = browser.extension.getBackgroundPage();
              browser.test.assertTrue(!!bgGlobal, "Event page still running");
              bgGlobal.notify_extensionPage_got_port_onMessage = resolve;
              port.postMessage("");
            });
          }
          browser.test.assertEq(
            1,
            cancelCount,
            "port.onMessage cancels suspend"
          );
          firstSuspendTestCompleted = true;
        } else {
          browser.test.assertTrue(firstSuspendTestCompleted, "First test done");
          browser.test.assertEq(2, suspendCount, "Second onSuspend");
          browser.test.sendMessage("FINAL_SUSPEND");
        }
      });
      browser.runtime.getBackgroundPage(bgGlobal => {
        browser.test.assertTrue(!!bgGlobal, "Event page has started");
        // Since the event page has started, this should trigger onConnect in
        // the event page. If somehow the event page has suspended in the
        // meantime, then we will detect that in runtime.onSuspend (and fail).
        port = browser.runtime.connect({});
        // Assuming that runtime.onConnect in the event page has received the
        // port and started listening, we should now wait for an attempt to
        // suspend the event page (and try to cancel that via port.onMessage).
        browser.test.log("Waiting for background to be suspended");
      });
    },
  });
});
