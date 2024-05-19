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

const server = createHttpServer({ hosts: ["example.com"] });
server.registerPathHandler("/", (req, res) => {
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.write("response from server");
});

add_setup(async () => {
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
  Services.prefs.setBoolPref("extensions.dnr.enabled", true);
  Services.prefs.setBoolPref("extensions.dnr.feedback", true);

  // NOTE: this test is going to load and validate the maximum amount of static rules
  // that an extension can enable, which on slower builds (in particular in tsan builds,
  // e.g. see Bug 1803801) have a higher chance that the test extension may have hit the
  // idle timeout and being suspended by the time the test is going to trigger API method
  // calls through test API events (which do not expect the lifetime of the event page).
  Services.prefs.setIntPref("extensions.background.idle.timeout", 300_000);

  // NOTE: reduce the static rules limits to reduce the amount of time needed to run
  // this xpcshell test.
  Services.prefs.setIntPref(
    "extensions.dnr.guaranteed_minimum_static_rules",
    30
  );
  Services.prefs.setIntPref("extensions.dnr.max_number_of_static_rulesets", 10);
  Services.prefs.setIntPref(
    "extensions.dnr.max_number_of_enabled_static_rulesets",
    2
  );

  // Sanity-check: confirm the custom limits prefs set are matched by the ExtensionDNRLimits
  // properties.
  const {
    GUARANTEED_MINIMUM_STATIC_RULES,
    MAX_NUMBER_OF_ENABLED_STATIC_RULESETS,
    MAX_NUMBER_OF_STATIC_RULESETS,
  } = ExtensionDNRLimits;
  equal(
    GUARANTEED_MINIMUM_STATIC_RULES,
    30,
    "Expect GUARANTEED_MINIMUM_STATIC_RULES to match the value set on the related pref"
  );
  equal(
    MAX_NUMBER_OF_ENABLED_STATIC_RULESETS,
    2,
    "Expect MAX_NUMBER_OF_ENABLED_STATIC_RULESETS to match the value set on the related pref"
  );
  equal(
    MAX_NUMBER_OF_STATIC_RULESETS,
    10,
    "Expect MAX_NUMBER_OF_STATIC_RULESETS to match the value set on the related pref"
  );

  setupTelemetryForTests();

  await ExtensionTestUtils.startAddonManager();
});

