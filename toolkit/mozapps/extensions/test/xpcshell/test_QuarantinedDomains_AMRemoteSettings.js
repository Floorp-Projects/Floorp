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
      id: "quarantinedDomains-testSet-toolong",
      quarantinedDomains: {
        [QUARANTINE_LIST_PREF]: "x".repeat(MAX_PREF_LENGTH + 1),
      },
    },
    {
      id: "quarantinedDomains-testSet1",
      quarantinedDomains: {
        [QUARANTINE_LIST_PREF]: quarantinedDomainsSets.testSet1,
      },
    },
    {
      id: "quarantinedDomains-testSet2",
      quarantinedDomains: {
        [QUARANTINE_LIST_PREF]: quarantinedDomainsSets.testSet2,
      },
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
}

add_setup(async () => {
  await AddonTestUtils.promiseStartupManager();
});

add_task(testQuarantinedDomainsFromRemoteSettings);
