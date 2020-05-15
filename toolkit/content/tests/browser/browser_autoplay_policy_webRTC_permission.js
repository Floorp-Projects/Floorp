/**
 * This test is used to ensure site which has granted 'camera' or 'microphone'
 * or 'screen' permission could be allowed to autoplay.
 */
"use strict";

const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);

const VIDEO_PAGE =
  "https://example.com/browser/toolkit/content/tests/browser/file_empty.html";

add_task(() => {
  return SpecialPowers.pushPrefEnv({
    set: [
      ["media.autoplay.default", SpecialPowers.Ci.nsIAutoplay.BLOCKED],
      ["media.autoplay.blocking_policy", 0],
      ["media.autoplay.block-event.enabled", true],
    ],
  });
});

async function testAutoplayWebRTCPermission(args) {
  info(`- Starting ${args.name} -`);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: VIDEO_PAGE,
    },
    async browser => {
      PermissionTestUtils.add(
        browser.currentURI,
        args.permission,
        Services.perms.ALLOW_ACTION
      );

      await loadAutoplayVideo(browser, args);
      await checkVideoDidPlay(browser, args);

      // Reset permission.
      PermissionTestUtils.remove(browser.currentURI, args.permission);

      info(`- Finished ${args.name} -`);
    }
  );
}

add_task(async function start_test() {
  await testAutoplayWebRTCPermission({
    name: "Site with camera permission",
    permission: "camera",
    shouldPlay: true,
    mode: "call play",
  });
  await testAutoplayWebRTCPermission({
    name: "Site with microphone permission",
    permission: "microphone",
    shouldPlay: true,
    mode: "call play",
  });
  await testAutoplayWebRTCPermission({
    name: "Site with screen permission",
    permission: "screen",
    shouldPlay: true,
    mode: "call play",
  });
});