add_task(async function test_getAvailableStaticRulesCountAndLimits() {
  const dnrStore = ExtensionDNRStore._getStoreForTesting();
  const { GUARANTEED_MINIMUM_STATIC_RULES } = ExtensionDNRLimits;
  equal(
    typeof GUARANTEED_MINIMUM_STATIC_RULES,
    "number",
    "Expect GUARANTEED_MINIMUM_STATIC_RULES to be a number"
  );

  const availableStaticRulesCount = GUARANTEED_MINIMUM_STATIC_RULES;

  const rule_resources = [
    {
      id: "ruleset_0",
      path: "/ruleset_0.json",
      enabled: true,
    },
    {
      id: "ruleset_1",
      path: "/ruleset_1.json",
      enabled: true,
    },
    // A ruleset initially disabled (to make sure it doesn't count for the
    // rules count limit).
    {
      id: "ruleset_disabled",
      path: "/ruleset_disabled.json",
      enabled: false,
    },
    // A ruleset including an invalid rule and valid rule.
    {
      id: "ruleset_withInvalid",
      path: "/ruleset_withInvalid.json",
      enabled: false,
    },
    // An empty ruleset (to make sure it can still be enabled/disabled just fine,
    // e.g. in case on some browser version all rules are technically invalid).
    {
      id: "ruleset_empty",
      path: "/ruleset_empty.json",
      enabled: false,
    },
  ];

  const files = {};
  const rules = {};

  const rulesetDisabledData = [getDNRRule({ id: 1 })];
  const ruleValid = getDNRRule({ id: 2, action: { type: "allow" } });
  const rulesetWithInvalidData = [
    getDNRRule({ id: 1, action: { type: "invalid_action" } }),
    ruleValid,
  ];

  rules.ruleset_0 = [getDNRRule({ id: 1 }), getDNRRule({ id: 2 })];

  rules.ruleset_1 = [];
  for (let i = 0; i < availableStaticRulesCount; i++) {
    rules.ruleset_1.push(getDNRRule({ id: i + 1 }));
  }

  for (const [k, v] of Object.entries(rules)) {
    files[`${k}.json`] = JSON.stringify(v);
  }
  files[`ruleset_disabled.json`] = JSON.stringify(rulesetDisabledData);
  files[`ruleset_withInvalid.json`] = JSON.stringify(rulesetWithInvalidData);
  files[`ruleset_empty.json`] = JSON.stringify([]);

  const extension = ExtensionTestUtils.loadExtension(
    getDNRExtension({
      id: "dnr-getAvailable-count-@mochitest",
      rule_resources,
      files,
    })
  );

  await extension.startup();
  await extension.awaitMessage("bgpage:ready");

  async function updateEnabledRulesets({ expectedErrorMessage, ...options }) {
    // Note: options = { disableRulesetIds, enableRulesetIds }
    extension.sendMessage("updateEnabledRulesets", options);
    let [result] = await extension.awaitMessage("updateEnabledRulesets:done");
    if (expectedErrorMessage) {
      Assert.deepEqual(
        result,
        { rejectedWithErrorMessage: expectedErrorMessage },
        "updateEnabledRulesets() should reject with the given error"
      );
    } else {
      Assert.deepEqual(
        result,
        undefined,
        "updateEnabledRulesets() should resolve without error"
      );
    }
  }

  const expectedEnabledRulesets = {};
  expectedEnabledRulesets.ruleset_0 = getSchemaNormalizedRules(
    extension,
    rules.ruleset_0
  );

  info(
    "Expect ruleset_1 to not be enabled because along with ruleset_0 exceeded the static rules count limit"
  );
  await assertDNRStoreData(dnrStore, extension, expectedEnabledRulesets);

  await assertDNRGetAvailableStaticRuleCount(
    extension,
    availableStaticRulesCount - rules.ruleset_0.length,
    "Got the available static rule count on ruleset_0 initially enabled"
  );

  // Try to enable ruleset_1 again from the API method.
  await updateEnabledRulesets({
    enableRulesetIds: ["ruleset_1"],
    expectedErrorMessage: `Number of rules across all enabled static rulesets exceeds GUARANTEED_MINIMUM_STATIC_RULES if ruleset "ruleset_1" were to be enabled.`,
  });

  info(
    "Expect ruleset_1 to not be enabled because still exceeded the static rules count limit"
  );
  await assertDNRGetEnabledRulesets(extension, ["ruleset_0"]);
  await assertDNRStoreData(dnrStore, extension, expectedEnabledRulesets);

  await assertDNRGetAvailableStaticRuleCount(
    extension,
    availableStaticRulesCount - rules.ruleset_0.length,
    "Got the available static rule count on ruleset_0 still the only one enabled"
  );

  await updateEnabledRulesets({
    disableRulesetIds: ["ruleset_0"],
    enableRulesetIds: ["ruleset_1"],
  });

  info("Expect ruleset_1 to be enabled along with disabling ruleset_0");
  await assertDNRGetEnabledRulesets(extension, ["ruleset_1"]);
  delete expectedEnabledRulesets.ruleset_0;
  expectedEnabledRulesets.ruleset_1 = getSchemaNormalizedRules(
    extension,
    rules.ruleset_1
  );
  await assertDNRStoreData(dnrStore, extension, expectedEnabledRulesets, {
    // Assert total amount of expected rules and only the first and last rule
    // individually, to avoid generating a huge amount of logs and potential
    // timeout failures on slower builds.
    assertIndividualRules: false,
  });

  await assertDNRGetAvailableStaticRuleCount(
    extension,
    0,
    "Expect no additional static rules count available when ruleset_1 is enabled"
  );

  info(
    "Expect ruleset_disabled to stay disabled because along with ruleset_1 exceeeds the limits"
  );
  await updateEnabledRulesets({
    enableRulesetIds: ["ruleset_disabled"],
    expectedErrorMessage: `Number of rules across all enabled static rulesets exceeds GUARANTEED_MINIMUM_STATIC_RULES if ruleset "ruleset_disabled" were to be enabled.`,
  });
  await assertDNRGetEnabledRulesets(extension, ["ruleset_1"]);
  await assertDNRStoreData(dnrStore, extension, expectedEnabledRulesets, {
    // Assert total amount of expected rules and only the first and last rule
    // individually, to avoid generating a huge amount of logs and potential
    // timeout failures on slower builds.
    assertIndividualRules: false,
  });
  await assertDNRGetAvailableStaticRuleCount(
    extension,
    0,
    "Expect no additional static rules count available"
  );

  info("Expect ruleset_empty to be enabled despite having reached the limit");
  await updateEnabledRulesets({
    enableRulesetIds: ["ruleset_empty"],
  });
  await assertDNRGetEnabledRulesets(extension, ["ruleset_1", "ruleset_empty"]);
  await assertDNRStoreData(
    dnrStore,
    extension,
    {
      ...expectedEnabledRulesets,
      ruleset_empty: [],
    },
    // Assert total amount of expected rules and only the first and last rule
    // individually, to avoid generating a huge amount of logs and potential
    // timeout failures on slower builds.
    { assertIndividualRules: false }
  );
  await assertDNRGetAvailableStaticRuleCount(
    extension,
    0,
    "Expect no additional static rules count available"
  );

  info("Expect invalid rules to not be counted towards the limits");
  await updateEnabledRulesets({
    disableRulesetIds: ["ruleset_1", "ruleset_empty"],
    enableRulesetIds: ["ruleset_withInvalid"],
  });
  await assertDNRGetEnabledRulesets(extension, ["ruleset_withInvalid"]);
  await assertDNRStoreData(dnrStore, extension, {
    // Only the valid rule has been actually loaded, and the invalid one
    // ignored.
    ruleset_withInvalid: [ruleValid],
  });
  await assertDNRGetAvailableStaticRuleCount(
    extension,
    availableStaticRulesCount - 1,
    "Expect only valid rules to be counted"
  );

  await extension.unload();

  Services.prefs.clearUserPref("extensions.background.idle.enabled");
});

