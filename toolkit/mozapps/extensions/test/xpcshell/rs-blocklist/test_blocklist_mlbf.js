/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.prefs.setBoolPref("extensions.blocklist.useMLBF", true);

const { ExtensionBlocklistMLBF } = ChromeUtils.import(
  "resource://gre/modules/Blocklist.jsm",
  null
);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
AddonTestUtils.useRealCertChecks = true;

// A real, signed XPI for use in the test.
const SIGNED_ADDON_XPI_FILE = do_get_file("../data/webext-implicit-id.xpi");
const SIGNED_ADDON_ID = "webext_implicit_id@tests.mozilla.org";
const SIGNED_ADDON_VERSION = "1.0";
const SIGNED_ADDON_KEY = `${SIGNED_ADDON_ID}:${SIGNED_ADDON_VERSION}`;
const SIGNED_ADDON_SIGN_TIME = 1459980789000; // notBefore of certificate.

function mockMLBF({ blocked = [], notblocked = [], generationTime }) {
  // Mock _fetchMLBF to be able to have a deterministic cascade filter.
  ExtensionBlocklistMLBF._fetchMLBF = async () => {
    return {
      cascadeFilter: {
        has(blockKey) {
          if (blocked.includes(blockKey)) {
            return true;
          }
          if (notblocked.includes(blockKey)) {
            return false;
          }
          throw new Error(`Block entry must explicitly be listed: ${blockKey}`);
        },
      },
      generationTime,
    };
  };
}

add_task(async function setup() {
  await promiseStartupManager();
  mockMLBF({});
  await AddonTestUtils.loadBlocklistRawData({
    extensionsMLBF: [MLBF_RECORD],
  });
});

// Checks: Initially unblocked, then blocked, then unblocked again.
add_task(async function signed_xpi_initially_unblocked() {
  mockMLBF({
    blocked: [],
    notblocked: [SIGNED_ADDON_KEY],
    generationTime: SIGNED_ADDON_SIGN_TIME + 1,
  });
  await ExtensionBlocklistMLBF._onUpdate();

  await promiseInstallFile(SIGNED_ADDON_XPI_FILE);

  let addon = await promiseAddonByID(SIGNED_ADDON_ID);
  Assert.equal(addon.blocklistState, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

  mockMLBF({
    blocked: [SIGNED_ADDON_KEY],
    notblocked: [],
    generationTime: SIGNED_ADDON_SIGN_TIME + 1,
  });
  await ExtensionBlocklistMLBF._onUpdate();
  Assert.equal(addon.blocklistState, Ci.nsIBlocklistService.STATE_BLOCKED);
  Assert.deepEqual(
    await Blocklist.getAddonBlocklistEntry(addon),
    {
      state: Ci.nsIBlocklistService.STATE_BLOCKED,
      url:
        "https://addons.mozilla.org/en-US/xpcshell/blocked-addon/webext_implicit_id@tests.mozilla.org/1.0/",
    },
    "Blocked addon should have blocked entry"
  );

  mockMLBF({
    blocked: [SIGNED_ADDON_KEY],
    notblocked: [],
    // MLBF generationTime is older, so "blocked" entry should not apply.
    generationTime: SIGNED_ADDON_SIGN_TIME - 1,
  });
  await ExtensionBlocklistMLBF._onUpdate();
  Assert.equal(addon.blocklistState, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

  await addon.uninstall();
});

// Checks: Initially blocked on install, then unblocked.
add_task(async function signed_xpi_blocked_on_install() {
  mockMLBF({
    blocked: [SIGNED_ADDON_KEY],
    notblocked: [],
    generationTime: SIGNED_ADDON_SIGN_TIME + 1,
  });
  await ExtensionBlocklistMLBF._onUpdate();

  await promiseInstallFile(SIGNED_ADDON_XPI_FILE);
  let addon = await promiseAddonByID(SIGNED_ADDON_ID);
  Assert.equal(addon.blocklistState, Ci.nsIBlocklistService.STATE_BLOCKED);
  Assert.ok(addon.appDisabled, "Blocked add-on is disabled on install");

  mockMLBF({
    blocked: [],
    notblocked: [SIGNED_ADDON_KEY],
    generationTime: SIGNED_ADDON_SIGN_TIME - 1,
  });
  await ExtensionBlocklistMLBF._onUpdate();
  Assert.equal(addon.blocklistState, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  Assert.ok(!addon.appDisabled, "Re-enabled after unblock");

  await addon.uninstall();
});

// An unsigned add-on cannot be blocked.
add_task(async function unsigned_not_blocked() {
  const UNSIGNED_ADDON_ID = "not-signed@tests.mozilla.org";
  const UNSIGNED_ADDON_VERSION = "1.0";
  const UNSIGNED_ADDON_KEY = `${UNSIGNED_ADDON_ID}:${UNSIGNED_ADDON_VERSION}`;
  mockMLBF({
    blocked: [UNSIGNED_ADDON_KEY],
    notblocked: [],
    generationTime: SIGNED_ADDON_SIGN_TIME + 1,
  });
  await ExtensionBlocklistMLBF._onUpdate();

  let unsignedAddonFile = createTempWebExtensionFile({
    manifest: {
      version: UNSIGNED_ADDON_VERSION,
      applications: { gecko: { id: UNSIGNED_ADDON_ID } },
    },
  });

  // Unsigned add-ons can generally only be loaded as a temporary install.
  let [addon] = await Promise.all([
    AddonManager.installTemporaryAddon(unsignedAddonFile),
    promiseWebExtensionStartup(UNSIGNED_ADDON_ID),
  ]);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_MISSING);
  Assert.equal(addon.signedDate, null);
  Assert.equal(addon.blocklistState, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  Assert.equal(
    await Blocklist.getAddonBlocklistState(addon),
    Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
    "Unsigned temporary add-on is not blocked"
  );
  await addon.uninstall();
});

// To make sure that unsigned_not_blocked did not trivially pass, we also check
// that add-ons can actually be blocked when installed as a temporary add-on.
add_task(async function signed_temporary() {
  mockMLBF({
    blocked: [SIGNED_ADDON_KEY],
    notblocked: [],
    generationTime: SIGNED_ADDON_SIGN_TIME + 1,
  });
  await ExtensionBlocklistMLBF._onUpdate();

  await Assert.rejects(
    AddonManager.installTemporaryAddon(SIGNED_ADDON_XPI_FILE),
    /Add-on webext_implicit_id@tests.mozilla.org is not compatible with application version/,
    "Blocklisted add-on cannot be installed"
  );
});
