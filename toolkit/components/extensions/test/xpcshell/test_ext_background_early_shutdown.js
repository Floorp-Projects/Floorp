/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { BrowserTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/BrowserTestUtils.sys.mjs"
);
const { ExtensionTestCommon } = ChromeUtils.importESModule(
  "resource://testing-common/ExtensionTestCommon.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  MockRegistrar: "resource://testing-common/MockRegistrar.sys.mjs",
  TestUtils: "resource://testing-common/TestUtils.sys.mjs",
});

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "43"
);

let { promiseRestartManager, promiseShutdownManager, promiseStartupManager } =
  AddonTestUtils;

const { Management } = ChromeUtils.importESModule(
  "resource://gre/modules/Extension.sys.mjs"
);

const server = AddonTestUtils.createHttpServer({
  hosts: ["example.com"],
});
function registerSlowStyleSheet() {
  // We can delay DOMContentLoaded of a background page by loading a slow
  // stylesheet and using `<script defer>`. For more detail about this
  // trick, see test_ext_background_iframe.js.
  let allowStylesheetToLoad;
  let stylesheetBlockerPromise = new Promise(resolve => {
    allowStylesheetToLoad = resolve;
  });
  let resolveFirstLoad;
  let firstLoadPromise = new Promise(resolve => {
    resolveFirstLoad = resolve;
  });
  let requestCount = 0;
  server.registerPathHandler("/slow.css", (request, response) => {
    response.setHeader("Cache-Control", "no-cache", false);
    response.setHeader("Content-Type", "text/css", false);
    response.processAsync();
    ++requestCount;
    resolveFirstLoad();
    stylesheetBlockerPromise.then(() => {
      response.write("body { color: rgb(1, 2, 3); }");
      response.finish();
    });
  });
  const getRequestCount = () => requestCount;
  return { allowStylesheetToLoad, getRequestCount, firstLoadPromise };
}

// Crashes a <browser>'s remote process.
// Based on BrowserTestUtils.crashFrame.
function crashFrame(browser) {
  if (!browser.isRemoteBrowser) {
    // The browser should be remote, or the test runner would be killed.
    throw new Error("<browser> must be remote");
  }

  // Trigger crash by sending a message to BrowserTestUtils actor.
  BrowserTestUtils.sendAsyncMessage(
    browser.browsingContext,
    "BrowserTestUtils:CrashFrame",
    {}
  );
}

// We can only crash the extension process when they are running out-of-process.
// Otherwise we would be killing the test runner itself...
const CAN_CRASH_EXTENSIONS = WebExtensionPolicy.useRemoteWebExtensions;

