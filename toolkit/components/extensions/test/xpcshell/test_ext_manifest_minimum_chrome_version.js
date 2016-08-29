/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";


add_task(function* test_manifest_minimum_chrome_version() {
  let normalized = yield ExtensionTestUtils.normalizeManifest({
    "minimum_chrome_version": "42",
  });

  equal(normalized.error, undefined, "Should not have an error");
  equal(normalized.errors.length, 0, "Should not have warnings");
});
