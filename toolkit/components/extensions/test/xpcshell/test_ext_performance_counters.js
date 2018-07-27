/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.import("resource://gre/modules/ExtensionParent.jsm");
const ENABLE_COUNTER_PREF = "extensions.webextensions.enablePerformanceCounters";

let {
  ParentAPIManager,
} = ExtensionParent;


async function test_counter() {
  async function background() {
    // creating a bookmark
    let folder = await browser.bookmarks.create({title: "Folder"});
    await browser.bookmarks.create({title: "Bookmark", url: "http://example.com",
                                    parentId: folder.id});
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
  await extension.unload();

  // check that the bookmarks.create API was tracked
  let counters = ParentAPIManager.performanceCounters;
  let counter = counters.get(extension.id).get("bookmarks.create");
  ok(counter.calls > 0);
  ok(counter.duration > 0);
}

add_task(function test_performance_counter() {
  return runWithPrefs([[ENABLE_COUNTER_PREF, true]], test_counter);
});
