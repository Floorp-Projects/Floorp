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
  const dnr = browser.declarativeNetRequest;
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

  async function testInvalidRule(rule, expectedError, isSchemaError) {
    if (isSchemaError) {
      // Schema validation error = thrown error instead of a rejection.
      browser.test.assertThrows(
        () => dnr.updateSessionRules({ addRules: [rule] }),
        expectedError,
        `Rule should be invalid (schema-validated): ${JSON.stringify(rule)}`
      );
    } else {
      await browser.test.assertRejects(
        dnr.updateSessionRules({ addRules: [rule] }),
        expectedError,
        `Rule should be invalid: ${JSON.stringify(rule)}`
      );
    }
  }
  async function testInvalidCondition(condition, expectedError, isSchemaError) {
    await testInvalidRule(
      { id: 1, condition, action: { type: "block" } },
      expectedError,
      isSchemaError
    );
  }
  async function testInvalidAction(action, expectedError, isSchemaError) {
    await testInvalidRule(
      { id: 1, condition: {}, action },
      expectedError,
      isSchemaError
    );
  }

  // The tests in this file merely verify whether rule registration and
  // retrieval works. test_ext_dnr_testMatchOutcome.js checks rule evaluation.
  async function testValidRule(rule) {
    await dnr.updateSessionRules({ addRules: [rule] });

    // Default rule with null for optional fields.
    const expectedRule = dnrTestUtils.makeRuleOutput();
    expectedRule.id = rule.id;
    Object.assign(expectedRule.condition, rule.condition);
    Object.assign(expectedRule.action, rule.action);
    if (rule.action.redirect) {
      expectedRule.action.redirect = {
        extensionPath: null,
        url: null,
        transform: null,
        regexSubstitution: null,
        ...rule.action.redirect,
      };
    }
    if (rule.action.requestHeaders) {
      expectedRule.action.requestHeaders = rule.action.requestHeaders.map(
        h => ({ header: null, operation: null, value: null, ...h })
      );
    }
    if (rule.action.responseHeaders) {
      expectedRule.action.responseHeaders = rule.action.responseHeaders.map(
        h => ({ header: null, operation: null, value: null, ...h })
      );
    }

    browser.test.assertDeepEq(
      [expectedRule],
      await dnr.getSessionRules(),
      "Rule should be valid"
    );

    await dnr.updateSessionRules({ removeRuleIds: [rule.id] });
  }
  async function testValidCondition(condition) {
    await testValidRule({ id: 1, condition, action: { type: "block" } });
  }
  async function testValidAction(action) {
    await testValidRule({ id: 1, condition: {}, action });
  }

  Object.assign(dnrTestUtils, {
    testInvalidRule,
    testInvalidCondition,
    testInvalidAction,
    testValidRule,
    testValidCondition,
    testValidAction,
  });
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

add_task(async function validate_resourceTypes() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const {
        testInvalidCondition,
        testInvalidRule,
        testValidRule,
        testValidCondition,
      } = dnrTestUtils;

      await testInvalidCondition(
        { resourceTypes: ["font", "image"], excludedResourceTypes: ["image"] },
        "resourceTypes and excludedResourceTypes should not overlap"
      );
      await testInvalidCondition(
        { resourceTypes: [], excludedResourceTypes: ["image"] },
        /resourceTypes: Array requires at least 1 items; you have 0/,
        /* isSchemaError */ true
      );
      await testValidCondition({
        resourceTypes: ["font"],
        excludedResourceTypes: ["image"],
      });
      await testValidCondition({
        resourceTypes: ["font"],
        excludedResourceTypes: [],
      });

      // Validation specific to allowAllRequests
      await testInvalidRule(
        {
          id: 1,
          condition: {},
          action: { type: "allowAllRequests" },
        },
        "An allowAllRequests rule must have a non-empty resourceTypes array"
      );
      await testInvalidRule(
        {
          id: 1,
          condition: { resourceTypes: [] },
          action: { type: "allowAllRequests" },
        },
        /resourceTypes: Array requires at least 1 items; you have 0/,
        /* isSchemaError */ true
      );
      await testInvalidRule(
        {
          id: 1,
          condition: { resourceTypes: ["main_frame", "image"] },
          action: { type: "allowAllRequests" },
        },
        "An allowAllRequests rule may only include main_frame/sub_frame in resourceTypes"
      );
      await testValidRule({
        id: 1,
        condition: { resourceTypes: ["main_frame"] },
        action: { type: "allowAllRequests" },
      });
      await testValidRule({
        id: 1,
        condition: { resourceTypes: ["sub_frame"] },
        action: { type: "allowAllRequests" },
      });
      browser.test.notifyPass();
    },
  });
});