add_setup(
  // Crash dumps are only generated when MOZ_CRASHREPORTER is set.
  // Crashes are only generated if tests can crash the extension process.
  { skip_if: () => !AppConstants.MOZ_CRASHREPORTER || !CAN_CRASH_EXTENSIONS },
  function setup_crash_reporter_override_and_cleaner() {
    const crashIds = [];
    // Override CrashService.sys.mjs to intercept crash dumps, for two reasons:
    //
    // - The standard CrashService.sys.mjs implementation uses nsICrashReporter
    //   through Services.appinfo. Because appinfo has been overridden with an
    //   incomplete implementation, a promise rejection is triggered when a
    //   missing method is called at https://searchfox.org/mozilla-central/rev/c615dc4db129ece5cce6c96eb8cab8c5a3e26ac3/toolkit/components/crashes/CrashService.sys.mjs#183
    //
    // - We want to intercept the generated crash dumps for expected crashes and
    //   remove them, to prevent the xpcshell test runner from misinterpreting
    //   them as "CRASH" failures.
    let mockClassId = MockRegistrar.register("@mozilla.org/crashservice;1", {
      addCrash(processType, crashType, id) {
        // The files are ready to be removed now. We however postpone cleanup
        // until the end of the test, to minimize noise during the test, and to
        // ensure that the cleanup completes fully.
        crashIds.push(id);
      },
      QueryInterface: ChromeUtils.generateQI(["nsICrashService"]),
    });
    registerCleanupFunction(async () => {
      MockRegistrar.unregister(mockClassId);

      // Cannot use Services.appinfo because createAppInfo overrides it.
      // eslint-disable-next-line mozilla/use-services
      const appinfo = Cc["@mozilla.org/toolkit/crash-reporter;1"].getService(
        Ci.nsICrashReporter
      );

      info(`Observed ${crashIds.length} crash dump(s).`);
      let deletedCount = 0;
      for (let id of crashIds) {
        info(`Checking whether dumpID ${id} should be removed`);
        let minidumpFile = appinfo.getMinidumpForID(id);
        let extraFile = appinfo.getExtraFileForID(id);
        let extra;
        try {
          extra = await IOUtils.readJSON(extraFile.path);
        } catch (e) {
          info(`Cannot parse crash metadata from ${extraFile.path} :: ${e}\n`);
          continue;
        }
        // The "BrowserTestUtils:CrashFrame" handler annotates the crash
        // report before triggering a crash.
        if (extra.TestKey !== "CrashFrame") {
          info(`Keeping ${minidumpFile.path}; we did not trigger the crash`);
          continue;
        }
        info(`Deleting minidump ${minidumpFile.path} and ${extraFile.path}`);
        minidumpFile.remove(false);
        extraFile.remove(false);
        ++deletedCount;
      }
      info(`Removed ${deletedCount} crash dumps out of ${crashIds.length}`);
    });
  }
);

/**
 * Crash background page of browser and wait for the crash to have been
 * detected and processed by ext-backgroundPage.js.
 *
 * @param {ExtensionWrapper} extension
 * @param {XULElement} [bgBrowser] - The background browser. Optional, but must
 *   be set if the background's ProxyContextParent has not been initialized yet.
 */
async function crashExtensionBackground(extension, bgBrowser) {
  bgBrowser ??= extension.extension.backgroundContext.xulBrowser;

  let byeProm = promiseExtensionEvent(extension, "shutdown-background-script");
  if (CAN_CRASH_EXTENSIONS) {
    info("Killing background page through process crash.");
    crashFrame(bgBrowser);
  } else {
    // If extensions are not running in out-of-process mode, then the
    // non-remote process should not be killed (or the test runner dies).
    // Remove <browser> instead, to simulate the immediate disconnection
    // of the message manager (that would happen if the process crashed).
    info("Closing background page by destroying <browser>.");

    if (extension.extension.backgroundState === "running") {
      // TODO bug 1844217: remove this whole if-block When close() is hooked up
      // to setBgStateStopped. It currently is not, and browser destruction is
      // currently not detected by the implementation.
      let messageManager = bgBrowser.messageManager;
      TestUtils.topicObserved(
        "message-manager-close",
        subject => subject === messageManager
      ).then(() => {
        Management.emit("extension-process-crash", { childID: 1337 });
      });
    }
    bgBrowser.remove();
  }

  info("Waiting for crash to be detected by the internals");
  await byeProm;
}

