// Enable signature checks for these tests
gUseRealCertChecks = true;

const DATA = "data/signing_checks";
const ID = "test@somewhere.com";

const profileDir = gProfD.clone();
profileDir.append("extensions");

function verifySignatures() {
  return new Promise(resolve => {
    let observer = (subject, topic, data) => {
      Services.obs.removeObserver(observer, "xpi-signature-changed");
      resolve(JSON.parse(data));
    };
    Services.obs.addObserver(observer, "xpi-signature-changed");

    info("Verifying signatures");
    const { XPIExports } = ChromeUtils.importESModule(
      "resource://gre/modules/addons/XPIExports.sys.mjs"
    );
    XPIExports.XPIDatabase.verifySignatures();
  });
}

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "4", "4");

add_setup(async () => {
  await promiseStartupManager();
});

add_task(function test_hasStrongSignature_helper() {
  const { hasStrongSignature } = ChromeUtils.importESModule(
    "resource://gre/modules/addons/crypto-utils.sys.mjs"
  );
  const { PKCS7_WITH_SHA1, PKCS7_WITH_SHA256, COSE_WITH_SHA256 } =
    Ci.nsIAppSignatureInfo;
  const testCases = [
    [false, "SHA1 only", [PKCS7_WITH_SHA1]],
    [true, "SHA256 only", [PKCS7_WITH_SHA256]],
    [true, "COSE only", [COSE_WITH_SHA256]],
    [true, "SHA1 and SHA256", [PKCS7_WITH_SHA1, PKCS7_WITH_SHA256]],
    [true, "SHA1 and COSE", [PKCS7_WITH_SHA1, COSE_WITH_SHA256]],
    [true, "SHA256 and COSE", [PKCS7_WITH_SHA256, COSE_WITH_SHA256]],
  ];
  for (const [expect, msg, signedTypes] of testCases) {
    Assert.equal(hasStrongSignature({ signedTypes }), expect, msg);
  }
});

add_task(async function test_addon_signedTypes() {
  // This test is allowing weak signatures to run assertions on the AddonWrapper.signedTypes
  // property also for extensions only including SHA1 signatures.
  const resetWeakSignaturePref =
    AddonTestUtils.setWeakSignatureInstallAllowed(true);

  const { PKCS7_WITH_SHA1, COSE_WITH_SHA256 } = Ci.nsIAppSignatureInfo;

  const { addon: addonSignedCOSE } = await promiseInstallFile(
    do_get_file("amosigned-mv3-cose.xpi")
  );
  const { addon: addonSignedSHA1 } = await promiseInstallFile(
    do_get_file("amosigned-sha1only.xpi")
  );

  Assert.deepEqual(
    addonSignedCOSE.signedTypes.sort(),
    [COSE_WITH_SHA256, PKCS7_WITH_SHA1].sort(),
    `Expect ${addonSignedCOSE.id} to be signed with both COSE and SHA1`
  );

  Assert.deepEqual(
    addonSignedSHA1.signedTypes,
    [PKCS7_WITH_SHA1],
    `Expect ${addonSignedSHA1.id} to be signed with SHA1 only`
  );

  await addonSignedSHA1.uninstall();
  await addonSignedCOSE.uninstall();

  resetWeakSignaturePref();
});

add_task(
  async function test_install_error_on_new_install_with_weak_signature() {
    // Ensure restrictions on weak signatures are enabled (this should be removed when
    // the new behavior is riding the train).
    const resetWeakSignaturePref =
      AddonTestUtils.setWeakSignatureInstallAllowed(false);

    const { messages } = await AddonTestUtils.promiseConsoleOutput(async () => {
      let install = await AddonManager.getInstallForFile(
        do_get_file("amosigned-sha1only.xpi")
      );

      await Assert.equal(
        install.state,
        AddonManager.STATE_DOWNLOAD_FAILED,
        "Expect install state to be STATE_DOWNLOAD_FAILED"
      );

      await Assert.rejects(
        install.install(),
        /Install failed: onDownloadFailed/,
        "Expected install to fail"
      );
    });

    resetWeakSignaturePref();

    // Checking the message expected to be logged in the Browser Console.
    AddonTestUtils.checkMessages(messages, {
      expected: [
        {
          message:
            /Invalid XPI: install rejected due to the package not including a strong cryptographic signature/,
        },
      ],
    });
  }
);

