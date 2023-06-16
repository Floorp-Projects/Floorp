/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineESModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
});

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
          16: path,
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
        16: path,
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

add_task(async function test_bss_gecko_android() {
  const addonId = "some@id";
  const isAndroid = AppConstants.platform == "android";

  const TEST_CASES = [
    {
      title: "gecko_android overrides gecko",
      browser_specific_settings: {
        gecko: {
          id: addonId,
          strict_min_version: "2",
          strict_max_version: "2",
        },
        gecko_android: {
          strict_min_version: "1",
          strict_max_version: "1",
        },
      },
      expectedError: isAndroid
        ? `Add-on ${addonId} is not compatible with application version. add-on minVersion: 1. add-on maxVersion: 1.`
        : `Add-on ${addonId} is not compatible with application version. add-on minVersion: 2. add-on maxVersion: 2.`,
    },
    {
      title:
        "strict_min_version in gecko_android overrides gecko.strict_min_version",
      browser_specific_settings: {
        gecko: {
          id: addonId,
          strict_min_version: "2",
          strict_max_version: "3",
        },
        gecko_android: {
          strict_min_version: "3",
        },
      },
      expectedError: isAndroid
        ? `Add-on ${addonId} is not compatible with application version. add-on minVersion: 3. add-on maxVersion: 3.`
        : `Add-on ${addonId} is not compatible with application version. add-on minVersion: 2. add-on maxVersion: 3.`,
    },
    {
      title:
        "strict_max_version in gecko_android overrides gecko.strict_max_version",
      browser_specific_settings: {
        gecko: {
          id: addonId,
          strict_min_version: "2",
          strict_max_version: "2",
        },
        gecko_android: {
          strict_max_version: "3",
        },
      },
      expectedError: isAndroid
        ? `Add-on ${addonId} is not compatible with application version. add-on minVersion: 2. add-on maxVersion: 3.`
        : `Add-on ${addonId} is not compatible with application version. add-on minVersion: 2. add-on maxVersion: 2.`,
    },
    {
      title: "no gecko_android",
      browser_specific_settings: {
        gecko: {
          id: addonId,
          strict_min_version: "2",
          strict_max_version: "2",
        },
      },
      expectedError: `Add-on ${addonId} is not compatible with application version. add-on minVersion: 2. add-on maxVersion: 2.`,
    },
    {
      title: "empty gecko_android",
      browser_specific_settings: {
        gecko: {
          id: addonId,
          strict_min_version: "2",
          strict_max_version: "2",
        },
        gecko_android: {},
      },
      expectedError: `Add-on ${addonId} is not compatible with application version. add-on minVersion: 2. add-on maxVersion: 2.`,
    },
    {
      title: "empty strict min/max versions in gecko_android",
      browser_specific_settings: {
        gecko: {
          id: addonId,
          strict_min_version: "2",
          strict_max_version: "2",
        },
        gecko_android: {
          strict_min_version: "",
          strict_max_version: "",
        },
      },
      expectedError: `Add-on ${addonId} is not compatible with application version. add-on minVersion: 2. add-on maxVersion: 2.`,
    },
    {
      title: "unsupported prop in gecko_android",
      browser_specific_settings: {
        gecko: {
          id: addonId,
          strict_min_version: "2",
          strict_max_version: "2",
        },
        gecko_android: {
          aPropThatIsNotSupported: "aPropThatIsNotSupported",
        },
      },
      expectedError: `Add-on ${addonId} is not compatible with application version. add-on minVersion: 2. add-on maxVersion: 2.`,
    },
    {
      title: "only strict min/max version in gecko_android",
      browser_specific_settings: {
        gecko: {
          id: addonId,
        },
        gecko_android: {
          strict_min_version: "3",
          strict_max_version: "4",
        },
      },
      expectedError: isAndroid
        ? `Add-on ${addonId} is not compatible with application version. add-on minVersion: 3. add-on maxVersion: 4.`
        : null,
    },
    {
      title: "only strict_min_version in gecko_android",
      browser_specific_settings: {
        gecko: {
          id: addonId,
        },
        gecko_android: {
          // The app version is set to `42` at the top of the file.
          strict_min_version: "100",
        },
      },
      expectedError: isAndroid
        ? `Add-on ${addonId} is not compatible with application version. add-on minVersion: 100.`
        : null,
    },
  ];

  for (const {
    title,
    browser_specific_settings,
    expectedError,
  } of TEST_CASES) {
    info(`verifying: ${title}`);

    // This task is mainly about verifying `bss.gecko_android` and some test
    // cases require a "valid" compatibility range by default, which would
    // break the assumption below (that the install is going to fail). This is
    // why we skip null errors, but only on non-Android builds.
    if (expectedError === null) {
      notEqual(
        AppConstants.platform,
        "android",
        `${title} - expected no error on a non-Android build`
      );
      continue;
    }

    const manifest = {
      manifest_version: 2,
      version: "1.0",
      browser_specific_settings,
    };

    const extension = ExtensionTestUtils.loadExtension({
      manifest,
      useAddonManager: "temporary",
    });

    await Assert.rejects(
      extension.startup(),
      new RegExp(expectedError),
      `${title} - expected error: ${expectedError}`
    );

    const addon = await AddonManager.getAddonByID(addonId);
    equal(addon, null, "add-on is not installed");
  }
});

add_task(
  { skip_if: AppConstants.platform !== "android" },
  async function test_bss_gecko_android_only() {
    const extension = ExtensionTestUtils.loadExtension({
      manifest: {
        manifest_version: 2,
        version: "1.0",
        browser_specific_settings: {
          gecko_android: {
            strict_min_version: "0",
          },
        },
      },
    });

    await extension.startup();

    const { manifest } = extension.extension;
    equal(
      manifest.browser_specific_settings.gecko_android.strict_min_version,
      "0",
      "expected strict min version"
    );

    await extension.unload();
  }
);
