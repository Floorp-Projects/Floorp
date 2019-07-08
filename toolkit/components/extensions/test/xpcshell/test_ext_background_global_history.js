/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { PlacesTestUtils } = ChromeUtils.import(
  "resource://testing-common/PlacesTestUtils.jsm"
);

add_task(async function test_global_history() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.sendMessage("background-loaded", location.href);
    },
  });

  await extension.startup();

  let backgroundURL = await extension.awaitMessage("background-loaded");

  await extension.unload();

  let exists = await PlacesTestUtils.isPageInDB(backgroundURL);
  ok(!exists, "Background URL should not be in history database");
});