// Verifies that a delayed background page is not loaded when an extension is
// shut down during startup.
add_task(async function test_unload_extension_before_background_page_startup() {
  await promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    background() {
      browser.test.sendMessage("background_startup_observed");
    },
  });

  // Delayed startup are only enabled for browser (re)starts, so we need to
  // install the extension first, and then unload it.
  // ^ Note: an alternative is to use APP_STARTUP, see elsewhere in this file.

  await extension.startup();
  await extension.awaitMessage("background_startup_observed");

  // Now the actual test: Unloading an extension before the startup has
  // finished should interrupt the start-up and abort pending delayed loads.
  info("Starting extension whose startup will be interrupted");
  await promiseRestartManager({ earlyStartup: false });
  await extension.awaitStartup();

  let extensionBrowserInsertions = 0;
  let onExtensionBrowserInserted = () => ++extensionBrowserInsertions;
  Management.on("extension-browser-inserted", onExtensionBrowserInserted);

  info("Unloading extension before the delayed background page starts loading");
  await extension.addon.disable();

  // Re-enable the add-on to let enough time pass to load a whole background
  // page. If at the end of this the original background page hasn't loaded,
  // we can consider the test successful.
  await extension.addon.enable();

  // Trigger the notification that would load a background page.
  info("Forcing pending delayed background page to load");
  AddonTestUtils.notifyLateStartup();

  // This is the expected message from the re-enabled add-on.
  await extension.awaitMessage("background_startup_observed");
  await extension.unload();

  await promiseShutdownManager();

  Management.off("extension-browser-inserted", onExtensionBrowserInserted);
  Assert.equal(
    extensionBrowserInsertions,
    1,
    "Extension browser should have been inserted only once"
  );
});

// Verifies that the "build" method of BackgroundPage in ext-backgroundPage.js
// does not deadlock when startup is interrupted by extension shutdown.
add_task(async function test_unload_extension_during_background_page_startup() {
  await promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    background() {
      browser.test.sendMessage("background_starting");
    },
  });

  // Delayed startup are only enabled for browser (re)starts, so we need to
  // install the extension first, and then reload it.
  // ^ Note: an alternative is to use APP_STARTUP, see elsewhere in this file.
  await extension.startup();
  await extension.awaitMessage("background_starting");

  await promiseRestartManager({ lateStartup: false });
  await extension.awaitStartup();

  let bgStartupPromise = new Promise(resolve => {
    function onBackgroundPageDone(eventName) {
      extension.extension.off(
        "background-script-started",
        onBackgroundPageDone
      );
      extension.extension.off(
        "background-script-aborted",
        onBackgroundPageDone
      );

      if (eventName === "background-script-aborted") {
        info("Background script startup was interrupted");
        resolve("bg_aborted");
      } else {
        info("Background script startup finished normally");
        resolve("bg_fully_loaded");
      }
    }
    extension.extension.on("background-script-started", onBackgroundPageDone);
    extension.extension.on("background-script-aborted", onBackgroundPageDone);
  });

  let bgStartingPromise = new Promise(resolve => {
    let backgroundLoadCount = 0;
    let backgroundPageUrl = extension.extension.baseURI.resolve(
      "_generated_background_page.html"
    );

    // Prevent the background page from actually loading.
    Management.once("extension-browser-inserted", (eventName, browser) => {
      // Intercept background page load.
      let browserFixupAndLoadURIString = browser.fixupAndLoadURIString;
      browser.fixupAndLoadURIString = function () {
        Assert.equal(++backgroundLoadCount, 1, "loadURI should be called once");
        Assert.equal(
          arguments[0],
          backgroundPageUrl,
          "Expected background page"
        );
        // Reset to "about:blank" to not load the actual background page.
        arguments[0] = "about:blank";
        browserFixupAndLoadURIString.apply(this, arguments);

        // And force the extension process to crash.
        if (CAN_CRASH_EXTENSIONS) {
          crashFrame(browser);
        } else {
          // If extensions are not running in out-of-process mode, then the
          // non-remote process should not be killed (or the test runner dies).
          // Remove <browser> instead, to simulate the immediate disconnection
          // of the message manager (that would happen if the process crashed).
          browser.remove();
        }
        resolve();
      };
    });
  });

  // Force background page to initialize.
  AddonTestUtils.notifyLateStartup();
  await bgStartingPromise;

  await extension.unload();
  await promiseShutdownManager();

  // This part is the regression test for bug 1501375. It verifies that the
  // background building completes eventually.
  // If it does not, then the next line will cause a timeout.
  info("Waiting for background builder to finish");
  let bgLoadState = await bgStartupPromise;
  Assert.equal(bgLoadState, "bg_aborted", "Startup should be interrupted");
});

