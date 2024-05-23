AddonTestUtils.overrideCertDB();
AddonTestUtils.usePrivilegedSignatures = id => id.startsWith("privileged-");

// Some tests in this test file can't run on android build because
// theme addon type isn't supported.
const skipOnAndroid = () => ({
  skip_if: () => AppConstants.platform === "android",
});

let profileDir;
add_setup(async function setup() {
  profileDir = gProfD.clone();
  profileDir.append("extensions");

  if (!profileDir.exists()) {
    profileDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  }

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  await promiseStartupManager();
});

// Extension without id in the manifest file and signed with AMO prod root
// (https://addons.mozilla.org/en-US/firefox/addon/reference-static-theme/).
const IMPLICIT_ID_XPI = "data/webext-implicit-id.xpi";
const IMPLICIT_ID_ID = "{46607a7b-1b2a-40ce-9afe-91cda52c46a6}";

// webext-implicit-id.xpi has a minimal manifest with no
// applications or browser_specific_settings, so its id comes
// from its signature, which should be the ID constant defined below.
add_task(skipOnAndroid(), async function test_implicit_id() {
  let addon = await promiseAddonByID(IMPLICIT_ID_ID);
  equal(addon, null, "Add-on is not installed");

  await promiseInstallFile(do_get_file(IMPLICIT_ID_XPI));

  addon = await promiseAddonByID(IMPLICIT_ID_ID);
  notEqual(addon, null, "Add-on is installed");

  await addon.uninstall();
});

// We should also be able to install webext-implicit-id.xpi temporarily
// and it should look just like the regular install (ie, the ID should
// come from the signature)
add_task(skipOnAndroid(), async function test_implicit_id_temp() {
  let addon = await promiseAddonByID(IMPLICIT_ID_ID);
  equal(addon, null, "Add-on is not installed");

  let xpifile = do_get_file(IMPLICIT_ID_XPI);
  await AddonManager.installTemporaryAddon(xpifile);

  addon = await promiseAddonByID(IMPLICIT_ID_ID);
  notEqual(addon, null, "Add-on is installed");

  // The sourceURI of a temporary installed addon should be equal to the
  // file url of the installed xpi file.
  equal(
    addon.sourceURI && addon.sourceURI.spec,
    Services.io.newFileURI(xpifile).spec,
    "SourceURI of the add-on has the expected value"
  );

  await addon.uninstall();
});

// Test that extension install error attach the detailed error messages to the
// Error object.
add_task(async function test_invalid_extension_install_errors() {
  const manifest = {
    name: "invalid",
    browser_specific_settings: {
      gecko: {
        id: "invalid@tests.mozilla.org",
      },
    },
    description: "extension with an invalid 'matches' value",
    manifest_version: 2,
    content_scripts: [
      {
        matches: "*://*.foo.com/*",
        js: ["content.js"],
      },
    ],
    version: "1.0",
  };

  const addonDir = await promiseWriteWebManifestForExtension(
    manifest,
    gTmpD,
    "the-addon-sub-dir"
  );

  await Assert.rejects(
    AddonManager.installTemporaryAddon(addonDir),
    err => {
      return (
        err.additionalErrors.length == 1 &&
        err.additionalErrors[0] ==
          `Reading manifest: Error processing content_scripts.0.matches: ` +
            `Expected array instead of "*://*.foo.com/*"`
      );
    },
    "Exception has the proper additionalErrors details"
  );

  Services.obs.notifyObservers(addonDir, "flush-cache-entry");
  addonDir.remove(true);
});

