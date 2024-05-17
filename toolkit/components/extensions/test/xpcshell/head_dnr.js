/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* exported assertDNRStoreData, getDNRRule, getSchemaNormalizedRule, getSchemaNormalizedRules
 */

ChromeUtils.defineESModuleGetters(this, {
  Schemas: "resource://gre/modules/Schemas.sys.mjs",
});

function getDNRRule({
  id = 1,
  priority = 1,
  action = {},
  condition = {},
} = {}) {
  return {
    id,
    priority,
    action: {
      type: "block",
      ...action,
    },
    condition: {
      ...condition,
    },
  };
}

const getSchemaNormalizedRule = (extensionTestWrapper, value) => {
  const { extension } = extensionTestWrapper;
  const validationContext = {
    url: extension.baseURI.spec,
    principal: extension.principal,
    logError: err => {
      // We don't expect this test helper function to be called on invalid rules,
      // and so we trigger an explicit test failure if we ever hit any.
      Assert.ok(
        false,
        `Unexpected logError on normalizing DNR rule ${JSON.stringify(
          value
        )} - ${err}`
      );
    },
    preprocessors: {},
    manifestVersion: extension.manifestVersion,
    // We shouldn't be ignoring unrecognized props here because they should
    // have been filtered out by the schema normalization that happened when
    // the extension was loaded.
    ignoreUnrecognizedProperties: false,
  };

  return Schemas.normalize(
    value,
    "declarativeNetRequest.Rule",
    validationContext
  );
};

const getSchemaNormalizedRules = (extensionTestWrapper, rules) => {
  return rules.map(rule => {
    const normalized = getSchemaNormalizedRule(extensionTestWrapper, rule);
    if (normalized.error) {
      throw new Error(
        `Unexpected DNR Rule normalization error: ${normalized.error}`
      );
    }
    return normalized.value;
  });
};

const assertDNRStoreData = async (
  dnrStore,
  extensionTestWrapper,
  expectedRulesets,
  { assertIndividualRules = true } = {}
) => {
  const extUUID = extensionTestWrapper.uuid;
  const rule_resources =
    extensionTestWrapper.extension.manifest.declarative_net_request
      ?.rule_resources;
  const expectedRulesetIds = Array.from(Object.keys(expectedRulesets));
  const expectedRulesetIndexesMap = expectedRulesetIds.reduce((acc, rsId) => {
    acc.set(
      rsId,
      rule_resources.findIndex(rr => rr.id === rsId)
    );
    return acc;
  }, new Map());

  ok(
    dnrStore._dataPromises.has(extUUID),
    "Got promise for the test extension DNR data being loaded"
  );

  await dnrStore._dataPromises.get(extUUID);

  ok(dnrStore._data.has(extUUID), "Got data for the test extension");

  const dnrExtData = dnrStore._data.get(extUUID);
  Assert.deepEqual(
    {
      schemaVersion: dnrExtData.schemaVersion,
      extVersion: dnrExtData.extVersion,
    },
    {
      schemaVersion: dnrExtData.constructor.VERSION,
      extVersion: extensionTestWrapper.extension.version,
    },
    "Got the expected data schema version and extension version in the store data"
  );
  Assert.deepEqual(
    Array.from(dnrExtData.staticRulesets.keys()),
    expectedRulesetIds,
    "Got the enabled rulesets in the stored data staticRulesets Map"
  );

  for (const rulesetId of expectedRulesetIds) {
    const expectedRulesetIdx = expectedRulesetIndexesMap.get(rulesetId);
    const expectedRulesetRules = getSchemaNormalizedRules(
      extensionTestWrapper,
      expectedRulesets[rulesetId]
    );
    const actualData = dnrExtData.staticRulesets.get(rulesetId);
    equal(
      actualData.idx,
      expectedRulesetIdx,
      `Got the expected ruleset index for ruleset id ${rulesetId}`
    );

    // Asserting an entire array of rules all at once will produce
    // a big enough output to don't be immediately useful to investigate
    // failures, asserting each rule individually would produce more
    // readable assertion failure logs.
    const assertRuleAtIdx = ruleIdx => {
      const actualRule = actualData.rules[ruleIdx];
      const expectedRule = expectedRulesetRules[ruleIdx];
      Assert.deepEqual(
        actualRule,
        expectedRule,
        `Got the expected rule at index ${ruleIdx} for ruleset id "${rulesetId}"`
      );
      Assert.equal(
        actualRule.constructor.name,
        "Rule",
        `Expect rule at index ${ruleIdx} to be an instance of the Rule class`
      );
      if (expectedRule.condition.regexFilter) {
        const compiledRegexFilter =
          actualData.rules[ruleIdx].condition.getCompiledRegexFilter();
        Assert.equal(
          compiledRegexFilter?.constructor.name,
          "RegExp",
          `Expect rule ${ruleIdx} condition.getCompiledRegexFilter() to return a compiled regexp filter`
        );
        Assert.equal(
          compiledRegexFilter?.source,
          new RegExp(expectedRule.condition.regexFilter).source,
          `Expect rule ${ruleIdx} condition's compiled RegExp source to match the regexFilter string`
        );
        Assert.equal(
          compiledRegexFilter?.ignoreCase,
          !expectedRule.condition.isUrlFilterCaseSensitive,
          `Expect rule ${ruleIdx} conditions's compiled RegExp ignoreCase to be set based on condition.isUrlFilterCaseSensitive`
        );
      }
    };

    // Some tests may be using a big enough number of rules that
    // the assertiongs would be producing a huge amount of log spam,
    // and so for those tests we only explicitly assert the first
    // and last rule and that the total amount of rules matches the
    // expected number of rules (there are still other tests explicitly
    // asserting all loaded rules).
    if (assertIndividualRules) {
      info(`Verify each individual rule loaded for ruleset id "${rulesetId}"`);
      for (let ruleIdx = 0; ruleIdx < expectedRulesetRules.length; ruleIdx++) {
        assertRuleAtIdx(ruleIdx);
      }
    } else if (expectedRulesetRules.length) {
      // NOTE: Only asserting the first and last rule also helps to speed up
      // the test is some slower builds when the number of expected rules is
      // big enough (e.g. the test task verifying the enforced rule count limits
      // was timing out in tsan build because asserting all indidual rules was
      // taking long enough and the event page was being suspended on the idle
      // timeout by the time we did run all these assertion and proceeding with
      // the rest of the test task assertions), we still confirm that the total
      // number of expected vs actual rules also matches right after these
      // assertions.
      info(
        `Verify the first and last rules loaded for ruleset id "${rulesetId}"`
      );
      const lastExpectedRuleIdx = expectedRulesetRules.length - 1;
      for (const ruleIdx of [0, lastExpectedRuleIdx]) {
        assertRuleAtIdx(ruleIdx);
      }
    }

    equal(
      actualData.rules.length,
      expectedRulesetRules.length,
      `Got the expected number of rules loaded for ruleset id "${rulesetId}"`
    );
  }
};
