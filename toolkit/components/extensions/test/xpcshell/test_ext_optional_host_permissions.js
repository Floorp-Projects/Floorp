"use strict";

async function runTest({
  manifest_version = 3,
  optional_host_permissions = undefined,
  expectOptionalOrigins = [],
  expectWarning = false,
}) {
  ExtensionTestUtils.failOnSchemaWarnings(!expectWarning);

  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version,
      optional_host_permissions,
    },
  });

  await extension.startup();

  let { warnings, manifestOptionalPermissions: actual } = extension.extension;

  Assert.deepEqual(
    actual.origins,
    expectOptionalOrigins,
    "Expected manifestOptionalPermissions."
  );

  Assert.deepEqual(actual.permissions, [], "Not expecting api permissions.");

  if (expectWarning) {
    Assert.ok(warnings[0].match(expectWarning), "Expected warning.");
  } else {
    Assert.ok(!warnings.length, "No warning expected.");
  }

  await extension.unload();
}

add_task(function test_api_permission_ignored() {
  return runTest({
    optional_host_permissions: ["https://example.com/", "tabs"],
    expectOptionalOrigins: ["https://example.com/"],
    expectWarning: /optional_host_permissions.+"tabs" must either: be/,
  });
});

add_task(function test_api_permission_all_urls() {
  return runTest({
    optional_host_permissions: ["<all_urls>"],
    expectOptionalOrigins: ["<all_urls>"],
  });
});

add_task(function test_api_permission_about_urls() {
  return runTest({
    optional_host_permissions: ["https://example.net/", "about:*"],
    expectOptionalOrigins: ["https://example.net/"],
  });
});

add_task(function test_api_permission_mv2() {
  return runTest({
    manifest_version: 2,
    optional_host_permissions: ["https://example.org/"],
    expectOptionalOrigins: [],
    expectWarning:
      /"optional_host_permissions" is unsupported in Manifest Version 2/,
  });
});
