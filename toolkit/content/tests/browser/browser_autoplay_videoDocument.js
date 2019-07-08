"use strict";

const PAGE =
  "https://example.com/browser/toolkit/content/tests/browser/audio.ogg";

function setup_test_preference() {
  return SpecialPowers.pushPrefEnv({
    set: [
      ["media.autoplay.default", SpecialPowers.Ci.nsIAutoplay.BLOCKED],
      ["media.autoplay.enabled.user-gestures-needed", true],
    ],
  });
}

function checkIsVideoDocumentAutoplay(browser) {
  return ContentTask.spawn(browser, null, async () => {
    const video = content.document.getElementsByTagName("video")[0];
    const played = video && (await video.play().then(() => true, () => false));
    ok(played, "Should be able to play in video document.");
  });
}

async function checkIsIframeVideoDocumentAutoplay(browser) {
  info("- create iframe video document -");
  await ContentTask.spawn(browser, PAGE, async pageURL => {
    const iframe = content.document.createElement("iframe");
    iframe.src = pageURL;
    content.document.body.appendChild(iframe);
    const iframeLoaded = new Promise((resolve, reject) => {
      iframe.addEventListener("load", e => resolve(), { once: true });
    });
    await iframeLoaded;
  });

  info("- check whether iframe video document starts playing -");
  await ContentTask.spawn(browser, null, async () => {
    const iframe = content.document.querySelector("iframe");
    const video = iframe.contentDocument.querySelector("video");
    ok(video.paused, "Subdoc video should not have played");
    is(video.played.length, 0, "Should have empty played ranges");
  });
}

add_task(async () => {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: PAGE,
    },
    async browser => {
      info("- setup test preference -");
      await setup_test_preference();

      info(`- check whether video document is autoplay -`);
      await checkIsVideoDocumentAutoplay(browser);
    }
  );
});

add_task(async () => {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:blank",
    },
    async browser => {
      info("- setup test preference -");
      await setup_test_preference();

      info(`- check whether video document in iframe is autoplay -`);
      await checkIsIframeVideoDocumentAutoplay(browser);
    }
  );
});
