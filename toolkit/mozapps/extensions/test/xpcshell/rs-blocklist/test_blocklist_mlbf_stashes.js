/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.prefs.setBoolPref("extensions.blocklist.useMLBF", true);
Services.prefs.setBoolPref("extensions.blocklist.useMLBF.stashes", true);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

const ExtensionBlocklistMLBF = getExtensionBlocklistMLBF();
const MLBF_LOAD_ATTEMPTS = [];
ExtensionBlocklistMLBF._fetchMLBF = async record => {
  MLBF_LOAD_ATTEMPTS.push(record);
  return {
    generationTime: 0,
    cascadeFilter: {
      has(blockKey) {
        if (blockKey === "@onlyblockedbymlbf:1") {
          return true;
        }
        throw new Error("bloom filter should not be used in this test");
      },
    },
  };
};

async function checkBlockState(addonId, version, expectBlocked) {
  let addon = {
    id: addonId,
    version,
    // Note: signedDate is missing, so the MLBF does not apply
    // and we will effectively only test stashing.
  };
  let state = await Blocklist.getAddonBlocklistState(addon);
  if (expectBlocked) {
    Assert.equal(state, Ci.nsIBlocklistService.STATE_BLOCKED);
  } else {
    Assert.equal(state, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  }
}

add_task(async function setup() {
  await promiseStartupManager();
});

// Tests that add-ons can be blocked / unblocked via the stash.
add_task(async function basic_stash() {
  await AddonTestUtils.loadBlocklistRawData({
    extensionsMLBF: [
      {
        stash_time: 0,
        stash: {
          blocked: ["@blocked:1"],
          unblocked: ["@notblocked:2"],
        },
      },
    ],
  });
  await checkBlockState("@blocked", "1", true);
  await checkBlockState("@notblocked", "2", false);
  // Not in stash (but unsigned, so shouldn't reach MLBF):
  await checkBlockState("@blocked", "2", false);

  Assert.equal(
    await Blocklist.getAddonBlocklistState({
      id: "@onlyblockedbymlbf",
      version: "1",
      signedDate: new Date(0), // = the MLBF's generationTime.
      signedState: AddonManager.SIGNEDSTATE_SIGNED,
    }),
    Ci.nsIBlocklistService.STATE_BLOCKED,
    "falls through to MLBF if entry is not found in stash"
  );

  Assert.deepEqual(MLBF_LOAD_ATTEMPTS, [null], "MLBF attachment not found");
});

// To complement the privileged_xpi_not_blocked in test_blocklist_mlbf.js,
// verify that privileged add-ons can still be blocked through stashes.
add_task(async function privileged_addon_blocked_by_stash() {
  const system_addon = {
    id: "@blocked",
    version: "1",
    signedDate: new Date(0), // = the MLBF's generationTime.
    signedState: AddonManager.SIGNEDSTATE_PRIVILEGED,
  };
  Assert.equal(
    await Blocklist.getAddonBlocklistState(system_addon),
    Ci.nsIBlocklistService.STATE_BLOCKED,
    "Privileged add-ons can still be blocked by a stash"
  );

  system_addon.signedState = AddonManager.SIGNEDSTATE_SYSTEM;
  Assert.equal(
    await Blocklist.getAddonBlocklistState(system_addon),
    Ci.nsIBlocklistService.STATE_BLOCKED,
    "Privileged system add-ons can still be blocked by a stash"
  );

  // For comparison, when an add-on is only blocked by a MLBF, the block
  // decision is ignored.
  system_addon.id = "@onlyblockedbymlbf";
  Assert.equal(
    await Blocklist.getAddonBlocklistState(system_addon),
    Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
    "Privileged add-ons cannot be blocked via a MLBF"
  );
  // (note that we haven't checked that SIGNEDSTATE_PRIVILEGED is not blocked
  // via the MLBF, but that is already covered by test_blocklist_mlbf.js ).
});

// To complement langpack_not_blocked_on_Nightly in test_blocklist_mlbf.js,
// verify that langpacks can still be blocked through stashes.
add_task(async function langpack_blocked_by_stash() {
  const langpack_addon = {
    id: "@blocked",
    type: "locale",
    version: "1",
    signedDate: new Date(0), // = the MLBF's generationTime.
    signedState: AddonManager.SIGNEDSTATE_SIGNED,
  };
  Assert.equal(
    await Blocklist.getAddonBlocklistState(langpack_addon),
    Ci.nsIBlocklistService.STATE_BLOCKED,
    "Langpack add-ons can still be blocked by a stash"
  );

  // For comparison, when an add-on is only blocked by a MLBF, the block
  // decision is ignored on Nightly (but blocked on non-Nightly).
  langpack_addon.id = "@onlyblockedbymlbf";
  if (AppConstants.NIGHTLY_BUILD) {
    Assert.equal(
      await Blocklist.getAddonBlocklistState(langpack_addon),
      Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
      "Langpack add-ons cannot be blocked via a MLBF on Nightly"
    );
  } else {
    Assert.equal(
      await Blocklist.getAddonBlocklistState(langpack_addon),
      Ci.nsIBlocklistService.STATE_BLOCKED,
      "Langpack add-ons can be blocked via a MLBF on non-Nightly"
    );
  }
});

// Tests that invalid stash entries are ignored.
add_task(async function invalid_stashes() {
  await AddonTestUtils.loadBlocklistRawData({
    extensionsMLBF: [
      {},
      { stash: null },
      { stash: 1 },
      { stash: {} },
      { stash: { blocked: ["@broken:1", "@okid:1"] } },
      { stash: { unblocked: ["@broken:2"] } },
      // The only correct entry:
      { stash: { blocked: ["@okid:2"], unblocked: ["@okid:1"] } },
      { stash: { blocked: ["@broken:1", "@okid:1"] } },
      { stash: { unblocked: ["@broken:2", "@okid:2"] } },
    ],
  });
  // The valid stash entry should be applied:
  await checkBlockState("@okid", "1", false);
  await checkBlockState("@okid", "2", true);
  // Entries from invalid stashes should be ignored:
  await checkBlockState("@broken", "1", false);
  await checkBlockState("@broken", "2", false);
});

// Blocklist stashes should be processed in the reverse chronological order.
add_task(async function stash_time_order() {
  await AddonTestUtils.loadBlocklistRawData({
    extensionsMLBF: [
      // "@a:1" and "@a:2" are blocked at time 1, but unblocked later.
      { stash_time: 2, stash: { blocked: [], unblocked: ["@a:1"] } },
      { stash_time: 1, stash: { blocked: ["@a:1", "@a:2"], unblocked: [] } },
      { stash_time: 3, stash: { blocked: [], unblocked: ["@a:2"] } },

      // "@b:1" and "@b:2" are unblocked at time 4, but blocked later.
      { stash_time: 5, stash: { blocked: ["@b:1"], unblocked: [] } },
      { stash_time: 4, stash: { blocked: [], unblocked: ["@b:1", "@b:2"] } },
      { stash_time: 6, stash: { blocked: ["@b:2"], unblocked: [] } },
    ],
  });
  await checkBlockState("@a", "1", false);
  await checkBlockState("@a", "2", false);

  await checkBlockState("@b", "1", true);
  await checkBlockState("@b", "2", true);
});

// Tests that the correct records+attachment are chosen depending on the pref.
add_task(async function mlbf_attachment_type_and_stash_is_correct() {
  MLBF_LOAD_ATTEMPTS.length = 0;
  const records = [
    { stash_time: 0, stash: { blocked: ["@blocked:1"], unblocked: [] } },
    { attachment_type: "bloomfilter-base", attachment: {}, generation_time: 0 },
    { attachment_type: "bloomfilter-full", attachment: {}, generation_time: 1 },
  ];
  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: records });
  // Check that the pref works.
  await checkBlockState("@blocked", "1", true);
  await toggleStashPref(false);
  await checkBlockState("@blocked", "1", false);
  await toggleStashPref(true);
  await checkBlockState("@blocked", "1", true);

  Assert.deepEqual(
    MLBF_LOAD_ATTEMPTS.map(r => r?.attachment_type),
    [
      // Initial load with pref true
      "bloomfilter-base",
      // Pref off.
      "bloomfilter-full",
      // Pref on again.
      "bloomfilter-base",
    ],
    "Expected attempts to load MLBF as part of update"
  );
});

