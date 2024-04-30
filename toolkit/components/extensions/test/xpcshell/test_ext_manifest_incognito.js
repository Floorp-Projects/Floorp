/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_manifest_incognito() {
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

  ExtensionTestUtils.failOnSchemaWarnings(false);
  normalized = await ExtensionTestUtils.normalizeManifest({
    incognito: "whatisthis",
  });
  ExtensionTestUtils.failOnSchemaWarnings(true);

  equal(
    normalized.error,
    'Error processing incognito: Invalid enumeration value "whatisthis"',
    "Should have an error"
  );
  Assert.deepEqual(normalized.errors, [], "Should not have a warning");

  // Sanity check: Default value of "incognito" is "spanning".
  normalized = await ExtensionTestUtils.normalizeManifest({});
  equal(normalized.error, undefined, "Should not have an error");
  equal(normalized.errors.length, 0, "Should not have warnings");
  equal(
    normalized.value.incognito,
    "spanning",
    "Should have the expected default value for incognito when unspecified"
  );
});

add_task(async function test_manifest_incognito_split_fallback_not_allowed() {
  ExtensionTestUtils.failOnSchemaWarnings(false);
  let normalized = await ExtensionTestUtils.normalizeManifest({
    incognito: "split",
  });
  ExtensionTestUtils.failOnSchemaWarnings(true);

  equal(normalized.error, undefined, "Should not have an error");
  Assert.deepEqual(
    normalized.errors,
    [
      `Warning processing incognito: incognito "split" is unsupported. Falling back to incognito "not_allowed".`,
    ],
    "Should have a warning"
  );
  equal(
    normalized.value.incognito,
    "not_allowed",
    "incognito:split should fall back to not_allowed by default"
  );
});
