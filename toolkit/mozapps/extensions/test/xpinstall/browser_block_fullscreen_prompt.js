/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This test tends to trigger a race in the fullscreen time telemetry,
// where the fullscreen enter and fullscreen exit events (which use the
// same histogram ID) overlap. That causes TelemetryStopwatch to log an
// error.
SimpleTest.ignoreAllUncaughtExceptions(true);

const ADDON_FILE_URI = "http://example.com/browser/toolkit/mozapps/extensions/test/xpinstall/amosigned.xpi";

const ADDON_EVENTS = [
  "addon-install-blocked",
  "addon-install-blocked-silent",
  "addon-install-complete",
  "addon-install-confirmation",
  "addon-install-disabled",
  "addon-install-failed",
  "addon-install-origin-blocked",
  "addon-install-started",
  "addon-progress",
  "addon-webext-permissions",
  "xpinstall-disabled",
];

/**
 * Registers observers for addon installation events and resolves promise on first matching event
 */
function waitForNextAddonEvent() {
  return Promise.race(ADDON_EVENTS.map( async (eventStr) => {
    await TestUtils.topicObserved(eventStr);
    return eventStr;
  }));
}

/**
 * Check if an addon installation prompt is visible
 */
function addonPromptVisible(browser) {
  if (!PopupNotifications.isPanelOpen) return false;
  if (ADDON_EVENTS.some(id => PopupNotifications.getNotification(id, browser) != null)) return true;
  return false;
}

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

// This tests if addon installation is blocked when requested in fullscreen
add_task(async function testFullscreenBlockAddonInstallPrompt() {
  // Open example.com
  await BrowserTestUtils.withNewTab("http://example.com", async function(browser) {
    await changeFullscreen(browser, true);

    // Navigate to addon file path
    BrowserTestUtils.loadURI(browser, ADDON_FILE_URI);

    // Wait for addon manager event and check if installation has been blocked
    let eventStr = await waitForNextAddonEvent();

    Assert.equal(eventStr, "addon-install-blocked-silent", "Addon installation was blocked");

    // Test if addon installation prompt has been blocked
    Assert.ok(!addonPromptVisible(), "Addon installation prompt not opened");

    await changeFullscreen(browser, false);
  });
});



// This tests if the addon install prompt is closed when entering fullscreen
add_task(async function testFullscreenCloseAddonInstallPrompt() {
  // Open example.com
  await BrowserTestUtils.withNewTab("http://example.com", async function(browser) {
    // Navigate to addon file path
    BrowserTestUtils.loadURI(browser, ADDON_FILE_URI);

    // Test if addon installation started
    let eventStr = await waitForNextAddonEvent();

    Assert.ok(eventStr === "addon-install-started", "Addon installation started");

    // Test if addon installation prompt is visible
    Assert.ok(addonPromptVisible(), "Addon installation prompt opened");

    // Switch to fullscreen
    await changeFullscreen(browser, true);

    // Test if addon installation prompt has been closed
    Assert.ok(!addonPromptVisible(), "Addon installation prompt closed for fullscreen");

    await changeFullscreen(browser, false);
  });
});
