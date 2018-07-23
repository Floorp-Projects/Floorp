"use strict";

const PAGE = "https://example.com/browser/toolkit/content/tests/browser/audio.ogg";

function setup_test_preference() {
  return SpecialPowers.pushPrefEnv({"set": [
    ["media.autoplay.default", SpecialPowers.Ci.nsIAutoplay.PROMPT],
    ["media.autoplay.enabled.user-gestures-needed", true],
    ["media.autoplay.ask-permission", true],
  ]});
}

function checkIsVideoDocumentAutoplay(browser) {
  return ContentTask.spawn(browser, null, async () => {
    let video = content.document.getElementsByTagName("video")[0];
    let played = video && await video.play().then(() => true, () => false);
    ok(played, "Should be able to play in video document.");
  });
}

add_task(async () => {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE,
  }, async (browser) => {
    info("- setup test preference -");
    await setup_test_preference();

    info(`- check whether video document is autoplay -`);
    await checkIsVideoDocumentAutoplay(browser);
  });
});

