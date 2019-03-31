/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// This test verifies that the extension mocks behave consistently, regardless
// of test type (xpcshell vs browser test).
// See also toolkit/components/extensions/test/browser/browser_ext_test_mock.js

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

AddonTestUtils.init(this);

add_task(async function setup() {
  await ExtensionTestUtils.startAddonManager();
});

add_task(async function test_MockExtension() {
  let ext = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {},
  });

  Assert.equal(ext.constructor.name, "InstallableWrapper", "expected class");
  Assert.ok(!ext.id, "Extension ID is initially unavailable");
  Assert.ok(!ext.uuid, "Extension UUID is initially unavailable");
  await checkExtensionStartupAndUnload(ext);
  // When useAddonManager is set, AOMExtensionWrapper clears the ID upon unload.
  // TODO: Fix AOMExtensionWrapper to not clear the ID after unload, and move
  // this assertion inside |checkExtensionStartupAndUnload| (since then the
  // behavior will be consistent across all test types).
  Assert.ok(!ext.id, "Extension ID is cleared after unload");
});

add_task(async function test_generated_Extension() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {},
  });

  Assert.equal(ext.constructor.name, "ExtensionWrapper", "expected class");
  // Without "useAddonManager", an Extension is generated and their IDs are
  // immediately available.
  Assert.ok(ext.id, "Extension ID is initially available");
  Assert.ok(ext.uuid, "Extension UUID is initially available");
  await checkExtensionStartupAndUnload(ext);
  Assert.ok(ext.id, "Extension ID exists after unload");
});