/**
 * Test helper used to simulate an update from a given pre-installed add-on xpi to a new xpi file for the same
 * add-on and assert the expected result and logged messages.
 *
 * @param {object}               params
 * @param {string}               params.currentAddonXPI
 *        The path to the add-on xpi to be pre-installed and then updated to `newAddonXPI`.
 * @param {string}               params.newAddonXPI
 *        The path to the add-on xpi to be installed as an update over `currentAddonXPI`.
 * @param {string}               params.newAddonVersion
 *        The add-on version expected for `newAddonXPI`.
 * @param {boolean}              params.expectedInstallOK
 *        Set to true for an update scenario that is expected to be successful.
 * @param {Array<string|RegExp>} params.expectedMessages
 *        Array of strings or RegExp for console messages expected to be logged.
 * @param {Array<string|RegExp>} params.forbiddenMessages
 *        Array of strings or RegExp for console messages expected to NOT be logged.
 */
async function testWeakSignatureXPIUpdate({
  currentAddonXPI,
  newAddonXPI,
  newAddonVersion,
  expectedInstallOK,
  expectedMessages,
  forbiddenMessages,
}) {
  // Temporarily allow weak signature to install the xpi as a new install.
  let resetWeakSignaturePref =
    AddonTestUtils.setWeakSignatureInstallAllowed(true);

  const { addon: addonFirstInstall } = await promiseInstallFile(
    currentAddonXPI
  );
  const addonId = addonFirstInstall.id;
  const initialAddonVersion = addonFirstInstall.version;

  resetWeakSignaturePref();

  // Make sure the install over is executed while weak signature is not allowed
  // for new installs to confirm that installing over is allowed.
  resetWeakSignaturePref = AddonTestUtils.setWeakSignatureInstallAllowed(false);

  info("Install over the existing installed addon");
  let addonInstalledOver;
  const { messages } = await AddonTestUtils.promiseConsoleOutput(async () => {
    const fileURL = Services.io.newFileURI(newAddonXPI).spec;
    let install = await AddonManager.getInstallForURL(fileURL, {
      existingAddon: addonFirstInstall,
      version: newAddonVersion,
    });

    addonInstalledOver = await install.install().catch(err => {
      if (expectedInstallOK) {
        ok(false, `Unexpected error hit on installing update XPI: ${err}`);
      } else {
        ok(true, `Install failed as expected: ${err}`);
      }
    });
  });

  resetWeakSignaturePref();

  if (expectedInstallOK) {
    Assert.equal(
      addonInstalledOver.id,
      addonFirstInstall.id,
      "Expect addon id to be the same"
    );
    Assert.equal(
      addonInstalledOver.version,
      newAddonVersion,
      "Got expected addon version after update xpi install completed"
    );
    await addonInstalledOver.uninstall();
  } else {
    Assert.equal(
      addonInstalledOver?.version,
      undefined,
      "Expect update addon xpi not installed successfully"
    );
    Assert.equal(
      (await AddonManager.getAddonByID(addonId)).version,
      initialAddonVersion,
      "Expect the addon version to match the initial XPI version"
    );
    await addonFirstInstall.uninstall();
  }

  Assert.equal(
    await AddonManager.getAddonByID(addonId),
    undefined,
    "Expect the test addon to be fully uninstalled"
  );

  // Checking the message logged in the Browser Console.
  AddonTestUtils.checkMessages(messages, {
    expected: expectedMessages,
    forbidden: forbiddenMessages,
  });
}

add_task(async function test_weak_install_over_weak_existing() {
  const addonId = "amosigned-xpi@tests.mozilla.org";
  await testWeakSignatureXPIUpdate({
    currentAddonXPI: do_get_file("amosigned-sha1only.xpi"),
    newAddonXPI: do_get_file("amosigned-sha1only.xpi"),
    newAddonVersion: "2.1",
    expectedInstallOK: true,
    expectedMessages: [
      {
        message: new RegExp(
          `Allow weak signature install over existing "${addonId}" XPI`
        ),
      },
    ],
  });
});

add_task(async function test_update_weak_to_strong_signature() {
  const addonId = "amosigned-xpi@tests.mozilla.org";
  await testWeakSignatureXPIUpdate({
    currentAddonXPI: do_get_file("amosigned-sha1only.xpi"),
    newAddonXPI: do_get_file("amosigned.xpi"),
    newAddonVersion: "2.2",
    expectedInstallOK: true,
    forbiddenMessages: [
      {
        message: new RegExp(
          `Allow weak signature install over existing "${addonId}" XPI`
        ),
      },
    ],
  });
});

add_task(async function test_update_strong_to_weak_signature() {
  const addonId = "amosigned-xpi@tests.mozilla.org";
  await testWeakSignatureXPIUpdate({
    currentAddonXPI: do_get_file("amosigned.xpi"),
    newAddonXPI: do_get_file("amosigned-sha1only.xpi"),
    newAddonVersion: "2.1",
    expectedInstallOK: false,
    expectedMessages: [
      {
        message: new RegExp(
          "Invalid XPI: install rejected due to the package not including a strong cryptographic signature"
        ),
      },
    ],
    forbiddenMessages: [
      {
        message: new RegExp(
          `Allow weak signature install over existing "${addonId}" XPI`
        ),
      },
    ],
  });
});

