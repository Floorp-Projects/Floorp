/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

AddonTestUtils.init(this);
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "43"
);

ChromeUtils.defineESModuleGetters(this, {
  computeSha1HashAsString: "resource://gre/modules/addons/crypto-utils.sys.mjs",
  QuarantinedDomains: "resource://gre/modules/ExtensionPermissions.sys.mjs",
});

add_setup(() => {
  setupTelemetryForTests();
});

add_task(async function test_QuarantinedDomainsList_telemetry() {
  const cleanupPrefs = () => {
    Services.prefs.clearUserPref(QuarantinedDomains.PREF_DOMAINSLIST_NAME);
  };
  registerCleanupFunction(cleanupPrefs);

  const assertDomainsListTelemetry = ({ prefValue, expected }) => {
    resetTelemetryData();
    Services.prefs.setStringPref(
      QuarantinedDomains.PREF_DOMAINSLIST_NAME,
      prefValue
    );
    Assert.deepEqual(
      {
        listsize: QuarantinedDomains.currentDomainsList.set.size,
        listhash: QuarantinedDomains.currentDomainsList.hash,
      },
      expected,
      "Got the expected domains list data computed for the probes"
    );
    Assert.deepEqual(
      {
        listsize: Glean.extensionsQuarantinedDomains.listsize.testGetValue(),
        listhash: Glean.extensionsQuarantinedDomains.listhash.testGetValue(),
      },
      expected,
      "Got the expected computed domains list probes recorded by the Glean metrics"
    );
    const scalars = Services.telemetry.getSnapshotForScalars().parent;
    Assert.deepEqual(
      {
        listsize: scalars?.["extensions.quarantinedDomains.listsize"],
        listhash: scalars?.["extensions.quarantinedDomains.listhash"],
      },
      expected,
      "Got the expected metrics mirrored into the unified telemetry scalars"
    );
  };

  let prefValue;

  info("Verify Glean 'Quarantined Domains list' probes on empty domain list");
  prefValue = "";
  assertDomainsListTelemetry({
    prefValue,
    expected: {
      listsize: 0,
      listhash: computeSha1HashAsString(prefValue),
    },
  });

  info(
    "Verify Glean 'Quarantined Domains list' probes on non-empty domain list"
  );
  prefValue = "example.com,example.org";
  assertDomainsListTelemetry({
    prefValue,
    expected: {
      listsize: 2,
      listhash: computeSha1HashAsString(prefValue),
    },
  });

  info(
    "Verify Glean 'Quarantined Domains list' probes on non-empty domain list with duplicated domains"
  );
  prefValue = "example.com,example.org, example.org, example.com ";
  assertDomainsListTelemetry({
    prefValue,
    expected: {
      listsize: 2,
      listhash: computeSha1HashAsString(prefValue),
    },
  });

  cleanupPrefs();
});
