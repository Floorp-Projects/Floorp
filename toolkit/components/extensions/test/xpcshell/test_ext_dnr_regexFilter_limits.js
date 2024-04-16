"use strict";

// This file verifies the quota on regexFilter rules for all ruleset.
//
// The generic rule limits (not specific to regexFilter) are covered elsewhere:
// - session_rules_total_rule_limit in test_ext_dnr_session_rules.js
// - test_dynamic_rules_count_limits in test_ext_dnr_dynamic_rules.js
//   (also checks that the quota of session and dynamic rules are separate.)
// - test_getAvailableStaticRulesCountAndLimits and test_static_rulesets_limits
//   in test_ext_dnr_static_rules.js.

ChromeUtils.defineESModuleGetters(this, {
  ExtensionDNRLimits: "resource://gre/modules/ExtensionDNRLimits.sys.mjs",
});

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

add_setup(async () => {
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
  Services.prefs.setBoolPref("extensions.dnr.enabled", true);
  Services.prefs.setBoolPref("extensions.dnr.feedback", true);

  // We want to install add-ons through the add-on manager in order to be able
  // to disable / re-enable the add-on.
  await ExtensionTestUtils.startAddonManager();
});

// Create an extension composed of the given test cases, and start or reload
// the extension before each test case.
//
// testCases is an array of:
// - name - unique name describing purpose of test
// - setup - optional function run before (re-)enabling the extension.
// - backgroundFn - logic to run in the extension's background.
// - checkConsoleMessages - function to run to verify the console messages
//   collected between extension (re)startup and the execution of backgroundFn.
//
// extensionDataTemplate should be a value for ExtensionTestUtils.loadExtension,
// without the background key.
async function startOrReloadExtensionForEach(testCases, extensionDataTemplate) {
  for (let testCase of testCases) {
    // Verify that the keys are supported, so that the test does not pass
    // trivially because of a typo or something.
    let okKeys = ["name", "setup", "backgroundFn", "checkConsoleMessages"];
    let keys = Object.keys(testCase).filter(k => !okKeys.includes(k));
    if (keys.length) {
      throw new Error(`Unexpected key in testCase ${testCase.name}: ${keys}`);
    }
  }
  if (extensionDataTemplate.background) {
    // background is generated here, so the template should not specify it.
    throw new Error("extensionDataTemplate.background should not be set");
  }
  function background(testCases) {
    browser.test.onMessage.addListener(async testName => {
      try {
        browser.test.log(`Starting backgroundFn for ${testName}`);
        await testCases.find(({ name }) => name === testName).backgroundFn();
      } catch (e) {
        browser.test.fail(`Unexpected error for ${testName}: ${e}`);
      }
      browser.test.log(`Completed backgroundFn for ${testName}`);
      browser.test.sendMessage(`${testName}:done`);
    });
    browser.test.sendMessage("background_started");
  }

  const serializedTestCases = testCases.map(
    ({ name, backgroundFn }) => `{name:"${name}",backgroundFn:${backgroundFn}}`
  );
  let extension = ExtensionTestUtils.loadExtension({
    ...extensionDataTemplate,
    background: `(${background})([${serializedTestCases.join(",")}])`,
  });

  for (let [i, { name, setup, checkConsoleMessages }] of testCases.entries()) {
    info(`Running test case: ${name}`);
    await setup?.();

    let { messages } = await promiseConsoleOutput(async () => {
      if (i === 0) {
        await extension.startup();
      } else {
        await extension.addon.enable();
      }
      await extension.awaitMessage("background_started");

      // DNR rule loading errors should be emitted at startup. But since the
      // rule loading is async and not blocking background startup, we need to
      // roundtrip through the DNR API before we can verify the error message.
      extension.sendMessage(name);
      await extension.awaitMessage(`${name}:done`);
    });

    checkConsoleMessages(name, messages);

    if (i === testCases.length - 1) {
      await extension.unload();
    } else {
      await extension.addon.disable();
    }
    info(`Completed test case: ${name}`);
  }
}