add_task(async function validate_requestMethods() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { testInvalidCondition, testValidCondition } = dnrTestUtils;

      await testInvalidCondition(
        { requestMethods: ["get"], excludedRequestMethods: ["post", "get"] },
        "requestMethods and excludedRequestMethods should not overlap"
      );
      await testInvalidCondition(
        { requestMethods: [] },
        /requestMethods: Array requires at least 1 items; you have 0/,
        /* isSchemaError */ true
      );
      await testInvalidCondition(
        { requestMethods: ["GET"] },
        "request methods must be in lower case"
      );
      await testInvalidCondition(
        { excludedRequestMethods: ["PUT"] },
        "request methods must be in lower case"
      );
      await testValidCondition({ excludedRequestMethods: [] });
      await testValidCondition({
        requestMethods: ["get", "head"],
        excludedRequestMethods: ["post"],
      });
      await testValidCondition({
        requestMethods: ["connect", "delete", "options", "patch", "put", "xxx"],
      });

      browser.test.notifyPass();
    },
  });
});

add_task(async function validate_tabIds() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { testInvalidCondition, testValidCondition } = dnrTestUtils;

      await testInvalidCondition(
        { tabIds: [1], excludedTabIds: [1] },
        "tabIds and excludedTabIds should not overlap"
      );
      await testInvalidCondition(
        { tabIds: [] },
        /tabIds: Array requires at least 1 items; you have 0/,
        /* isSchemaError */ true
      );
      await testValidCondition({ excludedTabIds: [] });
      await testValidCondition({ tabIds: [-1, 0, 1], excludedTabIds: [2] });
      await testValidCondition({ tabIds: [Number.MAX_SAFE_INTEGER] });

      browser.test.notifyPass();
    },
  });
});

