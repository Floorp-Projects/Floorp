/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

const {GlobalManager} = Cu.import("resource://gre/modules/Extension.jsm", {});

add_task(function* test_global_manager_shutdown_cleanup() {
  equal(GlobalManager.initialized, false,
        "GlobalManager start as not initialized");

  function background() {
    browser.test.notifyPass("background page loaded");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
  });

  yield extension.startup();
  yield extension.awaitFinish("background page loaded");

  equal(GlobalManager.initialized, true,
        "GlobalManager has been initialized once an extension is started");

  yield extension.unload();

  equal(GlobalManager.initialized, false,
        "GlobalManager has been uninitialized once all the webextensions have been stopped");
});
