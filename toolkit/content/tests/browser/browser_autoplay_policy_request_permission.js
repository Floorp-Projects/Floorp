/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

ChromeUtils.import("resource:///modules/SitePermissions.jsm", this);

const VIDEO_PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_empty.html";

add_task(() => {
  return SpecialPowers.pushPrefEnv({"set": [
    ["media.autoplay.default", SpecialPowers.Ci.nsIAutoplay.PROMPT],
    ["media.autoplay.enabled.user-gestures-needed", true],
    ["media.autoplay.ask-permission", true],
    ["media.autoplay.block-event.enabled", true],
  ]});
});

// Runs a content script that creates an autoplay video.
//  browser: the browser to run the script in.
//  args: test case definition, required members {
//    mode: String, "autoplay attribute" or "call play".
//  }
function loadAutoplayVideo(browser, args) {
  return ContentTask.spawn(browser, args, async (args) => {
    info("- create a new autoplay video -");
    let video = content.document.createElement("video");
    video.id = "v1";
    video.didPlayPromise = new Promise((resolve, reject) => {
      video.addEventListener("play", (e) => {
        video.didPlay = true;
        resolve();
      }, {once: true});
      video.addEventListener("blocked", (e) => {
        video.didPlay = false;
        resolve();
      }, {once: true});
    });
    if (args.mode == "autoplay attribute") {
      info("autoplay attribute set to true");
      video.autoplay = true;
    } else if (args.mode == "call play") {
      info("will call play() when reached loadedmetadata");
      video.addEventListener("loadedmetadata", (e) => {
        video.play().then(
          () => {
            info("video play() resolved");
          },
          () => {
            info("video play() rejected");
          });
      }, {once: true});
    } else {
      ok(false, "Invalid 'mode' arg");
    }
    video.src = "gizmo.mp4";
    content.document.body.appendChild(video);
  });
}

// Runs a content script that checks whether the video created by
// loadAutoplayVideo() started playing.
// Parameters:
//  browser: the browser to run the script in.
//  args: test case definition, required members {
//    name: String, description of test.
//    mode: String, "autoplay attribute" or "call play".
//    shouldPlay: boolean, whether video should play.
//  }
function checkVideoDidPlay(browser, args) {
  return ContentTask.spawn(browser, args, async (args) => {
    let video = content.document.getElementById("v1");
    await video.didPlayPromise;
    is(video.didPlay, args.shouldPlay,
      args.name + " should " + (!args.shouldPlay ? "not " : "") + "be able to autoplay");
    video.src = "";
    content.document.body.remove(video);
  });
}

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

async function testAutoplayUnknownPermission(args) {
  info("- Starting '" + args.name + "' -");
  info("- open new tab -");

  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: VIDEO_PAGE,
  }, async (browser) => {
    let promptShowing = () =>
      PopupNotifications.getNotification("autoplay-media", browser);

    // Set this site to ask permission to autoplay.
    SitePermissions.set(browser.currentURI, "autoplay-media", SitePermissions.UNKNOWN);
    ok(!promptShowing(), "Should not be showing permission prompt");

    let popupshown = BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
    await loadAutoplayVideo(browser, args);

    info("Awaiting popupshown");
    await popupshown;
    ok(promptShowing(), "Should now be showing permission prompt");

    // Click the appropriate doorhanger button.
    if (args.button == "allow") {
      info("Clicking allow button");
      PopupNotifications.panel.firstElementChild.button.click();
    } else if (args.button == "block") {
      info("Clicking block button");
      PopupNotifications.panel.firstChild.secondaryButton.click();
    } else {
      ok(false, "Invalid button field");
    }
    // Check that the video started playing.
    await checkVideoDidPlay(browser, args);

    // Reset permission.
    SitePermissions.remove(browser.currentURI, "autoplay-media");
    info("- Finished '" + args.name + "' -");
  });
}

// Test the permission UNKNOWN case; we should prompt for permission, and
// test pressing approve/block in both the autoplay attribute and call
// play case.
add_task(async () => {
  await testAutoplayUnknownPermission({
    name: "Unknown permission click allow autoplay attribute",
    button: "allow",
    shouldPlay: true,
    mode: "autoplay attribute",
  });
  await testAutoplayUnknownPermission({
    name: "Unknown permission click allow call play",
    button: "allow",
    shouldPlay: true,
    mode: "call play",
  });
  await testAutoplayUnknownPermission({
    name: "Unknown permission click block autoplay attribute",
    button: "block",
    shouldPlay: false,
    mode: "autoplay attribute",
  });
  await testAutoplayUnknownPermission({
    name: "Unknown permission click block call play",
    button: "block",
    shouldPlay: false,
    mode: "call play",
  });
});

// Test that if playback starts while the permission prompt is shown,
// that the prompt is hidden.
add_task(async () => {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: VIDEO_PAGE,
  }, async (browser) => {
    info("- Started test prompt hides upon play -");
    let promptShowing = () =>
      PopupNotifications.getNotification("autoplay-media", browser);

    // Set this site to ask permission to autoplay.
    SitePermissions.set(browser.currentURI, "autoplay-media", SitePermissions.UNKNOWN);
    ok(!promptShowing(), "Should not be showing permission prompt");

    let popupshown = BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
    await loadAutoplayVideo(browser, { mode: "call play" });

    info("Awaiting popupshown");
    await popupshown;
    ok(promptShowing(), "Should now be showing permission prompt");

    // Check that the video didn't start playing.
    await ContentTask.spawn(browser, null,
      async () => {
        let video = content.document.getElementById("v1");
        ok(video.paused && !video.didPlay, "Video should not be playing");
      });

    let popuphidden = BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popuphidden");

    await ContentTask.spawn(browser, null,
      async () => {
        // Gesture activate the document, i.e. simulate a click in the document,
        // to unblock autoplay,
        content.document.notifyUserGestureActivation();
        let video = content.document.getElementById("v1");
        // Gesture activating in itself should not cause the previous pending
        // play to proceed.
        ok(video.paused && !video.didPlay, "Video should not have played yet");
        // But trying to play again now that we're gesture activated will work...
        let played = await video.play().then(() => true, () => false);
        ok(played, "Should have played as now gesture activated");
        // And because we started playing, the previous promise returned in the
        // first call to play() above should also resolve too.
        await video.didPlayPromise;
        ok(video.didPlay, "Existing promise should resolve when media starts playing");
      });

    info("Awaiting popuphidden");
    await popuphidden;
    ok(!promptShowing(), "Permission prompt should have hidden when media started playing");

    // Reset permission.
    SitePermissions.remove(browser.currentURI, "autoplay-media");
    info("- Finished test prompt hides upon play -");
  });
});
