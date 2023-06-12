/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.prefs.setBoolPref("extensions.blocklist.useMLBF", true);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

const { Downloader } = ChromeUtils.importESModule(
  "resource://services-settings/Attachments.sys.mjs"
);

const { TelemetryController } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryController.sys.mjs"
);
const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const OLDEST_STASH = { stash: { blocked: [], unblocked: [] }, stash_time: 2e6 };
const NEWEST_STASH = { stash: { blocked: [], unblocked: [] }, stash_time: 5e6 };
const RECORDS_WITH_STASHES_AND_MLBF = [MLBF_RECORD, OLDEST_STASH, NEWEST_STASH];

const ExtensionBlocklistMLBF = getExtensionBlocklistMLBF();

function assertTelemetryScalars(expectedScalars) {
  // On Android, we only report to the Glean system telemetry system.
  if (IS_ANDROID_BUILD) {
    info(
      `Skip assertions on collected samples for ${expectedScalars} on android builds`
    );
    return;
  }
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
  if (!IS_ANDROID_BUILD) {
    // FOG needs a profile directory to put its data in.
    do_get_profile();
    // FOG needs to be initialized in order for data to flow.
    Services.fog.initializeFOG();
  }
  await TelemetryController.testSetup();
  await promiseStartupManager();

  // Disable the packaged record and attachment to make sure that the test
  // will not fall back to the packaged attachments.
  Downloader._RESOURCE_BASE_URL = "invalid://bogus";
});

add_task(async function test_initialization() {
  resetBlocklistTelemetry();
  ExtensionBlocklistMLBF.ensureInitialized();

  Assert.equal(undefined, testGetValue(Glean.blocklist.mlbfSource));
  Assert.equal(undefined, testGetValue(Glean.blocklist.mlbfGenerationTime));
  Assert.equal(undefined, testGetValue(Glean.blocklist.mlbfStashTimeOldest));
  Assert.equal(undefined, testGetValue(Glean.blocklist.mlbfStashTimeNewest));

  assertTelemetryScalars({
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
  resetBlocklistTelemetry();
  // Add one (invalid) value to the blocklist, to prevent the RemoteSettings
  // client from importing the JSON dump (which could potentially cause the
  // test to fail due to the unexpected imported records).
  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: [{}] });
  Assert.equal("unknown", testGetValue(Glean.blocklist.mlbfSource));

  Assert.equal(0, testGetValue(Glean.blocklist.mlbfGenerationTime).getTime());
  Assert.equal(0, testGetValue(Glean.blocklist.mlbfStashTimeOldest).getTime());
  Assert.equal(0, testGetValue(Glean.blocklist.mlbfStashTimeNewest).getTime());

  assertTelemetryScalars({
    "blocklist.mlbf_source": "unknown",
    "blocklist.mlbf_generation_time": "Missing Date",
    "blocklist.mlbf_stash_time_oldest": "Missing Date",
    "blocklist.mlbf_stash_time_newest": "Missing Date",
  });
});

// Test the telemetry that would be recorded in the common case.
add_task(async function test_common_good_case_with_stashes() {
  resetBlocklistTelemetry();
  // The exact content of the attachment does not matter in this test, as long
  // as the data is valid.
  await ExtensionBlocklistMLBF._client.db.saveAttachment(
    ExtensionBlocklistMLBF.RS_ATTACHMENT_ID,
    { record: MLBF_RECORD, blob: await load_mlbf_record_as_blob() }
  );
  await AddonTestUtils.loadBlocklistRawData({
    extensionsMLBF: RECORDS_WITH_STASHES_AND_MLBF,
  });
  Assert.equal("cache_match", testGetValue(Glean.blocklist.mlbfSource));
  Assert.equal(
    MLBF_RECORD.generation_time,
    testGetValue(Glean.blocklist.mlbfGenerationTime).getTime()
  );
  Assert.equal(
    OLDEST_STASH.stash_time,
    testGetValue(Glean.blocklist.mlbfStashTimeOldest).getTime()
  );
  Assert.equal(
    NEWEST_STASH.stash_time,
    testGetValue(Glean.blocklist.mlbfStashTimeNewest).getTime()
  );
  assertTelemetryScalars({
    "blocklist.mlbf_source": "cache_match",
    "blocklist.mlbf_generation_time": toUTC(MLBF_RECORD.generation_time),
    "blocklist.mlbf_stash_time_oldest": toUTC(OLDEST_STASH.stash_time),
    "blocklist.mlbf_stash_time_newest": toUTC(NEWEST_STASH.stash_time),
  });

  // The records and cached attachment carries over to the next tests.
});

// Test what happens when there are no stashes in the collection itself.
add_task(async function test_without_stashes() {
  resetBlocklistTelemetry();
  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: [MLBF_RECORD] });

  Assert.equal("cache_match", testGetValue(Glean.blocklist.mlbfSource));
  Assert.equal(
    MLBF_RECORD.generation_time,
    testGetValue(Glean.blocklist.mlbfGenerationTime).getTime()
  );

  Assert.equal(0, testGetValue(Glean.blocklist.mlbfStashTimeOldest).getTime());
  Assert.equal(0, testGetValue(Glean.blocklist.mlbfStashTimeNewest).getTime());

  assertTelemetryScalars({
    "blocklist.mlbf_source": "cache_match",
    "blocklist.mlbf_generation_time": toUTC(MLBF_RECORD.generation_time),
    "blocklist.mlbf_stash_time_oldest": "Missing Date",
    "blocklist.mlbf_stash_time_newest": "Missing Date",
  });
});

// Test what happens when the collection was inadvertently emptied,
// but still with a cached mlbf from before.
add_task(async function test_without_collection_but_cache() {
  resetBlocklistTelemetry();
  await AddonTestUtils.loadBlocklistRawData({
    // Insert a dummy record with a value of last_modified which is higher than
    // any value of last_modified in addons-bloomfilters.json, to prevent the
    // blocklist implementation from automatically falling back to the packaged
    // JSON dump.
    extensionsMLBF: [{ last_modified: Date.now() }],
  });
  Assert.equal("cache_fallback", testGetValue(Glean.blocklist.mlbfSource));
  Assert.equal(
    MLBF_RECORD.generation_time,
    testGetValue(Glean.blocklist.mlbfGenerationTime).getTime()
  );

  Assert.equal(0, testGetValue(Glean.blocklist.mlbfStashTimeOldest).getTime());
  Assert.equal(0, testGetValue(Glean.blocklist.mlbfStashTimeNewest).getTime());

  assertTelemetryScalars({
    "blocklist.mlbf_source": "cache_fallback",
    "blocklist.mlbf_generation_time": toUTC(MLBF_RECORD.generation_time),
    "blocklist.mlbf_stash_time_oldest": "Missing Date",
    "blocklist.mlbf_stash_time_newest": "Missing Date",
  });
});
