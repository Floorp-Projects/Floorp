"use strict";

const PAGE =
  "https://example.com/browser/toolkit/content/tests/browser/audio.ogg";

function setup_test_preference() {
  return SpecialPowers.pushPrefEnv({
    set: [
      ["media.autoplay.default", SpecialPowers.Ci.nsIAutoplay.BLOCKED],
      ["media.autoplay.blocking_policy", 0],
    ],
  });
}

async function checkIsVideoDocumentAutoplay(browser) {
  const played = await SpecialPowers.spawn(browser, [], async () => {
    const video = content.document.getElementsByTagName("video")[0];
    const played =
      video &&
      (await video.play().then(
        () => true,
        () => false
      ));
    return played;
  });
  ok(played, "Should be able to play in video document.");
}

async function checkIsIframeVideoDocumentAutoplay(browser) {
  info("- create iframe video document -");
  const iframeBC = await SpecialPowers.spawn(browser, [PAGE], async pageURL => {
    const iframe = content.document.createElement("iframe");
    iframe.src = pageURL;
    content.document.body.appendChild(iframe);
    const iframeLoaded = new Promise((resolve, reject) => {
      iframe.addEventListener("load", e => resolve(), { once: true });
    });
    await iframeLoaded;
    return iframe.browsingContext;
  });

  info("- check whether iframe video document starts playing -");
  const [paused, playedLength] = await SpecialPowers.spawn(iframeBC, [], () => {
    const video = content.document.querySelector("video");
    return [video.paused, video.played.length];
  });
  ok(paused, "Subdoc video should not have played");
  is(playedLength, 0, "Should have empty played ranges");
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