// Create the extensionDataTemplate value (without "background" key!) for use
// with ExtensionTestUtils.loadExtension, through startOrReloadExtensionForEach.
function makeExtensionDataTemplate({ manifest, files }) {
  return {
    // Note: no "background" key because startOrReloadExtensionForEach adds it.
    useAddonManager: "permanent",
    manifest: {
      manifest_version: 3,
      permissions: ["declarativeNetRequest", "declarativeNetRequestFeedback"],
      browser_specific_settings: { gecko: { id: "dnr@ext" } },
      ...manifest,
    },
    files,
  };
}

function genStaticRules(count) {
  let rules = [];
  for (let i = 1; i <= count; ++i) {
    rules.push({
      id: i,
      condition: { regexFilter: `prefix_${i}_suffix` },
      action: { type: "block" },
    });
  }
  return JSON.stringify(rules);
}

add_task(async function session_and_dynamic_regexFilter_limit() {
  let extensionDataTemplate = makeExtensionDataTemplate({});

  // Note: Every testPart* function will be serialized and be part of the test
  // extension's background script.

  async function testPart1_session_and_dynamic_quota() {
    let rules = [];
    const { MAX_NUMBER_OF_REGEX_RULES } = browser.declarativeNetRequest;
    for (let i = 1; i <= MAX_NUMBER_OF_REGEX_RULES; ++i) {
      rules.push({
        id: i,
        condition: { regexFilter: `prefix_${i}_suffix` },
        action: { type: "block" },
      });
    }
    const lastRuleId = rules[rules.length - 1].id;

    browser.test.log(`Adding ${rules.length} regex rules (dynamic)`);
    await browser.declarativeNetRequest.updateDynamicRules({
      addRules: rules,
    });

    browser.test.assertDeepEq(
      { matchedRules: [{ ruleId: lastRuleId, rulesetId: "_dynamic" }] },
      await browser.declarativeNetRequest.testMatchOutcome({
        url: `http://example.com/prefix_${lastRuleId}_suffix`,
        type: "other",
      }),
      "Expected last regexFilter to match the request"
    );

    // Dynamic and session rules should have a separate quota for regexFilter.
    browser.test.log(`Adding ${rules.length} regex rules (session)`);
    await browser.declarativeNetRequest.updateSessionRules({
      addRules: rules,
    });

    browser.test.assertDeepEq(
      { matchedRules: [{ ruleId: lastRuleId, rulesetId: "_session" }] },
      await browser.declarativeNetRequest.testMatchOutcome({
        url: `http://example.com/prefix_${lastRuleId}_suffix`,
        type: "other",
      }),
      "Expected registered regexFilter to match"
    );

    // Now we should not be able to add another one.
    const newRule = {
      id: lastRuleId + 1,
      condition: { regexFilter: "." },
      action: { type: "block" },
    };
    await browser.test.assertRejects(
      browser.declarativeNetRequest.updateSessionRules({ addRules: [newRule] }),
      `Number of regexFilter rules in ruleset "_session" exceeds MAX_NUMBER_OF_REGEX_RULES.`,
      "Should not allow regexFilter over quota for session ruleset"
    );
    await browser.test.assertRejects(
      browser.declarativeNetRequest.updateDynamicRules({ addRules: [newRule] }),
      `Number of regexFilter rules in ruleset "_dynamic" exceeds MAX_NUMBER_OF_REGEX_RULES.`,
      "Should not allow regexFilter over quota for dynamic ruleset"
    );
  }

  async function testPart2_after_reload() {
    browser.test.assertEq(
      0,
      (await browser.declarativeNetRequest.getSessionRules()).length,
      "Session rules gone after restart"
    );
    let rules = await browser.declarativeNetRequest.getDynamicRules();
    browser.test.assertEq(
      browser.declarativeNetRequest.MAX_NUMBER_OF_REGEX_RULES,
      rules.length,
      "Dynamic regexFilter rules still there after restart"
    );

    // This confirms that the quota for session rules is not somehow persisted
    // somewhere else.
    browser.test.log(`Verifying that we can add ${rules.length} rules again.`);
    await browser.declarativeNetRequest.updateSessionRules({
      addRules: rules,
    });
  }

  async function testPart3_too_many_regexFilters_stored_after_lowering_quota() {
    browser.test.assertEq(
      0,
      (await browser.declarativeNetRequest.getDynamicRules()).length,
      "Ignore all stored dynamic rules when regexFilter quota is exceeded"
    );
  }

  async function testPart4_reload_after_quota_back() {
    // Implementation detail: while the in-memory representation of the
    // dynamic rules has been wiped at the previous extension load, the disk
    // representation did not change because we only read the dynamic rules
    // without anything else triggering a save request.
    //
    // Therefore, when the limit was somehow restored, the on-disk data is
    // now considered valid again.
    browser.test.assertEq(
      browser.declarativeNetRequest.MAX_NUMBER_OF_REGEX_RULES,
      (await browser.declarativeNetRequest.getDynamicRules()).length,
      "On-disk dynamic rules accepted when regexFilter quota is not exceeded"
    );
  }

  // Expected warning in console when there are too many regexFilter rules in
  // the dynamic ruleset data on disk.
  const errorMsg = `Ignoring dynamic ruleset in extension "dnr@ext" because: Number of regexFilter rules in ruleset "_dynamic" exceeds MAX_NUMBER_OF_REGEX_RULES.`;
  function expectError(testName, messages) {
    Assert.equal(
      messages.filter(m => m.message.includes(errorMsg)).length,
      1,
      `${testName} should trigger the following error in the log: ${errorMsg}`
    );
  }
  function noErrors(testName, messages) {
    Assert.equal(
      messages.filter(m => m.message.includes(errorMsg)).length,
      0,
      `${testName} should not trigger any logged errors`
    );
  }

  const testCases = [
    {
      name: "testPart1_session_and_dynamic_quota",
      backgroundFn: testPart1_session_and_dynamic_quota,
      checkConsoleMessages: noErrors,
    },
    {
      name: "testPart2_after_reload",
      backgroundFn: testPart2_after_reload,
      checkConsoleMessages: noErrors,
    },
    {
      name: "testPart3_too_many_regexFilters_stored_after_lowering_quota",
      setup() {
        // Artificially decrease the max number of allowed regexFilter rules,
        // so that whatever that was stored on disk is no longer within quota.
        Services.prefs.setIntPref(
          "extensions.dnr.max_number_of_regex_rules",
          1
        );
      },
      backgroundFn: testPart3_too_many_regexFilters_stored_after_lowering_quota,
      checkConsoleMessages: expectError,
    },
    {
      name: "testPart4_reload_after_quota_back",
      setup() {
        // Restore the original quota after it was lowered in testPart3.
        Services.prefs.clearUserPref(
          "extensions.dnr.max_number_of_regex_rules"
        );
      },
      backgroundFn: testPart4_reload_after_quota_back,
      checkConsoleMessages: noErrors,
    },
  ];

  await startOrReloadExtensionForEach(testCases, extensionDataTemplate);
});

