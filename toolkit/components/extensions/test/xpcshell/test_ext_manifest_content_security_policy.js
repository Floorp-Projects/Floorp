/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";


add_task(function* test_manifest_csp() {
  let normalized = yield ExtensionTestUtils.normalizeManifest({
    "content_security_policy": "script-src 'self'; object-src 'none'",
  });

  equal(normalized.error, undefined, "Should not have an error");
  equal(normalized.errors.length, 0, "Should not have warnings");
  equal(normalized.value.content_security_policy,
        "script-src 'self'; object-src 'none'",
        "Should have the expected poilcy string");


  normalized = yield ExtensionTestUtils.normalizeManifest({
    "content_security_policy": "object-src 'none'",
  });

  equal(normalized.error, undefined, "Should not have an error");

  Assert.deepEqual(normalized.errors,
                   ["Error processing content_security_policy: SyntaxError: Policy is missing a required \u2018script-src\u2019 directive"],
                   "Should have the expected warning");

  equal(normalized.value.content_security_policy, null,
        "Invalid policy string should be omitted");
});
