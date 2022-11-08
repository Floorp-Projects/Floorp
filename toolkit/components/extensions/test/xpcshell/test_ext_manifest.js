/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "AddonManager",
  "resource://gre/modules/AddonManager.jsm"
);

AddonTestUtils.init(this);
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

async function testManifest(manifest, expectedError) {
  ExtensionTestUtils.failOnSchemaWarnings(false);
  let normalized = await ExtensionTestUtils.normalizeManifest(manifest);
  ExtensionTestUtils.failOnSchemaWarnings(true);

  if (expectedError) {
    ok(
      expectedError.test(normalized.error),
      `Should have an error for ${JSON.stringify(manifest)}, got ${
        normalized.error
      }`
    );
  } else {
    ok(
      !normalized.error,
      `Should not have an error ${JSON.stringify(manifest)}, ${
        normalized.error
      }`
    );
  }
  return normalized.errors;
}

async function testIconPaths(icon, manifest, expectedError) {
  let normalized = await ExtensionTestUtils.normalizeManifest(manifest);

  if (expectedError) {
    ok(
      expectedError.test(normalized.error),
      `Should have an error for ${JSON.stringify(icon)}`
    );
  } else {
    ok(!normalized.error, `Should not have an error ${JSON.stringify(icon)}`);
  }
}

add_setup(async () => {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_manifest() {
  let badpaths = ["", " ", "\t", "http://foo.com/icon.png"];
  for (let path of badpaths) {
    await testIconPaths(
      path,
      {
        icons: path,
      },
      /Error processing icons/
    );

    await testIconPaths(
      path,
      {
        icons: {
          "16": path,
        },
      },
      /Error processing icons/
    );
  }

  let paths = [
    "icon.png",
    "/icon.png",
    "./icon.png",
    "path to an icon.png",
    " icon.png",
  ];
  for (let path of paths) {
    // manifest.icons is an object
    await testIconPaths(
      path,
      {
        icons: path,
      },
      /Error processing icons/
    );

    await testIconPaths(path, {
      icons: {
        "16": path,
      },
    });
  }
});

add_task(async function test_manifest_warnings_on_unexpected_props() {
  let extension = await ExtensionTestUtils.loadExtension({
    manifest: {
      background: {
        scripts: ["bg.js"],
        wrong_prop: true,
      },
    },
    files: {
      "bg.js": "",
    },
  });

  ExtensionTestUtils.failOnSchemaWarnings(false);
  await extension.startup();
  ExtensionTestUtils.failOnSchemaWarnings(true);

  // Retrieve the warning message collected by the Extension class
  // packagingWarning method.
  const { warnings } = extension.extension;
  equal(warnings.length, 1, "Got the expected number of manifest warnings");

  const expectedMessage =
    "Reading manifest: Warning processing background.wrong_prop";
  ok(
    warnings[0].startsWith(expectedMessage),
    "Got the expected warning message format"
  );

  await extension.unload();
});

add_task(async function test_mv2_scripting_permission_always_enabled() {
  let warnings = await testManifest({
    manifest_version: 2,
    permissions: ["scripting"],
  });

  Assert.deepEqual(warnings, [], "Got no warnings");
});

add_task(
  {
    pref_set: [["extensions.manifestV3.enabled", true]],
  },
  async function test_mv3_scripting_permission_always_enabled() {
    let warnings = await testManifest({
      manifest_version: 3,
      permissions: ["scripting"],
    });

    Assert.deepEqual(warnings, [], "Got no warnings");
  }
);

add_task(async function test_simpler_version_format() {
  const TEST_CASES = [
    // Valid cases
    { version: "0", expectWarning: false },
    { version: "0.0", expectWarning: false },
    { version: "0.0.0", expectWarning: false },
    { version: "0.0.0.0", expectWarning: false },
    { version: "0.0.0.1", expectWarning: false },
    { version: "0.0.0.999999999", expectWarning: false },
    { version: "0.0.1.0", expectWarning: false },
    { version: "0.0.999999999", expectWarning: false },
    { version: "0.1.0.0", expectWarning: false },
    { version: "0.999999999", expectWarning: false },
    { version: "1", expectWarning: false },
    { version: "1.0", expectWarning: false },
    { version: "1.0.0", expectWarning: false },
    { version: "1.0.0.0", expectWarning: false },
    { version: "1.2.3.4", expectWarning: false },
    { version: "999999999", expectWarning: false },
    {
      version: "999999999.999999999.999999999.999999999",
      expectWarning: false,
    },
    // Invalid cases
    { version: ".", expectWarning: true },
    { version: ".999999999", expectWarning: true },
    { version: "0.0.0.0.0", expectWarning: true },
    { version: "0.0.0.00001", expectWarning: true },
    { version: "0.0.0.0010", expectWarning: true },
    { version: "0.0.00001", expectWarning: true },
    { version: "0.0.001", expectWarning: true },
    { version: "0.0.01.0", expectWarning: true },
    { version: "0.01.0", expectWarning: true },
    { version: "00001", expectWarning: true },
    { version: "0001", expectWarning: true },
    { version: "001", expectWarning: true },
    { version: "01", expectWarning: true },
    { version: "01.0", expectWarning: true },
    { version: "099999", expectWarning: true },
    { version: "0999999999", expectWarning: true },
    { version: "1.00000", expectWarning: true },
    { version: "1.1.-1", expectWarning: true },
    { version: "1.1000000000", expectWarning: true },
    { version: "1.1pre1aa", expectWarning: true },
    { version: "1.2.1000000000", expectWarning: true },
    { version: "1.2.3.4-a", expectWarning: true },
    { version: "1.2.3.4.5", expectWarning: true },
    { version: "1000000000", expectWarning: true },
    { version: "1000000000.0.0.0", expectWarning: true },
    { version: "999999999.", expectWarning: true },
  ];

  for (const { version, expectWarning } of TEST_CASES) {
    const normalized = await ExtensionTestUtils.normalizeManifest({ version });

    if (expectWarning) {
      Assert.deepEqual(
        normalized.errors,
        [
          `Warning processing version: version must be a version string ` +
            `consisting of at most 4 integers of at most 9 digits without ` +
            `leading zeros, and separated with dots`,
        ],
        `expected warning for version: ${version}`
      );
    } else {
      Assert.deepEqual(
        normalized.errors,
        [],
        `expected no warning for version: ${version}`
      );
    }
  }
});

add_task(async function test_applications() {
  const id = "some@id";
  const updateURL = "https://example.com/updates/";

  let extension = await ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 2,
      applications: {
        gecko: { id, update_url: updateURL },
      },
    },
    useAddonManager: "temporary",
  });

  ExtensionTestUtils.failOnSchemaWarnings(false);
  await extension.startup();
  ExtensionTestUtils.failOnSchemaWarnings(true);

  Assert.deepEqual(extension.extension.warnings, [], "expected no warnings");

  const addon = await AddonManager.getAddonByID(extension.id);
  ok(addon, "got an add-on");
  equal(addon.id, id, "got expected ID");
  equal(addon.updateURL, updateURL, "got expected update URL");

  await extension.unload();
});

add_task(
  {
    pref_set: [["extensions.manifestV3.enabled", true]],
  },
  async function test_applications_key_mv3() {
    let warnings = await testManifest({
      manifest_version: 3,
      applications: {},
    });

    Assert.deepEqual(
      warnings,
      [`Property "applications" is unsupported in Manifest Version 3`],
      `Manifest v3 with "applications" key logs an error.`
    );
  }
);
