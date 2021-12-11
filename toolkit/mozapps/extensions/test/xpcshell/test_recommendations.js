/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { XPIInstall } = ChromeUtils.import(
  "resource://gre/modules/addons/XPIInstall.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "ExtensionPermissions",
  "resource://gre/modules/ExtensionPermissions.jsm"
);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.usePrivilegedSignatures = false;

const testStartTime = Date.now();
const not_before = new Date(testStartTime - 3600000).toISOString();
const not_after = new Date(testStartTime + 3600000).toISOString();
const RECOMMENDATION_FILE_NAME = "mozilla-recommendation.json";

function createFileWithRecommendations(id, recommendation) {
  let files = {};
  if (recommendation) {
    files[RECOMMENDATION_FILE_NAME] = recommendation;
  }
  return AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      applications: { gecko: { id } },
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

add_task(async function setup() {
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
      applications: { gecko: { id } },
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
      applications: { gecko: { id } },
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
      applications: { gecko: { id } },
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
