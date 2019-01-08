/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

ChromeUtils.import("resource:///modules/SitePermissions.jsm", this);

const VIDEO_PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_empty.html";

add_task(() => {
  return SpecialPowers.pushPrefEnv({"set": [
    ["media.autoplay.default", SpecialPowers.Ci.nsIAutoplay.BLOCKED],
    ["media.autoplay.enabled.user-gestures-needed", true],
    ["media.autoplay.block-event.enabled", true],
  ]});
});

async function testAutoplayExistingPermission(args) {
  info("- Starting '" + args.name + "' -");
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: VIDEO_PAGE,
  }, async (browser) => {
    let promptShowing = () =>
    PopupNotifications.getNotification("autoplay-media", browser);

    SitePermissions.set(browser.currentURI, "autoplay-media", args.permission);
    ok(!promptShowing(), "Should not be showing permission prompt yet");

    await loadAutoplayVideo(browser, args);
    await checkVideoDidPlay(browser, args);

    // Reset permission.
    SitePermissions.remove(browser.currentURI, "autoplay-media");

    info("- Finished '" + args.name + "' -");
  });
}

// Test the simple ALLOW/BLOCK cases; when permission is already set to ALLOW,
// we shoud be able to autoplay via calling play(), or via the autoplay attribute,
// and when it's set to BLOCK, we should not.
add_task(async () => {
  await testAutoplayExistingPermission({
    name: "Prexisting allow permission autoplay attribute",
    permission: SitePermissions.ALLOW,
    shouldPlay: true,
    mode: "autoplay attribute",
  });
  await testAutoplayExistingPermission({
    name: "Prexisting allow permission call play",
    permission: SitePermissions.ALLOW,
    shouldPlay: true,
    mode: "call play",
  });
  await testAutoplayExistingPermission({
    name: "Prexisting block permission autoplay attribute",
    permission: SitePermissions.BLOCK,
    shouldPlay: false,
    mode: "autoplay attribute",
  });
  await testAutoplayExistingPermission({
    name: "Prexisting block permission call play",
    permission: SitePermissions.BLOCK,
    shouldPlay: false,
    mode: "call play",
  });
});
