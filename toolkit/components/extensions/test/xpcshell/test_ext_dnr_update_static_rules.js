/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ExtensionDNRLimits: "resource://gre/modules/ExtensionDNRLimits.sys.mjs",
  ExtensionDNRStore: "resource://gre/modules/ExtensionDNRStore.sys.mjs",
});

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

Services.scriptloader.loadSubScript(
  Services.io.newFileURI(do_get_file("head_dnr.js")).spec,
  this
);
Services.scriptloader.loadSubScript(
  Services.io.newFileURI(do_get_file("head_dnr_static_rules.js")).spec,
  this
);

add_setup(async () => {
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
  Services.prefs.setBoolPref("extensions.dnr.enabled", true);
  Services.prefs.setBoolPref("extensions.dnr.feedback", true);

  setupTelemetryForTests();

  await ExtensionTestUtils.startAddonManager();
});

add_task(async function test_update_individual_static_rules() {
  resetTelemetryData();

  const ruleset1 = [
    getDNRRule({
      id: 1,
      action: { type: "block" },
      condition: {
        resourceTypes: ["xmlhttprequest"],
        requestDomains: ["example.com"],
      },
    }),
  ];
  const ruleset2 = [
    getDNRRule({
      id: 1,
      action: { type: "block" },
      condition: {
        resourceTypes: ["xmlhttprequest"],
        requestDomains: ["example.org"],
      },
    }),
    getDNRRule({
      id: 2,
      action: { type: "block" },
      condition: {
        resourceTypes: ["xmlhttprequest"],
        requestDomains: ["example2.org"],
      },
    }),
  ];

  const rule_resources = [
    {
      id: "ruleset1",
      enabled: false,
      path: "ruleset1.json",
    },
    {
      id: "ruleset2",
      enabled: true,
      path: "ruleset2.json",
    },
  ];

  const files = {
    "ruleset1.json": JSON.stringify(ruleset1),
    "ruleset2.json": JSON.stringify(ruleset2),
  };

  const extension = ExtensionTestUtils.loadExtension(
    getDNRExtension({
      id: "update-individual-static-rules@xpcshell",
      rule_resources,
      files,
    })
  );

  await extension.startup();
  await extension.awaitMessage("bgpage:ready");

  const { GUARANTEED_MINIMUM_STATIC_RULES } = ExtensionDNRLimits;
  let expectedCount = GUARANTEED_MINIMUM_STATIC_RULES - ruleset2.length;
  await assertDNRGetEnabledRulesets(extension, ["ruleset2"]);
  await assertDNRGetAvailableStaticRuleCount(extension, expectedCount);

  // Sanity-check
  await assertDNRTestMatchOutcome(
    {
      extension,
      testRequest: { url: "https://example.org", type: "xmlhttprequest" },
      expected: {
        matchedRules: [{ ruleId: 1, rulesetId: "ruleset2" }],
      },
    },
    "Expect rule from ruleset2 to be matched on the test request"
  );

  info("Test errors on invalid ruleset ids");
  extension.sendMessage("getDisabledRuleIds", {
    rulesetId: "non-existing-ruleset",
  });
  Assert.deepEqual(
    await extension.awaitMessage("getDisabledRuleIds:done"),
    { rejectedWithErrorMessage: `Invalid ruleset id: "non-existing-ruleset"` },
    "Got the expected error rejected by getDisableRuleIds"
  );
  extension.sendMessage("updateStaticRules", {
    rulesetId: "non-existing-ruleset",
    disableRuleIds: [1],
  });
  Assert.deepEqual(
    await extension.awaitMessage("updateStaticRules:done"),
    [
      {
        rejectedWithErrorMessage: `Invalid ruleset id: "non-existing-ruleset"`,
      },
    ],
    "Got the expected error rejected by updateStaticRules"
  );

  info("Test disabling ruleId on enabled static ruleset");
  await updateStaticRules(extension, {
    rulesetId: "ruleset2",
    disableRuleIds: [ruleset2[0].id, ruleset2[1].id, 999],
    enableRuleIds: [ruleset2[1].id, 111],
  });

  await assertDNRGetDisabledRuleIds(extension, { rulesetId: "ruleset2" }, [
    ruleset2[0].id,
    // Non-existing ruleset id are still added to the disabled rule ids
    // in Chrome and so we also match the same behavior in Firefox too.
    999,
  ]);

  // Disabled rule shouldn't change the quota.
  await assertDNRGetAvailableStaticRuleCount(extension, expectedCount);

  await assertDNRTestMatchOutcome(
    {
      extension,
      testRequest: { url: "https://example.org", type: "xmlhttprequest" },
      expected: {
        matchedRules: [],
      },
    },
    "Expect disabled rule from ruleset2 to not be matched on the test request"
  );

  info(
    "Test enabling and disabling the same ruleId leaves the disabled rules id unchanged"
  );
  await updateStaticRules(extension, {
    rulesetId: "ruleset2",
    disableRuleIds: [999],
    // The number of times a rule id is specified does not matter,
    // the behavior is still the same as if the rule id was specified
    // only once (enabling + disabling the same rule id will still
    // disable it).
    enableRuleIds: [999, 999, 999],
  });
  await assertDNRGetDisabledRuleIds(extension, { rulesetId: "ruleset2" }, [
    ruleset2[0].id,
    999,
  ]);
  await assertDNRGetAvailableStaticRuleCount(extension, expectedCount);

  info("Test disabling ruleId on disabled static ruleset");
  await updateStaticRules(extension, {
    rulesetId: "ruleset1",
    disableRuleIds: [666, ruleset1[0].id],
  });
  await assertDNRGetDisabledRuleIds(extension, { rulesetId: "ruleset2" }, [
    ruleset2[0].id,
    999,
  ]);
  await assertDNRGetDisabledRuleIds(extension, { rulesetId: "ruleset1" }, [
    666,
    ruleset1[0].id,
  ]);
  await assertDNRGetAvailableStaticRuleCount(extension, expectedCount);

  info("Enable static ruleset1");
  expectedCount =
    GUARANTEED_MINIMUM_STATIC_RULES - ruleset1.length - ruleset2.length;

  await updateEnabledRulesets(extension, {
    enableRulesetIds: ["ruleset1"],
  });
  await assertDNRGetEnabledRulesets(extension, ["ruleset1", "ruleset2"]);
  await assertDNRGetAvailableStaticRuleCount(extension, expectedCount);

  await assertDNRTestMatchOutcome(
    {
      extension,
      testRequest: { url: "https://example.com", type: "xmlhttprequest" },
      expected: {
        matchedRules: [],
      },
    },
    "Expect disabled rule from ruleset1 to not be matched on the test request"
  );

  await updateStaticRules(extension, {
    rulesetId: "ruleset1",
    enableRuleIds: [ruleset1[0].id],
  });
  await assertDNRGetAvailableStaticRuleCount(extension, expectedCount);

  await assertDNRTestMatchOutcome(
    {
      extension,
      testRequest: { url: "https://example.com", type: "xmlhttprequest" },
      expected: {
        matchedRules: [{ ruleId: 1, rulesetId: "ruleset1" }],
      },
    },
    "Expect re-enabled rule from ruleset1 to be matched on the test request"
  );

  info("Test disabled rules after AOM restart");

  // Make sure the DNR data is stored on disk.
  let dnrStore = ExtensionDNRStore._getStoreForTesting();
  await dnrStore.waitSaveCacheDataForTesting();

  await AddonTestUtils.promiseShutdownManager();

  // Drop the DNRStore cache file to ensure we will have to load
  // back the DNRStore data from the JSONFile.
  const { cacheFile } = dnrStore.getFilePaths(extension.uuid);
  ok(
    await IOUtils.exists(cacheFile),
    `Expect a DNRStore cache file found at ${cacheFile}`
  );
  await IOUtils.remove(cacheFile);
  ok(
    !(await IOUtils.exists(cacheFile)),
    `Expect a DNRStore cache file ${cacheFile} to be removed`
  );

  // Recreate a new DNR store to clear the data still cached in memory.
  ExtensionDNRStore._recreateStoreForTesting();

  await AddonTestUtils.promiseStartupManager();

  await extension.awaitStartup();
  await extension.awaitMessage("bgpage:ready");

  info("Test disabled rules has been restored from the stored data");
  await assertDNRGetEnabledRulesets(extension, ["ruleset1", "ruleset2"]);
  await assertDNRGetDisabledRuleIds(extension, { rulesetId: "ruleset1" }, [
    666,
  ]);
  await assertDNRGetDisabledRuleIds(extension, { rulesetId: "ruleset2" }, [
    ruleset2[0].id,
    999,
  ]);
  await assertDNRGetAvailableStaticRuleCount(extension, expectedCount);
  await assertDNRTestMatchOutcome(
    {
      extension,
      testRequest: { url: "https://example.com", type: "xmlhttprequest" },
      expected: {
        matchedRules: [{ ruleId: 1, rulesetId: "ruleset1" }],
      },
    },
    "Expect re-enabled rule from ruleset1 to be matched on the test request"
  );

  info("Test disabled rule ids are dropped on addon updates to a new version");
  dnrStore = ExtensionDNRStore._getStoreForTesting();
  await dnrStore.save(extension.extension);

  const { storeFile } = dnrStore.getFilePaths(extension.uuid);
  ok(await IOUtils.exists(storeFile), `DNR storeFile ${storeFile} found`);
  let storedData = await IOUtils.readJSON(storeFile, {
    decompress: true,
  });
  Assert.equal(
    storedData.schemaVersion,
    ExtensionDNRStore.SCHEMA_VERSION,
    "Got the expected schemaVersion"
  );
  Assert.deepEqual(
    storedData.disabledStaticRuleIds,
    {
      ruleset1: [666],
      ruleset2: [1, 999],
    },
    "Got the expected disableStaticRuleIds data stored"
  );

  await extension.upgrade(
    getDNRExtension({
      id: "update-individual-static-rules@xpcshell",
      version: "3.0",
      rule_resources,
      files,
    })
  );
  await extension.awaitMessage("bgpage:ready");

  await assertDNRGetEnabledRulesets(extension, ["ruleset2"]);
  await assertDNRGetDisabledRuleIds(extension, { rulesetId: "ruleset1" }, []);
  await assertDNRGetDisabledRuleIds(extension, { rulesetId: "ruleset2" }, []);

  await dnrStore.save(extension.extension);
  storedData = await IOUtils.readJSON(storeFile, {
    decompress: true,
  });
  Assert.equal(storedData.extVersion, "3.0", "Got the expected extVersion");
  // Expect disabledStaticRuleIds to be omitted if there are no disabled
  // rules (similarly to staticRulesets expected to be omitted if there
  // are no static rulesets enabled).
  Assert.deepEqual(
    storedData.disabledStaticRuleIds,
    undefined,
    "Got an undefined disableStaticRuleIds property as expected"
  );

  await extension.unload();
});
