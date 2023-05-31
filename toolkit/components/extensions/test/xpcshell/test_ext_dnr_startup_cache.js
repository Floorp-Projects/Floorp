/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Schemas } = ChromeUtils.importESModule(
  "resource://gre/modules/Schemas.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  ExtensionDNR: "resource://gre/modules/ExtensionDNR.sys.mjs",
  ExtensionDNRStore: "resource://gre/modules/ExtensionDNRStore.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetters(this, {
  aomStartup: [
    "@mozilla.org/addons/addon-manager-startup;1",
    "amIAddonManagerStartup",
  ],
});

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

Services.scriptloader.loadSubScript(
  Services.io.newFileURI(do_get_file("head_dnr.js")).spec,
  this
);

const EXT_ID = "test-dnr-store-startup-cache@test-extension";
const TEMP_EXT_ID = "test-dnr-store-temporarily-installed@test-extension";

// Test rulesets should include fields that are special cased during Rule object deserialization
// from the plain objects loaded from the StartupCache data objects.
// In particular regexFilter are included to make sure that RuleValidator.deserializeRule is
// internally compiling the regexFilter and storing it into the RuleCondition instance as expected
// (implicitly asserted internally by the test helper assertDNRStoreData in head_dnr.js).
const RULESET_1_DATA = [
  getDNRRule({
    id: 1,
    action: { type: "allow" },
    condition: { resourceTypes: ["main_frame"], regexFilter: "http://from/$" },
  }),
  getDNRRule({
    id: 2,
    action: { type: "allow" },
    condition: {
      resourceTypes: ["main_frame"],
      regexFilter: "http://from2/$",
      isUrlFilterCaseSensitive: true,
    },
  }),
];
const RULESET_2_DATA = [
  getDNRRule({
    action: { type: "block" },
    condition: { resourceTypes: ["main_frame", "script"] },
  }),
];

function getDNRExtension({
  id = EXT_ID,
  version = "1.0",
  useAddonManager = "permanent",
  background,
  rule_resources,
  declarative_net_request,
  files,
}) {
  // Omit declarative_net_request if rule_resources isn't defined
  // (because declarative_net_request fails the manifest validation
  // if rule_resources is missing).
  const dnr = rule_resources ? { rule_resources } : undefined;

  return {
    background,
    useAddonManager,
    manifest: {
      manifest_version: 3,
      version,
      permissions: ["declarativeNetRequest", "declarativeNetRequestFeedback"],
      // Needed to make sure the upgraded extension will have the same id and
      // same uuid (which is mapped based on the extension id).
      browser_specific_settings: {
        gecko: { id },
      },
      declarative_net_request: declarative_net_request
        ? { ...declarative_net_request, ...(dnr ?? {}) }
        : dnr,
    },
    files,
  };
}

add_setup(async () => {
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
  Services.prefs.setBoolPref("extensions.dnr.enabled", true);
  Services.prefs.setBoolPref("extensions.dnr.feedback", true);

  setupTelemetryForTests();

  await ExtensionTestUtils.startAddonManager();
});

