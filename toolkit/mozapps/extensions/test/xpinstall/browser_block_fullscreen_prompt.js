/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This test tends to trigger a race in the fullscreen time telemetry,
// where the fullscreen enter and fullscreen exit events (which use the
// same histogram ID) overlap. That causes TelemetryStopwatch to log an
// error.
SimpleTest.ignoreAllUncaughtExceptions(true);

/**
 * Spawns content task in browser to enter / leave fullscreen
 * @param browser - Browser to use for JS fullscreen requests
 * @param {Boolean} fullscreenState - true to enter fullscreen, false to leave
 */
function changeFullscreen(browser, fullscreenState) {
  return ContentTask.spawn(browser, fullscreenState, async function(state) {
    if (state) {
      await content.document.body.requestFullscreen();
    } else {
      await content.document.exitFullscreen();
    }
  });
}

function triggerInstall(browser, trigger) {
  return ContentTask.spawn(browser, trigger, async function(trigger) {
    content.InstallTrigger.install(trigger);
  });
}

// This tests if addon installation is blocked when requested in fullscreen
add_task(async function testFullscreenBlockAddonInstallPrompt() {
  // Open example.com
  await BrowserTestUtils.openNewForegroundTab(gBrowser, TESTROOT);

  // Enter and wait for fullscreen
  await changeFullscreen(gBrowser.selectedBrowser, true);
  await TestUtils.waitForCondition(() => window.fullScreen, "Waiting for window to enter fullscreen");

  // Trigger addon installation and expect it to be blocked
  let addonEventPromise = TestUtils.topicObserved("addon-install-blocked-silent");
  await triggerInstall(gBrowser.selectedBrowser, {"XPI": "amosigned.xpi"});
  await addonEventPromise;

  // Test if addon installation prompt has been blocked
  let panelOpened;
  try {
    panelOpened = await TestUtils.waitForCondition(() => PopupNotifications.isPanelOpen, 100, 10);
  } catch (ex) {
    panelOpened = false;
  }
  is(panelOpened, false, "Addon installation prompt not opened");

  window.fullScreen = false;
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});


// This tests if the addon install prompt is closed when entering fullscreen
add_task(async function testFullscreenCloseAddonInstallPrompt() {
  let triggers = encodeURIComponent(JSON.stringify({
    "XPI": "amosigned.xpi",
  }));
  let target = TESTROOT + "installtrigger.html?" + triggers;

  // Open example.com
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com");

  // Trigger addon installation
  let addonEventPromise = TestUtils.topicObserved("addon-install-blocked");
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, target);
  // Wait for addon install event
  await addonEventPromise;

  // Test if addon installation prompt is visible
  await TestUtils.waitForCondition(() => PopupNotifications.isPanelOpen, "Waiting for addon installation prompt to open");
  Assert.ok(PopupNotifications.getNotification("addon-install-blocked", gBrowser.selectedBrowser) != null, "Opened notification is installation blocked prompt");

  // Switch to fullscreen and test for addon installation prompt close
  await changeFullscreen(gBrowser.selectedBrowser, true);
  await TestUtils.waitForCondition(() => window.fullScreen, "Waiting for window to enter fullscreen");
  await TestUtils.waitForCondition(() => !PopupNotifications.isPanelOpen, "Waiting for addon installation prompt to close");

  window.fullScreen = false;
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
