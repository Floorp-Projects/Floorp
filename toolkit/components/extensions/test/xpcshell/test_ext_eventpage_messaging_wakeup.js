"use strict";

// This test checks that the event page is awakened as expected when a
// extension messaging event is triggered.
// - onMessage / onConnect: triggered by the extension's own extension page.
// - onMessage / onConnect: triggered by the extension's own content script.
// - onMessageExternal / onConnectExternal: triggered by another extension.
//
// Note: the behavior in persistent background scripts (at browser startup) is
// covered by test_ext_messaging_startup.js.
//
// These events delay suspend of the event page, which is partially verified by
// test_ext_eventpage_messaging.js.

const { ExtensionTestCommon } = ChromeUtils.importESModule(
  "resource://testing-common/ExtensionTestCommon.sys.mjs"
);

const server = createHttpServer({ hosts: ["example.com"] });
server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
});

async function testEventPageWakeup({
  backgroundScript,
  triggerScript,
  triggerFromContentScript = false, // default to calling from extension page.
  triggerFromOtherExtension = false, // default to calling from own extension.
  skipInitialPriming = false, // default to start+suspend before starting test.
}) {
  function loadExtension(id, withBackground, withTrigger) {
    let extensionData = {
      manifest: { browser_specific_settings: { gecko: { id } } },
    };
    if (withBackground) {
      extensionData.manifest.background = { persistent: false };
      extensionData.background = backgroundScript;
      if (skipInitialPriming) {
        // Delay event page startup until an explicit event.
        extensionData.startupReason = "APP_STARTUP";
        // APP_STARTUP is not enough, delayedStartup is needed (bug 1756225).
        extensionData.delayedStartup = true;
      }
    }
    if (withTrigger) {
      extensionData.files = {
        "trigger.html": `<!DOCTYPE html><script src="trigger.js"></script>`,
        "trigger.js": triggerScript,
      };
      if (triggerFromContentScript) {
        // trigger.html unused, use content script instead:
        extensionData.manifest.content_scripts = [
          {
            js: ["trigger.js"],
            run_at: "document_end",
            matches: ["http://example.com/dummy"],
          },
        ];
      }
    }
    return ExtensionTestUtils.loadExtension(extensionData);
  }

  await ExtensionTestCommon.resetStartupPromises();
  // Event-triggered wakeup is blocked on browserPaintedPromise, so unblock it:
  await ExtensionTestCommon.notifyEarlyStartup();

  let extension = loadExtension("@ext", true, !triggerFromOtherExtension);
  await extension.startup();
  if (!skipInitialPriming) {
    // Default test behavior is ADDON_INSTALL, which starts up the background.
    Assert.equal(extension.extension.backgroundState, "running", "Bg started");
    // Not strictly needed because the background has already started. But to
    // more closely match reality, flag as fully started.
    await ExtensionTestCommon.notifyLateStartup();
    // Suspend event page so we can verify that it wakes up by triggerScript.
    await extension.terminateBackground();
    Assert.equal(extension.extension.backgroundState, "stopped", "Bg closed");
  } else {
    // Event page initially suspended, waiting for event.
    Assert.equal(extension.extension.backgroundState, "stopped", "Bg inactive");
  }

  let extension2;
  if (triggerFromOtherExtension) {
    extension2 = loadExtension("@other-ext", false, true);
    await extension2.startup();
  }

  let url;
  if (triggerFromContentScript) {
    url = "http://example.com/dummy";
  } else if (triggerFromOtherExtension) {
    url = `moz-extension://${extension2.uuid}/trigger.html`;
  } else {
    url = `moz-extension://${extension.uuid}/trigger.html`;
  }
  let contentPage = await ExtensionTestUtils.loadContentPage(url);
  info("Waiting for event page to be awakened by event");
  await extension.awaitMessage("TRIGGER_TEST_DONE");
  // Unload test extensions first to avoid an issue on Windows platforms.
  await extension.unload();
  if (triggerFromOtherExtension) {
    await extension2.unload();
  }
  await contentPage.close();
}

add_task(async function test_sendMessage_without_onMessage() {
  await testEventPageWakeup({
    backgroundScript() {
      // No runtime.onMessage listener here.
    },
    async triggerScript() {
      browser.test.assertTrue(
        !browser.extension.getBackgroundPage(),
        "Event page suspended before sendMessage()"
      );
      await browser.test.assertRejects(
        browser.runtime.sendMessage(""),
        "Could not establish connection. Receiving end does not exist.",
        "sendMessage without onMessage should reject"
      );
      browser.test.assertTrue(
        // TODO bug 1852317: This should be "!" instead of "!!", i.e. the event
        // page should not wake up.
        !!browser.extension.getBackgroundPage(),
        "Existence of event page after sendMessage()"
      );
      browser.test.sendMessage("TRIGGER_TEST_DONE");
    },
    // For completeness, start background before suspending. This makes sure
    // that if the persistent listener mechanism is used for runtime.onMessage,
    // that we have started the background at least once to enable the
    // framework to detect (the lack of) persistent listeners, so that it can
    // know that waking the event page doesn't make a difference.
    // (note: persistent listeners are currently not used here: bug 1852317)
    skipInitialPriming: false,
  });
});