add_task(async function test_dnr_startup_cache_save_and_load() {
  resetTelemetryData();

  const rule_resources = [
    {
      id: "ruleset_1",
      enabled: true,
      path: "ruleset_1.json",
    },
    {
      id: "ruleset_2",
      enabled: false,
      path: "ruleset_2.json",
    },
  ];
  const files = {
    "ruleset_1.json": JSON.stringify(RULESET_1_DATA),
    "ruleset_2.json": JSON.stringify(RULESET_2_DATA),
  };

  let dnrStore = ExtensionDNRStore._getStoreForTesting();
  let sandboxStoreSpies = sinon.createSandbox();
  const spyScheduleCacheDataSave = sandboxStoreSpies.spy(
    dnrStore,
    "scheduleCacheDataSave"
  );

  const extension = ExtensionTestUtils.loadExtension(
    getDNRExtension({ rule_resources, files })
  );

  const temporarilyInstalledExt = ExtensionTestUtils.loadExtension(
    getDNRExtension({
      id: TEMP_EXT_ID,
      useAddonManager: "temporary",
      rule_resources,
      files,
    })
  );

  assertDNRTelemetryMetricsNoSamples(
    [
      {
        metric: "validateRulesTime",
        mirroredName: "WEBEXT_DNR_VALIDATE_RULES_MS",
        mirroredType: "histogram",
      },
    ],
    "before any test extensions have been loaded"
  );

  await temporarilyInstalledExt.startup();
  await extension.startup();
  info(
    "Wait for DNR initialization completed for the temporarily installed extension"
  );
  await ExtensionDNR.ensureInitialized(temporarilyInstalledExt.extension);
  info(
    "Wait for DNR initialization completed for the permanently installed extension"
  );
  await ExtensionDNR.ensureInitialized(extension.extension);

  assertDNRTelemetryMetricsSamplesCount(
    [
      {
        metric: "validateRulesTime",
        mirroredName: "WEBEXT_DNR_VALIDATE_RULES_MS",
        mirroredType: "histogram",
        expectedSamplesCount: 2,
      },
    ],
    "after two test extensions have been loaded"
  );

  Assert.equal(
    spyScheduleCacheDataSave.callCount,
    1,
    "Expect ExtensionDNRStore scheduleCacheDataSave method to have been called once"
  );

  sandboxStoreSpies.restore();

  const extUUID = extension.uuid;
  const { cacheFile } = dnrStore.getFilePaths(extUUID);

  await assertDNRStoreData(dnrStore, extension, {
    ruleset_1: getSchemaNormalizedRules(extension, RULESET_1_DATA),
  });

  assertDNRTelemetryMetricsNoSamples(
    [
      {
        metric: "startupCacheWriteTime",
        mirroredName: "WEBEXT_DNR_STARTUPCACHE_WRITE_MS",
        mirroredType: "histogram",
      },
      {
        metric: "startupCacheWriteSize",
        mirroredName: "WEBEXT_DNR_STARTUPCACHE_WRITE_BYTES",
        mirroredType: "histogram",
      },
      // Expected no startup cache file to be loaded or used for a newly installed extension.
      {
        metric: "startupCacheReadSize",
        mirroredName: "WEBEXT_DNR_STARTUPCACHE_READ_BYTES",
        mirroredType: "histogram",
      },
      {
        metric: "startupCacheReadTime",
        mirroredName: "WEBEXT_DNR_STARTUPCACHE_READ_MS",
        mirroredType: "histogram",
      },
      {
        metric: "startupCacheEntries",
        label: "miss",
        mirroredName: "extensions.apis.dnr.startup_cache_entries",
        mirroredType: "keyedScalar",
      },
      {
        metric: "startupCacheEntries",
        label: "hit",
        mirroredName: "extensions.apis.dnr.startup_cache_entries",
        mirroredType: "keyedScalar",
      },
    ],
    "on loading dnr rules for newly installed extension"
  );
  await dnrStore.waitSaveCacheDataForTesting();
  assertDNRTelemetryMetricsSamplesCount(
    [
      {
        metric: "startupCacheWriteTime",
        mirroredName: "WEBEXT_DNR_STARTUPCACHE_WRITE_MS",
        mirroredType: "histogram",
        expectedSamplesCount: 1,
      },
      {
        metric: "startupCacheWriteSize",
        mirroredName: "WEBEXT_DNR_STARTUPCACHE_WRITE_BYTES",
        mirroredType: "histogram",
        expectedSamplesCount: 1,
      },
    ],
    "after writing DNR startup cache data to disk"
  );

  ok(
    await IOUtils.exists(cacheFile),
    "Expect the DNR store startupCache file exist"
  );

  const assertDNRStoreDataLoadOnStartup = async ({
    expectLoadedFromCache,
    expectClearLastUpdateTagPref,
  }) => {
    info(
      `Mock browser restart and assert DNR rules ${
        expectLoadedFromCache ? "NOT " : ""
      }going through Schemas.normalize`
    );
    await AddonTestUtils.promiseShutdownManager();
    // Recreate the DNR store to more easily mock its initial state after a browser restart.
    dnrStore = ExtensionDNRStore._recreateStoreForTesting();
    const StoreData = ExtensionDNRStore._getStoreDataClassForTesting();

    let sandbox = sinon.createSandbox();
    const schemasNormalizeSpy = sandbox.spy(Schemas, "normalize");
    const ruleValidatorAddRulesSpy = sandbox.spy(
      ExtensionDNR.RuleValidator.prototype,
      "addRules"
    );
    const deserializeRuleSpy = sandbox.spy(
      ExtensionDNR.RuleValidator,
      "deserializeRule"
    );
    const clearLastUpdateTagPrefSpy = sandbox.spy(
      StoreData,
      "clearLastUpdateTagPref"
    );
    const scheduleCacheDataSaveSpy = sandbox.spy(
      dnrStore,
      "scheduleCacheDataSave"
    );

    resetTelemetryData();
    await AddonTestUtils.promiseStartupManager();
    await extension.awaitStartup();
    await ExtensionDNR.ensureInitialized(extension.extension);

    if (expectLoadedFromCache) {
      assertDNRTelemetryMetricsSamplesCount(
        [
          {
            metric: "startupCacheReadSize",
            mirroredName: "WEBEXT_DNR_STARTUPCACHE_READ_BYTES",
            mirroredType: "histogram",
            expectedSamplesCount: 1,
          },
          {
            metric: "startupCacheReadTime",
            mirroredName: "WEBEXT_DNR_STARTUPCACHE_READ_MS",
            mirroredType: "histogram",
            expectedSamplesCount: 1,
          },
        ],
        "after app startup and expected startup cache hit"
      );
      assertDNRTelemetryMetricsGetValueEq(
        [
          {
            metric: "startupCacheEntries",
            label: "hit",
            expectedGetValue: 1,
            mirroredName: "extensions.apis.dnr.startup_cache_entries",
            mirroredType: "keyedScalar",
          },
        ],
        "after app startup and expected startup cache hit"
      );
      assertDNRTelemetryMetricsNoSamples(
        [
          {
            metric: "validateRulesTime",
            mirroredName: "WEBEXT_DNR_VALIDATE_RULES_MS",
            mirroredType: "histogram",
          },
          {
            metric: "startupCacheEntries",
            label: "miss",
            mirroredName: "extensions.apis.dnr.startup_cache_entries",
            mirroredType: "keyedScalar",
          },
        ],
        "after DNR store loaded startup cache data"
      );
    } else {
      assertDNRTelemetryMetricsSamplesCount(
        [
          {
            metric: "validateRulesTime",
            mirroredName: "WEBEXT_DNR_VALIDATE_RULES_MS",
            mirroredType: "histogram",
            expectedSamplesCount: 1,
          },
          {
            metric: "startupCacheReadSize",
            mirroredName: "WEBEXT_DNR_STARTUPCACHE_READ_BYTES",
            mirroredType: "histogram",
            expectedSamplesCount: 1,
          },
          {
            metric: "startupCacheReadTime",
            mirroredName: "WEBEXT_DNR_STARTUPCACHE_READ_MS",
            mirroredType: "histogram",
            expectedSamplesCount: 1,
          },
        ],
        "after app startup and expected startup cache miss"
      );
      assertDNRTelemetryMetricsGetValueEq(
        [
          {
            metric: "startupCacheEntries",
            label: "miss",
            expectedGetValue: 1,
            mirroredName: "extensions.apis.dnr.startup_cache_entries",
            mirroredType: "keyedScalar",
          },
        ],
        "after app startup and expected startup cache miss"
      );
      assertDNRTelemetryMetricsNoSamples(
        [
          {
            metric: "startupCacheEntries",
            label: "hit",
            mirroredName: "extensions.apis.dnr.startup_cache_entries",
            mirroredType: "keyedScalar",
          },
        ],
        "after DNR store loaded startup cache data"
      );
    }

    Assert.equal(
      scheduleCacheDataSaveSpy.called,
      !expectLoadedFromCache,
      "scheduleCacheDataSave to not be called when the extension DNR rules are initialized from startup cache data"
    );

    Assert.equal(
      clearLastUpdateTagPrefSpy.callCount,
      expectClearLastUpdateTagPref ? 1 : 0,
      "Expect clearLastUpdateTagPrefSpy to have been called the expected number of times"
    );
    if (expectClearLastUpdateTagPref === true) {
      Assert.ok(
        clearLastUpdateTagPrefSpy.calledWith(extension.uuid),
        "Expect clearLastUpdateTagPrefSpy to have been called with the test extension uuid"
      );
    }

    Assert.equal(
      schemasNormalizeSpy.calledWith(
        sinon.match.any,
        sinon.match("declarativeNetRequest.Rule"),
        sinon.match.any
      ),
      !expectLoadedFromCache,
      `Expect DNR rules to ${
        expectLoadedFromCache ? "NOT " : ""
      }be going through Schemas.normalize`
    );

    Assert.equal(
      ruleValidatorAddRulesSpy.called,
      !expectLoadedFromCache,
      `Expect DNR rules to ${
        expectLoadedFromCache ? "NOT " : ""
      }be going through RuleValidator addRules`
    );

    Assert.equal(
      deserializeRuleSpy.called,
      expectLoadedFromCache,
      `Expect RuleValidator.deserializeRule to ${
        expectLoadedFromCache ? "NOT " : ""
      }be called to convert StartupCache data back into Rule class instances`
    );

    await assertDNRStoreData(dnrStore, extension, {
      ruleset_1: getSchemaNormalizedRules(extension, RULESET_1_DATA),
    });

    sandbox.restore();
  };

  const expectedLastUpdateTag = ExtensionDNRStore._getLastUpdateTag(
    extension.uuid
  );

  Assert.ok(
    typeof expectedLastUpdateTag == "string" && !!expectedLastUpdateTag.length,
    `Expect lastUpdateTag for ${extension.id} to be set to a non empty string: ${expectedLastUpdateTag}`
  );

  Assert.equal(
    ExtensionDNRStore._getLastUpdateTag(temporarilyInstalledExt.uuid),
    null,
    `Expect no lastUpdateTag value set for temporarily installed extensions`
  );

  await assertDNRStoreDataLoadOnStartup({
    expectLoadedFromCache: true,
    expectClearLastUpdateTagPref: false,
  });

  {
    const { buffer } = await IOUtils.read(cacheFile);
    const decodedData = aomStartup.decodeBlob(buffer);
    Assert.equal(
      expectedLastUpdateTag,
      decodedData?.cacheData.get(extension.uuid).lastUpdateTag,
      "Expect cacheData entry's lastUpdateTag to match the value stored in the related pref"
    );
    Assert.equal(
      decodedData?.cacheData.has(temporarilyInstalledExt.uuid),
      false,
      "Expect no cache data entry for temporarily installed extensions"
    );

    info("Confirm startupCache data dropped if last tag pref value mismatches");
    ExtensionDNRStore._storeLastUpdateTag(
      extension.uuid,
      "mismatching-tag-value"
    );
    Assert.notEqual(
      ExtensionDNRStore._getLastUpdateTag(extension.uuid),
      decodedData?.cacheData.get(extension.uuid).lastUpdateTag,
      "Expect cacheData.lastDNRStoreUpdateTag to NOT match the tampered value stored in the related pref"
    );
  }

  await assertDNRStoreDataLoadOnStartup({
    expectLoadedFromCache: false,
    expectClearLastUpdateTagPref: true,
  });

  info(
    "Verify that startupCache data mismatching with the StoreData schema version is being dropped"
  );
  await dnrStore.waitSaveCacheDataForTesting();
  await assertDNRStoreDataLoadOnStartup({
    expectLoadedFromCache: true,
    expectClearLastUpdateTagPref: false,
  });

  {
    info("Tamper the StoreData version in the startupCache data");
    const { buffer } = await IOUtils.read(cacheFile);
    const decodedData = aomStartup.decodeBlob(buffer);
    decodedData.cacheData.get(extUUID).schemaVersion = -1;
    await IOUtils.write(
      cacheFile,
      new Uint8Array(aomStartup.encodeBlob(decodedData))
    );
  }

  await assertDNRStoreDataLoadOnStartup({
    expectLoadedFromCache: false,
    expectClearLastUpdateTagPref: true,
  });

  info(
    "Verify that startupCache data mismatching with the extension version is being dropped"
  );
  await dnrStore.waitSaveCacheDataForTesting();
  await assertDNRStoreDataLoadOnStartup({
    expectLoadedFromCache: true,
    expectClearLastUpdateTagPref: false,
  });

  {
    info("Tamper the extension version in the startupCache data");
    const { buffer } = await IOUtils.read(cacheFile);
    const decodedData = aomStartup.decodeBlob(buffer);
    decodedData.cacheData.get(extUUID).extVersion = "0.1";
    await IOUtils.write(
      cacheFile,
      new Uint8Array(aomStartup.encodeBlob(decodedData))
    );
  }
  await assertDNRStoreDataLoadOnStartup({
    expectLoadedFromCache: false,
    expectClearLastUpdateTagPref: true,
  });

  await extension.unload();

  Assert.equal(
    ExtensionDNRStore._getLastUpdateTag(extension.uuid),
    null,
    "LastUpdateTag pref should have been removed after addon uninstall"
  );
});