// When stashes are disabled, "bloomfilter-full" may be used (as seen in the
// previous test, mlbf_attachment_type_and_stash_is_correct). With stashes
// enabled, "bloomfilter-full" should be ignored, however.
add_task(async function mlbf_bloomfilter_full_ignored() {
  MLBF_LOAD_ATTEMPTS.length = 0;

  await AddonTestUtils.loadBlocklistRawData({
    extensionsMLBF: [{ attachment_type: "bloomfilter-full", attachment: {} }],
  });

  // When stashes are enabled, only bloomfilter-base records should be used.
  // Since there are no such records, we shouldn't find anything.
  Assert.deepEqual(MLBF_LOAD_ATTEMPTS, [null], "no matching MLBFs found");
});

// Tests that the most recent MLBF is downloaded.
add_task(async function mlbf_generation_time_recent() {
  MLBF_LOAD_ATTEMPTS.length = 0;
  const records = [
    { attachment_type: "bloomfilter-base", attachment: {}, generation_time: 2 },
    { attachment_type: "bloomfilter-base", attachment: {}, generation_time: 3 },
    { attachment_type: "bloomfilter-base", attachment: {}, generation_time: 1 },
  ];
  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: records });
  Assert.equal(
    MLBF_LOAD_ATTEMPTS[0].generation_time,
    3,
    "expected to load most recent MLBF"
  );
});
