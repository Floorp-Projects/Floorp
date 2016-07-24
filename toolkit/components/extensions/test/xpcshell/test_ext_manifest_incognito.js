/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";


add_task(function* test_manifest_incognito() {
  let normalized = yield ExtensionTestUtils.normalizeManifest({
    "incognito": "spanning",
  });

  equal(normalized.error, undefined, "Should not have an error");
  equal(normalized.errors.length, 0, "Should not have warnings");
  equal(normalized.value.incognito,
        "spanning",
        "Should have the expected incognito string");

  normalized = yield ExtensionTestUtils.normalizeManifest({
    "incognito": "split",
  });

  equal(normalized.error, undefined, "Should not have an error");
  Assert.deepEqual(normalized.errors,
                   ['Error processing incognito: Invalid enumeration value "split"'],
                   "Should have the expected warning");
  equal(normalized.value.incognito, null,
        "Invalid incognito string should be omitted");
});
