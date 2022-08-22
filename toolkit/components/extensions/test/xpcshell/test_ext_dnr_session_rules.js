"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ExtensionDNR: "resource://gre/modules/ExtensionDNR.sys.mjs",
});

add_setup(() => {
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
  Services.prefs.setBoolPref("extensions.dnr.enabled", true);
});

// This function is serialized and called in the context of the test extension's
// background page. dnrTestUtils is passed to the background function.
function makeDnrTestUtils() {
  const dnrTestUtils = {};
  dnrTestUtils.makeRuleInput = id => {
    return {
      id,
      condition: {},
      action: { type: "block" },
    };
  };
  dnrTestUtils.makeRuleOutput = id => {
    return {
      id,
      condition: {
        urlFilter: null,
        regexFilter: null,
        isUrlFilterCaseSensitive: null,
        initiatorDomains: null,
        excludedInitiatorDomains: null,
        requestDomains: null,
        excludedRequestDomains: null,
        resourceTypes: null,
        excludedResourceTypes: null,
        requestMethods: null,
        excludedRequestMethods: null,
        domainType: null,
        tabIds: null,
        excludedTabIds: null,
      },
      action: {
        type: "block",
        redirect: null,
        requestHeaders: null,
        responseHeaders: null,
      },
      priority: 1,
    };
  };
  return dnrTestUtils;
}

async function runAsDNRExtension({ background, unloadTestAtEnd = true }) {
  let extension = ExtensionTestUtils.loadExtension({
    background: `(${background})((${makeDnrTestUtils})())`,
    manifest: {
      manifest_version: 3,
      permissions: ["declarativeNetRequest", "declarativeNetRequestFeedback"],
    },
  });
  await extension.startup();
  await extension.awaitFinish();
  if (unloadTestAtEnd) {
    await extension.unload();
  }
  return extension;
}

add_task(async function register_and_retrieve_session_rules() {
  let extension = await runAsDNRExtension({
    background: async dnrTestUtils => {
      const dnr = browser.declarativeNetRequest;
      // Rules input to updateSessionRules:
      const RULE_1234_IN = dnrTestUtils.makeRuleInput(1234);
      const RULE_4321_IN = dnrTestUtils.makeRuleInput(4321);
      const RULE_9001_IN = dnrTestUtils.makeRuleInput(9001);
      // Rules expected to be returned by getSessionRules:
      const RULE_1234_OUT = dnrTestUtils.makeRuleOutput(1234);
      const RULE_4321_OUT = dnrTestUtils.makeRuleOutput(4321);
      const RULE_9001_OUT = dnrTestUtils.makeRuleOutput(9001);

      await dnr.updateSessionRules({
        // Deliberately rule 4321 before 1234, see next getSessionRules test.
        addRules: [RULE_4321_IN, RULE_1234_IN],
        removeRuleIds: [1234567890], // Invalid rules should be ignored.
      });
      browser.test.assertDeepEq(
        // Order is same as the original input.
        [RULE_4321_OUT, RULE_1234_OUT],
        await dnr.getSessionRules(),
        "getSessionRules() returns all registered session rules"
      );

      await browser.test.assertRejects(
        dnr.updateSessionRules({
          addRules: [RULE_9001_IN, RULE_1234_IN],
          removeRuleIds: [RULE_4321_IN.id],
        }),
        "Duplicate rule ID: 1234",
        "updateSessionRules of existing rule without removeRuleIds should fail"
      );
      browser.test.assertDeepEq(
        [RULE_4321_OUT, RULE_1234_OUT],
        await dnr.getSessionRules(),
        "session rules should not be changed if an error has occurred"
      );

      // From [4321,1234] to [1234,9001,4321]; 4321 moves to the end because
      // the rule is deleted before inserted, NOT updated in-place.
      await dnr.updateSessionRules({
        addRules: [RULE_9001_IN, RULE_4321_IN],
        removeRuleIds: [RULE_4321_IN.id],
      });
      browser.test.assertDeepEq(
        [RULE_1234_OUT, RULE_9001_OUT, RULE_4321_OUT],
        await dnr.getSessionRules(),
        "existing session rule ID can be re-used for a new rule"
      );

      await dnr.updateSessionRules({
        removeRuleIds: [RULE_1234_IN.id, RULE_4321_IN.id, RULE_9001_IN.id],
      });
      browser.test.assertDeepEq(
        [],
        await dnr.getSessionRules(),
        "deleted all rules"
      );

      browser.test.notifyPass();
    },
    unloadTestAtEnd: false,
  });

  const realExtension = extension.extension;
  Assert.ok(
    ExtensionDNR.getRuleManager(realExtension, /* createIfMissing= */ false),
    "Rule manager exists before unload"
  );
  await extension.unload();
  Assert.ok(
    !ExtensionDNR.getRuleManager(realExtension, /* createIfMissing= */ false),
    "Rule manager erased after unload"
  );
});