add_task(async function test_static_rulesets_limits() {
  const dnrStore = ExtensionDNRStore._getStoreForTesting();

  const getRulesetManifestData = (rulesetNumber, enabled) => {
    return {
      id: `ruleset_${rulesetNumber}`,
      enabled,
      path: `ruleset_${rulesetNumber}.json`,
    };
  };
  const {
    MAX_NUMBER_OF_STATIC_RULESETS,
    MAX_NUMBER_OF_ENABLED_STATIC_RULESETS,
  } = ExtensionDNRLimits;

  equal(
    typeof MAX_NUMBER_OF_STATIC_RULESETS,
    "number",
    "Expect MAX_NUMBER_OF_STATIC_RULESETS to be a number"
  );
  equal(
    typeof MAX_NUMBER_OF_ENABLED_STATIC_RULESETS,
    "number",
    "Expect MAX_NUMBER_OF_ENABLED_STATIC_RULESETS to be a number"
  );
  Assert.greater(
    MAX_NUMBER_OF_STATIC_RULESETS,
    MAX_NUMBER_OF_ENABLED_STATIC_RULESETS,
    "Expect MAX_NUMBER_OF_STATIC_RULESETS to be greater"
  );

  const rules = [getDNRRule()];

  const rule_resources = [];
  const files = {};
  for (let i = 0; i < MAX_NUMBER_OF_STATIC_RULESETS + 1; i++) {
    const enabled = i < MAX_NUMBER_OF_ENABLED_STATIC_RULESETS + 1;
    files[`ruleset_${i}.json`] = JSON.stringify(rules);
    rule_resources.push(getRulesetManifestData(i, enabled));
  }

  let extension = ExtensionTestUtils.loadExtension(
    getDNRExtension({
      rule_resources,
      files,
    })
  );

  const expectedEnabledRulesets = {};

  const { messages } = await AddonTestUtils.promiseConsoleOutput(async () => {
    ExtensionTestUtils.failOnSchemaWarnings(false);
    await extension.startup();
    ExtensionTestUtils.failOnSchemaWarnings(true);
    await extension.awaitMessage("bgpage:ready");

    for (let i = 0; i < MAX_NUMBER_OF_ENABLED_STATIC_RULESETS; i++) {
      expectedEnabledRulesets[`ruleset_${i}`] = getSchemaNormalizedRules(
        extension,
        rules
      );
    }

    await assertDNRGetEnabledRulesets(
      extension,
      Array.from(Object.keys(expectedEnabledRulesets))
    );
    await assertDNRStoreData(dnrStore, extension, expectedEnabledRulesets);
  });

  AddonTestUtils.checkMessages(messages, {
    expected: [
      // Warnings emitted from the manifest schema validation.
      {
        message:
          /declarative_net_request: Static rulesets are exceeding the MAX_NUMBER_OF_STATIC_RULESETS limit/,
      },
      {
        message:
          /declarative_net_request: Enabled static rulesets are exceeding the MAX_NUMBER_OF_ENABLED_STATIC_RULESETS limit .* "ruleset_2"/,
      },
      // Error reported on the browser console as part of loading enabled rulesets)
      // on enabled rulesets being ignored because exceeding the limit.
      {
        message:
          /Ignoring enabled static ruleset exceeding the MAX_NUMBER_OF_ENABLED_STATIC_RULESETS .* "ruleset_2"/,
      },
    ],
  });

  info(
    "Verify updateEnabledRulesets reject when the request is exceeding the enabled rulesets count limit"
  );
  extension.sendMessage("updateEnabledRulesets", {
    disableRulesetIds: ["ruleset_0"],
    enableRulesetIds: ["ruleset_2", "ruleset_3"],
  });

  await Assert.rejects(
    extension.awaitMessage("updateEnabledRulesets:done").then(results => {
      if (results[0].rejectedWithErrorMessage) {
        return Promise.reject(new Error(results[0].rejectedWithErrorMessage));
      }
      return results[0];
    }),
    /updatedEnabledRulesets request is exceeding MAX_NUMBER_OF_ENABLED_STATIC_RULESETS/,
    "Expected rejection on updateEnabledRulesets exceeting enabled rulesets count limit"
  );

  // Confirm that the expected rulesets didn't change neither.
  await assertDNRGetEnabledRulesets(
    extension,
    Array.from(Object.keys(expectedEnabledRulesets))
  );
  await assertDNRStoreData(dnrStore, extension, expectedEnabledRulesets);

  info(
    "Verify updateEnabledRulesets applies the expected changes when resolves successfully"
  );
  extension.sendMessage(
    "updateEnabledRulesets",
    {
      disableRulesetIds: ["ruleset_0"],
      enableRulesetIds: ["ruleset_2"],
    },
    {
      disableRulesetIds: ["ruleset_2"],
      enableRulesetIds: ["ruleset_3"],
    }
  );
  await extension.awaitMessage("updateEnabledRulesets:done");

  // Expect ruleset_0 disabled, ruleset_2 to be enabled but then disabled by the
  // second update queued after the first one, and ruleset_3 to be enabled.
  delete expectedEnabledRulesets.ruleset_0;
  expectedEnabledRulesets.ruleset_3 = getSchemaNormalizedRules(
    extension,
    rules
  );

  await assertDNRGetEnabledRulesets(
    extension,
    Array.from(Object.keys(expectedEnabledRulesets))
  );
  await assertDNRStoreData(dnrStore, extension, expectedEnabledRulesets);

  // Ensure all changes were stored and reloaded from disk store and the
  // DNR store update queue can accept new updates.
  info("Verify static rules load and updates after extension is restarted");

  // NOTE: promiseRestartManager will not be enough to make sure the
  // DNR store data for the test extension is going to be loaded from
  // the DNR startup cache file.
  // See test_ext_dnr_startup_cache.js for a test case that more completely
  // simulates ExtensionDNRStore initialization on browser restart.
  await AddonTestUtils.promiseRestartManager();

  await extension.awaitStartup();
  await extension.awaitMessage("bgpage:ready");
  await assertDNRGetEnabledRulesets(
    extension,
    Array.from(Object.keys(expectedEnabledRulesets))
  );
  await assertDNRStoreData(dnrStore, extension, expectedEnabledRulesets);

  extension.sendMessage("updateEnabledRulesets", {
    disableRulesetIds: ["ruleset_3"],
  });
  await extension.awaitMessage("updateEnabledRulesets:done");
  delete expectedEnabledRulesets.ruleset_3;
  await assertDNRGetEnabledRulesets(
    extension,
    Array.from(Object.keys(expectedEnabledRulesets))
  );
  await assertDNRStoreData(dnrStore, extension, expectedEnabledRulesets);

  await extension.unload();
});
