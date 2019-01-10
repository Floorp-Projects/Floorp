/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_manifest() {
  let normalized = await ExtensionTestUtils.normalizeManifest({
    "browser_action": {
      "default_icon": "",
    },
  });

  equal(normalized.error,
        `Error processing browser_action.default_icon: Value "" must either: be an object value, or match the pattern /^\\w/`,
        "Should have an error");
  Assert.deepEqual(normalized.errors, [], "Should not have a warning");

  normalized = await ExtensionTestUtils.normalizeManifest({
    "browser_action": {
      "default_icon": {
        "16": "",
      },
    },
  });

  equal(normalized.error,
        "Error processing browser_action.default_icon: Value must either: .16 must match the pattern /^\\w/, or be a string value",
        "Should have an error");
  Assert.deepEqual(normalized.errors, [], "Should not have a warning");

  normalized = await ExtensionTestUtils.normalizeManifest({
    "browser_action": {
      "default_icon": {
        "16": "icon.png",
      },
    },
  });

  equal(normalized.error, undefined, "Should not have an error");
  Assert.deepEqual(normalized.errors, [], "Should not have a warning");

  normalized = await ExtensionTestUtils.normalizeManifest({
    "browser_action": {
      "default_icon": "icon.png",
    },
  });

  equal(normalized.error, undefined, "Should not have an error");
  Assert.deepEqual(normalized.errors, [], "Should not have a warning");
});
