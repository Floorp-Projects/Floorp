/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Globals imported from head_telemetry.js
/* globals setupTelemetryForTests, resetTelemetryData */

const { QuarantinedDomains } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionPermissions.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  computeSha1HashAsString: "resource://gre/modules/addons/crypto-utils.sys.mjs",
});

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "42", "42");

const QUARANTINE_LIST_PREF = "extensions.quarantinedDomains.list";

function assertQuarantinedListPref(expectedPrefValue) {
  Assert.equal(
    Services.prefs.getPrefType(QUARANTINE_LIST_PREF),
    Services.prefs.PREF_STRING,
    `Expect ${QUARANTINE_LIST_PREF} preference type to be string`
  );

  Assert.equal(
    Services.prefs.getStringPref(QUARANTINE_LIST_PREF),
    expectedPrefValue,
    `Got the expected value set on ${QUARANTINE_LIST_PREF}`
  );
}

function assertQuarantinedListTelemetry(expectedTelemetryHash) {
  Assert.deepEqual(
    {
      listhash: Glean.extensionsQuarantinedDomains.listhash.testGetValue(),
      remotehash: Glean.extensionsQuarantinedDomains.remotehash.testGetValue(),
    },
    expectedTelemetryHash,
    "Got the expected computed domains list probes recorded by the Glean metrics"
  );

  const scalars = Services.telemetry.getSnapshotForScalars().parent;
  Assert.deepEqual(
    {
      listhash: scalars?.["extensions.quarantinedDomains.listhash"],
      remotehash: scalars?.["extensions.quarantinedDomains.remotehash"],
    },
    expectedTelemetryHash,
    "Got the expected metrics mirrored into the unified telemetry scalars"
  );
}

