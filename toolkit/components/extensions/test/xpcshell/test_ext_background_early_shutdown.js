/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { ExtensionTestCommon } = ChromeUtils.importESModule(
  "resource://testing-common/ExtensionTestCommon.sys.mjs"
);

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

const { ExtensionProcessCrashObserver, Management } =
  ChromeUtils.importESModule("resource://gre/modules/Extension.sys.mjs");

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

// We can only crash the extension process when they are running out-of-process.
// Otherwise we would be killing the test runner itself...
const CAN_CRASH_EXTENSIONS = WebExtensionPolicy.useRemoteWebExtensions;

add_setup(function () {
  // Set a high threshold because this test crashes a few times on purpose and
  // we don't want to disable process spawning.
  Services.prefs.setIntPref("extensions.webextensions.crash.threshold", 100);

  // Need a profile to init Glean.
  do_get_profile();
  Services.fog.initializeFOG();

  Assert.equal(
    ExtensionProcessCrashObserver.appInForeground,
    AppConstants.platform !== "android",
    "Expect appInForeground to be initially true on desktop and false on android builds"
  );

  // For Android build we mock the app moving in the foreground for the first time
  // (which, in a real Fenix instance, happens when the application receives the first
  // call to the onPause lifecycle callback and the geckoview-initial-foreground
  // topic is being notified to Gecko as a side-effect of that).
  //
  // We have to mock the app moving in the foreground before any of the test extension
  // startup, so that both Desktop and Mobile builds are in the same initial foreground
  // state for the rest of the test file.
  if (AppConstants.platform === "android") {
    info("Mock geckoview-initial-foreground observer service topic");
    ExtensionProcessCrashObserver.observe(null, "geckoview-initial-foreground");
    Assert.equal(
      ExtensionProcessCrashObserver.appInForeground,
      true,
      "Expect appInForeground to be true after geckoview-initial-foreground topic"
    );
  }
});

add_setup(
  // Crash dumps are only generated when MOZ_CRASHREPORTER is set.
  // Crashes are only generated if tests can crash the extension process.
  { skip_if: () => !AppConstants.MOZ_CRASHREPORTER || !CAN_CRASH_EXTENSIONS },
  setup_crash_reporter_override_and_cleaner
);

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

add_task(
  {
    // TODO: consider adding explicit coverage for auto-restart behavior
    // when a crash is hit while there is not background context yet.
    pref_set: [
      ["extensions.background.disableRestartPersistentAfterCrash", true],
    ],
  },
  async function test_crash_while_starting_background_without_context() {
    await do_test_crash_while_starting_background({ withContext: false });
  }
);

add_task(
  {
    // Expected auto-restart behavior is tested in the test task named
    // test_persistent_restarted_after_crash.
    pref_set: [
      ["extensions.background.disableRestartPersistentAfterCrash", true],
    ],
  },
  async function test_crash_while_starting_background_with_context() {
    await do_test_crash_while_starting_background({ withContext: true });
  }
);

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

add_task(
  {
    // Disable auto-restart persistent background pages after a crash, this test
    // case is checking that the backgroundState is set to stopped when an
    // extension process crash is it but if the background page is restarted
    // automatically then the background state will be already set to "starting".
    pref_set: [
      ["extensions.background.disableRestartPersistentAfterCrash", true],
    ],
  },
  async function test_crash_after_background_startup() {
    await do_test_crash_while_running_background({ isEventPage: false });
  }
);

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