// Verifies correct state when background crashes while starting.
// The difference with test_unload_extension_during_background_page_startup is
// that in this version, the background has progressed far enough for the
// extension context to have been initialized, but before the background is
// considered loaded.
// withContext: Whether to trigger initialization of ProxyContextParent.
async function do_test_crash_while_starting_background({
  // Whether to trigger initialization of ProxyContextParent during startup.
  withContext = false,
  // Whether to use an event page instead of a persistent background page.
  isEventPage = false,
}) {
  let extension = ExtensionTestUtils.loadExtension({
    // Delay startup, so that we can get an extension reference before the
    // background page starts.
    startupReason: "APP_STARTUP",
    // APP_STARTUP is not enough, delayedStartup is needed (bug 1756225).
    delayedStartup: true,
    manifest: {
      background: {
        page: "background.html",
        persistent: !isEventPage,
      },
    },
    files: {
      "background.html": `<!DOCTYPE html>
         <meta charset="utf-8">
         <script src="background-immediate.js"></script>

         <!-- Delays DOMContentLoaded - see registerSlowStyleSheet -->
         <link rel="stylesheet" href="http://example.com/slow.css">
         <script src="background-deferred.js" defer></script>
      `,
      "background-immediate.js": String.raw`
        dump("background-immediate.js is executing as expected.\n");
        if (${!!withContext}) {
          // Accessing the browser API triggers context creation.
          browser.test.sendMessage("background_started_to_load");
        }
      `,
      "background-deferred.js": () => {
        dump("background-deferred.js is UNEXPECTEDLY executing.\n");
        browser.test.fail("Background startup should have been interrupted");
      },
    },
  });
  let slowStyleSheet = registerSlowStyleSheet();
  await ExtensionTestCommon.resetStartupPromises();
  await extension.startup();
  function assertBackgroundState(expected, message) {
    Assert.equal(extension.extension.backgroundState, expected, message);
  }

  assertBackgroundState("stopped", "Background should not have started yet");

  let bgBrowserPromise = new Promise(resolve => {
    Management.once("extension-browser-inserted", (eventName, browser) => {
      assertBackgroundState("starting", "State when bg <browser> is inserted");
      resolve(browser);
    });
  });

  info("Triggering background creation...");
  await ExtensionTestCommon.notifyEarlyStartup();
  await ExtensionTestCommon.notifyLateStartup();

  let bgBrowser = await bgBrowserPromise;

  if (withContext) {
    info("Waiting for background-immediate.js to notify us...");
    await extension.awaitMessage("background_started_to_load");
    Assert.ok(
      extension.extension.backgroundContext,
      "Context exists when an extension API was called"
    );
    // Probably resolved by now, but wait explicitly in case it hasn't, so we
    // know that the stylesheet has started to load.
    await slowStyleSheet.firstLoadPromise;
  } else {
    // Wait for the stylesheet request to infer that the background content has
    // started to be loaded.
    await slowStyleSheet.firstLoadPromise;
    Assert.ok(
      !extension.extension.backgroundContext,
      "Context should not be set while loading"
    );
  }

  // Still starting because registerSlowStyleSheet postponed startup completion.
  assertBackgroundState("starting", "Background should still be loading");

  await crashExtensionBackground(extension, bgBrowser);

  assertBackgroundState("stopped", "Background state after crash");

  // Now that the background is gone, the server can respond without the
  // possibility of triggering the execution of background-deferred.js
  slowStyleSheet.allowStylesheetToLoad();
  await extension.unload();

  // Can't be 0 because the background has started to load.
  // Can't be 2 because we are loading the background only once.
  Assert.equal(
    slowStyleSheet.getRequestCount(),
    1,
    "Expected exactly one request for slow.css from background page"
  );
}

add_task(async function test_crash_while_starting_background_without_context() {
  await do_test_crash_while_starting_background({ withContext: false });
});