add_task(async function test_connect_without_onConnect() {
  await testEventPageWakeup({
    backgroundScript() {
      // No runtime.onConnect listener here.
    },
    triggerScript() {
      browser.test.assertTrue(
        !browser.extension.getBackgroundPage(),
        "Event page suspended before sendMessage()"
      );
      let port = browser.runtime.connect({});
      port.onDisconnect.addListener(() => {
        browser.test.assertEq(
          "Could not establish connection. Receiving end does not exist.",
          port.error?.message,
          "connect() without onConnect should disconnect port with an error"
        );
        browser.test.assertTrue(
          // TODO bug 1852317: This should be "!" instead of "!!", i.e. the
          // event page should not wake up.
          !!browser.extension.getBackgroundPage(),
          "Existence of event page after connect()"
        );
        browser.test.sendMessage("TRIGGER_TEST_DONE");
      });
    },
    // For completeness, start background before suspending. This makes sure
    // that if the persistent listener mechanism is used for runtime.onConnect,
    // that we have started the background at least once to enable the
    // framework to detect (the lack of) persistent listeners, so that it can
    // know that waking the event page doesn't make a difference.
    // (note: persistent listeners are currently not used here: bug 1852317)
    skipInitialPriming: false,
  });
});

async function testEventPageWakeupWithSendMessage({
  triggerFromContentScript,
  triggerFromOtherExtension,
  skipInitialPriming,
}) {
  let backgroundScript, triggerScript;
  // Note: using dump() instead of browser.test.log for logging, to rule out
  // any side effects from extension API calls.
  if (triggerFromOtherExtension) {
    backgroundScript = () => {
      dump("Event page started, listening to onMessageExternal\n");
      browser.runtime.onMessageExternal.addListener(() => {
        browser.test.sendMessage("TRIGGER_TEST_DONE");
      });
    };
    triggerScript = () => {
      dump("Calling sendMessage, expecting onMessageExternal\n");
      browser.runtime.sendMessage("@ext", "msg");
    };
  } else {
    backgroundScript = () => {
      dump("Event page started, listening to onMessage\n");
      browser.runtime.onMessage.addListener(() => {
        browser.test.sendMessage("TRIGGER_TEST_DONE");
      });
    };
    triggerScript = () => {
      dump("Calling sendMessage, expecting onMessage\n");
      browser.runtime.sendMessage("msg");
    };
  }
  await testEventPageWakeup({
    backgroundScript,
    triggerScript,
    triggerFromContentScript,
    triggerFromOtherExtension,
    skipInitialPriming,
  });
}

add_task(async function test_wakeup_onMessage() {
  await testEventPageWakeupWithSendMessage({
    triggerFromContentScript: false,
    triggerFromOtherExtension: false,
  });
});

add_task(async function test_wakeup_onMessage_on_first_run() {
  await testEventPageWakeupWithSendMessage({
    triggerFromContentScript: false,
    triggerFromOtherExtension: false,
    skipInitialPriming: true,
  });
});

add_task(async function test_wakeup_onMessage_by_content_script() {
  await testEventPageWakeupWithSendMessage({
    triggerFromContentScript: true,
    triggerFromOtherExtension: false,
  });
});

add_task(async function test_wakeup_onMessageExternal() {
  await testEventPageWakeupWithSendMessage({
    triggerFromContentScript: false,
    triggerFromOtherExtension: true,
  });
});

add_task(async function test_wakeup_onMessageExternal_by_content_script() {
  await testEventPageWakeupWithSendMessage({
    triggerFromContentScript: true,
    triggerFromOtherExtension: true,
  });
});

async function testEventPageWakeupWithConnect({
  triggerFromContentScript,
  triggerFromOtherExtension,
  skipInitialPriming,
}) {
  let backgroundScript, triggerScript;
  // Note: using dump() instead of browser.test.log for logging, to rule out
  // any side effects from extension API calls.
  if (triggerFromOtherExtension) {
    backgroundScript = () => {
      dump("Event page started, listening to onConnectExternal\n");
      browser.runtime.onConnectExternal.addListener(() => {
        browser.test.sendMessage("TRIGGER_TEST_DONE");
      });
    };
    triggerScript = () => {
      dump("Calling connect, expecting onConnectExternal\n");
      browser.runtime.connect("@ext", {});
    };
  } else {
    backgroundScript = () => {
      dump("Event page started, listening to onConnect\n");
      browser.runtime.onConnect.addListener(() => {
        browser.test.sendMessage("TRIGGER_TEST_DONE");
      });
    };
    triggerScript = () => {
      dump("Calling connect, expecting onConnect\n");
      browser.runtime.connect({});
    };
  }
  await testEventPageWakeup({
    backgroundScript,
    triggerScript,
    triggerFromContentScript,
    triggerFromOtherExtension,
    skipInitialPriming,
  });
}

add_task(async function test_wakeup_onConnect() {
  await testEventPageWakeupWithConnect({
    triggerFromContentScript: false,
    triggerFromOtherExtension: false,
  });
});

add_task(async function test_wakeup_onConnect_on_first_run() {
  await testEventPageWakeupWithConnect({
    triggerFromContentScript: false,
    triggerFromOtherExtension: false,
    skipInitialPriming: true,
  });
});

add_task(async function test_wakeup_onConnect_by_content_script() {
  await testEventPageWakeupWithConnect({
    triggerFromContentScript: true,
    triggerFromOtherExtension: false,
  });
});

add_task(async function test_wakeup_onConnectExternal() {
  await testEventPageWakeupWithConnect({
    triggerFromContentScript: false,
    triggerFromOtherExtension: true,
  });
});

add_task(async function test_wakeup_onConnectExternal_by_content_script() {
  await testEventPageWakeupWithConnect({
    triggerFromContentScript: true,
    triggerFromOtherExtension: true,
  });
});