add_task(
  {
    skip_if: () => !CAN_CRASH_EXTENSIONS,
    pref_set: [
      ["extensions.webextensions.crash.threshold", 3],
      // Set a long timeframe to make sure the few crashes we produce in this
      // test will all be counted within the same timeframe.
      ["extensions.webextensions.crash.timeframe", 60 * 1000],
    ],
  },
  async function test_process_spawning_disabled_because_of_too_many_crashes() {
    // Force-enable process spawning because that will reset the internals of
    // the crash observer.
    ExtensionProcessCrashObserver.enableProcessSpawning();
    Services.fog.testResetFOG();

    function assertCrashThresholdTelemetry({ expectToBeSet }) {
      // Desktop builds are only expected to record crashed_over_threshold_fg,
      // on Android builds xpcshell tests are detected as being in foreground
      // unless we explicitly mock the app being moved in the background as
      // the test tasks test_background_restarted_after_crash already does
      // (and crashed_over_threshold_bg is covered in that test task).

      equal(
        undefined,
        Glean.extensions.processEvent.crashed_over_threshold_bg.testGetValue(),
        `Initial value of crashed_over_threshold_bg.`
      );

      if (expectToBeSet) {
        ok(
          Glean.extensions.processEvent.crashed_over_threshold_fg.testGetValue() >
            0,
          "Expect crashed_over_threshold_fg count to be set."
        );
        return;
      }

      equal(
        undefined,
        Glean.extensions.processEvent.crashed_over_threshold_fg.testGetValue(),
        `Initial value of crashed_over_threshold_fg.`
      );
    }

    assertCrashThresholdTelemetry({ expectToBeSet: false });

    Assert.equal(
      ExtensionProcessCrashObserver.processSpawningDisabled,
      false,
      "Expect process spawning to be enabled"
    );

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

    // Crash/restart a few times to force the crash observer to disable process
    // spawning on the crash _after_ the loop. Note that the value below should
    // match the "threshold" pref set above.
    const TEST_RESTART_ATTEMPTS = 3;
    for (let i = 1; i <= TEST_RESTART_ATTEMPTS; ++i) {
      info(
        `Crash/restart extension background, attempt ${i}/${TEST_RESTART_ATTEMPTS}`
      );

      await crashExtensionBackground(extension);
      assertBackgroundState("stopped", "Background state after crash");

      triggerEventInEventPage();
      await extension.awaitMessage("event_fired");
      assertBackgroundState(
        "running",
        "Persistent event can wake up event page"
      );
    }

    assertCrashThresholdTelemetry({ expectToBeSet: false });

    info("Crash one more time");
    await crashExtensionBackground(extension);

    assertCrashThresholdTelemetry({ expectToBeSet: true });

    Assert.ok(
      ExtensionProcessCrashObserver.processSpawningDisabled,
      "Expect process spawning to be disabled"
    );

    info("Trigger an event, which shouldn't wake up the event page");
    triggerEventInEventPage();
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(resolve => setTimeout(resolve, 100));
    assertBackgroundState("stopped", "Background should not have started yet");

    info("Enable process spawning");
    ExtensionProcessCrashObserver.enableProcessSpawning();
    Assert.equal(
      ExtensionProcessCrashObserver.processSpawningDisabled,
      false,
      "Expect process spawning to be enabled"
    );
    assertBackgroundState("stopped", "Background should still be suspended");

    info("Trigger an event, which should wake up the event page");
    triggerEventInEventPage();
    await extension.awaitMessage("event_fired");
    assertBackgroundState("running", "Persistent event can wake up event page");

    info("Crash again");
    await crashExtensionBackground(extension);
    assertBackgroundState("stopped", "Background state after crash");
    Assert.equal(
      ExtensionProcessCrashObserver.processSpawningDisabled,
      false,
      "Expect process spawning to be enabled"
    );

    info("Trigger an event, which should wake up the event page again");
    triggerEventInEventPage();
    await extension.awaitMessage("event_fired");
    assertBackgroundState("running", "Persistent event can wake up event page");

    await extension.unload();
  }
);