add_task(async function validate_domains() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { testInvalidCondition, testValidCondition } = dnrTestUtils;

      await testInvalidCondition(
        { requestDomains: [] },
        /requestDomains: Array requires at least 1 items; you have 0/,
        /* isSchemaError */ true
      );
      await testInvalidCondition(
        { initiatorDomains: [] },
        /initiatorDomains: Array requires at least 1 items; you have 0/,
        /* isSchemaError */ true
      );
      // The include and exclude overlaps, but the validator doesn't reject it:
      await testValidCondition({
        requestDomains: ["example.com"],
        excludedRequestDomains: ["example.com"],
        initiatorDomains: ["example.com"],
        excludedInitiatorDomains: ["example.com"],
      });
      await testValidCondition({
        excludedRequestDomains: [],
        excludedInitiatorDomains: [],
      });

      // "null" is valid as a way to match an opaque initiator.
      await testInvalidCondition(
        { requestDomains: [null] },
        /requestDomains\.0: Expected string instead of null/,
        /* isSchemaError */ true
      );
      await testValidCondition({ requestDomains: ["null"] });

      // IPv4 adress should be 4 digits separated by a dot.
      await testInvalidCondition(
        { requestDomains: ["1.2"] },
        /requestDomains\.0: Error: Invalid domain 1.2/,
        /* isSchemaError */ true
      );
      await testValidCondition({ requestDomains: ["0.0.1.2"] });

      // IPv6 should be wrapped in brackets.
      await testInvalidCondition(
        { requestDomains: ["::1"] },
        /requestDomains\.0: Error: Invalid domain ::1/,
        /* isSchemaError */ true
      );
      // IPv6 addresses cannot contain dots.
      await testInvalidCondition(
        { requestDomains: ["[::ffff:127.0.0.1]"] },
        /requestDomains\.0: Error: Invalid domain \[::ffff:127\.0\.0\.1\]/,
        /* isSchemaError */ true
      );
      await testValidCondition({
        // "[::ffff:7f00:1]" is the canonical form of "[::ffff:127.0.0.1]".
        requestDomains: ["[::1]", "[::ffff:7f00:1]"],
      });

      // International Domain Names should be punycode-encoded.
      await testInvalidCondition(
        { requestDomains: ["straß.de"] },
        /requestDomains\.0: Error: Invalid domain straß.de/,
        /* isSchemaError */ true
      );
      await testValidCondition({ requestDomains: ["xn--stra-yna.de"] });

      // Domain may not contain a port.
      await testInvalidCondition(
        { requestDomains: ["a.com:1234"] },
        /requestDomains\.0: Error: Invalid domain a.com:1234/,
        /* isSchemaError */ true
      );
      // Upper case is not canonical.
      await testInvalidCondition(
        { requestDomains: ["UPPERCASE"] },
        /requestDomains\.0: Error: Invalid domain UPPERCASE/,
        /* isSchemaError */ true
      );
      // URL encoded is not canonical.
      await testInvalidCondition(
        { requestDomains: ["ex%61mple.com"] },
        /requestDomains\.0: Error: Invalid domain ex%61mple.com/,
        /* isSchemaError */ true
      );

      // Verify that the validation is applied to all domain-related keys.
      for (let domainsKey of [
        "initiatorDomains",
        "excludedInitiatorDomains",
        "requestDomains",
        "excludedRequestDomains",
      ]) {
        await testInvalidCondition(
          { [domainsKey]: [""] },
          new RegExp(String.raw`${domainsKey}\.0: Error: Invalid domain \)`),
          /* isSchemaError */ true
        );
      }

      browser.test.notifyPass();
    },
  });
});

