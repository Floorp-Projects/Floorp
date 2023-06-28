/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "42", "42");

const QUARANTINE_LIST_PREF = "extensions.quarantinedDomains.list";

async function testQuarantinedDomainsFromRemoteSettings() {
  // Same as MAX_PREF_LENGTH as defined in Preferences.cpp,
  // see https://searchfox.org/mozilla-central/rev/06510249/modules/libpref/Preferences.cpp#162
  const MAX_PREF_LENGTH = 1 * 1024 * 1024;
  const quarantinedDomainsSets = {
    testSet1: "example.com,example.org",
    testSet2: "someothersite.org,testset2.org",
  };
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
  Assert.equal(
    Services.prefs.getStringPref(QUARANTINE_LIST_PREF),
    quarantinedDomainsSets.testSet2,
    `Got the expected value set on ${QUARANTINE_LIST_PREF}`
  );

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
  Assert.equal(
    Services.prefs.getStringPref(QUARANTINE_LIST_PREF),
    NEW_PREF_VALUE,
    `Got the expected value set on ${QUARANTINE_LIST_PREF} on record`
  );

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
  Assert.equal(
    Services.prefs.getStringPref(QUARANTINE_LIST_PREF),
    quarantinedDomainsSets.testSet1,
    `Got the expected value set on ${QUARANTINE_LIST_PREF} on record`
  );
}

add_setup(async () => {
  await AddonTestUtils.promiseStartupManager();
});

add_task(testQuarantinedDomainsFromRemoteSettings);