add_task(async function test_signedTypes_stored_in_addonDB() {
  const { addon: addonAfterInstalled } = await promiseInstallFile(
    do_get_file("amosigned-mv3-cose.xpi")
  );
  const addonId = addonAfterInstalled.id;

  const { PKCS7_WITH_SHA1, COSE_WITH_SHA256 } = Ci.nsIAppSignatureInfo;
  const expectedSignedTypes = [COSE_WITH_SHA256, PKCS7_WITH_SHA1].sort();

  Assert.deepEqual(
    addonAfterInstalled.signedTypes.sort(),
    expectedSignedTypes,
    `Got expected ${addonId} signedTyped after install`
  );

  await promiseRestartManager();

  const addonAfterAOMRestart = await AddonManager.getAddonByID(addonId);

  Assert.deepEqual(
    addonAfterAOMRestart.signedTypes.sort(),
    expectedSignedTypes,
    `Got expected ${addonId} signedTyped after AOM restart`
  );

  const removeSignedStateFromAddonDB = async () => {
    const addon_db_file = Services.dirsvc.get("ProfD", Ci.nsIFile);
    addon_db_file.append("extensions.json");
    const addon_db_data = await IOUtils.readJSON(addon_db_file.path);

    const addon_db_data_tampered = {
      ...addon_db_data,
      addons: addon_db_data.addons.map(addonData => {
        // Tamper the data of the test extension to mock the
        // scenario.
        delete addonData.signedTypes;
        return addonData;
      }),
    };
    await IOUtils.writeJSON(addon_db_file.path, addon_db_data_tampered);
  };

  // Shutdown the AddonManager and tamper the AddonDB to confirm XPIProvider.checkForChanges
  // calls originated internally when opening a profile created from a previous Firefox version
  // is going to populate the new signedTypes property.
  info(
    "Check that XPIProvider.checkForChanges(true) will recompute missing signedTypes properties"
  );
  await promiseShutdownManager();
  await removeSignedStateFromAddonDB();
  await promiseStartupManager();

  // Expect the signedTypes property to be undefined because of the
  // AddonDB data being tampered earlier in this test.
  const addonAfterAppUpgrade = await AddonManager.getAddonByID(addonId);
  Assert.deepEqual(
    addonAfterAppUpgrade.signedTypes,
    undefined,
    `Got empty ${addonId} signedTyped set to undefied after AddonDB data tampered`
  );

  // Mock call to XPIDatabase.checkForChanges expected to be hit when the application
  // is updated.
  AddonTestUtils.getXPIExports().XPIProvider.checkForChanges(
    /* aAppChanged */ true
  );

  Assert.deepEqual(
    addonAfterAppUpgrade.signedTypes?.sort(),
    expectedSignedTypes.sort(),
    `Got expected ${addonId} signedTyped after XPIProvider.checkForChanges recomputed it`
  );

  // Shutdown the AddonManager and tamper the AddonDB to confirm XPIDatabase.updateCompatibility
  // would populate signedTypes if missing.
  info(
    "Check that XPIDatabase.updateCompatibility will recompute missing signedTypes properties"
  );
  await promiseShutdownManager();
  await removeSignedStateFromAddonDB();
  await promiseStartupManager();

  // Expect the signedTypes property to be undefined because of the
  // AddonDB data being tampered earlier in this test.
  const addonAfterUpdateCompatibility = await AddonManager.getAddonByID(
    addonId
  );
  Assert.deepEqual(
    addonAfterUpdateCompatibility.signedTypes,
    undefined,
    `Got empty ${addonId} signedTyped set to undefied after AddonDB data tampered`
  );

  // Mock call to XPIDatabase.updateCompatibility expected to be originated from
  // XPIDatabaseReconcile.processFileChanges and confirm that signedTypes has been
  // recomputed as expected.
  AddonTestUtils.getXPIExports().XPIDatabaseReconcile.processFileChanges(
    {},
    true
  );
  Assert.deepEqual(
    addonAfterUpdateCompatibility.signedTypes?.sort(),
    expectedSignedTypes.sort(),
    `Got expected ${addonId} signedTyped after XPIDatabase.updateCompatibility recomputed it`
  );

  // Restart the AddonManager and confirm that XPIDatabase.verifySignatures would not recompute
  // the content of the signedTypes array if all values are still the same.
  info(
    "Check that XPIDatabase.updateCompatibility will recompute missing signedTypes properties"
  );
  await promiseRestartManager();

  let listener = {
    onPropertyChanged(_addon, properties) {
      Assert.deepEqual(
        properties,
        [],
        `No properties should have been changed for ${_addon.id}`
      );
      Assert.ok(
        false,
        `onPropertyChanged should have not been called for ${_addon.id}`
      );
    },
  };

  AddonManager.addAddonListener(listener);
  await verifySignatures();
  AddonManager.removeAddonListener(listener);

  // Shutdown the AddonManager and tamper the AddonDB to set signedTypes to undefined
  // then confirm that XPIDatabase.verifySignatures does not hit an exception due to
  // signedTypes assumed to always be set to an array.
  info(
    "Check that XPIDatabase.verifySignatures does not fail when signedTypes is undefined"
  );
  await promiseShutdownManager();
  await removeSignedStateFromAddonDB();
  await promiseStartupManager();

  // Expect the signedTypes property to be undefined because of the
  // AddonDB data being tampered earlier in this test.
  const addonUndefinedSignedTypes = await AddonManager.getAddonByID(addonId);
  Assert.deepEqual(
    addonUndefinedSignedTypes.signedTypes,
    undefined,
    `Got empty ${addonId} signedTyped set to undefied after AddonDB data tampered`
  );
  await verifySignatures();
  Assert.deepEqual(
    addonUndefinedSignedTypes.signedTypes?.sort(),
    expectedSignedTypes.sort(),
    `Got expected ${addonId} signedTyped after XPIDatabase.verifySignatures recomputed it`
  );

  await addonUndefinedSignedTypes.uninstall();
});