add_task(async function validate_actions() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { testInvalidAction, testValidAction } = dnrTestUtils;

      await testValidAction({ type: "allow" });
      // Note: allowAllRequests is already covered in validate_resourceTypes
      await testValidAction({ type: "block" });
      await testValidAction({ type: "upgradeScheme" });
      await testValidAction({ type: "block" });

      // redirect actions, invalid cases
      await testInvalidAction(
        { type: "redirect" },
        "A redirect rule must have a non-empty action.redirect object"
      );
      await testInvalidAction(
        { type: "redirect", redirect: {} },
        "A redirect rule must have a non-empty action.redirect object"
      );
      await testInvalidAction(
        { type: "redirect", redirect: { extensionPath: "/", url: "http://a" } },
        "redirect.extensionPath and redirect.url are mutually exclusive"
      );
      await testInvalidAction(
        { type: "redirect", redirect: { extensionPath: "", url: "http://a" } },
        "redirect.extensionPath and redirect.url are mutually exclusive"
      );
      await testInvalidAction(
        { type: "redirect", redirect: { extensionPath: "" } },
        "redirect.extensionPath should start with a '/'"
      );
      await testInvalidAction(
        {
          type: "redirect",
          redirect: { extensionPath: browser.runtime.getURL("/") },
        },
        "redirect.extensionPath should start with a '/'"
      );
      await testInvalidAction(
        { type: "redirect", redirect: { url: "javascript:" } },
        /Access denied for URL javascript:/,
        /* isSchemaError */ true
      );
      await testInvalidAction(
        { type: "redirect", redirect: { url: "JAVASCRIPT:// Hmmm" } },
        /Access denied for URL javascript:\/\/ Hmmm/,
        /* isSchemaError */ true
      );
      await testInvalidAction(
        { type: "redirect", redirect: { url: "about:addons" } },
        /Access denied for URL about:addons/,
        /* isSchemaError */ true
      );
      // TODO bug 1622986: allow redirects to data:-URLs.
      await testInvalidAction(
        { type: "redirect", redirect: { url: "data:," } },
        /Access denied for URL data:,/,
        /* isSchemaError */ true
      );

      // redirect actions, valid cases
      await testValidAction({
        type: "redirect",
        redirect: { extensionPath: "/foo.txt" },
      });
      await testValidAction({
        type: "redirect",
        redirect: { url: "https://example.com/" },
      });
      await testValidAction({
        type: "redirect",
        redirect: { url: browser.runtime.getURL("/") },
      });

      // modifyHeaders actions, invalid cases
      await testInvalidAction(
        { type: "modifyHeaders" },
        "A modifyHeaders rule must have a non-empty requestHeaders or modifyHeaders list"
      );
      await testInvalidAction(
        { type: "modifyHeaders", requestHeaders: [] },
        /requestHeaders: Array requires at least 1 items; you have 0/,
        /* isSchemaError */ true
      );
      await testInvalidAction(
        { type: "modifyHeaders", responseHeaders: [] },
        /responseHeaders: Array requires at least 1 items; you have 0/,
        /* isSchemaError */ true
      );
      await testInvalidAction(
        {
          type: "modifyHeaders",
          requestHeaders: [{ header: "", operation: "remove" }],
        },
        "header must be non-empty"
      );
      await testInvalidAction(
        {
          type: "modifyHeaders",
          responseHeaders: [{ header: "", operation: "remove" }],
        },
        "header must be non-empty"
      );
      await testInvalidAction(
        {
          type: "modifyHeaders",
          responseHeaders: [{ header: "x", operation: "append" }],
        },
        "value is required for operations append/set"
      );
      await testInvalidAction(
        {
          type: "modifyHeaders",
          responseHeaders: [{ header: "x", operation: "set" }],
        },
        "value is required for operations append/set"
      );
      await testInvalidAction(
        {
          type: "modifyHeaders",
          responseHeaders: [{ header: "x", operation: "remove", value: "x" }],
        },
        "value must not be provided for operation remove"
      );
      await testInvalidAction(
        {
          type: "modifyHeaders",
          responseHeaders: [{ header: "x", operation: "REMOVE", value: "x" }],
        },
        /operation: Invalid enumeration value "REMOVE"/,
        /* isSchemaError */ true
      );
      await testInvalidAction(
        {
          type: "modifyHeaders",
          // "append" is documented to be disallowed.
          requestHeaders: [{ header: "x", operation: "append", value: "x" }],
        },
        /operation: Invalid enumeration value "append"/,
        /* isSchemaError */ true
      );

      // modifyHeaders actions, valid cases
      await testValidAction({
        type: "modifyHeaders",
        requestHeaders: [{ header: "x", operation: "set", value: "x" }],
      });
      await testValidAction({
        type: "modifyHeaders",
        responseHeaders: [{ header: "x", operation: "set", value: "x" }],
      });
      await testValidAction({
        type: "modifyHeaders",
        requestHeaders: [{ header: "y", operation: "set", value: "y" }],
        responseHeaders: [{ header: "z", operation: "set", value: "z" }],
      });
      await testValidAction({
        type: "modifyHeaders",
        requestHeaders: [
          { header: "reqh", operation: "set", value: "b" },
          // "append" is documented to be disallowed.
          // { header: "reqh", operation: "append", value: "b" },
          { header: "reqh", operation: "remove" },
        ],
        responseHeaders: [
          { header: "resh", operation: "set", value: "b" },
          { header: "resh", operation: "append", value: "b" },
          { header: "resh", operation: "remove" },
        ],
      });

      await testInvalidAction(
        { type: "MODIFYHEADERS" },
        /type: Invalid enumeration value "MODIFYHEADERS"/,
        /* isSchemaError */ true
      );

      browser.test.notifyPass();
    },
  });
});
