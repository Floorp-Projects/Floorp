/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { XPIInstall } = ChromeUtils.import(
  "resource://gre/modules/addons/XPIInstall.jsm"
);

ChromeUtils.defineESModuleGetters(this, {
  ExtensionPermissions: "resource://gre/modules/ExtensionPermissions.sys.mjs",
  Management: "resource://gre/modules/Extension.sys.mjs",
});

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.usePrivilegedSignatures = false;

const testStartTime = Date.now();
const not_before = new Date(testStartTime - 3600000).toISOString();
const not_after = new Date(testStartTime + 3600000).toISOString();
const RECOMMENDATION_FILE_NAME = "mozilla-recommendation.json";

const server = AddonTestUtils.createHttpServer();
const SERVER_BASE_URL = `http://localhost:${server.identity.primaryPort}`;
// Allow the test extensions to be updated from an insecure update url.
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);
Services.prefs.setCharPref(
  "extensions.update.background.url",
  `${SERVER_BASE_URL}/upgrade.json`
);

function createFileWithRecommendations(id, recommendation, version = "1.0.0") {
  let files = {};
  if (recommendation) {
    files[RECOMMENDATION_FILE_NAME] = recommendation;
  }
  return AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version,
      browser_specific_settings: { gecko: { id } },
    },
    files,
  });
}

async function installAddonWithRecommendations(id, recommendation) {
  let xpi = createFileWithRecommendations(id, recommendation);
  let install = await AddonTestUtils.promiseInstallFile(xpi);
  return install.addon;
}

function checkRecommended(addon, recommended = true) {
  equal(
    addon.isRecommended,
    recommended,
    "The add-on isRecommended state is correct"
  );
  equal(
    addon.recommendationStates.includes("recommended"),
    recommended,
    "The add-on recommendationStates is correct"
  );
}

function waitForPendingExtension(extId) {
  return new Promise(resolve => {
    Management.on("startup", function startupListener() {
      const pendingExtensionsMap =
        Services.ppmm.sharedData.get("extensions/pending");
      if (pendingExtensionsMap.has(extId)) {
        Management.off("startup", startupListener);
        resolve(pendingExtensionsMap.get(extId));
      }
    });
  });
}

async function assertPendingExtensionIgnoreQuarantined({
  addonId,
  expectedIgnoreQuarantined,
}) {
  info(
    `Reload ${addonId} and verify ignoreQuarantine in extensions/pending sharedData`
  );
  const promisePendingExtension = waitForPendingExtension(addonId);
  const addon = await AddonManager.getAddonByID(addonId);
  await addon.disable();
  await addon.enable();
  Assert.deepEqual(
    (await promisePendingExtension).ignoreQuarantine,
    expectedIgnoreQuarantined,
    `Expect ignoreQuarantine to be true in pending/extensions details for ${addon.id}`
  );
}

function assertQuarantinedFromURI({ domain, expected }) {
  const { processType, PROCESS_TYPE_DEFAULT } = Services.appinfo;
  const processTypeStr =
    processType === PROCESS_TYPE_DEFAULT ? "Main Process" : "Child Process";
  const testURI = Services.io.newURI(`https://${domain}/`);
  for (const [addonId, expectedQuarantinedFromURI] of Object.entries(
    expected
  )) {
    Assert.equal(
      WebExtensionPolicy.getByID(addonId).quarantinedFromURI(testURI),
      expectedQuarantinedFromURI,
      `Expect ${addonId} to ${
        expectedQuarantinedFromURI ? "not be" : "be"
      } quarantined from ${domain} in ${processTypeStr}`
    );
  }
}

async function assertQuarantinedFromURIInChildProcessAsync({
  domain,
  expected,
}) {
  // Doesn't matter what content url we us here, as long as we are
  // using a content url to be able to run the assertions from a
  // child process.
  const testUrl = SERVER_BASE_URL;
  const page = await ExtensionTestUtils.loadContentPage(testUrl);
  // TODO(rpl): look into Bug 1648545 changes and determine what
  // would need to change to use page.spawn instead.
  await page.legacySpawn({ domain, expected }, assertQuarantinedFromURI);
  await page.close();
}

function getUpdatesJSONFor(id, version) {
  return {
    updates: [
      {
        version,
        update_link: `${SERVER_BASE_URL}/addons/${id}.xpi`,
      },
    ],
  };
}

