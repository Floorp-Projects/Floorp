/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * @fileOverview Verifies that the MLBF dump of the addons blocklist is
 * correctly registered.
 */

Services.prefs.setBoolPref("extensions.blocklist.useMLBF", true);

const { ExtensionBlocklist: ExtensionBlocklistMLBF } = Blocklist;

// A known blocked version from bug 1626602.
const blockedAddon = {
  id: "{6f62927a-e380-401a-8c9e-c485b7d87f0d}",
  version: "9.2.0",
  // The following date is the date of the first checked in MLBF. Any MLBF
  // generated in the future should be generated after this date, to be useful.
  signedDate: new Date(1588098908496), // 2020-04-28 (dummy date)
};

// A known add-on that is not blocked, as of writing. It is likely not going
// to be blocked because it does not have any executable code.
const nonBlockedAddon = {
  id: "disable-ctrl-q-and-cmd-q@robwu.nl",
  version: "1",
  signedDate: new Date(1482430349000), // 2016-12-22 (actual signing time).
};

async function sha256(arrayBuffer) {
  Cu.importGlobalProperties(["crypto"]);
  let hash = await crypto.subtle.digest("SHA-256", arrayBuffer);
  const toHex = b => b.toString(16).padStart(2, "0");
  return Array.from(new Uint8Array(hash), toHex).join("");
}

add_task(async function verify_dump_first_run() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  // Tapping into the internals of ExtensionBlocklistMLBF._fetchMLBF to observe
  // MLBF request details.
  const observed = [];

  ExtensionBlocklistMLBF.ensureInitialized();
  // Despite being called "download", this does not actually access the network
  // when there is a valid dump.
  const originalImpl = ExtensionBlocklistMLBF._client.attachments.download;
  ExtensionBlocklistMLBF._client.attachments.download = function(record) {
    let downloadPromise = originalImpl.apply(this, arguments);
    observed.push({ inputRecord: record, downloadPromise });
    return downloadPromise;
  };

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

  Assert.equal(observed.length, 1, "expected number of MLBF download requests");

  const { inputRecord, downloadPromise } = observed[0];

  Assert.ok(inputRecord, "addons-bloomfilters collection dump exists");

  const downloadResult = await downloadPromise;

  // Verify that the "download" result really originates from the local dump.
  // "dump_match" means that the record exists in the collection and that an
  // attachment was found.
  //
  // If this fails:
  // - "dump_fallback" means that the MLBF attachment is out of sync with the
  //   collection data.
  // - undefined could mean that the implementation of Attachments.jsm changed.
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