add_task(
  {
    skip_if: () => !CAN_CRASH_EXTENSIONS,
    pref_set: [
      ["extensions.webextensions.crash.threshold", 2],
      // Set a long timeframe to make sure the few crashes we produce in this
      // test will all be counted within the same timeframe.
      ["extensions.webextensions.crash.timeframe", 60 * 1000],
    ],
  },
  async function test_background_restarted_after_crash() {
    // Force-enable process spawning because that will reset the internals of
    // the crash observer.
    ExtensionProcessCrashObserver.enableProcessSpawning();
    Assert.equal(
      ExtensionProcessCrashObserver.processSpawningDisabled,
      false,
      "Expect process spawning to be enabled"
    );

    function assertCrashThresholdTelemetry({ fg, bg }) {
      if (fg) {
        ok(
          Glean.extensions.processEvent.crashed_over_threshold_fg.testGetValue() >
            0,
          "Expect crashed_over_threshold_fg count to be set."
        );
      } else {
        equal(
          undefined,
          Glean.extensions.processEvent.crashed_over_threshold_fg.testGetValue(),
          `Initial value of crashed_over_threshold_fg.`
        );
      }
      if (bg) {
        ok(
          Glean.extensions.processEvent.crashed_over_threshold_bg.testGetValue() >
            0,
          "Expect crashed_over_threshold_bg count to be set."
        );
      } else {
        equal(
          undefined,
          Glean.extensions.processEvent.crashed_over_threshold_bg.testGetValue(),
          `Initial value of crashed_over_threshold_bg.`
        );
      }
    }

    // Setup test environment to match a fully started browser instance
    await ExtensionTestCommon.resetStartupPromises();
    await AddonTestUtils.notifyEarlyStartup();
    Services.fog.testResetFOG();

    let extension = ExtensionTestUtils.loadExtension({
      manifest: {
        background: { persistent: true },
      },
      background() {
        window.addEventListener(
          "load",
          () => {
            browser.test.sendMessage("persistentbg_started");
          },
          { once: true }
        );
      },
    });

    await extension.startup();
    await extension.awaitMessage("persistentbg_started");

    function assertBackgroundState(expected, message) {
      Assert.equal(extension.extension.backgroundState, expected, message);
    }

    async function assertStillStoppedAfterTimeout(timeout = 100) {
      // Confirm that the state is still stopped and the background page
      // was not actually in the process of being restarted.
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      await new Promise(resolve => setTimeout(resolve, timeout));
      assertBackgroundState("stopped", "Background should still be stopped");
    }

    async function mockCrashOnAndroidAppInBackground() {
      info("Mock application-background observer service topic");
      ExtensionProcessCrashObserver.observe(null, "application-background");

      Assert.equal(
        ExtensionProcessCrashObserver.appInForeground,
        false,
        "Got expected value set on ExtensionProcessCrashObserver.appInForeground"
      );
      await crashExtensionBackground(extension);
      assertBackgroundState(
        "stopped",
        "Persistent Background state after crash while in the background"
      );

      await assertStillStoppedAfterTimeout();

      info("Mock application-foreground observer service topic");
      ExtensionProcessCrashObserver.observe(null, "application-foreground");

      Assert.equal(
        ExtensionProcessCrashObserver.appInForeground,
        true,
        "Ensure the application is detected as being in foreground"
      );
    }

    assertBackgroundState("running", "Background after extension.startup()");

    // Restart a few times to verify that the behavior is consistent over time.
    info(
      "Testing that a crashed persistent background is restarted after a crash"
    );

    Assert.equal(
      ExtensionProcessCrashObserver.appInForeground,
      true,
      "Ensure the application is detected as being in foreground"
    );
    await crashExtensionBackground(extension);

    await extension.awaitMessage("persistentbg_started");
    assertBackgroundState("running", "Persistent Background state after crash");

    // Mock application moved into the background and background page
    // auto-restart to be deferred to the application being moved
    // back in the foreground.
    if (ExtensionProcessCrashObserver._isAndroid) {
      await mockCrashOnAndroidAppInBackground();
    } else {
      await crashExtensionBackground(extension);
    }

    info("Wait for the persistent background context to be started");
    await extension.awaitMessage("persistentbg_started");
    assertBackgroundState("running", "Persistent Background state after crash");

    info("Mock another crash to be exceeding enforced crash threshold");

    assertCrashThresholdTelemetry({ fg: false, bg: false });

    // Mock application moved into the background and background page
    // auto-restart to be deferred to the application being moved
    // back in the foreground.
    if (ExtensionProcessCrashObserver._isAndroid) {
      await mockCrashOnAndroidAppInBackground();
      assertCrashThresholdTelemetry({ fg: false, bg: true });
    } else {
      await crashExtensionBackground(extension);
      assertCrashThresholdTelemetry({ fg: true, bg: false });
    }

    Assert.ok(
      ExtensionProcessCrashObserver.processSpawningDisabled,
      "Expect process spawning to be disabled"
    );

    assertBackgroundState(
      "stopped",
      "Persistent Background state after crash exceeding threshold"
    );

    await assertStillStoppedAfterTimeout();

    info("Enable process spawning");
    ExtensionProcessCrashObserver.enableProcessSpawning();
    Assert.equal(
      ExtensionProcessCrashObserver.processSpawningDisabled,
      false,
      "Expect process spawning to be enabled"
    );

    info("Wait for Persistent Background Context to be started");
    await extension.awaitMessage("persistentbg_started");
    assertBackgroundState("running", "Persistent Background to be running");

    // Crash again to confirm the threshold has been reset.
    await crashExtensionBackground(extension);
    info("Wait for Persistent Background Context to be started");
    await extension.awaitMessage("persistentbg_started");
    assertBackgroundState("running", "Persistent Background to be running");

    // Crash again to cover explicitly exceeding the crash threshold
    // while the application is in foreground.
    if (ExtensionProcessCrashObserver._isAndroid) {
      Assert.equal(
        ExtensionProcessCrashObserver.appInForeground,
        true,
        "Ensure the application is detected as being in foreground"
      );

      await crashExtensionBackground(extension);

      info("Wait for Persistent Background Context to be started");
      await extension.awaitMessage("persistentbg_started");
      assertBackgroundState("running", "Persistent Background to be running");

      // Crash one more time to exceed the threshold.
      await crashExtensionBackground(extension);

      assertCrashThresholdTelemetry({ fg: true, bg: true });

      Assert.ok(
        ExtensionProcessCrashObserver.processSpawningDisabled,
        "Expect process spawning to be disabled"
      );
      await assertStillStoppedAfterTimeout();
    }

    await extension.unload();
  }
);
