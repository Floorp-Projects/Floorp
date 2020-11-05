/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);

const VIDEO_PAGE_URI =
  "https://example.com/browser/toolkit/content/tests/browser/file_empty.html";
const SAME_ORIGIN_FRAME_URI =
  "https://example.com/browser/toolkit/content/tests/browser/file_mediaplayback_frame.html";
const DIFFERENT_ORIGIN_FRAME_URI =
  "https://example.org/browser/toolkit/content/tests/browser/file_mediaplayback_frame.html";

const gPermissionName = "autoplay-media";

function setTestingPreferences(defaultSetting) {
  info(`set default autoplay setting to '${defaultSetting}'`);
  let defaultValue =
    defaultSetting == "blocked"
      ? SpecialPowers.Ci.nsIAutoplay.BLOCKED
      : SpecialPowers.Ci.nsIAutoplay.ALLOWED;
  return SpecialPowers.pushPrefEnv({
    set: [
      ["media.autoplay.default", defaultValue],
      ["media.autoplay.blocking_policy", 0],
      ["media.autoplay.block-event.enabled", true],
    ],
  });
}

async function testAutoplayExistingPermission(args) {
  info("- Starting '" + args.name + "' -");
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: VIDEO_PAGE_URI,
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

/**
 * The permission of the main page's domain would determine the final autoplay
 * result when a page contains multiple iframes which are in the different
 * domain from the main pages's.
 * That means we would not check the permission of iframe's domain, even if it
 * has been set.
 */
add_task(async function testExistingPermissionForIframe() {
  await setTestingPreferences("blocked" /* default setting */);
  await testAutoplayExistingPermissionForIframe({
    name: "Prexisting ALLOW for main page with same origin iframe",
    permissionForParent: Services.perms.ALLOW_ACTION,
    isIframeDifferentOrgin: true,
    shouldPlay: true,
  });

  await testAutoplayExistingPermissionForIframe({
    name: "Prexisting ALLOW for main page with different origin iframe",
    permissionForParent: Services.perms.ALLOW_ACTION,
    isIframeDifferentOrgin: false,
    shouldPlay: true,
  });

  await testAutoplayExistingPermissionForIframe({
    name:
      "Prexisting ALLOW for main page, prexisting DENY for different origin iframe",
    permissionForParent: Services.perms.ALLOW_ACTION,
    permissionForChild: Services.perms.DENY_ACTION,
    isIframeDifferentOrgin: false,
    shouldPlay: true,
  });

  await testAutoplayExistingPermissionForIframe({
    name:
      "Prexisting DENY for main page, prexisting ALLOW for different origin iframe",
    permissionForParent: Services.perms.DENY_ACTION,
    permissionForChild: Services.perms.ALLOW_ACTION,
    isIframeDifferentOrgin: false,
    shouldPlay: false,
  });
});

/**
 * The following are helper functions.
 */
async function testAutoplayExistingPermissionForIframe(args) {
  info(`Start test : ${args.name}`);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: VIDEO_PAGE_URI,
    },
    async browser => {
      setupSitesPermission(browser, args);

      await createIframe(browser, args);
      await checkAutplayInIframe(browser, args);

      clearSitesPermission(browser, args);
    }
  );
  info(`Finish test : ${args.name}`);
}

function setupSitesPermission(
  browser,
  {
    isIframeDifferentOrgin,
    permissionForParent,
    permissionForChild = Services.perms.UNKNOWN_ACTION,
  }
) {
  info(`setupSitesPermission`);
  // Set permission for the main page's domain
  setPermissionForBrowser(browser, browser.currentURI, permissionForParent);
  if (isIframeDifferentOrgin) {
    // Set permission for different domain of the iframe
    setPermissionForBrowser(
      browser,
      DIFFERENT_ORIGIN_FRAME_URI,
      permissionForChild
    );
  }
}

function clearSitesPermission(browser, { isIframeDifferentOrgin }) {
  info(`clearSitesPermission`);
  // Clear permission for the main page's domain
  setPermissionForBrowser(
    browser,
    browser.currentURI,
    Services.perms.UNKNOWN_ACTION
  );
  if (isIframeDifferentOrgin) {
    // Clear permission for different domain of the iframe
    setPermissionForBrowser(
      browser,
      DIFFERENT_ORIGIN_FRAME_URI,
      Services.perms.UNKNOWN_ACTION
    );
  }
}

function setPermissionForBrowser(browser, uri, permValue) {
  const promptShowing = () =>
    PopupNotifications.getNotification(gPermissionName, browser);
  PermissionTestUtils.add(uri, gPermissionName, permValue);
  ok(!promptShowing(), "Should not be showing permission prompt yet");
  is(
    PermissionTestUtils.testExactPermission(uri, gPermissionName),
    permValue,
    "Set permission correctly"
  );
}

function createIframe(browser, { isIframeDifferentOrgin }) {
  const iframeURL = isIframeDifferentOrgin
    ? DIFFERENT_ORIGIN_FRAME_URI
    : SAME_ORIGIN_FRAME_URI;
  return SpecialPowers.spawn(browser, [iframeURL], async url => {
    info(`Create iframe and wait until it finsihes loading`);
    const iframe = content.document.createElement("iframe");
    iframe.src = url;
    content.document.body.appendChild(iframe);
    await new Promise(r => (iframe.onload = r));
  });
}

function checkAutplayInIframe(browser, args) {
  return SpecialPowers.spawn(browser, [args], async ({ shouldPlay }) => {
    info(`check if media in iframe can start playing`);
    const iframe = content.document.getElementsByTagName("iframe")[0];
    if (!iframe) {
      ok(false, `can not get the iframe!`);
      return;
    }
    iframe.contentWindow.postMessage("play", "*");
    await new Promise(r => {
      content.onmessage = event => {
        if (shouldPlay) {
          is(event.data, "played", `played media in iframe`);
        } else {
          is(event.data, "blocked", `blocked media in iframe`);
        }
        r();
      };
    });
  });
}