function registerUpdateXPIFile({ id, version, recommendationStates }) {
  const recommendation = {
    addon_id: id,
    states: recommendationStates,
    validity: { not_before, not_after },
  };
  let xpi = createFileWithRecommendations(id, recommendation, version);
  server.registerFile(`/addons/${id}.xpi`, xpi);
}

function waitForBootstrapUpdateMethod(addonId, newVersion) {
  return new Promise(resolve => {
    function listener(_evt, { method, params }) {
      if (
        method === "update" &&
        params.id === addonId &&
        params.newVersion === newVersion
      ) {
        AddonTestUtils.off("bootstrap-method", listener);
        info(`Update bootstrap method called for ${addonId} ${newVersion}`);
        resolve({ addonId, method, params });
      }
    }
    AddonTestUtils.on("bootstrap-method", listener);
  });
}

function assertUpdateBootstrapCall(detailsBootstrapUpdates, expected) {
  const actualPerAddonId = detailsBootstrapUpdates
    .map(({ addonId, params }) => {
      return [addonId, params.recommendationState?.states];
    })
    .reduce((acc, [addonId, states]) => {
      acc[addonId] = states;
      return acc;
    }, {});
  Assert.deepEqual(
    actualPerAddonId,
    expected,
    `Got the expected recommendation states in the update bootstrap calls`
  );
}

add_setup(async () => {
  await ExtensionTestUtils.startAddonManager();
});

add_task(async function text_no_file() {
  const id = "no-recommendations-file@test.web.extension";
  let addon = await installAddonWithRecommendations(id, null);

  checkRecommended(addon, false);

  await addon.uninstall();
});

add_task(async function text_malformed_file() {
  const id = "no-recommendations-file@test.web.extension";
  let addon = await installAddonWithRecommendations(id, "This is not JSON");

  checkRecommended(addon, false);

  await addon.uninstall();
});

add_task(async function test_valid_recommendation_file() {
  const id = "recommended@test.web.extension";
  let addon = await installAddonWithRecommendations(id, {
    addon_id: id,
    states: ["recommended"],
    validity: { not_before, not_after },
  });

  checkRecommended(addon);

  await addon.uninstall();
});

add_task(async function test_multiple_valid_recommendation_file() {
  const id = "recommended@test.web.extension";
  let addon = await installAddonWithRecommendations(id, {
    addon_id: id,
    states: ["recommended", "something"],
    validity: { not_before, not_after },
  });

  checkRecommended(addon);
  ok(
    addon.recommendationStates.includes("something"),
    "The add-on recommendationStates contains something"
  );

  await addon.uninstall();
});

add_task(async function test_unsigned() {
  // Don't override the certificate, so that the test add-on is unsigned.
  AddonTestUtils.useRealCertChecks = true;
  // Allow unsigned add-on to be installed.
  Services.prefs.setBoolPref("xpinstall.signatures.required", false);

  const id = "unsigned@test.web.extension";
  let addon = await installAddonWithRecommendations(id, {
    addon_id: id,
    states: ["recommended"],
    validity: { not_before, not_after },
  });

  checkRecommended(addon, false);

  await addon.uninstall();
  AddonTestUtils.useRealCertChecks = false;
  Services.prefs.setBoolPref("xpinstall.signatures.required", true);
});

add_task(async function test_temporary() {
  const id = "temporary@test.web.extension";
  let xpi = createFileWithRecommendations(id, {
    addon_id: id,
    states: ["recommended"],
    validity: { not_before, not_after },
  });
  let addon = await XPIInstall.installTemporaryAddon(xpi);

  checkRecommended(addon, false);

  await addon.uninstall();
});

// Tests that unpacked temporary add-ons are not recommended.
add_task(async function test_temporary_directory() {
  const id = "temporary-dir@test.web.extension";
  let files = ExtensionTestCommon.generateFiles({
    manifest: {
      browser_specific_settings: { gecko: { id } },
    },
    files: {
      [RECOMMENDATION_FILE_NAME]: {
        addon_id: id,
        states: ["recommended"],
        validity: { not_before, not_after },
      },
    },
  });
  let extDir = await AddonTestUtils.promiseWriteFilesToExtension(
    gTmpD.path,
    id,
    files,
    true
  );

  let addon = await XPIInstall.installTemporaryAddon(extDir);

  checkRecommended(addon, false);

  await addon.uninstall();
  extDir.remove(true);
});

