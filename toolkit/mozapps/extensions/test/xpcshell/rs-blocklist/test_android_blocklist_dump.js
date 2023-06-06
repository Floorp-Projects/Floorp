/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// A known blocked version from bug 1626602.
// Same as in test_blocklist_mlbf_dump.js.
const blockedAddon = {
  id: "{6f62927a-e380-401a-8c9e-c485b7d87f0d}",
  version: "9.2.0",
  signedDate: new Date(1588098908496), // 2020-04-28 (dummy date)
  signedState: AddonManager.SIGNEDSTATE_SIGNED,
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
      await Blocklist.getAddonBlocklistState(nonBlockedAddon),
      Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
      "A known non-blocked add-on should not be blocked"
    );

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
  ExtensionBlocklistMLBF._fetchMLBF = async function (record) {
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
