/**
 * After the tab has been visited, all media should be able to start playing.
 * This test is used to ensure that playing media from a cross-origin iframe in
 * a tab that has been already visited won't fail.
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

add_task(async function testCrossOriginIframeShouldBeAbleToStart() {
  info("Create a new foreground tab");
  const originalTab = gBrowser.selectedTab;
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

  info("Make the tab to background");
  await BrowserTestUtils.switchTab(gBrowser, originalTab);

  info(
    "As tab has been visited, a cross-origin iframe should be able to start media"
  );
  const IFRAME_FILE = "file_iframe_media.html";
  await createIframe(
    tab.linkedBrowser,
    getTestWebBasedURL(IFRAME_FILE, { crossOrigin: true })
  );
  await ensureCORSIframeCanStartPlayingMedia(tab.linkedBrowser);

  info("Remove tab");
  BrowserTestUtils.removeTab(tab);
});

/**
 * Following are helper functions
 */
function createIframe(browser, iframeUrl) {
  return SpecialPowers.spawn(browser, [iframeUrl], async url => {
    info(`Create iframe and wait until it finishes loading`);
    const iframe = content.document.createElement("iframe");
    const iframeLoaded = new Promise(r => (iframe.onload = r));
    iframe.src = url;
    content.document.body.appendChild(iframe);
    await iframeLoaded;
  });
}

function ensureCORSIframeCanStartPlayingMedia(browser) {
  return SpecialPowers.spawn(browser, [], async _ => {
    info(`check if media in iframe can start playing`);
    const iframe = content.document.querySelector("iframe");
    if (!iframe) {
      ok(false, `can not get the iframe!`);
      return;
    }
    const playPromise = new Promise(r => {
      content.onmessage = event => {
        is(event.data, "played", `started playing media from CORS iframe`);
        r();
      };
    });
    iframe.contentWindow.postMessage("play", "*");
    await playPromise;
  });
}
