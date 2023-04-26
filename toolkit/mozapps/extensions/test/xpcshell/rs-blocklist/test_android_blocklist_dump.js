/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Blocklist v3 will be enabled on release in bug 1824863.
// TODO bug 1824863: Remove this when blocklist v3 is enabled.
const IS_USING_BLOCKLIST_V3 = AppConstants.NIGHTLY_BUILD;

// When bug 1639050 is fixed, this whole test can be removed as it is already
// covered by test_blocklist_mlbf_dump.js.

// A known blocked version from bug 1626602.
// Same as in test_blocklist_mlbf_dump.js.
const blockedAddon = {
  id: "{6f62927a-e380-401a-8c9e-c485b7d87f0d}",
  version: "9.2.0",
  signedDate: new Date(1588098908496), // 2020-04-28 (dummy date)
  signedState: AddonManager.SIGNEDSTATE_SIGNED,
};

// A known blocked version from bug 1681884, blocklist v3 only but not v2,
// i.e. not listed in services/settings/dumps/blocklists/addons.json.
const blockedAddonV3only = {
  id: "{011f65f0-7143-470a-83ca-20ec4297f3f4}",
  version: "1.0",
  // omiting signedDate/signedState: in blocklist v2 those don't matter.
  // In v3 those do matter, so if blocklist v3 were to be enabled, then
  // the test would fail.
};

// A known add-on that is not blocked, as of writing. It is likely not going
// to be blocked because it does not have any executable code.
// Same as in test_blocklist_mlbf_dump.js.
const nonBlockedAddon = {
  id: "disable-ctrl-q-and-cmd-q@robwu.nl",
  version: "1",
  signedDate: new Date(1482430349000), // 2016-12-22 (actual signing time).
  signedState: AddonManager.SIGNEDSTATE_SIGNED,
};

add_task(
  { skip_if: () => IS_USING_BLOCKLIST_V3 },
  async function verify_blocklistv2_dump_first_run() {
    createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

    Assert.equal(
      await Blocklist.getAddonBlocklistState(blockedAddon),
      Ci.nsIBlocklistService.STATE_BLOCKED,
      "A add-on that is known to be on the v2 blocklist should be blocked"
    );
    Assert.equal(
      await Blocklist.getAddonBlocklistState(blockedAddonV3only),
      Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
      "An add-on that is not part of the v2 blocklist should not be blocked"
    );

    Assert.equal(
      await Blocklist.getAddonBlocklistState(nonBlockedAddon),
      Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
      "A known non-blocked add-on should not be blocked"
    );
  }
);

add_task(
  { skip_if: () => !IS_USING_BLOCKLIST_V3 },
  async function verify_a_known_blocked_add_on_is_not_detected_as_blocked_at_first_run() {
    const MLBF_LOAD_RESULTS = [];
    const MLBF_LOAD_ATTEMPTS = [];
    const onLoadAttempts = record => MLBF_LOAD_ATTEMPTS.push(record);
    const onLoadResult = promise => MLBF_LOAD_RESULTS.push(promise);
    spyOnExtensionBlocklistMLBF(onLoadAttempts, onLoadResult);

    // The addons blocklist data is not packaged and will be downloaded after install
    Assert.equal(
      await Blocklist.getAddonBlocklistState(blockedAddon),
      Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
      "A known blocked add-on should not be blocked at first"
    );

    await Assert.rejects(
      MLBF_LOAD_RESULTS[0],
      /DownloadError: Could not download addons-mlbf.bin/,
      "Should not find any packaged attachment"
    );

    MLBF_LOAD_ATTEMPTS.length = 0;
    MLBF_LOAD_RESULTS.length = 0;

    Assert.equal(
      await Blocklist.getAddonBlocklistState(blockedAddon),
      Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
      "Blocklist is still not populated"
    );
    Assert.deepEqual(
      MLBF_LOAD_ATTEMPTS,
      [],
      "MLBF is not fetched again after the first lookup"
    );
  }
);

function spyOnExtensionBlocklistMLBF(onLoadAttempts, onLoadResult) {
  const ExtensionBlocklistMLBF = getExtensionBlocklistMLBF();
  // Tapping into the internals of ExtensionBlocklistMLBF._fetchMLBF to observe
  const originalFetchMLBF = ExtensionBlocklistMLBF._fetchMLBF;
  ExtensionBlocklistMLBF._fetchMLBF = async function(record) {
    onLoadAttempts(record);
    let promise = originalFetchMLBF.apply(this, arguments);
    onLoadResult(promise);
    return promise;
  };

  registerCleanupFunction(
    () => (ExtensionBlocklistMLBF._fetchMLBF = originalFetchMLBF)
  );

  return ExtensionBlocklistMLBF;
}
