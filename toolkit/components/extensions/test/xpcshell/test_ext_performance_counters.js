/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { ExtensionParent } = ChromeUtils.import(
  "resource://gre/modules/ExtensionParent.jsm"
);

const ENABLE_COUNTER_PREF =
  "extensions.webextensions.enablePerformanceCounters";
const TIMING_MAX_AGE = "extensions.webextensions.performanceCountersMaxAge";

let { ParentAPIManager } = ExtensionParent;

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms)); // eslint-disable-line mozilla/no-arbitrary-setTimeout
}

async function retrieveSpecificCounter(apiName, expectedCount) {
  let currentCount = 0;
  let data;
  while (currentCount < expectedCount) {
    data = await ParentAPIManager.retrievePerformanceCounters();
    for (let [console, counters] of data) {
      for (let [api, counter] of counters) {
        if (api == apiName) {
          currentCount += counter.calls;
        }
      }
    }
    await sleep(100);
  }
  return data;
}

async function test_counter() {
  async function background() {
    // creating a bookmark is done in the parent
    let folder = await browser.bookmarks.create({ title: "Folder" });
    await browser.bookmarks.create({
      title: "Bookmark",
      url: "http://example.com",
      parentId: folder.id,
    });

    // getURL() is done in the child, let do three
    browser.runtime.getURL("beasts/frog.html");
    browser.runtime.getURL("beasts/frog2.html");
    browser.runtime.getURL("beasts/frog3.html");
    browser.test.sendMessage("done");
  }

  let extensionData = {
    background,
    manifest: {
      permissions: ["bookmarks"],
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();
  await extension.awaitMessage("done");

  let counters = await retrieveSpecificCounter("getURL", 3);
  await extension.unload();

  // check that the bookmarks.create API was tracked
  let counter = counters.get(extension.id).get("bookmarks.create");
  ok(counter.calls > 0);
  ok(counter.duration > 0);

  // check that the getURL API was tracked
  counter = counters.get(extension.id).get("getURL");
  ok(counter.calls > 0);
  ok(counter.duration > 0);
}

add_task(function test_performance_counter() {
  return runWithPrefs(
    [
      [ENABLE_COUNTER_PREF, true],
      [TIMING_MAX_AGE, 1],
    ],
    test_counter
  );
});
