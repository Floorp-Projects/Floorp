/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * @fileOverview Verifies that the MLBF dump of the addons blocklist is
 * correctly registered.
 */

Services.prefs.setBoolPref("extensions.blocklist.useMLBF", true);

const ExtensionBlocklistMLBF = getExtensionBlocklistMLBF();

// A known blocked version from bug 1626602.
const blockedAddon = {
  id: "{6f62927a-e380-401a-8c9e-c485b7d87f0d}",
  version: "9.2.0",
  // The following date is the date of the first checked in MLBF. Any MLBF
  // generated in the future should be generated after this date, to be useful.
  signedDate: new Date(1588098908496), // 2020-04-28 (dummy date)
  signedState: AddonManager.SIGNEDSTATE_SIGNED,
};

// A known add-on that is not blocked, as of writing. It is likely not going
// to be blocked because it does not have any executable code.
const nonBlockedAddon = {
  id: "disable-ctrl-q-and-cmd-q@robwu.nl",
  version: "1",
  signedDate: new Date(1482430349000), // 2016-12-22 (actual signing time).
  signedState: AddonManager.SIGNEDSTATE_SIGNED,
};

async function sha256(arrayBuffer) {
  let hash = await crypto.subtle.digest("SHA-256", arrayBuffer);
  const toHex = b => b.toString(16).padStart(2, "0");
  return Array.from(new Uint8Array(hash), toHex).join("");
}

// A list of { inputRecord, downloadPromise }:
// - inputRecord is the record that was used for looking up the MLBF.
// - downloadPromise is the result of trying to download it.
const observed = [];

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
  ExtensionBlocklistMLBF.ensureInitialized();

  // Tapping into the internals of ExtensionBlocklistMLBF._fetchMLBF to observe
  // MLBF request details.

  // Despite being called "download", this does not actually access the network
  // when there is a valid dump.
  const originalImpl = ExtensionBlocklistMLBF._client.attachments.download;
  ExtensionBlocklistMLBF._client.attachments.download = function (record) {
    let downloadPromise = originalImpl.apply(this, arguments);
    observed.push({ inputRecord: record, downloadPromise });
    return downloadPromise;
  };

  await promiseStartupManager();
});

async function verifyBlocklistWorksWithDump() {
  Assert.equal(
    await Blocklist.getAddonBlocklistState(blockedAddon),
    Ci.nsIBlocklistService.STATE_BLOCKED,
    "A add-on that is known to be on the blocklist should be blocked"
  );
  Assert.equal(
    await Blocklist.getAddonBlocklistState(nonBlockedAddon),
    Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
    "A known non-blocked add-on should not be blocked"
  );
}

add_task(async function verify_dump_first_run() {
  await verifyBlocklistWorksWithDump();
  Assert.equal(observed.length, 1, "expected number of MLBF download requests");

  const { inputRecord, downloadPromise } = observed.pop();

  Assert.ok(inputRecord, "addons-bloomfilters collection dump exists");

  const downloadResult = await downloadPromise;

  // Verify that the "download" result really originates from the local dump.
  // "dump_match" means that the record exists in the collection and that an
  // attachment was found.
  //
  // If this fails:
  // - "dump_fallback" means that the MLBF attachment is out of sync with the
  //   collection data.
  // - undefined could mean that the implementation of Attachments.sys.mjs changed.
  Assert.equal(
    downloadResult._source,
    "dump_match",
    "MLBF attachment should match the RemoteSettings collection"
  );

  Assert.equal(
    await sha256(downloadResult.buffer),
    inputRecord.attachment.hash,
    "The content of the attachment should actually matches the record"
  );
});

add_task(async function use_dump_fallback_when_collection_is_out_of_sync() {
  await AddonTestUtils.loadBlocklistRawData({
    // last_modified higher than any value in addons-bloomfilters.json.
    extensionsMLBF: [{ last_modified: Date.now() }],
  });
  Assert.equal(observed.length, 1, "Expected new download on update");

  const { inputRecord, downloadPromise } = observed.pop();
  Assert.equal(inputRecord, null, "No MLBF record found");

  const downloadResult = await downloadPromise;
  Assert.equal(
    downloadResult._source,
    "dump_fallback",
    "should have used fallback despite the absence of a MLBF record"
  );

  await verifyBlocklistWorksWithDump();
  Assert.equal(observed.length, 0, "Blocklist uses cached result");
});

// Verifies that the dump would supersede local data. This can happen after an
// application upgrade, where the local database contains outdated records from
// a previous application version.
add_task(async function verify_dump_supersedes_old_dump() {
  // Delete in-memory value; otherwise the cached record from the previous test
  // task would be re-used and nothing would be downloaded.
  delete ExtensionBlocklistMLBF._mlbfData;

  await AddonTestUtils.loadBlocklistRawData({
    // last_modified lower than any value in addons-bloomfilters.json.
    extensionsMLBF: [{ last_modified: 1 }],
  });
  Assert.equal(observed.length, 1, "Expected new download on update");

  const { inputRecord, downloadPromise } = observed.pop();
  Assert.ok(inputRecord, "should have read from addons-bloomfilters dump");

  const downloadResult = await downloadPromise;
  Assert.equal(
    downloadResult._source,
    "dump_match",
    "Should have replaced outdated collection records with dump"
  );

  await verifyBlocklistWorksWithDump();
  Assert.equal(observed.length, 0, "Blocklist uses cached result");
});
