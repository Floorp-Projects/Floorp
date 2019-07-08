/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { XPIInstall } = ChromeUtils.import(
  "resource://gre/modules/addons/XPIInstall.jsm"
);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

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

add_task(async function setup() {
  await ExtensionTestUtils.startAddonManager();
});

add_task(async function text_no_file() {
  const id = "no-recommendations-file@test.web.extension";
  let addon = await installAddonWithRecommendations(id, null);

  ok(!addon.isRecommended, "The add-on is not recommended");

  await addon.uninstall();
});

add_task(async function text_malformed_file() {
  const id = "no-recommendations-file@test.web.extension";
  let addon = await installAddonWithRecommendations(id, "This is not JSON");

  ok(!addon.isRecommended, "The add-on is not recommended");

  await addon.uninstall();
});

add_task(async function test_valid_recommendation_file() {
  const id = "recommended@test.web.extension";
  let addon = await installAddonWithRecommendations(id, {
    addon_id: id,
    states: ["recommended"],
    validity: { not_before, not_after },
  });

  ok(addon.isRecommended, "The add-on is recommended");

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

  ok(!addon.isRecommended, "The add-on is not recommended");

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

  ok(!addon.isRecommended, "The add-on is not recommended");

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

  ok(!addon.isRecommended, "The add-on is not recommended");

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

  ok(!extension.addon.isRecommended, "The add-on is not recommended");

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

  ok(!addon.isRecommended, "The add-on is not recommended");

  await addon.uninstall();
});

add_task(async function test_not_recommended() {
  const id = "not-recommended@test.web.extension";
  let addon = await installAddonWithRecommendations(id, {
    addon_id: id,
    states: ["something"],
    validity: { not_before, not_after },
  });

  ok(!addon.isRecommended, "The add-on is not recommended");

  await addon.uninstall();
});

add_task(async function test_id_missing() {
  const id = "no-id@test.web.extension";
  let addon = await installAddonWithRecommendations(id, {
    states: ["recommended"],
    validity: { not_before, not_after },
  });

  ok(!addon.isRecommended, "The add-on is not recommended");

  await addon.uninstall();
});

add_task(async function test_expired() {
  const id = "expired@test.web.extension";
  let addon = await installAddonWithRecommendations(id, {
    addon_id: id,
    states: ["recommended"],
    validity: { not_before, not_after: not_before },
  });

  ok(!addon.isRecommended, "The add-on is not recommended");

  await addon.uninstall();
});

add_task(async function test_not_valid_yet() {
  const id = "expired@test.web.extension";
  let addon = await installAddonWithRecommendations(id, {
    addon_id: id,
    states: ["recommended"],
    validity: { not_before: not_after, not_after },
  });

  ok(!addon.isRecommended, "The add-on is not recommended");

  await addon.uninstall();
});

add_task(async function test_states_missing() {
  const id = "states-missing@test.web.extension";
  let addon = await installAddonWithRecommendations(id, {
    addon_id: id,
    validity: { not_before, not_after },
  });

  ok(!addon.isRecommended, "The add-on is not recommended");

  await addon.uninstall();
});

add_task(async function test_validity_missing() {
  const id = "validity-missing@test.web.extension";
  let addon = await installAddonWithRecommendations(id, {
    addon_id: id,
    states: ["recommended"],
  });

  ok(!addon.isRecommended, "The add-on is not recommended");

  await addon.uninstall();
});

add_task(async function test_not_before_missing() {
  const id = "not-before-missing@test.web.extension";
  let addon = await installAddonWithRecommendations(id, {
    addon_id: id,
    states: ["recommended"],
    validity: { not_after },
  });

  ok(!addon.isRecommended, "The add-on is not recommended");

  await addon.uninstall();
});

add_task(async function test_bad_states() {
  const id = "bad-states@test.web.extension";
  let addon = await installAddonWithRecommendations(id, {
    addon_id: id,
    states: { recommended: true },
    validity: { not_before, not_after },
  });

  ok(!addon.isRecommended, "The add-on is not recommended");

  await addon.uninstall();
});

add_task(async function test_recommendation_persist_restart() {
  const id = "persisted-recommendation@test.web.extension";
  let addon = await installAddonWithRecommendations(id, {
    addon_id: id,
    states: ["recommended"],
    validity: { not_before, not_after },
  });

  ok(addon.isRecommended, "The add-on is recommended");

  await AddonTestUtils.promiseRestartManager();

  addon = await AddonManager.getAddonByID(id);

  ok(addon.isRecommended, "The add-on is still recommended");

  await addon.uninstall();
});
