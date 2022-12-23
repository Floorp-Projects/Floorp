"use strict";

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);
const LOCATION =
  "https://example.com/browser/toolkit/components/telemetry/tests/browser/";
const CORS_LOCATION =
  "https://example.org/browser/toolkit/components/telemetry/tests/browser/";
const MEDIA_SCALAR_NAME = "media.element_in_page_count";

/**
 * 'media.element_in_page_count' is a permanant scalar, this test is used to
 * check if that scalar can be accumulated correctly under different situations.
 */
add_task(async function start_tests() {
  // Clean all scalars first to prevent being interfered by former test.
  TelemetryTestUtils.getProcessScalars("parent", false, true /* clear */);

  await testMediaInPageScalar({
    description: "load a page with one media element",
    url: "file_media.html",
    expectedScalarCount: 1,
  });
  await testMediaInPageScalar({
    description: "load a page with multiple media elements",
    url: "file_media.html",
    options: {
      createSecondMedia: true,
    },
    expectedScalarCount: 1,
  });
  await testMediaInPageScalar({
    description: "load a page with media element created from iframe",
    url: "file_iframe.html",
    options: {
      iframeUrl: "file_media.html",
    },
    expectedScalarCount: 1,
  });
  await testMediaInPageScalar({
    description: "load a page with media element created from CORS iframe",
    url: "file_iframe.html",
    options: {
      iframeUrl: "file_media.html",
      CORSIframe: true,
    },
    expectedScalarCount: 1,
  });
  await testMediaInPageScalar({
    description: "run multiple tabs, all loading media page",
    url: "file_media.html",
    options: {
      tabNums: 2,
    },
    expectedScalarCount: 2,
  });
});

async function testMediaInPageScalar({
  description,
  url,
  options,
  expectedScalarCount,
} = {}) {
  info(`media scalar should be undefined in the start`);
  let scalars = TelemetryTestUtils.getProcessScalars("parent");
  is(scalars[MEDIA_SCALAR_NAME], undefined, "has not created media scalar yet");

  info(`run test '${description}'`);
  url = LOCATION + url;
  await runMediaPage(url, options);

  info(`media scalar should be increased to ${expectedScalarCount}`);
  scalars = TelemetryTestUtils.getProcessScalars(
    "parent",
    false,
    true /* clear */
  );
  is(
    scalars[MEDIA_SCALAR_NAME],
    expectedScalarCount,
    "media scalar count is correct"
  );
  info("============= Next Testcase =============");
}

/**
 * The following are helper functions.
 */
async function runMediaPage(url, options = {}) {
  const tabNums = options.tabNums ? options.tabNums : 1;
  for (let idx = 0; idx < tabNums; idx++) {
    info(`open a tab loading media page`);
    const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
    if (options.iframeUrl) {
      let iframeURL = options.CORSIframe ? CORS_LOCATION : LOCATION;
      iframeURL += options.iframeUrl;
      await loadPageForIframe(tab, iframeURL);
    }

    if (options.createSecondMedia) {
      info(`create second media in the page`);
      await createMedia(tab);
    }

    info(`remove tab`);
    await BrowserTestUtils.removeTab(tab);
    await BrowserUtils.promiseObserved("window-global-destroyed");
  }
}

function createMedia(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], _ => {
    const video = content.document.createElement("VIDEO");
    video.src = "gizmo.mp4";
    video.loop = true;
    content.document.body.appendChild(video);
  });
}

function loadPageForIframe(tab, url) {
  return SpecialPowers.spawn(tab.linkedBrowser, [url], async url => {
    const iframe = content.document.getElementById("iframe");
    iframe.src = url;
    await new Promise(r => (iframe.onload = r));
  });
}
