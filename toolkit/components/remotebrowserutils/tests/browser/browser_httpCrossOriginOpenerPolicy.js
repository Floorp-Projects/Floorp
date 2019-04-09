/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

const {E10SUtils} = ChromeUtils.import("resource://gre/modules/E10SUtils.jsm");

const PREF_NAME = "browser.tabs.remote.useCrossOriginOpenerPolicy";

function httpURL(filename, host = "https://example.com") {
  let root = getRootDirectory(gTestPath)
    .replace("chrome://mochitests/content", host);
  return root + filename;
}

async function performLoad(browser, opts, action) {
  let loadedPromise = BrowserTestUtils.browserStopped(
    browser, opts.url, opts.maybeErrorPage);
  await action();
  await loadedPromise;
}

async function test_coop(start, target, expectedProcessSwitch) {
  return BrowserTestUtils.withNewTab({
    gBrowser,
    url: start,
    waitForStateStop: true,
  }, async function(_browser) {
    info(`Test tab ready: ${start}`);

    await new Promise(resolve => setTimeout(resolve, 20));
    let browser = gBrowser.selectedBrowser;
    let firstProcessID = await ContentTask.spawn(browser, null, () => {
      return Services.appinfo.processID;
    });

    info(`firstProcessID: ${firstProcessID}`);

    await performLoad(browser, {
      url: target,
      maybeErrorPage: false,
    }, async () => {
      BrowserTestUtils.loadURI(browser, target);
      if (expectedProcessSwitch) {
        await BrowserTestUtils.waitForEvent(gBrowser.getTabForBrowser(browser), "SSTabRestored");
      }
    });

    info(`Navigated to: ${target}`);
    await new Promise(resolve => setTimeout(resolve, 20));
    browser = gBrowser.selectedBrowser;
    let secondProcessID = await ContentTask.spawn(browser, null, () => {
      return Services.appinfo.processID;
    });

    info(`secondProcessID: ${secondProcessID}`);
    if (expectedProcessSwitch) {
      Assert.notEqual(firstProcessID, secondProcessID, `from: ${start} to ${target}`);
    } else {
      Assert.equal(firstProcessID, secondProcessID, `from: ${start} to ${target}`);
    }
  });
}

// Check that multiple navigations of the same tab will only switch processes
// when it's expected.
add_task(async function test_multiple_nav_process_switches() {
  await SpecialPowers.pushPrefEnv({set: [[PREF_NAME, true]]});
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: httpURL("coop_header.sjs", "https://example.org"),
    waitForStateStop: true,
  }, async function(browser) {
    await new Promise(resolve => setTimeout(resolve, 20));
    let prevPID = await ContentTask.spawn(browser, null, () => {
      return Services.appinfo.processID;
    });

    let target = httpURL("coop_header.sjs?.", "https://example.org");
    await performLoad(browser, {
      url: target,
      maybeErrorPage: false,
    }, async () => {
      BrowserTestUtils.loadURI(browser, target);
    });

    let currentPID = await ContentTask.spawn(browser, null, () => {
      return Services.appinfo.processID;
    });

    Assert.equal(prevPID, currentPID);
    prevPID = currentPID;

    target = httpURL("coop_header.sjs?same-origin", "https://example.org");
    await performLoad(browser, {
      url: target,
      maybeErrorPage: false,
    }, async () => {
      BrowserTestUtils.loadURI(browser, target);
      await BrowserTestUtils.waitForEvent(gBrowser.getTabForBrowser(browser), "SSTabRestored");
    });

    await new Promise(resolve => setTimeout(resolve, 20));
    currentPID = await ContentTask.spawn(browser, null, () => {
      return Services.appinfo.processID;
    });

    Assert.notEqual(prevPID, currentPID);
    prevPID = currentPID;

    target = httpURL("coop_header.sjs?same-origin", "https://example.com");
    await performLoad(browser, {
      url: target,
      maybeErrorPage: false,
    }, async () => {
      BrowserTestUtils.loadURI(browser, target);
      await BrowserTestUtils.waitForEvent(gBrowser.getTabForBrowser(browser), "SSTabRestored");
    });

    await new Promise(resolve => setTimeout(resolve, 20));
    currentPID = await ContentTask.spawn(browser, null, () => {
      return Services.appinfo.processID;
    });

    Assert.notEqual(prevPID, currentPID);
    prevPID = currentPID;

    target = httpURL("coop_header.sjs?same-origin.#4", "https://example.com");
    await performLoad(browser, {
      url: target,
      maybeErrorPage: false,
    }, async () => {
      BrowserTestUtils.loadURI(browser, target);
    });

    await new Promise(resolve => setTimeout(resolve, 20));
    currentPID = await ContentTask.spawn(browser, null, () => {
      return Services.appinfo.processID;
    });

    Assert.equal(prevPID, currentPID);
    prevPID = currentPID;
  });
});

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
  await test_coop(httpURL("coop_header.sjs?same-origin#1", "https://example.com"), httpURL("coop_header.sjs?same-origin#1", "https://example.org"), true);
  await test_coop(httpURL("coop_header.sjs?same-origin#2", "https://example.com"), httpURL("coop_header.sjs?same-site#2", "https://example.org"), true);
});
