/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.prefs.setBoolPref("extensions.blocklist.useMLBF", true);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

const { Downloader } = ChromeUtils.import(
  "resource://services-settings/Attachments.jsm"
);

const { TelemetryController } = ChromeUtils.import(
  "resource://gre/modules/TelemetryController.jsm"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const OLDEST_STASH = { stash: { blocked: [], unblocked: [] }, stash_time: 2e6 };
const NEWEST_STASH = { stash: { blocked: [], unblocked: [] }, stash_time: 5e6 };
const RECORDS_WITH_STASHES_AND_MLBF = [MLBF_RECORD, OLDEST_STASH, NEWEST_STASH];

const ExtensionBlocklistMLBF = getExtensionBlocklistMLBF();

function assertTelemetryScalars(expectedScalars) {
  let scalars = TelemetryTestUtils.getProcessScalars("parent");

  for (const scalarName of Object.keys(expectedScalars || {})) {
    equal(
      scalars[scalarName],
      expectedScalars[scalarName],
      `Got the expected value for ${scalarName} scalar`
    );
  }
}

function toUTC(time) {
  return new Date(time).toUTCString();
}

add_task(async function setup() {
  await TelemetryController.testSetup();
  await promiseStartupManager();

  // Disable the packaged record and attachment to make sure that the test
  // will not fall back to the packaged attachments.
  Downloader._RESOURCE_BASE_URL = "invalid://bogus";
});

add_task(async function test_initialization() {
  ExtensionBlocklistMLBF.ensureInitialized();
  assertTelemetryScalars({
    "blocklist.mlbf_enabled": true,
    // In other parts of this test, this value is not checked any more.
    // test_blocklist_telemetry.js already checks lastModified_rs_addons_mlbf.
    "blocklist.lastModified_rs_addons_mlbf": undefined,
    "blocklist.mlbf_source": undefined,
    "blocklist.mlbf_generation_time": undefined,
    "blocklist.mlbf_stash_time_oldest": undefined,
    "blocklist.mlbf_stash_time_newest": undefined,
  });
});

// Test what happens if there is no blocklist data at all.
add_task(async function test_without_mlbf() {
  // Add one (invalid) value to the blocklist, to prevent the RemoteSettings
  // client from importing the JSON dump (which could potentially cause the
  // test to fail due to the unexpected imported records).
  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: [{}] });
  assertTelemetryScalars({
    "blocklist.mlbf_enabled": true,
    "blocklist.mlbf_source": "unknown",
    "blocklist.mlbf_generation_time": "Missing Date",
    "blocklist.mlbf_stash_time_oldest": "Missing Date",
    "blocklist.mlbf_stash_time_newest": "Missing Date",
  });
});

// Test the telemetry that would be recorded in the common case.
add_task(async function test_common_good_case_with_stashes() {
  // The exact content of the attachment does not matter in this test, as long
  // as the data is valid.
  await ExtensionBlocklistMLBF._client.db.saveAttachment(
    ExtensionBlocklistMLBF.RS_ATTACHMENT_ID,
    { record: MLBF_RECORD, blob: await load_mlbf_record_as_blob() }
  );
  await AddonTestUtils.loadBlocklistRawData({
    extensionsMLBF: RECORDS_WITH_STASHES_AND_MLBF,
  });
  assertTelemetryScalars({
    "blocklist.mlbf_enabled": true,
    "blocklist.mlbf_source": "cache_match",
    "blocklist.mlbf_generation_time": toUTC(MLBF_RECORD.generation_time),
    "blocklist.mlbf_stash_time_oldest": toUTC(OLDEST_STASH.stash_time),
    "blocklist.mlbf_stash_time_newest": toUTC(NEWEST_STASH.stash_time),
  });

  // The records and cached attachment carries over to the next tests.
});

// Test what happens when there are no stashes in the collection itself.
add_task(async function test_without_stashes() {
  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: [MLBF_RECORD] });
  assertTelemetryScalars({
    "blocklist.mlbf_enabled": true,
    "blocklist.mlbf_source": "cache_match",
    "blocklist.mlbf_generation_time": toUTC(MLBF_RECORD.generation_time),
    "blocklist.mlbf_stash_time_oldest": "Missing Date",
    "blocklist.mlbf_stash_time_newest": "Missing Date",
  });
});

// Test what happens when the collection was inadvertently emptied,
// but still with a cached mlbf from before.
add_task(async function test_without_collection_but_cache() {
  await AddonTestUtils.loadBlocklistRawData({
    // Insert a dummy record with a value of last_modified which is higher than
    // any value of last_modified in addons-bloomfilters.json, to prevent the
    // blocklist implementation from automatically falling back to the packaged
    // JSON dump.
    extensionsMLBF: [{ last_modified: Date.now() }],
  });
  assertTelemetryScalars({
    "blocklist.mlbf_enabled": true,
    "blocklist.mlbf_source": "cache_fallback",
    "blocklist.mlbf_generation_time": toUTC(MLBF_RECORD.generation_time),
    "blocklist.mlbf_stash_time_oldest": "Missing Date",
    "blocklist.mlbf_stash_time_newest": "Missing Date",
  });
});

// Test that the mlbf_enabled scalar is updated in response to preference
// changes.
add_task(async function test_toggle_preferences() {
  // Disable the blocklist, to prevent the v2 blocklist from initializing.
  // We only care about scalar updates in response to preference changes.
  Services.prefs.setBoolPref("extensions.blocklist.enabled", false);
  // Sanity check: scalars haven't changed.
  assertTelemetryScalars({
    "blocklist.mlbf_enabled": true,
    "blocklist.mlbf_generation_time": toUTC(MLBF_RECORD.generation_time),
  });

  // The pref should be ignored by default.
  Services.prefs.setBoolPref("extensions.blocklist.useMLBF", false);
  assertTelemetryScalars({
    "blocklist.mlbf_enabled": true,
    "blocklist.mlbf_generation_time": toUTC(MLBF_RECORD.generation_time),
  });

  // Explicitly enabling blocklist v2 via a test-only API works.
  // The test helper expects blocklist v3 to have been enabled by default,
  // so restore the pref before calling enable_blocklist_v2_instead_of_useMLBF:
  Services.prefs.setBoolPref("extensions.blocklist.useMLBF", true);
  enable_blocklist_v2_instead_of_useMLBF();
  assertTelemetryScalars({
    "blocklist.mlbf_enabled": false,
    "blocklist.mlbf_generation_time": toUTC(MLBF_RECORD.generation_time),
  });
});
