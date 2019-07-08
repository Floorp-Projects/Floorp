/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

let { Management } = ChromeUtils.import(
  "resource://gre/modules/Extension.jsm",
  null
);
function getNextContext() {
  return new Promise(resolve => {
    Management.on("proxy-context-load", function listener(type, context) {
      Management.off("proxy-context-load", listener);
      resolve(context);
    });
  });
}

add_task(async function test_storage_api_without_permissions() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      // Force API initialization.
      try {
        browser.storage.onChanged.addListener(() => {});
      } catch (e) {
        // Ignore.
      }
    },

    manifest: {
      permissions: [],
    },
  });

  let contextPromise = getNextContext();
  await extension.startup();

  let context = await contextPromise;

  // Force API initialization.
  void context.apiObj;

  ok(
    !("storage" in context.apiObj),
    "The storage API should not be initialized"
  );

  await extension.unload();
});

add_task(async function test_storage_api_with_permissions() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.storage.onChanged.addListener(() => {});
    },

    manifest: {
      permissions: ["storage"],
    },
  });

  let contextPromise = getNextContext();
  await extension.startup();

  let context = await contextPromise;

  // Force API initialization.
  void context.apiObj;

  equal(
    typeof context.apiObj.storage,
    "object",
    "The storage API should be initialized"
  );

  await extension.unload();
});
