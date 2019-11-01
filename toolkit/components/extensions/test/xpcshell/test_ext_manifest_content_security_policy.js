/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

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
      'Error processing content_security_policy: Value "object-src \'none\'" must either: match the format "contentSecurityPolicy", or be an object value',
    ],
    "Should have the expected warning"
  );

  equal(
    normalized.value.content_security_policy,
    null,
    "Invalid policy string should be omitted"
  );
});

add_task(async function test_manifest_csp_v3() {
  let normalized = await ExtensionTestUtils.normalizeManifest({
    content_security_policy: {
      extension_pages: "script-src 'self'; object-src 'none'",
    },
  });

  equal(normalized.error, undefined, "Should not have an error");
  equal(normalized.errors.length, 0, "Should not have warnings");
  equal(
    normalized.value.content_security_policy.extension_pages,
    "script-src 'self'; object-src 'none'",
    "Should have the expected poilcy string"
  );

  ExtensionTestUtils.failOnSchemaWarnings(false);
  normalized = await ExtensionTestUtils.normalizeManifest({
    content_security_policy: {
      extension_pages: "object-src 'none'",
    },
  });
  ExtensionTestUtils.failOnSchemaWarnings(true);

  equal(normalized.error, undefined, "Should not have an error");
  equal(normalized.errors.length, 2, "Should have warnings");
  Assert.deepEqual(
    normalized.errors,
    [
      "Error processing content_security_policy.extension_pages: Policy is missing a required ‘script-src’ directive",
      'Error processing content_security_policy: Value must either: be a string value, or .extension_pages must match the format "contentSecurityPolicy"',
    ],
    "Should have the expected warning"
  );
});
