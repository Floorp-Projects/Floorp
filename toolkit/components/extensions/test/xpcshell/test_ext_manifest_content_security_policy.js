/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);

add_task(async function test_manifest_csp() {
  let normalized = await ExtensionTestUtils.normalizeManifest({
    content_security_policy: "script-src 'self'; object-src 'none'",
  });

  equal(normalized.error, undefined, "Should not have an error");
  equal(normalized.errors.length, 0, "Should not have warnings");
  equal(
    normalized.value.content_security_policy,
    "script-src 'self'; object-src 'none'",
    "Should have the expected policy string"
  );

  ExtensionTestUtils.failOnSchemaWarnings(false);
  normalized = await ExtensionTestUtils.normalizeManifest({
    content_security_policy: "object-src 'none'",
  });
  ExtensionTestUtils.failOnSchemaWarnings(true);

  equal(normalized.error, undefined, "Should not have an error");

  Assert.deepEqual(
    normalized.errors,
    [
      "Error processing content_security_policy: Policy is missing a required ‘script-src’ directive",
    ],
    "Should have the expected warning"
  );

  equal(
    normalized.value.content_security_policy,
    null,
    "Invalid policy string should be omitted"
  );

  ExtensionTestUtils.failOnSchemaWarnings(false);
  normalized = await ExtensionTestUtils.normalizeManifest({
    manifest_version: 2,
    content_security_policy: {
      extension_pages: "script-src 'self'; object-src 'none'",
    },
  });
  ExtensionTestUtils.failOnSchemaWarnings(true);

  Assert.deepEqual(
    normalized.errors,
    [
      `Error processing content_security_policy: Expected string instead of {"extension_pages":"script-src 'self'; object-src 'none'"}`,
    ],
    "Should have the expected warning"
  );
});

add_task(async function test_manifest_csp_v3() {
  ExtensionTestUtils.failOnSchemaWarnings(false);
  let normalized = await ExtensionTestUtils.normalizeManifest({
    manifest_version: 3,
    content_security_policy: "script-src 'self'; object-src 'none'",
  });
  ExtensionTestUtils.failOnSchemaWarnings(true);

  Assert.deepEqual(
    normalized.errors,
    [
      `Error processing content_security_policy: Expected object instead of "script-src 'self'; object-src 'none'"`,
    ],
    "Should have the expected warning"
  );

  normalized = await ExtensionTestUtils.normalizeManifest({
    manifest_version: 3,
    content_security_policy: {
      extension_pages: "script-src 'self' 'unsafe-eval'; object-src 'none'",
    },
  });

  Assert.deepEqual(
    normalized.errors,
    [
      "Error processing content_security_policy.extension_pages: ‘script-src’ directive contains a forbidden 'unsafe-eval' keyword",
    ],
    "Should have the expected warning"
  );
  equal(
    normalized.value.content_security_policy.extension_pages,
    null,
    "Should have the expected policy string"
  );

  ExtensionTestUtils.failOnSchemaWarnings(false);
  normalized = await ExtensionTestUtils.normalizeManifest({
    manifest_version: 3,
    content_security_policy: {
      extension_pages: "object-src 'none'",
    },
  });
  ExtensionTestUtils.failOnSchemaWarnings(true);

  equal(normalized.error, undefined, "Should not have an error");
  equal(normalized.errors.length, 1, "Should have warnings");
  Assert.deepEqual(
    normalized.errors,
    [
      "Error processing content_security_policy.extension_pages: Policy is missing a required ‘script-src’ directive",
    ],
    "Should have the expected warning for extension_pages CSP"
  );
});