add_task(async function test_builtin() {
  const id = "builtin@test.web.extension";
  let extension = await installBuiltinExtension({
    manifest: {
      browser_specific_settings: { gecko: { id } },
    },
    background: `browser.test.sendMessage("started");`,
    files: {
      [RECOMMENDATION_FILE_NAME]: {
        addon_id: id,
        states: ["recommended"],
        validity: { not_before, not_after },
      },
    },
  });
  await extension.awaitMessage("started");

  checkRecommended(extension.addon, false);

  await extension.unload();
});

add_task(async function test_theme() {
  const id = "theme@test.web.extension";
  let xpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      browser_specific_settings: { gecko: { id } },
      theme: {},
    },
    files: {
      [RECOMMENDATION_FILE_NAME]: {
        addon_id: id,
        states: ["recommended"],
        validity: { not_before, not_after },
      },
    },
  });
  let { addon } = await AddonTestUtils.promiseInstallFile(xpi);

  checkRecommended(addon, false);

  await addon.uninstall();
});

add_task(async function test_not_recommended() {
  const id = "not-recommended@test.web.extension";
  let addon = await installAddonWithRecommendations(id, {
    addon_id: id,
    states: ["something"],
    validity: { not_before, not_after },
  });

  checkRecommended(addon, false);
  ok(
    addon.recommendationStates.includes("something"),
    "The add-on recommendationStates contains something"
  );

  await addon.uninstall();
});

add_task(async function test_id_missing() {
  const id = "no-id@test.web.extension";
  let addon = await installAddonWithRecommendations(id, {
    states: ["recommended"],
    validity: { not_before, not_after },
  });

  checkRecommended(addon, false);

  await addon.uninstall();
});

add_task(async function test_expired() {
  const id = "expired@test.web.extension";
  let addon = await installAddonWithRecommendations(id, {
    addon_id: id,
    states: ["recommended", "something"],
    validity: { not_before, not_after: not_before },
  });

  checkRecommended(addon, false);
  ok(
    !addon.recommendationStates.length,
    "The add-on recommendationStates does not contain anything"
  );

  await addon.uninstall();
});

add_task(async function test_not_valid_yet() {
  const id = "expired@test.web.extension";
  let addon = await installAddonWithRecommendations(id, {
    addon_id: id,
    states: ["recommended"],
    validity: { not_before: not_after, not_after },
  });

  checkRecommended(addon, false);

  await addon.uninstall();
});

add_task(async function test_states_missing() {
  const id = "states-missing@test.web.extension";
  let addon = await installAddonWithRecommendations(id, {
    addon_id: id,
    validity: { not_before, not_after },
  });

  checkRecommended(addon, false);

  await addon.uninstall();
});

add_task(async function test_validity_missing() {
  const id = "validity-missing@test.web.extension";
  let addon = await installAddonWithRecommendations(id, {
    addon_id: id,
    states: ["recommended"],
  });

  checkRecommended(addon, false);

  await addon.uninstall();
});

add_task(async function test_not_before_missing() {
  const id = "not-before-missing@test.web.extension";
  let addon = await installAddonWithRecommendations(id, {
    addon_id: id,
    states: ["recommended"],
    validity: { not_after },
  });

  checkRecommended(addon, false);

  await addon.uninstall();
});

add_task(async function test_bad_states() {
  const id = "bad-states@test.web.extension";
  let addon = await installAddonWithRecommendations(id, {
    addon_id: id,
    states: { recommended: true },
    validity: { not_before, not_after },
  });

  checkRecommended(addon, false);

  await addon.uninstall();
});

add_task(async function test_recommendation_persist_restart() {
  const id = "persisted-recommendation@test.web.extension";
  let addon = await installAddonWithRecommendations(id, {
    addon_id: id,
    states: ["recommended"],
    validity: { not_before, not_after },
  });

  checkRecommended(addon);

  await AddonTestUtils.promiseRestartManager();

  addon = await AddonManager.getAddonByID(id);

  checkRecommended(addon);

  await addon.uninstall();
});