// We should be able to temporarily install an unsigned web extension
// that does not have an ID in its manifest.
add_task(async function test_unsigned_no_id_temp_install() {
  AddonTestUtils.useRealCertChecks = true;
  const manifest = {
    name: "no ID",
    description: "extension without an ID",
    manifest_version: 2,
    version: "1.0",
  };

  const addonDir = await promiseWriteWebManifestForExtension(
    manifest,
    gTmpD,
    "the-addon-sub-dir"
  );
  const testDate = new Date();
  const addon = await AddonManager.installTemporaryAddon(addonDir);

  ok(addon.id, "ID should have been auto-generated");
  Assert.less(
    Math.abs(addon.installDate - testDate),
    10000,
    "addon has an expected installDate"
  );
  Assert.less(
    Math.abs(addon.updateDate - testDate),
    10000,
    "addon has an expected updateDate"
  );

  // The sourceURI of a temporary installed addon should be equal to the
  // file url of the installed source dir.
  equal(
    addon.sourceURI && addon.sourceURI.spec,
    Services.io.newFileURI(addonDir).spec,
    "SourceURI of the add-on has the expected value"
  );

  // Install the same directory again, as if re-installing or reloading.
  const secondAddon = await AddonManager.installTemporaryAddon(addonDir);
  // The IDs should be the same.
  equal(secondAddon.id, addon.id, "Reinstalled add-on has the expected ID");
  equal(
    secondAddon.installDate.valueOf(),
    addon.installDate.valueOf(),
    "Reloaded add-on has the expected installDate."
  );

  await secondAddon.uninstall();
  Services.obs.notifyObservers(addonDir, "flush-cache-entry");
  addonDir.remove(true);
  AddonTestUtils.useRealCertChecks = false;
});

// We should be able to install two extensions from manifests without IDs
// at different locations and get two unique extensions.
add_task(async function test_multiple_no_id_extensions() {
  AddonTestUtils.useRealCertChecks = true;
  const manifest = {
    name: "no ID",
    description: "extension without an ID",
    manifest_version: 2,
    version: "1.0",
  };

  let extension1 = ExtensionTestUtils.loadExtension({
    manifest,
    useAddonManager: "temporary",
  });

  let extension2 = ExtensionTestUtils.loadExtension({
    manifest,
    useAddonManager: "temporary",
  });

  await Promise.all([extension1.startup(), extension2.startup()]);

  const allAddons = await AddonManager.getAllAddons();

  info(`Found these add-ons: ${allAddons.map(a => a.name).join(", ")}`);
  const filtered = allAddons.filter(addon => addon.name === manifest.name);
  // Make sure we have two add-ons by the same name.
  equal(filtered.length, 2, "Two add-ons are installed with the same name");

  await extension1.unload();
  await extension2.unload();
  AddonTestUtils.useRealCertChecks = false;
});

// Test that we can get the ID from browser_specific_settings
add_task(async function test_bss_id() {
  const ID = "webext_bss_id@tests.mozilla.org";

  let manifest = {
    name: "bss test",
    description: "test that ID may be in browser_specific_settings",
    manifest_version: 2,
    version: "1.0",

    browser_specific_settings: {
      gecko: {
        id: ID,
      },
    },
  };

  let addon = await promiseAddonByID(ID);
  equal(addon, null, "Add-on is not installed");

  let extension = ExtensionTestUtils.loadExtension({
    manifest,
    useAddonManager: "temporary",
  });
  await extension.startup();

  addon = await promiseAddonByID(ID);
  notEqual(addon, null, "Add-on is installed");

  await extension.unload();
});

// Test that if we have IDs in both browser_specific_settings and applications,
// that we prefer the ID in browser_specific_settings.
add_task(async function test_two_ids() {
  const GOOD_ID = "two_ids@tests.mozilla.org";
  const BAD_ID = "i_am_obsolete@tests.mozilla.org";

  let manifest = {
    name: "two id test",
    description:
      "test a web extension with ids in both applications and browser_specific_settings",
    manifest_version: 2,
    version: "1.0",

    applications: {
      gecko: {
        id: BAD_ID,
      },
    },

    browser_specific_settings: {
      gecko: {
        id: GOOD_ID,
      },
    },
  };

  let extension = ExtensionTestUtils.loadExtension({
    manifest,
    useAddonManager: "temporary",
  });
  await extension.startup();

  let addon = await promiseAddonByID(BAD_ID);
  equal(addon, null, "Add-on is not found using bad ID");
  addon = await promiseAddonByID(GOOD_ID);
  notEqual(addon, null, "Add-on is found using good ID");

  await extension.unload();
});