add_task(async function test_detect_and_reschedule_save_cache_on_new_changes() {
  const rule_resources = [
    {
      id: "ruleset_1",
      enabled: true,
      path: "ruleset_1.json",
    },
  ];
  const files = {
    "ruleset_1.json": JSON.stringify(RULESET_1_DATA),
  };

  let dnrStore = ExtensionDNRStore._getStoreForTesting();
  let sandboxStore = sinon.createSandbox();
  const spyScheduleCacheDataSave = sandboxStore.spy(
    dnrStore,
    "scheduleCacheDataSave"
  );

  let extension;
  const tamperedLastUpdateTag = Services.uuid.generateUUID().toString();
  let resolvePromiseSaveCacheRescheduled;
  let promiseSaveCacheRescheduled = new Promise(resolve => {
    resolvePromiseSaveCacheRescheduled = resolve;
  });
  const realDetectStartupCacheDataChanged =
    dnrStore.detectStartupCacheDataChanged.bind(dnrStore);
  const stubDetectCacheDataChanges = sandboxStore.stub(
    dnrStore,
    "detectStartupCacheDataChanged"
  );

  stubDetectCacheDataChanges.callsFake(seenLastUpdateTags => {
    const extData = dnrStore._data.get(extension.extension.uuid);
    Assert.ok(extData, "Got StoreData instance for the test extension");
    Assert.ok(
      typeof extData.lastUpdateTag === "string" &&
        !!extData.lastUpdateTag.length,
      "Expect a non empty lastUpdateTag assigned to the extension StoreData"
    );
    Assert.deepEqual(
      Array.from(seenLastUpdateTags),
      [extData.lastUpdateTag],
      "Expects the extension storeData lastUpdateTag to have been seen"
    );
    if (stubDetectCacheDataChanges.callCount == 1) {
      Assert.notEqual(
        extData.lastUpdateTag,
        tamperedLastUpdateTag,
        "New tampered lastUpdateTag should not be equal to the one already set"
      );
      extData.lastUpdateTag = tamperedLastUpdateTag;
      Assert.equal(
        realDetectStartupCacheDataChanged(seenLastUpdateTags),
        true,
        "Expect dnrStore.detectStartupCacheDataChanged to detect a change"
      );
      return true;
    }
    Assert.equal(
      realDetectStartupCacheDataChanged(seenLastUpdateTags),
      false,
      "Expect dnrStore.detectStartupCacheDataChanged to NOT have detected any change"
    );

    Promise.resolve().then(resolvePromiseSaveCacheRescheduled);
    return false;
  });

  extension = ExtensionTestUtils.loadExtension(
    getDNRExtension({
      id: "test-reschedule-save-on-detected-changes@test",
      rule_resources,
      files,
    })
  );

  await extension.startup();
  info(
    "Wait for DNR initialization completed for the permanently installed extension"
  );
  await ExtensionDNR.ensureInitialized(extension.extension);
  info("Wait for the saveCacheDataNow task to have been rescheduled");
  await promiseSaveCacheRescheduled;

  Assert.equal(
    spyScheduleCacheDataSave.callCount,
    2,
    "Expect ExtensionDNRStore scheduleCacheDataSave method to have been called twice"
  );
  Assert.equal(
    stubDetectCacheDataChanges.callCount,
    2,
    "Expect ExtensionDNRStore detectStartupCacheDataChanged method to have been called twice"
  );

  sandboxStore.restore();

  await extension.unload();
});
