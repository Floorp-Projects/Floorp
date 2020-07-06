/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * @fileOverview Tests the MLBF and RemoteSettings synchronization logic.
 */

Services.prefs.setBoolPref("extensions.blocklist.useMLBF", true);

const { Downloader } = ChromeUtils.import(
  "resource://services-settings/Attachments.jsm"
);

const ExtensionBlocklistMLBF = getExtensionBlocklistMLBF();

// This test needs to interact with the RemoteSettings client.
ExtensionBlocklistMLBF.ensureInitialized();

add_task(async function fetch_invalid_mlbf_record() {
  let invalidRecord = {
    attachment: { size: 1, hash: "definitely not valid" },
    generation_time: 1,
  };

  // _fetchMLBF(invalidRecord) may succeed if there is a MLBF dump packaged with
  // the application. This test intentionally hides the actual path to get
  // deterministic results. To check whether the dump is correctly registered,
  // run test_blocklist_mlbf_dump.js

  // Forget about the packaged attachment.
  Downloader._RESOURCE_BASE_URL = "invalid://bogus";
  // NetworkError is expected here. The JSON.parse error could be triggered via
  // _baseAttachmentsURL < downloadAsBytes < download < download < _fetchMLBF if
  // the request to  services.settings.server (http://localhost/dummy-kinto/v1)
  // is fulfilled (but with invalid JSON). That request is not expected to be
  // fulfilled in the first place, but that is not a concern of this test.
  // This test passes if _fetchMLBF() rejects when given an invalid record.
  await Assert.rejects(
    ExtensionBlocklistMLBF._fetchMLBF(invalidRecord),
    /NetworkError|SyntaxError: JSON\.parse/,
    "record not found when there is no packaged MLBF"
  );
});

// Other tests can mock _testMLBF, so let's verify that it works as expected.
add_task(async function fetch_valid_mlbf() {
  await ExtensionBlocklistMLBF._client.db.saveAttachment(
    ExtensionBlocklistMLBF.RS_ATTACHMENT_ID,
    { record: MLBF_RECORD, blob: await load_mlbf_record_as_blob() }
  );

  const result = await ExtensionBlocklistMLBF._fetchMLBF(MLBF_RECORD);
  Assert.equal(result.cascadeHash, MLBF_RECORD.attachment.hash, "hash OK");
  Assert.equal(result.generationTime, MLBF_RECORD.generation_time, "time OK");
  Assert.ok(result.cascadeFilter.has("@blocked:1"), "item blocked");
  Assert.ok(!result.cascadeFilter.has("@unblocked:2"), "item not blocked");

  const result2 = await ExtensionBlocklistMLBF._fetchMLBF({
    attachment: { size: 1, hash: "invalid" },
    generation_time: Date.now(),
  });
  Assert.equal(
    result2.cascadeHash,
    MLBF_RECORD.attachment.hash,
    "The cached MLBF should be used when the attachment is invalid"
  );

  // The attachment is kept in the database for use by the next test task.
});

// Test that results of the public API are consistent with the MLBF file.
add_task(async function public_api_uses_mlbf() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
  await promiseStartupManager();

  const blockedAddon = {
    id: "@blocked",
    version: "1",
    signedDate: new Date(0), // a date in the past, before MLBF's generationTime.
    signedState: AddonManager.SIGNEDSTATE_SIGNED,
  };
  const nonBlockedAddon = {
    id: "@unblocked",
    version: "2",
    signedDate: new Date(0), // a date in the past, before MLBF's generationTime.
    signedState: AddonManager.SIGNEDSTATE_SIGNED,
  };

  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: [MLBF_RECORD] });

  Assert.deepEqual(
    await Blocklist.getAddonBlocklistEntry(blockedAddon),
    {
      state: Ci.nsIBlocklistService.STATE_BLOCKED,
      url:
        "https://addons.mozilla.org/en-US/xpcshell/blocked-addon/@blocked/1/",
    },
    "Blocked addon should have blocked entry"
  );

  Assert.deepEqual(
    await Blocklist.getAddonBlocklistEntry(nonBlockedAddon),
    null,
    "Non-blocked addon should not be blocked"
  );

  Assert.equal(
    await Blocklist.getAddonBlocklistState(blockedAddon),
    Ci.nsIBlocklistService.STATE_BLOCKED,
    "Blocked entry should have blocked state"
  );

  Assert.equal(
    await Blocklist.getAddonBlocklistState(nonBlockedAddon),
    Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
    "Non-blocked entry should have unblocked state"
  );

  // Note: Blocklist collection and attachment carries over to the next test.
});

// Verifies that the metadata (time of validity) of an updated MLBF record is
// correctly used, even if the MLBF itself has not changed.
add_task(async function fetch_updated_mlbf_same_hash() {
  const recordUpdate = {
    ...MLBF_RECORD,
    generation_time: MLBF_RECORD.generation_time + 1,
  };
  const blockedAddonUpdate = {
    id: "@blocked",
    version: "1",
    signedDate: new Date(recordUpdate.generation_time),
    signedState: AddonManager.SIGNEDSTATE_SIGNED,
  };

  // The blocklist already includes "@blocked:1", but the last specified
  // generation time is MLBF_RECORD.generation_time. So the addon cannot be
  // blocked, because the block decision could be a false positive.
  Assert.equal(
    await Blocklist.getAddonBlocklistState(blockedAddonUpdate),
    Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
    "Add-on not blocked before blocklist update"
  );

  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: [recordUpdate] });
  // The MLBF is now known to apply to |blockedAddonUpdate|.

  Assert.equal(
    await Blocklist.getAddonBlocklistState(blockedAddonUpdate),
    Ci.nsIBlocklistService.STATE_BLOCKED,
    "Add-on blocked after update"
  );

  // Note: Blocklist collection and attachment carries over to the next test.
});

// Checks the remaining cases of database corruption that haven't been handled
// before.
add_task(async function handle_database_corruption() {
  const blockedAddon = {
    id: "@blocked",
    version: "1",
    signedDate: new Date(0), // a date in the past, before MLBF's generationTime.
    signedState: AddonManager.SIGNEDSTATE_SIGNED,
  };
  async function checkBlocklistWorks() {
    Assert.equal(
      await Blocklist.getAddonBlocklistState(blockedAddon),
      Ci.nsIBlocklistService.STATE_BLOCKED,
      "Add-on should be blocked by the blocklist"
    );
  }

  // In the fetch_invalid_mlbf_record we checked that a cached / packaged MLBF
  // attachment is used as a fallback when the record is invalid. Here we also
  // check that there is a fallback when there is no record at all.

  // Include a dummy record in the list, to prevent RemoteSettings from
  // importing a JSON dump with unexpected records.
  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: [{}] });
  // When the collection is empty, the last known MLBF should be used anyway.
  await checkBlocklistWorks();

  // Now we also remove the cached file...
  await ExtensionBlocklistMLBF._client.db.saveAttachment(
    ExtensionBlocklistMLBF.RS_ATTACHMENT_ID,
    null
  );
  // Deleting the file shouldn't cause issues because the MLBF is loaded once
  // and then kept in memory.
  await checkBlocklistWorks();

  // Force an update while we don't have any blocklist data nor cache.
  await ExtensionBlocklistMLBF._onUpdate();
  // As a fallback, continue to use the in-memory version of the blocklist.
  await checkBlocklistWorks();

  // Memory gone, e.g. after a browser restart.
  delete ExtensionBlocklistMLBF._mlbfData;
  Assert.equal(
    await Blocklist.getAddonBlocklistState(blockedAddon),
    Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
    "Blocklist can't work if all blocklist data is gone"
  );
});
