/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Cu.import("resource://testing-common/PlacesTestUtils.jsm");

add_task(function* test_global_history() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.sendMessage("background-loaded", location.href);
    },
  });

  yield extension.startup();

  let backgroundURL = yield extension.awaitMessage("background-loaded");

  yield extension.unload();

  let exists = yield PlacesTestUtils.isPageInDB(backgroundURL);
  ok(!exists, "Background URL should not be in history database");
});
