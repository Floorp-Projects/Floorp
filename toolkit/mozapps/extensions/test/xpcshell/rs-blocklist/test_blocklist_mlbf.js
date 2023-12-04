/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.prefs.setBoolPref("extensions.blocklist.useMLBF", true);

const ExtensionBlocklistMLBF = getExtensionBlocklistMLBF();

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
AddonTestUtils.useRealCertChecks = true;

// A real, signed XPI for use in the test.
const SIGNED_ADDON_XPI_FILE = do_get_file("../data/webext-implicit-id.xpi");
const SIGNED_ADDON_ID = "webext_implicit_id@tests.mozilla.org";
const SIGNED_ADDON_VERSION = "1.0";
const SIGNED_ADDON_KEY = `${SIGNED_ADDON_ID}:${SIGNED_ADDON_VERSION}`;
const SIGNED_ADDON_SIGN_TIME = 1459980789000; // notBefore of certificate.

// A real, signed sitepermission XPI for use in the test.
const SIGNED_SITEPERM_XPI_FILE = do_get_file("webmidi_permission.xpi");
const SIGNED_SITEPERM_ADDON_ID = "webmidi@test.mozilla.org";
const SIGNED_SITEPERM_ADDON_VERSION = "1.0.2";
const SIGNED_SITEPERM_KEY = `${SIGNED_SITEPERM_ADDON_ID}:${SIGNED_SITEPERM_ADDON_VERSION}`;
const SIGNED_SITEPERM_SIGN_TIME = 1637606460000; // notBefore of certificate.

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

  const install = await promiseInstallFile(SIGNED_ADDON_XPI_FILE);
  Assert.equal(install.error, 0, "Install should not have an error");

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
      url: "https://addons.mozilla.org/en-US/xpcshell/blocked-addon/webext_implicit_id@tests.mozilla.org/1.0/",
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

  const install = await promiseInstallFile(SIGNED_ADDON_XPI_FILE);
  Assert.equal(
    install.error,
    AddonManager.ERROR_BLOCKLISTED,
    "Install should have an error"
  );

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
      browser_specific_settings: { gecko: { id: UNSIGNED_ADDON_ID } },
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

// A privileged add-on cannot be blocked by the MLBF.
// It can still be blocked by a stash, which is tested in
// privileged_addon_blocked_by_stash in test_blocklist_mlbf_stashes.js.
add_task(async function privileged_xpi_not_blocked() {
  mockMLBF({
    blocked: ["test@tests.mozilla.org:2.0"],
    notblocked: [],
    generationTime: 1546297200000, // 1 jan 2019 = after the cert's notBefore
  });
  await ExtensionBlocklistMLBF._onUpdate();

  const install = await promiseInstallFile(
    do_get_file("../data/signing_checks/privileged.xpi")
  );
  Assert.equal(install.error, 0, "Install should not have an error");

  let addon = await promiseAddonByID("test@tests.mozilla.org");
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_PRIVILEGED);
  Assert.equal(addon.blocklistState, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  await addon.uninstall();
});

// Langpacks cannot be blocked via the MLBF on Nightly.
// It can still be blocked by a stash, which is tested in
// langpack_blocked_by_stash in test_blocklist_mlbf_stashes.js.
add_task(
  // We do not support langpacks on Android.
  { skip_if: () => AppConstants.platform == "android" },
  async function langpack_not_blocked_on_Nightly() {
    mockMLBF({
      blocked: ["langpack-klingon@firefox.mozilla.org:1.0"],
      notblocked: [],
      generationTime: 1546297200000, // 1 jan 2019 = after the cert's notBefore
    });
    await ExtensionBlocklistMLBF._onUpdate();

    await promiseInstallFile(
      do_get_file("../data/signing_checks/langpack_signed.xpi")
    );
    let addon = await promiseAddonByID("langpack-klingon@firefox.mozilla.org");
    Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_SIGNED);
    if (AppConstants.NIGHTLY_BUILD) {
      // Langpacks built for Nightly are currently signed by releng and not
      // submitted to AMO, so we have to ignore the blocks of the MLBF.
      Assert.equal(
        addon.blocklistState,
        Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
        "Langpacks cannot be blocked via the MLBF"
      );
    } else {
      // On non-Nightly, langpacks are submitted through AMO so we will enforce
      // the MLBF blocklist for them.
      Assert.equal(addon.blocklistState, Ci.nsIBlocklistService.STATE_BLOCKED);
    }
    await addon.uninstall();
  }
);

// Checks: Signed sitepermission addon, initially blocked on install, then unblocked.
add_task(
  // We do not support this add-on type on Android.
  { skip_if: () => AppConstants.platform == "android" },
  async function signed_sitepermission_xpi_blocked_on_install() {
    mockMLBF({
      blocked: [SIGNED_SITEPERM_KEY],
      notblocked: [],
      generationTime: SIGNED_SITEPERM_SIGN_TIME + 1,
    });
    await ExtensionBlocklistMLBF._onUpdate();

    const install = await promiseInstallFile(SIGNED_SITEPERM_XPI_FILE);
    Assert.equal(
      install.error,
      AddonManager.ERROR_BLOCKLISTED,
      "Install should have an error"
    );

    let addon = await promiseAddonByID(SIGNED_SITEPERM_ADDON_ID);
    // NOTE: if this assertion fails, then SIGNED_SITEPERM_SIGN_TIME has to be
    // updated accordingly otherwise the addon would not be blocked on install
    // as this test expects (using the value got from `addon.signedDate.getTime()`)
    equal(
      addon.signedDate?.getTime(),
      SIGNED_SITEPERM_SIGN_TIME,
      "The addon xpi has the expected signedDate timestamp"
    );
    Assert.equal(
      addon.blocklistState,
      Ci.nsIBlocklistService.STATE_BLOCKED,
      "Got the expected STATE_BLOCKED blocklistState"
    );
    Assert.ok(addon.appDisabled, "Blocked add-on is disabled on install");

    mockMLBF({
      blocked: [],
      notblocked: [SIGNED_SITEPERM_KEY],
      generationTime: SIGNED_SITEPERM_SIGN_TIME - 1,
    });
    await ExtensionBlocklistMLBF._onUpdate();
    Assert.equal(
      addon.blocklistState,
      Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
      "Got the expected STATE_NOT_BLOCKED blocklistState"
    );
    Assert.ok(!addon.appDisabled, "Re-enabled after unblock");

    await addon.uninstall();
  }
);