// Test that strict_min_version and strict_max_version are enforced for
// loading temporary extension.
add_task(async function test_strict_min_max() {
  // the app version being compared to is 1.9.2
  const addonId = "strict_min_max@tests.mozilla.org";
  const MANIFEST = {
    name: "strict min max test",
    description: "test strict min and max with temporary loading",
    manifest_version: 2,
    version: "1.0",
  };

  // bad max good min
  let apps = {
    browser_specific_settings: {
      gecko: {
        id: addonId,
        strict_min_version: "1",
        strict_max_version: "1",
      },
    },
  };
  let testManifest = Object.assign(apps, MANIFEST);

  let extension = ExtensionTestUtils.loadExtension({
    manifest: testManifest,
    useAddonManager: "temporary",
  });

  let expectedMsg = new RegExp(
    "Add-on strict_min_max@tests.mozilla.org is not compatible with application version. " +
      "add-on minVersion: 1. add-on maxVersion: 1."
  );
  await Assert.rejects(
    extension.startup(),
    expectedMsg,
    "Install rejects when specified maxVersion is not valid"
  );

  let addon = await promiseAddonByID(addonId);
  equal(addon, null, "Add-on is not installed");

  // bad min good max
  apps = {
    browser_specific_settings: {
      gecko: {
        id: addonId,
        strict_min_version: "2",
        strict_max_version: "2",
      },
    },
  };
  testManifest = Object.assign(apps, MANIFEST);

  extension = ExtensionTestUtils.loadExtension({
    manifest: testManifest,
    useAddonManager: "temporary",
  });

  expectedMsg = new RegExp(
    "Add-on strict_min_max@tests.mozilla.org is not compatible with application version. " +
      "add-on minVersion: 2. add-on maxVersion: 2."
  );
  await Assert.rejects(
    extension.startup(),
    expectedMsg,
    "Install rejects when specified minVersion is not valid"
  );

  addon = await promiseAddonByID(addonId);
  equal(addon, null, "Add-on is not installed");

  // bad both
  apps = {
    browser_specific_settings: {
      gecko: {
        id: addonId,
        strict_min_version: "2",
        strict_max_version: "1",
      },
    },
  };
  testManifest = Object.assign(apps, MANIFEST);

  extension = ExtensionTestUtils.loadExtension({
    manifest: testManifest,
    useAddonManager: "temporary",
  });

  expectedMsg = new RegExp(
    "Add-on strict_min_max@tests.mozilla.org is not compatible with application version. " +
      "add-on minVersion: 2. add-on maxVersion: 1."
  );
  await Assert.rejects(
    extension.startup(),
    expectedMsg,
    "Install rejects when specified minVersion and maxVersion are not valid"
  );

  addon = await promiseAddonByID(addonId);
  equal(addon, null, "Add-on is not installed");

  // bad only min
  apps = {
    browser_specific_settings: {
      gecko: {
        id: addonId,
        strict_min_version: "2",
      },
    },
  };
  testManifest = Object.assign(apps, MANIFEST);

  extension = ExtensionTestUtils.loadExtension({
    manifest: testManifest,
    useAddonManager: "temporary",
  });

  expectedMsg = new RegExp(
    "Add-on strict_min_max@tests.mozilla.org is not compatible with application version. " +
      "add-on minVersion: 2."
  );
  await Assert.rejects(
    extension.startup(),
    expectedMsg,
    "Install rejects when specified minVersion and maxVersion are not valid"
  );

  addon = await promiseAddonByID(addonId);
  equal(addon, null, "Add-on is not installed");

  // bad only max
  apps = {
    browser_specific_settings: {
      gecko: {
        id: addonId,
        strict_max_version: "1",
      },
    },
  };
  testManifest = Object.assign(apps, MANIFEST);

  extension = ExtensionTestUtils.loadExtension({
    manifest: testManifest,
    useAddonManager: "temporary",
  });

  expectedMsg = new RegExp(
    "Add-on strict_min_max@tests.mozilla.org is not compatible with application version. " +
      "add-on maxVersion: 1."
  );
  await Assert.rejects(
    extension.startup(),
    expectedMsg,
    "Install rejects when specified minVersion and maxVersion are not valid"
  );

  addon = await promiseAddonByID(addonId);
  equal(addon, null, "Add-on is not installed");

  // good both
  apps = {
    browser_specific_settings: {
      gecko: {
        id: addonId,
        strict_min_version: "1",
        strict_max_version: "2",
      },
    },
  };
  testManifest = Object.assign(apps, MANIFEST);

  extension = ExtensionTestUtils.loadExtension({
    manifest: testManifest,
    useAddonManager: "temporary",
  });

  await extension.startup();
  addon = await promiseAddonByID(addonId);

  notEqual(addon, null, "Add-on is installed");
  equal(addon.id, addonId, "Installed add-on has the expected ID");
  await extension.unload();

  // good only min
  let newId = "strict_min_only@tests.mozilla.org";
  apps = {
    browser_specific_settings: {
      gecko: {
        id: newId,
        strict_min_version: "1",
      },
    },
  };
  testManifest = Object.assign(apps, MANIFEST);

  extension = ExtensionTestUtils.loadExtension({
    manifest: testManifest,
    useAddonManager: "temporary",
  });

  await extension.startup();
  addon = await promiseAddonByID(newId);

  notEqual(addon, null, "Add-on is installed");
  equal(addon.id, newId, "Installed add-on has the expected ID");

  await extension.unload();

  // good only max
  newId = "strict_max_only@tests.mozilla.org";
  apps = {
    browser_specific_settings: {
      gecko: {
        id: newId,
        strict_max_version: "2",
      },
    },
  };
  testManifest = Object.assign(apps, MANIFEST);

  extension = ExtensionTestUtils.loadExtension({
    manifest: testManifest,
    useAddonManager: "temporary",
  });

  await extension.startup();
  addon = await promiseAddonByID(newId);

  notEqual(addon, null, "Add-on is installed");
  equal(addon.id, newId, "Installed add-on has the expected ID");

  await extension.unload();

  // * in min will throw an error
  for (let version of ["0.*", "0.*.0"]) {
    newId = "strict_min_star@tests.mozilla.org";
    let minStarApps = {
      browser_specific_settings: {
        gecko: {
          id: newId,
          strict_min_version: version,
        },
      },
    };

    let minStarTestManifest = Object.assign(minStarApps, MANIFEST);

    let minStarExtension = ExtensionTestUtils.loadExtension({
      manifest: minStarTestManifest,
      useAddonManager: "temporary",
    });

    await Assert.rejects(
      minStarExtension.startup(),
      /The use of '\*' in strict_min_version is invalid/,
      "loading an extension with a * in strict_min_version throws an exception"
    );

    let minStarAddon = await promiseAddonByID(newId);
    equal(minStarAddon, null, "Add-on is not installed");
  }

  // incompatible extension but with compatibility checking off
  newId = "checkCompatibility@tests.mozilla.org";
  apps = {
    browser_specific_settings: {
      gecko: {
        id: newId,
        strict_max_version: "1",
      },
    },
  };
  testManifest = Object.assign(apps, MANIFEST);

  extension = ExtensionTestUtils.loadExtension({
    manifest: testManifest,
    useAddonManager: "temporary",
  });

  let savedCheckCompatibilityValue = AddonManager.checkCompatibility;
  AddonManager.checkCompatibility = false;
  await extension.startup();
  addon = await promiseAddonByID(newId);

  notEqual(addon, null, "Add-on is installed");
  equal(addon.id, newId, "Installed add-on has the expected ID");

  await extension.unload();
  AddonManager.checkCompatibility = savedCheckCompatibilityValue;
});