async function testQuarantinedDomainsFromRemoteSettings() {
  // Same as MAX_PREF_LENGTH as defined in Preferences.cpp,
  // see https://searchfox.org/mozilla-central/rev/06510249/modules/libpref/Preferences.cpp#162
  const MAX_PREF_LENGTH = 1 * 1024 * 1024;
  const quarantinedDomainsSets = {
    testSet1: "example.com,example.org",
    testSet2: "someothersite.org,testset2.org",
  };

  // Make sure there isn't initially any pre-existing telemetry data.
  resetTelemetryData();

  await setAndEmitFakeRemoteSettingsData([
    {
      id: "quarantinedDomains-01-testSet-toolong",
      // We expect this entry to throw when trying to set a string pref
      // that doesn't fit in the string prefs size limits.
      quarantinedDomains: {
        [QUARANTINE_LIST_PREF]: "x".repeat(MAX_PREF_LENGTH + 1),
      },
      installTriggerDeprecation: null,
    },
    {
      id: "quarantinedDomains-02-testSet1",
      quarantinedDomains: {
        [QUARANTINE_LIST_PREF]: quarantinedDomainsSets.testSet1,
      },
      installTriggerDeprecation: null,
    },
    {
      // We expect this pref to override the pref set based on the
      // previous entry.
      id: "quarantinedDomains-03-testSet2",
      quarantinedDomains: {
        [QUARANTINE_LIST_PREF]: quarantinedDomainsSets.testSet2,
      },
      installTriggerDeprecation: null,
    },
    {
      // Expect this entry to leave the domains list pref unchanged.
      id: "quarantinedDomains-04-null",
      quarantinedDomains: null,
      installTriggerDeprecation: null,
    },
  ]);

  Assert.equal(
    Services.prefs.getPrefType(QUARANTINE_LIST_PREF),
    Services.prefs.PREF_STRING,
    `Expect ${QUARANTINE_LIST_PREF} preference type to be string`
  );
  // The entry too big to fix in the pref value should throw but not preventing
  // the other entries from being processed.
  // The Last collection entry setting the pref wins, and so we expect
  // the pref to be set to the domains listed in the collection
  // entry with id "quarantinedDomains-testSet2".
  assertQuarantinedListPref(quarantinedDomainsSets.testSet2);
  assertQuarantinedListTelemetry({
    listhash: computeSha1HashAsString(quarantinedDomainsSets.testSet2),
    remotehash: computeSha1HashAsString(quarantinedDomainsSets.testSet2),
  });

  // Confirm that the updated quarantined domains list is now reflected
  // by the results returned by WebExtensionPolicy.isQuarantinedURI.
  // NOTE: Additional test coverage over the quarantined domains behaviors
  // are part of a separate xpcshell test
  // (see toolkit/components/extensions/test/xpcshell/test_QuarantinedDomains.js).
  for (const domain of quarantinedDomainsSets.testSet2.split(",")) {
    let uri = Services.io.newURI(`https://${domain}/`);
    ok(
      WebExtensionPolicy.isQuarantinedURI(uri),
      `Expect ${domain} to be quarantined`
    );
  }

  for (const domain of quarantinedDomainsSets.testSet1.split(",")) {
    let uri = Services.io.newURI(`https://${domain}/`);
    ok(
      !WebExtensionPolicy.isQuarantinedURI(uri),
      `Expect ${domain} to not be quarantined`
    );
  }

  const NEW_PREF_VALUE = "newdomain1.org,newdomain2.org";
  await setAndEmitFakeRemoteSettingsData([
    {
      // This entry doesn't includes an installTriggerDeprecation property
      // (and then we verify that the pref is still set as expected).
      id: "quarantinedDomains-withoutInstallTriggerDeprecation",
      quarantinedDomains: {
        [QUARANTINE_LIST_PREF]: NEW_PREF_VALUE,
      },
    },
  ]);
  assertQuarantinedListPref(NEW_PREF_VALUE);
  assertQuarantinedListTelemetry({
    listhash: computeSha1HashAsString(NEW_PREF_VALUE),
    remotehash: computeSha1HashAsString(NEW_PREF_VALUE),
  });

  await setAndEmitFakeRemoteSettingsData([
    {
      // This entry includes an unexpected property
      // (and then we verify that the pref is still set as expected).
      id: "quarantinedDomains-withoutInstallTriggerDeprecation",
      quarantinedDomains: {
        [QUARANTINE_LIST_PREF]: quarantinedDomainsSets.testSet1,
      },
      someUnexpectedProperty: "some unexpected value",
    },
  ]);
  assertQuarantinedListPref(quarantinedDomainsSets.testSet1);
  assertQuarantinedListTelemetry({
    listhash: computeSha1HashAsString(quarantinedDomainsSets.testSet1),
    remotehash: computeSha1HashAsString(quarantinedDomainsSets.testSet1),
  });

  info(
    "Tamper with the domains list pref value, verify the remotesettings value is set back after restart"
  );
  const MANUALLY_CHANGED_PREF_VALUE =
    quarantinedDomainsSets.testSet1 + ",test123.example.org";
  Services.prefs.setStringPref(
    QUARANTINE_LIST_PREF,
    MANUALLY_CHANGED_PREF_VALUE
  );
  // At this point we expect the value of the hash recorded in telemetry to differ
  // between the listhash and remotehash glean metrics.
  assertQuarantinedListTelemetry({
    listhash: computeSha1HashAsString(MANUALLY_CHANGED_PREF_VALUE),
    remotehash: computeSha1HashAsString(quarantinedDomainsSets.testSet1),
  });

  // Then, we expect the remotehash and listhash to match each other again
  // after the browser restart and the pref value to be back to the last
  // value got from RemoteSettings.
  info("Mock browser restart");
  // Clear telemetry data that was collected so far.
  resetTelemetryData();
  const promisePrefChanged = TestUtils.waitForPrefChange(QUARANTINE_LIST_PREF);
  await AddonTestUtils.promiseRestartManager();
  info(
    `Wait for expected change notified for the ${QUARANTINE_LIST_PREF} pref`
  );
  await promisePrefChanged;

  assertQuarantinedListPref(quarantinedDomainsSets.testSet1);
  assertQuarantinedListTelemetry({
    listhash: computeSha1HashAsString(quarantinedDomainsSets.testSet1),
    remotehash: computeSha1HashAsString(quarantinedDomainsSets.testSet1),
  });
}

add_setup(async () => {
  setupTelemetryForTests();
  await AddonTestUtils.promiseStartupManager();

  Assert.ok(
    QuarantinedDomains._initialized,
    "QuarantinedDomains is initialized"
  );
});

add_task(testQuarantinedDomainsFromRemoteSettings);
