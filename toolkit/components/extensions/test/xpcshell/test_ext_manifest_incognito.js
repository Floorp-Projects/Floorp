/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_manifest_incognito() {
  Services.prefs.setBoolPref("extensions.allowPrivateBrowsingByDefault", false);

  let normalized = await ExtensionTestUtils.normalizeManifest({
    incognito: "spanning",
  });

  equal(normalized.error, undefined, "Should not have an error");
  equal(normalized.errors.length, 0, "Should not have warnings");
  equal(
    normalized.value.incognito,
    "spanning",
    "Should have the expected incognito string"
  );

  normalized = await ExtensionTestUtils.normalizeManifest({
    incognito: "not_allowed",
  });

  equal(normalized.error, undefined, "Should not have an error");
  equal(normalized.errors.length, 0, "Should not have warnings");
  equal(
    normalized.value.incognito,
    "not_allowed",
    "Should have the expected incognito string"
  );

  normalized = await ExtensionTestUtils.normalizeManifest({
    incognito: "split",
  });

  equal(
    normalized.error,
    'Error processing incognito: Invalid enumeration value "split"',
    "Should have an error"
  );
  Assert.deepEqual(normalized.errors, [], "Should not have a warning");
  equal(
    normalized.value,
    undefined,
    "Invalid incognito string should be undefined"
  );
  Services.prefs.clearUserPref("extensions.allowPrivateBrowsingByDefault");
});
