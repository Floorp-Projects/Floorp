/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.prefs.setBoolPref("extensions.blocklist.useMLBF", true);
Services.prefs.setBoolPref("extensions.blocklist.useMLBF.stashes", true);

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
    "blocklist.mlbf_stashes": true,
    // In other parts of this test, this value is not checked any more.
    // test_blocklist_telemetry.js already checks lastModified_rs_addons_mlbf.
    "blocklist.lastModified_rs_addons_mlbf": undefined,
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
    "blocklist.mlbf_stashes": true,
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
    "blocklist.mlbf_stashes": true,
    "blocklist.mlbf_generation_time": toUTC(MLBF_RECORD.generation_time),
    "blocklist.mlbf_stash_time_oldest": toUTC(OLDEST_STASH.stash_time),
    "blocklist.mlbf_stash_time_newest": toUTC(NEWEST_STASH.stash_time),
  });

  // The records and cached attachment carries over to the next tests.
});

add_task(async function test_toggle_stash_pref() {
  // The previous test had imported RECORDS_WITH_STASHES_AND_MLBF.
  // Verify that toggling the pref causes those stashes to be ignored.
  await toggleStashPref(false, () => {
    // Telemetry should be updated immediately after setting the pref.
    assertTelemetryScalars({
      "blocklist.mlbf_enabled": true,
      "blocklist.mlbf_stashes": false,
    });
  });
  assertTelemetryScalars({
    "blocklist.mlbf_enabled": true,
    "blocklist.mlbf_stashes": false,
    "blocklist.mlbf_generation_time": toUTC(MLBF_RECORD.generation_time),
    "blocklist.mlbf_stash_time_oldest": "Missing Date",
    "blocklist.mlbf_stash_time_newest": "Missing Date",
  });

  await toggleStashPref(true, () => {
    // Telemetry should be updated immediately after setting the pref.
    assertTelemetryScalars({
      "blocklist.mlbf_enabled": true,
      "blocklist.mlbf_stashes": true,
    });
  });
  assertTelemetryScalars({
    "blocklist.mlbf_enabled": true,
    "blocklist.mlbf_stashes": true,
    "blocklist.mlbf_generation_time": toUTC(MLBF_RECORD.generation_time),
    "blocklist.mlbf_stash_time_oldest": toUTC(OLDEST_STASH.stash_time),
    "blocklist.mlbf_stash_time_newest": toUTC(NEWEST_STASH.stash_time),
  });
});

// Test what happens when there are no stashes in the collection itself.
add_task(async function test_without_stashes() {
  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: [MLBF_RECORD] });
  assertTelemetryScalars({
    "blocklist.mlbf_enabled": true,
    "blocklist.mlbf_stashes": true,
    "blocklist.mlbf_generation_time": toUTC(MLBF_RECORD.generation_time),
    "blocklist.mlbf_stash_time_oldest": "Missing Date",
    "blocklist.mlbf_stash_time_newest": "Missing Date",
  });
});

// Test that the mlbf_enabled and mlbf_stashes scalars are updated in response
// to preference changes.
add_task(async function test_toggle_preferences() {
  // Disable the blocklist, to prevent the v2 blocklist from initializing.
  // We only care about scalar updates in response to preference changes.
  Services.prefs.setBoolPref("extensions.blocklist.enabled", false);
  // Sanity check: scalars haven't changed.
  assertTelemetryScalars({
    "blocklist.mlbf_enabled": true,
    "blocklist.mlbf_stashes": true,
    "blocklist.mlbf_generation_time": toUTC(MLBF_RECORD.generation_time),
  });

  Services.prefs.setBoolPref("extensions.blocklist.useMLBF.stashes", false);
  assertTelemetryScalars({
    "blocklist.mlbf_enabled": true,
    "blocklist.mlbf_stashes": false,
    "blocklist.mlbf_generation_time": toUTC(MLBF_RECORD.generation_time),
  });

  Services.prefs.setBoolPref("extensions.blocklist.useMLBF", false);
  assertTelemetryScalars({
    "blocklist.mlbf_enabled": false,
    "blocklist.mlbf_stashes": false,
    "blocklist.mlbf_generation_time": toUTC(MLBF_RECORD.generation_time),
  });

  Services.prefs.setBoolPref("extensions.blocklist.useMLBF.stashes", true);
  assertTelemetryScalars({
    "blocklist.mlbf_enabled": false,
    // The mlbf_stashes scalar is only updated when useMLBF is true.
    "blocklist.mlbf_stashes": false,
    "blocklist.mlbf_generation_time": toUTC(MLBF_RECORD.generation_time),
  });
});
