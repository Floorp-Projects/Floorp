"use strict";

const {E10SUtils} = ChromeUtils.import("resource://gre/modules/E10SUtils.jsm");

const PREF_NAME = "browser.tabs.remote.useCrossOriginOpenerPolicy";

function httpURL(filename, host = "https://example.com") {
  let root = getRootDirectory(gTestPath)
    .replace("chrome://mochitests/content", host);
  return root + filename;
}

async function test_coop(start, target, expectedProcessSwitch) {
  return BrowserTestUtils.withNewTab({
    gBrowser, url: start,
  }, async function(browser) {
    info(`Test tab ready: ${start}`);

    let firstProcessID = await ContentTask.spawn(browser, null, () => {
      return Services.appinfo.processID;
    });

    info(`firstProcessID: ${firstProcessID}`);

    BrowserTestUtils.loadURI(browser, target);
    await BrowserTestUtils.browserLoaded(browser);

    info(`Navigated to: ${target}`);
    let secondProcessID = await ContentTask.spawn(browser, null, () => {
      return Services.appinfo.processID;
    });

    is(firstProcessID != secondProcessID, expectedProcessSwitch, `from: ${start} to ${target}`);
  });
}

add_task(async function test_disabled() {
  await SpecialPowers.pushPrefEnv({set: [[PREF_NAME, false]]});
  await test_coop(httpURL("coop_header.sjs", "https://example.com"), httpURL("coop_header.sjs", "https://example.com"), false);
  await test_coop(httpURL("coop_header.sjs?same-origin", "http://example.com"), httpURL("coop_header.sjs", "http://example.com"), false);
  await test_coop(httpURL("coop_header.sjs", "http://example.com"), httpURL("coop_header.sjs?same-origin", "http://example.com"), false);
  await test_coop(httpURL("coop_header.sjs?same-origin", "http://example.com"), httpURL("coop_header.sjs?same-site", "http://example.com"), false); // assuming we don't have fission yet :)
});

add_task(async function test_enabled() {
  await SpecialPowers.pushPrefEnv({set: [[PREF_NAME, true]]});
  await test_coop(httpURL("coop_header.sjs", "https://example.com"), httpURL("coop_header.sjs", "https://example.com"), false);
  await test_coop(httpURL("coop_header.sjs", "https://example.com"), httpURL("coop_header.sjs?same-origin", "https://example.org"), true);
  await test_coop(httpURL("coop_header.sjs?same-origin", "https://example.com"), httpURL("coop_header.sjs", "https://example.org"), true);
  await test_coop(httpURL("coop_header.sjs?same-origin", "https://example.com"), httpURL("coop_header.sjs?same-site", "https://example.org"), true);
});
