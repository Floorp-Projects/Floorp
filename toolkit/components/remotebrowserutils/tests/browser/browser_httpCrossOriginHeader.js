"use strict";

const {E10SUtils} = ChromeUtils.import("resource://gre/modules/E10SUtils.jsm");

const PREF_NAME = "browser.tabs.remote.useCrossOriginPolicy";

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

async function test_policy(start, target, expectError) {
  return BrowserTestUtils.withNewTab({
    gBrowser,
    url: start,
    waitForStateStop: true,
  }, async function(browser) {
    info(`Test tab ready: ${start}`);

    await performLoad(browser, {
      url: target,
      maybeErrorPage: expectError,
    }, async () => {
      BrowserTestUtils.loadURI(browser, target);
    });

    info(`Navigated to: ${target}`);

    let isError = await ContentTask.spawn(browser, null, () => {
      return content.document.documentURI.startsWith("about:neterror");
    });

    Assert.equal(isError, expectError);
  });
}

add_task(async function test_disabled() {
  await SpecialPowers.pushPrefEnv({set: [[PREF_NAME, false]]});
  await test_policy(httpURL("cross_origin_header.sjs?anonymous", "https://example.com"), httpURL("cross_origin_header.sjs?anonymous", "https://example.com"), false);
  await test_policy(httpURL("cross_origin_header.sjs?use-credentials", "https://example.com"), httpURL("cross_origin_header.sjs", "https://example.org"), false);
  await test_policy(httpURL("cross_origin_header.sjs?use-credentials", "https://example.com"), httpURL("cross_origin_header.sjs", "https://example.com"), false);
  await test_policy(httpURL("cross_origin_header.sjs?anonymous", "https://example.com"), httpURL("cross_origin_header.sjs", "https://example.org"), false);
  await test_policy(httpURL("cross_origin_header.sjs?anonymous", "https://example.com"), httpURL("cross_origin_header.sjs", "https://example.com"), false);
});


add_task(async function test_enabled() {
  await SpecialPowers.pushPrefEnv({set: [[PREF_NAME, true]]});
  await test_policy(httpURL("cross_origin_header.sjs", "https://example.com"), httpURL("cross_origin_header.sjs", "https://example.org"), false);
  await test_policy(httpURL("cross_origin_header.sjs", "https://example.com"), httpURL("cross_origin_header.sjs", "https://example.com"), false);
  await test_policy(httpURL("cross_origin_header.sjs", "https://example.com"), httpURL("cross_origin_header.sjs?use-credentials", "https://example.com"), false);
  await test_policy(httpURL("cross_origin_header.sjs?use-credentials", "https://example.com"), httpURL("cross_origin_header.sjs?use-credentials", "https://example.com"), false);
  await test_policy(httpURL("cross_origin_header.sjs?use-credentials", "https://example.com"), httpURL("cross_origin_header.sjs?use-credentials", "https://example.org"), false);
  await test_policy(httpURL("cross_origin_header.sjs?use-credentials", "https://example.com"), httpURL("cross_origin_header.sjs?anonymous", "https://example.com"), false);
  await test_policy(httpURL("cross_origin_header.sjs?anonymous", "https://example.com"), httpURL("cross_origin_header.sjs?anonymous", "https://example.com"), false);
  await test_policy(httpURL("cross_origin_header.sjs?use-credentials", "https://example.com"), httpURL("cross_origin_header.sjs", "https://example.org"), true);
  await test_policy(httpURL("cross_origin_header.sjs?use-credentials", "https://example.com"), httpURL("cross_origin_header.sjs", "https://example.com"), true);
  await test_policy(httpURL("cross_origin_header.sjs?anonymous", "https://example.com"), httpURL("cross_origin_header.sjs", "https://example.org"), true);
  await test_policy(httpURL("cross_origin_header.sjs?anonymous", "https://example.com"), httpURL("cross_origin_header.sjs", "https://example.com"), true);
});

// Loading an iframe without the header in a page that does should be an error
add_task(async function test_frame_is_blocked() {
  await SpecialPowers.pushPrefEnv({set: [[PREF_NAME, true]]});
  let start = httpURL("cross_origin_header.sjs?use-credentials", "https://example.com");
  return BrowserTestUtils.withNewTab({
    gBrowser,
    url: start,
    waitForStateStop: true,
  }, async function(browser) {
    info(`Test tab ready: ${start}`);

    await ContentTask.spawn(browser,
                            httpURL("cross_origin_header.sjs?anonymous", "https://example.org"),
                            async (target) => {
      let subframe = content.document.createElement("iframe");
      subframe.src = target;

      let loaded = ContentTaskUtils.waitForEvent(content.wrappedJSObject, "DOMFrameContentLoaded");
      content.document.body.appendChild(subframe);
      await loaded;

      info(`frame uri: ${subframe.contentDocument.documentURI}`);
      Assert.ok(!subframe.contentDocument.documentURI.startsWith("about:neterror"), "Loading the frame should work");

      let url = new URL(target);
      url.search = "";

      loaded = ContentTaskUtils.waitForEvent(content.wrappedJSObject, "DOMFrameContentLoaded");
      subframe.src = url.href;
      await loaded;

      Assert.ok(subframe.contentDocument.documentURI.startsWith("about:neterror"), "navigation to page without header should error");
    });

    await ContentTask.spawn(browser,
                            httpURL("cross_origin_header.sjs", "https://example.org"),
                            async (target) => {
      let subframe = content.document.createElement("iframe");
      subframe.src = target;

      let loaded = ContentTaskUtils.waitForEvent(content.wrappedJSObject, "DOMFrameContentLoaded");
      content.document.body.appendChild(subframe);
      await loaded;

      info(`frame uri: ${subframe.contentDocument.documentURI}`);
      Assert.ok(subframe.contentDocument.documentURI.startsWith("about:neterror"), "Loading the frame has failed");
    });
  });
});

add_task(async function test_frame2() {
  await SpecialPowers.pushPrefEnv({set: [[PREF_NAME, true]]});
  let start = httpURL("cross_origin_header.sjs", "https://example.com");
  return BrowserTestUtils.withNewTab({
    gBrowser,
    url: start,
    waitForStateStop: true,
  }, async function(browser) {
    info(`Test tab ready: ${start}`);

    let iframe_target = httpURL("cross_origin_header.sjs?use-credentials", "https://example.org");
    await ContentTask.spawn(browser, iframe_target, async (target) => {
      let subframe = content.document.createElement("iframe");
      subframe.src = target;

      let loadedPromise = ContentTaskUtils.waitForEvent(subframe, "load");
      content.document.body.appendChild(subframe);
      await loadedPromise;

      Assert.ok(!subframe.contentDocument.documentURI.startsWith("about:neterror"), "should not be an error");
      let url = new URL(target);
      url.search = "";

      let loaded = ContentTaskUtils.waitForEvent(content.wrappedJSObject, "DOMFrameContentLoaded");
      subframe.src = url.href;
      await loaded;

      Assert.ok(subframe.contentDocument.documentURI.startsWith("about:neterror"), "navigation to page without header should error");
    });
  });
});