add_task(
  {
    pref_set: [["xpinstall.signatures.required", false]],
    // Skip this test on builds where disabling xpi signature checks is not supported.
    skip_if: () => AppConstants.MOZ_REQUIRE_SIGNING,
  },
  async function test_weak_signature_not_restricted_on_disabled_signature_checks() {
    // Ensure the restriction on installing xpi using only weak signatures is enabled.
    let resetWeakSignaturePref =
      AddonTestUtils.setWeakSignatureInstallAllowed(false);
    const { addon } = await promiseInstallFile(
      do_get_file("amosigned-sha1only.xpi")
    );
    Assert.notEqual(addon, null, "Expect addon to be installed");
    resetWeakSignaturePref();
    await addon.uninstall();
  }
);

add_task(useAMOStageCert(), async function test_no_change() {
  // Install the first add-on
  await promiseInstallFile(do_get_file(`${DATA}/signed1.xpi`));

  let addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.appDisabled, false);
  Assert.equal(addon.isActive, true);
  Assert.equal(addon.pendingOperations, AddonManager.PENDING_NONE);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_SIGNED);

  // Swap in the files from the next add-on
  manuallyUninstall(profileDir, ID);
  await manuallyInstall(do_get_file(`${DATA}/signed2.xpi`), profileDir, ID);

  let listener = {
    onPropertyChanged(_addon) {
      Assert.ok(false, `Got unexpected onPropertyChanged for ${_addon.id}`);
    },
  };

  AddonManager.addAddonListener(listener);

  // Trigger the check
  let changes = await verifySignatures();
  Assert.equal(changes.enabled.length, 0);
  Assert.equal(changes.disabled.length, 0);

  Assert.equal(addon.appDisabled, false);
  Assert.equal(addon.isActive, true);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_SIGNED);

  await addon.uninstall();
  AddonManager.removeAddonListener(listener);
});

add_task(useAMOStageCert(), async function test_disable() {
  // Install the first add-on
  await promiseInstallFile(do_get_file(`${DATA}/signed1.xpi`));

  let addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.ok(addon.isActive);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_SIGNED);

  // Swap in the files from the next add-on
  manuallyUninstall(profileDir, ID);
  await manuallyInstall(do_get_file(`${DATA}/unsigned.xpi`), profileDir, ID);

  let changedProperties = [];
  let listener = {
    onPropertyChanged(_, properties) {
      changedProperties.push(...properties);
    },
  };
  AddonManager.addAddonListener(listener);

  // Trigger the check
  let [changes] = await Promise.all([
    verifySignatures(),
    promiseAddonEvent("onDisabling"),
  ]);

  Assert.equal(changes.enabled.length, 0);
  Assert.equal(changes.disabled.length, 1);
  Assert.equal(changes.disabled[0], ID);

  Assert.deepEqual(
    changedProperties,
    ["signedState", "signedTypes", "appDisabled"],
    "Got onPropertyChanged events for signedState and appDisabled"
  );

  Assert.ok(addon.appDisabled);
  Assert.ok(!addon.isActive);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_MISSING);

  await addon.uninstall();
  AddonManager.removeAddonListener(listener);
});