add_task(async function test_isLineExtension_internal_svg_permission() {
  async function assertLineExtensionStateAndPermission(
    addonId,
    expectLineExtension,
    isRestart
  ) {
    const { extension } = WebExtensionPolicy.getByID(addonId);

    const msgShould = expectLineExtension ? "should" : "should not";

    equal(
      extension.hasPermission("internal:svgContextPropertiesAllowed"),
      expectLineExtension,
      `"${addonId}" ${msgShould} have permission internal:svgContextPropertiesAllowed`
    );
    if (isRestart) {
      const { permissions } = await ExtensionPermissions.get(addonId);
      Assert.deepEqual(
        permissions,
        expectLineExtension ? ["internal:svgContextPropertiesAllowed"] : [],
        `ExtensionPermission.get("${addonId}") result ${msgShould} include internal:svgContextPropertiesAllowed permission`
      );
    }
  }

  const idLineExt = "line-extension@test.web.extension";
  await installAddonWithRecommendations(idLineExt, {
    addon_id: idLineExt,
    states: ["line"],
    validity: { not_before, not_after },
  });

  info(`Test line extension ${idLineExt}`);
  await assertLineExtensionStateAndPermission(idLineExt, true, false);
  await AddonTestUtils.promiseRestartManager();
  info(`Test ${idLineExt} again after AOM restart`);
  await assertLineExtensionStateAndPermission(idLineExt, true, true);
  let addon = await AddonManager.getAddonByID(idLineExt);
  await addon.uninstall();

  const idNonLineExt = "non-line-extension@test.web.extension";
  await installAddonWithRecommendations(idNonLineExt, {
    addon_id: idNonLineExt,
    states: ["recommended"],
    validity: { not_before, not_after },
  });

  info(`Test non line extension: ${idNonLineExt}`);
  await assertLineExtensionStateAndPermission(idNonLineExt, false, false);
  await AddonTestUtils.promiseRestartManager();
  info(`Test ${idNonLineExt} again after AOM restart`);
  await assertLineExtensionStateAndPermission(idNonLineExt, false, true);
  addon = await AddonManager.getAddonByID(idNonLineExt);
  await addon.uninstall();
});