add_task(async function static_regexFilter_limit() {
  const { MAX_NUMBER_OF_REGEX_RULES } = ExtensionDNRLimits;

  let extensionDataTemplate = makeExtensionDataTemplate({
    manifest: {
      declarative_net_request: {
        rule_resources: [
          // limit_plus_1 is over quota, but the other rules should be loaded
          // if possible.
          { id: "limit_plus_1", path: "limit_plus_1.json", enabled: true },
          { id: "just_one", path: "just_one.json", enabled: true },
          { id: "just_two", path: "just_two.json", enabled: true },
          { id: "limit_minus_2", path: "limit_minus_2.json", enabled: true },
          { id: "limit_minus_1", path: "limit_minus_1.json", enabled: false },
        ],
      },
    },
    files: {
      "limit_plus_1.json": genStaticRules(MAX_NUMBER_OF_REGEX_RULES + 1),
      "just_one.json": genStaticRules(1),
      "just_two.json": genStaticRules(2),
      "limit_minus_2.json": genStaticRules(MAX_NUMBER_OF_REGEX_RULES - 2),
      "limit_minus_1.json": genStaticRules(MAX_NUMBER_OF_REGEX_RULES - 1),
    },
  });

  // Note: Every testPart* function will be serialized and be part of the test
  // extension's background script.

  async function testPart1_start_over_static_quota() {
    browser.test.assertDeepEq(
      ["just_one", "just_two"],
      await browser.declarativeNetRequest.getEnabledRulesets(),
      "Should only load rules that fit in the quota for static rules"
    );
  }

  async function testPart2_after_reload() {
    // Should still be the same.
    browser.test.assertDeepEq(
      ["just_one", "just_two"],
      await browser.declarativeNetRequest.getEnabledRulesets(),
      "Should only load rules that fit in the quota for static rules (again)"
    );

    await browser.declarativeNetRequest.updateEnabledRulesets({
      disableRulesetIds: ["just_one"],
    });

    browser.test.assertDeepEq(
      ["just_two"],
      await browser.declarativeNetRequest.getEnabledRulesets(),
      "After disabling 'just_one', there should only be one enabled ruleset"
    );
  }

  async function testPart3_after_updateEnabledRulesets_within_limit() {
    browser.test.assertDeepEq(
      ["just_two"],
      await browser.declarativeNetRequest.getEnabledRulesets(),
      "limit_minus_2 should still be disabled because of a previous updateEnabledRulesets call"
    );

    // This should succeed, as there is now enough space.
    await browser.declarativeNetRequest.updateEnabledRulesets({
      enableRulesetIds: ["limit_minus_2"],
    });
    browser.test.assertDeepEq(
      ["just_two", "limit_minus_2"],
      await browser.declarativeNetRequest.getEnabledRulesets(),
      "limit_minus_2 should be enabled by updateEnabledRulesets"
    );

    await browser.test.assertRejects(
      browser.declarativeNetRequest.updateEnabledRulesets({
        enableRulesetIds: ["just_one"],
      }),
      `Number of regexFilter rules across all enabled static rulesets exceeds MAX_NUMBER_OF_REGEX_RULES if ruleset "just_one" were to be enabled.`,
      "Should not be able to enable just_one because limit was reached"
    );
  }

  async function testPart4_toggling_rulesets_at_quota() {
    browser.test.assertDeepEq(
      ["just_two", "limit_minus_2"],
      await browser.declarativeNetRequest.getEnabledRulesets(),
      "Should have the two rulesets occupying all quota from the previous run"
    );

    await browser.declarativeNetRequest.updateEnabledRulesets({
      disableRulesetIds: ["just_two"],
      enableRulesetIds: ["just_one"],
    });
    browser.test.assertDeepEq(
      ["just_one", "limit_minus_2"],
      await browser.declarativeNetRequest.getEnabledRulesets(),
      "Should be able to replace a ruleset as long as the result is within quota"
    );

    // Try to enable just_one + just_two (+existing limit_minus_2).
    await browser.test.assertRejects(
      browser.declarativeNetRequest.updateEnabledRulesets({
        disableRulesetIds: ["just_one"],
        enableRulesetIds: ["just_one", "just_two"],
      }),
      `Number of regexFilter rules across all enabled static rulesets exceeds MAX_NUMBER_OF_REGEX_RULES if ruleset "just_two" were to be enabled.`,
      "Should reject updateEnabledRulesets that would exceed the quota by 1"
    );
    browser.test.assertDeepEq(
      ["just_one", "limit_minus_2"],
      await browser.declarativeNetRequest.getEnabledRulesets(),
      "Rulesets should not have changed due to rejection"
    );
  }

  async function testPart5_after_doubling_quota() {
    browser.test.assertDeepEq(
      ["just_one", "limit_minus_2"],
      await browser.declarativeNetRequest.getEnabledRulesets(),
      "Initial ruleset not changed after bumping the quota"
    );
    // Explicitly disable before re-enabling them all to see whether the order
    // of passed-in rulesets has any impact on the evaluation order at startup.
    await browser.declarativeNetRequest.updateEnabledRulesets({
      disableRulesetIds: ["limit_minus_2", "just_one"],
    });
    await browser.declarativeNetRequest.updateEnabledRulesets({
      enableRulesetIds: [
        "limit_minus_2",
        "just_two",
        "just_one",
        "limit_minus_1",
      ],
    });
    browser.test.assertDeepEq(
      ["just_one", "just_two", "limit_minus_2", "limit_minus_1"],
      await browser.declarativeNetRequest.getEnabledRulesets(),
      "All rulesets within new quota - ruleset order should match manifest order"
    );
  }

  async function testPart6_after_restoring_original_quota_half() {
    browser.test.assertDeepEq(
      ["just_one", "just_two"],
      await browser.declarativeNetRequest.getEnabledRulesets(),
      "Should have trimmed excess rules in the manifest order"
    );
  }

  const errorMsgPattern =
    /Ignoring static ruleset "([^"]+)" in extension "dnr@ext" because: Number of regexFilter rules across all enabled static rulesets exceeds MAX_NUMBER_OF_REGEX_RULES if ruleset "\1" were to be enabled./;
  function checkFailedRulesets(testName, messages, rulesetIds) {
    let actualRulesetIds = messages
      .map(m => errorMsgPattern.exec(m.message)?.[1])
      .filter(Boolean);
    Assert.deepEqual(
      rulesetIds,
      actualRulesetIds,
      `${testName} should only trigger errors for rejected rulesets at start`
    );
  }

  const testCases = [
    {
      name: "testPart1_start_over_static_quota",
      backgroundFn: testPart1_start_over_static_quota,
      checkConsoleMessages: (n, m) =>
        checkFailedRulesets(n, m, ["limit_plus_1", "limit_minus_2"]),
    },
    {
      name: "testPart2_after_reload",
      backgroundFn: testPart2_after_reload,
      // The extension has not called updateEnabledRulesets, so the "enabled"
      // state of limit_minus_2 from manifest.json is still the extension's
      // desired state for the ruleset. When the browser thus tries to enable
      // the ruleset, it should encounter the same error as before.
      //
      // An alternative would be for the latest understanding of "enabled" to
      // be persisted to disk and used when we load the persisted ruleset state.
      // But if we do that, then we would not be able to distinguish "disabled
      // because of a browser limit" from "disabled by extension". And if we
      // cannot do that, then we would not be able to enable rulesets from
      // already-installed extensions if we were to bump the limits in a browser
      // update.
      //
      // Note: even if caching is implemented (bug 1803365), the observed
      // behavior should happen, because the cache is cleared when we disable
      // the extension.
      checkConsoleMessages: (n, m) =>
        checkFailedRulesets(n, m, ["limit_plus_1", "limit_minus_2"]),
    },
    {
      name: "testPart3_after_updateEnabledRulesets_within_limit",
      backgroundFn: testPart3_after_updateEnabledRulesets_within_limit,
      checkConsoleMessages: (n, m) => checkFailedRulesets(n, m, []),
    },
    {
      name: "testPart4_toggling_rulesets_at_quota",
      backgroundFn: testPart4_toggling_rulesets_at_quota,
      checkConsoleMessages: (n, m) => checkFailedRulesets(n, m, []),
    },
    {
      name: "testPart5_after_doubling_quota",
      setup() {
        Services.prefs.setIntPref(
          "extensions.dnr.max_number_of_regex_rules",
          2 * MAX_NUMBER_OF_REGEX_RULES
        );
      },
      backgroundFn: testPart5_after_doubling_quota,
      checkConsoleMessages: (n, m) => checkFailedRulesets(n, m, []),
    },
    {
      name: "testPart6_after_restoring_original_quota_half",
      setup() {
        // Restore the original quota after it was raised in testPart5.
        Services.prefs.clearUserPref(
          "extensions.dnr.max_number_of_regex_rules"
        );
      },
      backgroundFn: testPart6_after_restoring_original_quota_half,
      checkConsoleMessages: (n, m) =>
        checkFailedRulesets(n, m, ["limit_minus_2", "limit_minus_1"]),
    },
  ];

  await startOrReloadExtensionForEach(testCases, extensionDataTemplate);
});
