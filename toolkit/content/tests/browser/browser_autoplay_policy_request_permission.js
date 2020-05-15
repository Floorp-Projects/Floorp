/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);

const VIDEO_PAGE =
  "https://example.com/browser/toolkit/content/tests/browser/file_empty.html";

function setTestingPreferences(defaultSetting) {
  info(`set default autoplay setting to '${defaultSetting}'`);
  let defaultValue =
    defaultSetting == "blocked"
      ? SpecialPowers.Ci.nsIAutoplay.BLOCKED
      : SpecialPowers.Ci.nsIAutoplay.ALLOWED;
  return SpecialPowers.pushPrefEnv({
    set: [
      ["media.autoplay.default", defaultValue],
      ["media.autoplay.enabled.user-gestures-needed", true],
      ["media.autoplay.block-event.enabled", true],
    ],
  });
}

async function testAutoplayExistingPermission(args) {
  info("- Starting '" + args.name + "' -");
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: VIDEO_PAGE,
    },
    async browser => {
      let promptShowing = () =>
        PopupNotifications.getNotification("autoplay-media", browser);

      PermissionTestUtils.add(
        browser.currentURI,
        "autoplay-media",
        args.permission
      );
      ok(!promptShowing(), "Should not be showing permission prompt yet");

      await loadAutoplayVideo(browser, args);
      await checkVideoDidPlay(browser, args);

      // Reset permission.
      PermissionTestUtils.remove(browser.currentURI, "autoplay-media");

      info("- Finished '" + args.name + "' -");
    }
  );
}

async function testAutoplayExistingPermissionAgainstDefaultSetting(args) {
  await setTestingPreferences(args.defaultSetting);
  await testAutoplayExistingPermission(args);
}

// Test the simple ALLOW/BLOCK cases; when permission is already set to ALLOW,
// we shoud be able to autoplay via calling play(), or via the autoplay attribute,
// and when it's set to BLOCK, we should not.
add_task(async () => {
  await setTestingPreferences("blocked" /* default setting */);
  await testAutoplayExistingPermission({
    name: "Prexisting allow permission autoplay attribute",
    permission: Services.perms.ALLOW_ACTION,
    shouldPlay: true,
    mode: "autoplay attribute",
  });
  await testAutoplayExistingPermission({
    name: "Prexisting allow permission call play",
    permission: Services.perms.ALLOW_ACTION,
    shouldPlay: true,
    mode: "call play",
  });
  await testAutoplayExistingPermission({
    name: "Prexisting block permission autoplay attribute",
    permission: Services.perms.DENY_ACTION,
    shouldPlay: false,
    mode: "autoplay attribute",
  });
  await testAutoplayExistingPermission({
    name: "Prexisting block permission call play",
    permission: Services.perms.DENY_ACTION,
    shouldPlay: false,
    mode: "call play",
  });
});

/**
 * These tests are used to ensure the autoplay setting for specific site can
 * always override the default autoplay setting.
 */
add_task(async () => {
  await testAutoplayExistingPermissionAgainstDefaultSetting({
    name:
      "Site has prexisting allow permission but default setting is 'blocked'",
    permission: Services.perms.ALLOW_ACTION,
    defaultSetting: "blocked",
    shouldPlay: true,
    mode: "autoplay attribute",
  });
  await testAutoplayExistingPermissionAgainstDefaultSetting({
    name:
      "Site has prexisting block permission but default setting is 'allowed'",
    permission: Services.perms.DENY_ACTION,
    defaultSetting: "allowed",
    shouldPlay: false,
    mode: "autoplay attribute",
  });
});
