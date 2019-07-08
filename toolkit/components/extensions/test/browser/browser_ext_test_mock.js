/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// This test verifies that the extension mocks behave consistently, regardless
// of test type (xpcshell vs browser test).
// See also toolkit/components/extensions/test/xpcshell/test_ext_test_mock.js

// Check the state of the extension object. This should be consistent between
// browser tests and xpcshell tests.
async function checkExtensionStartupAndUnload(ext) {
  await ext.startup();
  Assert.ok(ext.id, "Extension ID should be available");
  Assert.ok(ext.uuid, "Extension UUID should be available");
  await ext.unload();
  // Once set nothing clears the UUID.
  Assert.ok(ext.uuid, "Extension UUID exists after unload");
}

add_task(async function test_MockExtension() {
  // When "useAddonManager" is set, a MockExtension is created in the main
  // process, which does not necessarily behave identically to an Extension.
  let ext = ExtensionTestUtils.loadExtension({
    // xpcshell/test_ext_test_mock.js tests "temporary", so here we use
    // "permanent" to have even more test coverage.
    useAddonManager: "permanent",
    manifest: { applications: { gecko: { id: "@permanent-mock-extension" } } },
  });

  Assert.ok(!ext.id, "Extension ID is initially unavailable");
  Assert.ok(!ext.uuid, "Extension UUID is initially unavailable");
  await checkExtensionStartupAndUnload(ext);
  Assert.ok(ext.id, "Extension ID exists after unload");
});

add_task(async function test_generated_Extension() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {},
  });

  Assert.ok(!ext.id, "Extension ID is initially unavailable");
  Assert.ok(!ext.uuid, "Extension UUID is initially unavailable");
  await checkExtensionStartupAndUnload(ext);
  Assert.ok(ext.id, "Extension ID exists after unload");
});
