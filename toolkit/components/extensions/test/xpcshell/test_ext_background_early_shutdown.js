/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { BrowserTestUtils } = ChromeUtils.import(
  "resource://testing-common/BrowserTestUtils.jsm"
);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "43"
);

let {
  promiseRestartManager,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

Services.prefs.setBoolPref(
  "extensions.webextensions.background-delayed-startup",
  true
);

let { Management } = ChromeUtils.import(
  "resource://gre/modules/Extension.jsm",
  null
);

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

  await extension.startup();
  await extension.awaitMessage("background_startup_observed");

  // Now the actual test: Unloading an extension before the startup has
  // finished should interrupt the start-up and abort pending delayed loads.
  info("Starting extension whose startup will be interrupted");
  ExtensionParent._resetStartupPromises();
  await promiseRestartManager();
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
  Services.obs.notifyObservers(null, "sessionstore-windows-restored");

  // This is the expected message from the re-enabled add-on.
  await extension.awaitMessage("background_startup_observed");
  await extension.unload();

  await promiseShutdownManager();
  ExtensionParent._resetStartupPromises();

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
  await extension.startup();
  await extension.awaitMessage("background_starting");

  ExtensionParent._resetStartupPromises();
  await promiseRestartManager();
  await extension.awaitStartup();

  let bgStartupPromise = new Promise(resolve => {
    function onBackgroundPageDone(eventName) {
      extension.extension.off("startup", onBackgroundPageDone);
      extension.extension.off("background-page-aborted", onBackgroundPageDone);

      if (eventName === "background-page-aborted") {
        info("Background page startup was interrupted");
        resolve("bg_aborted");
      } else {
        info("Background page startup finished normally");
        resolve("bg_fully_loaded");
      }
    }
    extension.extension.on("startup", onBackgroundPageDone);
    extension.extension.on("background-page-aborted", onBackgroundPageDone);
  });

  let bgStartingPromise = new Promise(resolve => {
    let backgroundLoadCount = 0;
    let backgroundPageUrl = extension.extension.baseURI.resolve(
      "_generated_background_page.html"
    );

    // Prevent the background page from actually loading.
    Management.once("extension-browser-inserted", (eventName, browser) => {
      // Intercept background page load.
      let browserLoadURI = browser.loadURI;
      browser.loadURI = function() {
        Assert.equal(++backgroundLoadCount, 1, "loadURI should be called once");
        Assert.equal(
          arguments[0],
          backgroundPageUrl,
          "Expected background page"
        );
        // Reset to "about:blank" to not load the actual background page.
        arguments[0] = "about:blank";
        browserLoadURI.apply(this, arguments);

        // And force the extension process to crash.
        if (browser.isRemote) {
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
  Services.obs.notifyObservers(null, "sessionstore-windows-restored");
  await bgStartingPromise;

  await extension.unload();
  await promiseShutdownManager();

  // This part is the regression test for bug 1501375. It verifies that the
  // background building completes eventually.
  // If it does not, then the next line will cause a timeout.
  info("Waiting for background builder to finish");
  let bgLoadState = await bgStartupPromise;
  Assert.equal(bgLoadState, "bg_aborted", "Startup should be interrupted");

  ExtensionParent._resetStartupPromises();
});