add_task(
  {
    pref_set: [
      ["extensions.quarantinedDomains.enabled", true],
      ["extensions.quarantinedDomains.list", "quarantined.example.org"],
    ],
  },
  async function test_recommended_exempt_from_quarantined() {
    const invalidRecommendedId = "invalid-recommended@test.web.extension";
    const validRecommendedId = "recommended@test.web.extension";
    const validAndroidRecommendedId = "recommended-android@test.web.extension";
    const lineExtensionId = "line@test.web.extension";
    const validMultiRecommendedId = "recommended-multi@test.web.extension";
    // NOTE: confirm that any future recommendation state that was considered
    // valid and signed by AMO is also going to be exempt, which does also include
    // recommendation states that we are not using anymore but are still technically
    // supported by autograph (e.g. verified), see:
    // https://github.com/mozilla-services/autograph/blob/8a34847a/autograph.yaml#L1456-L1460
    const validFutureRecStateId = "fake-future-valid-state@test.web.extension";

    const recommendationStatesPerId = {
      [invalidRecommendedId]: null,
      [validRecommendedId]: ["recommended"],
      [validAndroidRecommendedId]: ["recommended-android"],
      [lineExtensionId]: ["line"],
      [validFutureRecStateId]: ["fake-future-valid-state"],
      [validMultiRecommendedId]: ["recommended", "recommended-android"],
    };

    for (const [extId, expectedRecStates] of Object.entries(
      recommendationStatesPerId
    )) {
      const recommendationData = expectedRecStates
        ? {
            addon_id: extId,
            states: expectedRecStates,
            validity: { not_before, not_after },
          }
        : null;
      await installAddonWithRecommendations(extId, recommendationData);
      // Check that the expected recommendation states are reflected by the
      // value returned by the AddonWrapper.recommendationStates getter.
      const addon = await AddonManager.getAddonByID(extId);
      Assert.deepEqual(
        addon.recommendationStates,
        expectedRecStates ?? [],
        `Addon ${extId} has the expected recommendation states`
      );
    }

    assertQuarantinedFromURI({
      domain: "quarantined.example.org",
      expected: {
        [invalidRecommendedId]: true,
        [validRecommendedId]: false,
        [validAndroidRecommendedId]: false,
        [lineExtensionId]: false,
        [validFutureRecStateId]: false,
        [validMultiRecommendedId]: false,
      },
    });

    await assertQuarantinedFromURIInChildProcessAsync({
      domain: "quarantined.example.org",
      expected: {
        [invalidRecommendedId]: true,
        [validRecommendedId]: false,
        [validAndroidRecommendedId]: false,
        [lineExtensionId]: false,
        [validFutureRecStateId]: false,
        [validMultiRecommendedId]: false,
      },
    });

    // NOTE: we only cover the 3 basic cases in the rest of this test case
    // (we have verified that ignoreQuarantine is being set to the expected
    // value and so the other cases shouldn't matter for the behaviors being
    // explicitly covered by the remaining part of this test task).

    // Make sure the ignoreQuarantine property is also propagated in the child
    // processes while the extensions may still be not fully initialized (and
    // so listed in the `extensions/pending` sharedData entry).
    await assertPendingExtensionIgnoreQuarantined({
      addonId: validRecommendedId,
      expectedIgnoreQuarantined: true,
    });
    await assertPendingExtensionIgnoreQuarantined({
      addonId: lineExtensionId,
      expectedIgnoreQuarantined: true,
    });
    await assertPendingExtensionIgnoreQuarantined({
      addonId: invalidRecommendedId,
      expectedIgnoreQuarantined: false,
    });

    info("Verify ignoreQuarantine again after application restart");

    await AddonTestUtils.promiseRestartManager();
    assertQuarantinedFromURI({
      domain: "quarantined.example.org",
      expected: {
        [invalidRecommendedId]: true,
        [validRecommendedId]: false,
        [lineExtensionId]: false,
      },
    });

    info("Verify ignoreQuarantine again after addon updates");

    AddonTestUtils.registerJSON(server, "/upgrade.json", {
      addons: {
        [invalidRecommendedId]: getUpdatesJSONFor(
          invalidRecommendedId,
          "2.0.0"
        ),
        [validRecommendedId]: getUpdatesJSONFor(validRecommendedId, "2.0.0"),
        [lineExtensionId]: getUpdatesJSONFor(lineExtensionId, "2.0.0"),
      },
    });
    registerUpdateXPIFile({
      id: invalidRecommendedId,
      version: "2.0.0",
      recommendationStates: recommendationStatesPerId[invalidRecommendedId],
    });
    registerUpdateXPIFile({
      id: validRecommendedId,
      version: "2.0.0",
      recommendationStates: recommendationStatesPerId[validRecommendedId],
    });
    registerUpdateXPIFile({
      id: lineExtensionId,
      version: "2.0.0",
      recommendationStates: recommendationStatesPerId[lineExtensionId],
    });

    const promiseUpdatesInstalled = Promise.all([
      waitForBootstrapUpdateMethod(invalidRecommendedId, "2.0.0"),
      waitForBootstrapUpdateMethod(validRecommendedId, "2.0.0"),
      waitForBootstrapUpdateMethod(lineExtensionId, "2.0.0"),
    ]);

    const promiseBackgroundUpdatesFound = TestUtils.topicObserved(
      "addons-background-updates-found"
    );
    let [
      extensionInvalidRecommended,
      extensionValidRecommended,
      extensionLine,
    ] = [
      ExtensionTestUtils.expectExtension(invalidRecommendedId),
      ExtensionTestUtils.expectExtension(validRecommendedId),
      ExtensionTestUtils.expectExtension(lineExtensionId),
    ];

    await AddonManagerPrivate.backgroundUpdateCheck();
    await promiseBackgroundUpdatesFound;

    assertUpdateBootstrapCall(await promiseUpdatesInstalled, {
      [invalidRecommendedId]: null,
      [validRecommendedId]: ["recommended"],
      [lineExtensionId]: ["line"],
    });

    // Wait the test extension to be fully started (prevents logspam
    // due to the AOM trying to uninstall them while being started).
    await Promise.all([
      extensionInvalidRecommended.awaitStartup(),
      extensionValidRecommended.awaitStartup(),
      extensionLine.awaitStartup(),
    ]);

    // Uninstall all test extensions.
    await Promise.all(
      Object.keys(recommendationStatesPerId).map(async addonId => {
        const addon = await AddonManager.getAddonByID(addonId);
        await addon.uninstall();
      })
    );
  }
);