add_task(async function test_crash_while_starting_background_with_context() {
  await do_test_crash_while_starting_background({ withContext: true });
});

add_task(async function test_crash_while_starting_event_page_without_context() {
  await do_test_crash_while_starting_background({
    withContext: false,
    isEventPage: true,
  });
});

add_task(async function test_crash_while_starting_event_page_with_context() {
  await do_test_crash_while_starting_background({
    withContext: true,
    isEventPage: true,
  });
});

async function do_test_crash_while_running_background({ isEventPage = false }) {
  // wakeupBackground() only wakes up after the early startup notification.
  // Trigger explicitly to be independent of other tests.
  await ExtensionTestCommon.resetStartupPromises();
  await AddonTestUtils.notifyEarlyStartup();

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      background: { persistent: !isEventPage },
    },
    background() {
      window.onload = () => {
        browser.test.sendMessage("background_has_fully_loaded");
      };
    },
  });
  await extension.startup();
  function assertBackgroundState(expected, message) {
    Assert.equal(extension.extension.backgroundState, expected, message);
  }

  await extension.awaitMessage("background_has_fully_loaded");
  assertBackgroundState("running", "Background should have started");

  await crashExtensionBackground(extension);
  assertBackgroundState("stopped", "Background state after crash");

  await extension.wakeupBackground();
  await extension.awaitMessage("background_has_fully_loaded");
  assertBackgroundState("running", "Background resumed after crash recovery");
  await extension.terminateBackground();
  assertBackgroundState("stopped", "Background can sleep after crash recovery");

  await extension.unload();
}

add_task(async function test_crash_after_background_startup() {
  await do_test_crash_while_running_background({ isEventPage: false });
});

add_task(async function test_crash_after_event_page_startup() {
  await do_test_crash_while_running_background({ isEventPage: true });
});

add_task(async function test_crash_and_wakeup_via_persistent_listeners() {
  // Ensure that the background can start in response to a primed listener.
  await ExtensionTestCommon.resetStartupPromises();
  await AddonTestUtils.notifyEarlyStartup();

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      background: { persistent: false },
      optional_permissions: ["tabs"],
    },
    background() {
      const domreadyPromise = new Promise(resolve => {
        window.addEventListener("load", resolve, { once: true });
      });
      browser.permissions.onAdded.addListener(() => {
        browser.test.log("permissions.onAdded has fired");
        // Wait for DOMContentLoaded to have fired before notifying the test.
        // This guarantees that backgroundState is "running" instead of
        // potentially "starting".
        domreadyPromise.then(() => {
          browser.test.sendMessage("event_fired");
        });
      });
    },
  });
  await extension.startup();
  function assertBackgroundState(expected, message) {
    Assert.equal(extension.extension.backgroundState, expected, message);
  }
  function triggerEventInEventPage() {
    // Trigger an event, with the expectation that the event page will wake up.
    // As long as we are the only one to trigger the extension API event in this
    // test, the exact event is not significant. Trigger permissions.onAdded:
    Management.emit("change-permissions", {
      extensionId: extension.id,
      added: {
        origins: [],
        permissions: ["tabs"],
      },
    });
  }

  assertBackgroundState("running", "Background after extension.startup()");

  // Sanity check: triggerEventInEventPage does actually trigger event_fired.
  triggerEventInEventPage();
  await extension.awaitMessage("event_fired");

  // Restart a few times to verify that the behavior is consistent over time.
  const TEST_RESTART_ATTEMPTS = 5;
  for (let i = 1; i <= TEST_RESTART_ATTEMPTS; ++i) {
    info(`Testing that a crashed background wakes via event, attempt ${i}/5`);

    await crashExtensionBackground(extension);

    assertBackgroundState("stopped", "Background state after crash");

    triggerEventInEventPage();
    await extension.awaitMessage("event_fired");

    assertBackgroundState("running", "Persistent event can wake up event page");
  }

  await extension.unload();
});