// Check permissions prompt
add_task(async function test_permissions_prompt() {
  const manifest = {
    name: "permissions test",
    description: "permissions test",
    manifest_version: 2,
    version: "1.0",

    permissions: ["tabs", "storage", "https://*.example.com/*", "<all_urls>"],
  };

  let xpi = ExtensionTestCommon.generateXPI({ manifest });

  let install = await AddonManager.getInstallForFile(xpi);

  let perminfo;
  install.promptHandler = info => {
    perminfo = info;
    return Promise.resolve();
  };

  await install.install();

  notEqual(perminfo, undefined, "Permission handler was invoked");
  equal(
    perminfo.existingAddon,
    null,
    "Permission info does not include an existing addon"
  );
  notEqual(perminfo.addon, null, "Permission info includes the new addon");
  let perms = perminfo.addon.userPermissions;
  deepEqual(
    perms.permissions,
    ["tabs", "storage"],
    "API permissions are correct"
  );
  deepEqual(
    perms.origins,
    ["https://*.example.com/*", "<all_urls>"],
    "Host permissions are correct"
  );

  let addon = await promiseAddonByID(perminfo.addon.id);
  notEqual(addon, null, "Extension was installed");

  await addon.uninstall();
  await IOUtils.remove(xpi.path);
});

// Check normalized optional origins.
add_task(async function test_normalized_optional_origins() {
  const assertAddonWrapperPermissionsProperties = async (
    manifest,
    expected
  ) => {
    let xpi = ExtensionTestCommon.generateXPI({ manifest });

    let install = await AddonManager.getInstallForFile(xpi);

    let perminfo;
    let installPromptCompletedDeferred = Promise.withResolvers();
    let installPromptShownDeferred = Promise.withResolvers();
    install.promptHandler = info => {
      perminfo = info;
      installPromptShownDeferred.resolve();
      return installPromptCompletedDeferred.promise;
    };

    const promiseInstalled = install.install();
    info("Wait for the install prompt");
    await installPromptShownDeferred.promise;

    equal(
      await promiseAddonByID(perminfo.addon.id),
      null,
      "Extension should not be installed yet"
    );
    notEqual(perminfo, undefined, "Permission handler was invoked");
    notEqual(perminfo.addon, null, "Permission info includes the new addon");
    equal(
      perminfo.addon.isPrivileged,
      expected.isPrivileged,
      `Expect the addon to be ${expected.isPrivileged ? "" : "non-"}privileged`
    );
    let addon = perminfo.addon;
    deepEqual(
      addon.userPermissions,
      expected.userPermissions,
      "userPermissions are correct"
    );
    deepEqual(
      addon.optionalPermissions,
      expected.optionalPermissions,
      "optionalPermissions are correct"
    );
    info("Assert normalized optional origins on non-installed extension");
    deepEqual(
      addon.optionalOriginsNormalized,
      expected.optionalOriginsNormalized,
      "optionalOriginsNormalized are correct"
    );

    installPromptCompletedDeferred.resolve();
    await promiseInstalled;
    addon = await promiseAddonByID(perminfo.addon.id);
    notEqual(addon, null, "Extension was installed successfully");

    info("Assert normalized optional origins again on installed extension");
    deepEqual(
      addon.optionalOriginsNormalized,
      expected.optionalOriginsNormalized,
      "Normalized origin permissions are correct"
    );

    await addon.uninstall();
    await IOUtils.remove(xpi.path);
  };

  info(
    "Test normalized optional origins on non-privileged ManifestV2 extension"
  );
  const manifestV2 = {
    name: "permissions test mv2",
    manifest_version: 2,
    // Include a permission that would trigger an install time prompt.
    permissions: ["tabs"],
    // Include optional origins to be normalized.
    // NOTE: we expect the normalized origins to be deduplicated.
    optional_permissions: [
      "http://*.example.com/",
      "http://*.example.com/somepath/",
      "*://example.org/*",
      "*://example.org/another-path/*",
      "file://*/*",
    ],
  };

  await assertAddonWrapperPermissionsProperties(manifestV2, {
    isPrivileged: false,
    userPermissions: { permissions: manifestV2.permissions, origins: [] },
    optionalPermissions: {
      permissions: [],
      origins: manifestV2.optional_permissions,
    },
    optionalOriginsNormalized: [
      "http://*.example.com/*",
      "*://example.org/*",
      "file://*/*",
    ],
  });

  info(
    "Test normalized optional origins on non-privileged ManifestV3 extension"
  );
  const manifestV3 = {
    name: "permissions test mv3",
    manifest_version: 3,
    // Include a permission that would trigger an install time prompt.
    permissions: ["tabs"],

    // Content Scripts match patterns are also expected to be part
    // of the optional host permissions (and to be deduplicated).
    content_scripts: [
      {
        matches: ["*://*/*"],
        js: ["script.js"],
      },
      {
        matches: ["*://example.org/*"],
        js: ["script2.js"],
      },
    ],

    // Include optional origins to be normalized.
    host_permissions: [
      "http://*.example.com/",
      "*://example.org/*",
      "file://*/*",
    ],
  };

  await assertAddonWrapperPermissionsProperties(manifestV3, {
    isPrivileged: false,
    userPermissions: { permissions: manifestV3.permissions, origins: [] },
    optionalPermissions: {
      permissions: [],
      origins: [...manifestV3.host_permissions, "*://*/*"],
    },
    optionalOriginsNormalized: [
      "http://*.example.com/*",
      "*://example.org/*",
      "file://*/*",
      "*://*/*",
    ],
  });

  info("Test normalized optional origins on privileged ManifestV2 extension");
  const manifestV2Privileged = {
    name: "permissions test privileged mv2",
    manifest_version: 2,
    browser_specific_settings: { gecko: { id: "privileged-mv2@ext" } },
    // Include a permission that would trigger an install time prompt.
    permissions: ["tabs", "mozillaAddons"],
    optional_permissions: [
      "http://*.example.com/",
      "*://example.org/*",
      "file://*/*",
      "resource://gre/",
    ],
  };

  await assertAddonWrapperPermissionsProperties(manifestV2Privileged, {
    isPrivileged: true,
    userPermissions: {
      permissions: manifestV2Privileged.permissions,
      origins: [],
    },
    optionalPermissions: {
      permissions: [],
      origins: manifestV2Privileged.optional_permissions,
    },
    optionalOriginsNormalized: [
      "http://*.example.com/*",
      "*://example.org/*",
      "file://*/*",
      "resource://gre/*",
    ],
  });
});

