/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* test_storage_api_without_permissions() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {},

    manifest: {
      permissions: [],
    },
  });

  yield extension.startup();

  let context = Array.from(extension.extension.views)[0];

  ok(!("storage" in context._unwrappedAPIs),
     "The storage API should not be initialized");

  yield extension.unload();
});

add_task(function* test_storage_api_with_permissions() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {},

    manifest: {
      permissions: ["storage"],
    },
  });

  yield extension.startup();

  let context = Array.from(extension.extension.views)[0];

  equal(typeof context._unwrappedAPIs.storage, "object",
        "The storage API should be initialized");

  yield extension.unload();
});
