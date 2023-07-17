/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

const { ExtensionProcessCrashObserver, Management } =
  ChromeUtils.importESModule("resource://gre/modules/Extension.sys.mjs");

AddonTestUtils.initMochitest(this);

add_task(async function test_ExtensionProcessCrashObserver() {
  let mv2Extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      manifest_version: 2,
    },
    background() {
      browser.test.sendMessage("background_running");
    },
  });

  await mv2Extension.startup();
  await mv2Extension.awaitMessage("background_running");

  let { currentProcessChildID, lastCrashedProcessChildID } =
    ExtensionProcessCrashObserver;

  Assert.notEqual(
    currentProcessChildID,
    undefined,
    "Expect ExtensionProcessCrashObserver.currentProcessChildID to be set"
  );

  Assert.equal(
    ChromeUtils.getAllDOMProcesses().find(
      pp => pp.childID == currentProcessChildID
    )?.remoteType,
    "extension",
    "Expect a child process with remoteType extension to be found for the process childID set"
  );

  Assert.notEqual(
    lastCrashedProcessChildID,
    currentProcessChildID,
    "Expect lastCrashedProcessChildID to not be set to the same value that currentProcessChildID is set"
  );

  let mv3Extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      manifest_version: 3,
    },
    background() {
      browser.test.sendMessage("background_running");
    },
  });

  const waitForExtensionBrowserInserted = () =>
    new Promise(resolve => {
      const listener = (_eventName, browser) => {
        if (!browser.getAttribute("webextension-view-type") === "background") {
          return;
        }
        Management.off("extension-browser-inserted", listener);
        resolve(browser);
      };
      Management.on("extension-browser-inserted", listener);
    });

  const waitForExtensionProcessCrashNotified = () =>
    new Promise(resolve => {
      Management.once("extension-process-crash", (_evt, data) => resolve(data));
    });

  const promiseBackgroundBrowser = waitForExtensionBrowserInserted();

  const promiseExtensionProcessCrashNotified =
    waitForExtensionProcessCrashNotified();

  await mv3Extension.startup();
  await mv3Extension.awaitMessage("background_running");
  const bgPageBrowser = await promiseBackgroundBrowser;

  Assert.ok(
    Glean.extensions.processEvent.created.testGetValue() > 0,
    "Expect glean processEvent.created to be set"
  );

  info("Force extension process crash");
  // Clear any existing telemetry data, so that we can be sure we can
  // assert the glean process_event metric labels values to be strictly
  // equal to 1 after the extension process crashed.
  Services.fog.testResetFOG();
  // NOTE: shouldShowTabCrashPage option needs to be set to false
  // to make sure crashFrame method resolves without waiting for a
  // tab crash page (which is not going to be shown for a background
  // page browser element).
  await BrowserTestUtils.crashFrame(
    bgPageBrowser,
    /* shouldShowTabCrashPage */ false
  );

  info("Verify ExtensionProcessCrashObserver after extension process crash");
  Assert.equal(
    ExtensionProcessCrashObserver.lastCrashedProcessChildID,
    currentProcessChildID,
    "Expect ExtensionProcessCrashObserver.lastCrashedProcessChildID to be set to the expected childID"
  );

  info("Expect the same childID to have been notified as a Management event");
  Assert.deepEqual(
    await promiseExtensionProcessCrashNotified,
    { childID: currentProcessChildID },
    "Got the expected childID notified as part of the extension-process-crash Management event"
  );

  Assert.ok(
    Glean.extensions.processEvent.crashed.testGetValue() > 0,
    "Expect glean processEvent.crashed to be set"
  );
  info("Wait for mv3 extension shutdown");
  await mv3Extension.unload();
  info("Wait for mv2 extension shutdown");
  await mv2Extension.unload();
});