// Check permissions prompt cancellation
add_task(async function test_permissions_prompt_cancel() {
  const manifest = {
    name: "permissions test",
    description: "permissions test",
    manifest_version: 2,
    version: "1.0",

    permissions: ["webRequestBlocking"],
  };

  let xpi = ExtensionTestCommon.generateXPI({ manifest });

  let install = await AddonManager.getInstallForFile(xpi);

  let perminfo;
  install.promptHandler = info => {
    perminfo = info;
    return Promise.reject();
  };

  await promiseCompleteInstall(install);

  notEqual(perminfo, undefined, "Permission handler was invoked");

  let addon = await promiseAddonByID(perminfo.addon.id);
  equal(addon, null, "Extension was not installed");

  await IOUtils.remove(xpi.path);
});

// Test that bss overrides applications if both are present.
add_task(async function test_duplicate_bss() {
  const ID = "expected@tests.mozilla.org";

  const manifest = {
    manifest_version: 2,
    version: "1.0",
    applications: {
      gecko: { id: "unexpected@tests.mozilla.org" },
    },
    browser_specific_settings: {
      gecko: { id: ID },
    },
  };

  const extension = ExtensionTestUtils.loadExtension({
    manifest,
    useAddonManager: "temporary",
  });
  ExtensionTestUtils.failOnSchemaWarnings(false);
  await extension.startup();
  ExtensionTestUtils.failOnSchemaWarnings(true);

  const addon = await promiseAddonByID(ID);
  notEqual(addon, null, "Add-on is installed");

  await extension.unload();
});
