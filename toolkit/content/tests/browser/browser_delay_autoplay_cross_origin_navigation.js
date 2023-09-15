/**
 * This test is used to ensure that media would still be able to play even if
 * the page has been navigated to a cross-origin url.
 */
"use strict";

add_task(async function setupTestEnvironment() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.autoplay.default", 0],
      ["media.block-autoplay-until-in-foreground", true],
    ],
  });
});

add_task(async function testCrossOriginNavigation() {
  info("Create a new foreground tab");
  const tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "about:blank"
  );

  info("As tab has been visited, media should be allowed to start");
  const MEDIA_FILE = "gizmo.mp4";
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [getTestWebBasedURL(MEDIA_FILE)],
    async url => {
      let vid = content.document.createElement("video");
      vid.src = url;
      ok(
        await vid.play().then(
          _ => true,
          _ => false
        ),
        "video started playing"
      );
    }
  );

  info("Navigate to a cross-origin video file");
  BrowserTestUtils.startLoadingURIString(
    tab.linkedBrowser,
    getTestWebBasedURL(MEDIA_FILE, { crossOrigin: true })
  );
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info(
    "As tab has been visited, a cross-origin media should also be able to start"
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], async _ => {
    let vid = content.document.querySelector("video");
    ok(vid, "Video exists");
    ok(
      await vid.play().then(
        _ => true,
        _ => false
      ),
      "video started playing"
    );
  });

  info("Remove tab");
  BrowserTestUtils.removeTab(tab);
});
