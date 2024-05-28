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

  function serializeForLog(rule) {
    // JSON-stringify, but drop null values (replacing them with undefined
    // causes JSON.stringify to drop them), so that optional keys with the null
    // values are hidden.
    let str = JSON.stringify(rule, rep => rep ?? undefined);
    // VERY_LONG_STRING consists of many 'X'. Shorten to avoid logspam.
    str = str.replace(/x{10,}/g, xxx => `x{${xxx.length}}`);
    return str;
  }

  async function testInvalidRule(rule, expectedError, isSchemaError) {
    if (isSchemaError) {
      // Schema validation error = thrown error instead of a rejection.
      browser.test.assertThrows(
        () => dnr.updateSessionRules({ addRules: [rule] }),
        expectedError,
        `Rule should be invalid (schema-validated): ${serializeForLog(rule)}`
      );
    } else {
      await browser.test.assertRejects(
        dnr.updateSessionRules({ addRules: [rule] }),
        expectedError,
        `Rule should be invalid: ${serializeForLog(rule)}`
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
      if (rule.action.redirect.transform) {
        expectedRule.action.redirect.transform = {
          scheme: null,
          username: null,
          password: null,
          host: null,
          port: null,
          path: null,
          query: null,
          queryTransform: null,
          fragment: null,
          ...rule.action.redirect.transform,
        };
        if (rule.action.redirect.transform.queryTransform) {
          const qt = {
            removeParams: null,
            addOrReplaceParams: null,
            ...rule.action.redirect.transform.queryTransform,
          };
          if (qt.addOrReplaceParams) {
            qt.addOrReplaceParams = qt.addOrReplaceParams.map(v => ({
              key: null,
              value: null,
              replaceOnly: false,
              ...v,
            }));
          }
          expectedRule.action.redirect.transform.queryTransform = qt;
        }
      }
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

      browser.test.assertDeepEq(
        [],
        await dnr.getSessionRules({ ruleIds: [] }),
        "getSessionRules() returns no rule because ruleIds is an empty array"
      );
      browser.test.assertDeepEq(
        [],
        await dnr.getSessionRules({ ruleIds: [1234567890] }),
        "getSessionRules() returns no rule because rule ID is non-existent"
      );
      browser.test.assertDeepEq(
        [RULE_4321_OUT],
        await dnr.getSessionRules({ ruleIds: [4321] }),
        "getSessionRules() returns a rule"
      );
      browser.test.assertDeepEq(
        [RULE_1234_OUT],
        await dnr.getSessionRules({ ruleIds: [1234] }),
        "getSessionRules() returns a rule"
      );
      // The order is the same as the original input above, not the order of
      // the IDs in `ruleIds`.
      browser.test.assertDeepEq(
        [RULE_4321_OUT, RULE_1234_OUT],
        await dnr.getSessionRules({ ruleIds: [1234, 4321] }),
        "getSessionRules() returns two rules"
      );
      browser.test.assertDeepEq(
        [RULE_4321_OUT, RULE_1234_OUT],
        await dnr.getSessionRules({}),
        "getSessionRules() returns all the rules because there is no ruleIds"
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

// Basic urlFilter validation; test_ext_dnr_urlFilter.js has more tests.
add_task(async function validate_urlFilter() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { testInvalidCondition, testValidCondition } = dnrTestUtils;

      await testInvalidCondition(
        { urlFilter: "", regexFilter: "" },
        "urlFilter and regexFilter are mutually exclusive"
      );

      await testInvalidCondition(
        { urlFilter: 0 },
        /urlFilter: Expected string instead of 0/,
        /* isSchemaError */ true
      );
      await testInvalidCondition(
        { urlFilter: "" },
        "urlFilter should not be an empty string"
      );
      await testInvalidCondition(
        { urlFilter: "||*" },
        "urlFilter should not start with '||*'" // should use '*' instead.
      );
      await testInvalidCondition(
        { urlFilter: "||*/" },
        "urlFilter should not start with '||*'" // should use '*' instead.
      );
      await testInvalidCondition(
        { urlFilter: "straß.de" },
        "urlFilter should not contain non-ASCII characters"
      );
      await testValidCondition({ urlFilter: "xn--stra-yna.de" });
      await testValidCondition({ urlFilter: "||xn--stra-yna.de/" });

      // The following are all logically equivalent to "||*" (and ""), but are
      // considered valid in the DNR API implemented/documented by Chrome.
      await testValidCondition({ urlFilter: "*" });
      await testValidCondition({ urlFilter: "****************" });
      await testValidCondition({ urlFilter: "||" });
      await testValidCondition({ urlFilter: "|" });
      await testValidCondition({ urlFilter: "|*|" });
      await testValidCondition({ urlFilter: "^" });
      await testValidCondition({ urlFilter: null });

      await testValidCondition({ urlFilter: "||example^" });
      await testValidCondition({ urlFilter: "||example.com" });
      await testValidCondition({ urlFilter: "||example.com/index^" });
      await testValidCondition({ urlFilter: ".gif|" });
      await testValidCondition({ urlFilter: "|https:" });
      await testValidCondition({ urlFilter: "|https:*" });
      await testValidCondition({ urlFilter: "e" });
      await testValidCondition({ urlFilter: "%80" });
      await testValidCondition({ urlFilter: "*e*" }); // FYI: same as just "e".
      await testValidCondition({ urlFilter: "*e*|" }); // FYI: same as just "e".

      let validchars = "";
      for (let i = 0; i < 0x80; ++i) {
        validchars += String.fromCharCode(i);
      }
      await testValidCondition({ urlFilter: validchars });
      // Confirming that 0x80 and up is invalid.
      await testInvalidCondition(
        { urlFilter: "\x80" },
        "urlFilter should not contain non-ASCII characters"
      );

      browser.test.notifyPass();
    },
  });
});

// Basic regexFilter validation; test_ext_dnr_regexFilter.js has more tests.
add_task(async function validate_regexFilter() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { testInvalidCondition, testValidCondition } = dnrTestUtils;

      // This check is duplicated in validate_urlFilter.
      await testInvalidCondition(
        { urlFilter: "", regexFilter: "" },
        "urlFilter and regexFilter are mutually exclusive"
      );

      await testInvalidCondition(
        { regexFilter: /regex/ },
        /regexFilter: Expected string instead of \{\}/,
        /* isSchemaError */ true
      );

      await testInvalidCondition(
        { regexFilter: "" },
        "regexFilter should not be an empty string"
      );
      await testInvalidCondition(
        { regexFilter: "*" },
        "regexFilter is not a valid regular expression"
      );
      await testValidCondition(
        { regexFilter: "^https://example\\.com\\/" },
        "regexFilter with valid regexp should be accepted"
      );

      browser.test.notifyPass();
    },
  });
});

add_task(async function validate_actions() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { testInvalidAction, testValidAction, testValidRule } =
        dnrTestUtils;

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
        "redirect.url, redirect.extensionPath, redirect.transform and redirect.regexSubstitution are mutually exclusive"
      );
      await testInvalidAction(
        { type: "redirect", redirect: { extensionPath: "", url: "http://a" } },
        "redirect.url, redirect.extensionPath, redirect.transform and redirect.regexSubstitution are mutually exclusive"
      );
      await testInvalidAction(
        {
          type: "redirect",
          redirect: { regexSubstitution: "", transform: {} },
        },
        "redirect.url, redirect.extensionPath, redirect.transform and redirect.regexSubstitution are mutually exclusive"
      );
      await testInvalidAction(
        {
          type: "redirect",
          redirect: { regexSubstitution: "x", transform: {}, url: "http://a" },
        },
        "redirect.url, redirect.extensionPath, redirect.transform and redirect.regexSubstitution are mutually exclusive"
      );
      await testInvalidAction(
        {
          type: "redirect",
          redirect: {
            url: "http://a",
            extensionPath: "/",
            transform: {},
            regexSubstitution: "http://a",
          },
        },
        "redirect.url, redirect.extensionPath, redirect.transform and redirect.regexSubstitution are mutually exclusive"
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
      await testInvalidAction(
        { type: "redirect", redirect: { regexSubstitution: "http:///" } },
        "redirect.regexSubstitution requires the regexFilter condition to be specified"
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
      await testValidAction({
        type: "redirect",
        redirect: { transform: {} },
      });
      // redirect.transform is validated in validate_action_redirect_transform.
      await testValidRule({
        id: 1,
        condition: { regexFilter: ".+" },
        action: {
          type: "redirect",
          redirect: { regexSubstitution: "http://example.com/" },
        },
      });
      // ^ redirect.regexSubstitution is tested by test_ext_dnr_regexFilter.js.

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
          // Note: contrary to Chrome, we support "append" for requestHeaders:
          // https://bugzilla.mozilla.org/show_bug.cgi?id=1797404#c1
          { header: "reqh", operation: "append", value: "b" },
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

// This test task only verifies that a redirect transform is validated upon
// registration. A transform can result in an invalid redirect despite passing
// validation (see e.g. VERY_LONG_STRING below).
// test_ext_dnr_redirect_transform.js will test the behavior of such cases.
add_task(async function validate_action_redirect_transform() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { testInvalidAction, testValidAction } = dnrTestUtils;

      const GENERIC_TRANSFORM_ERROR =
        "redirect.transform does not describe a valid URL transformation";

      const testValidTransform = transform =>
        testValidAction({ type: "redirect", redirect: { transform } });
      const testInvalidTransform = (transform, expectedError, isSchemaError) =>
        testInvalidAction(
          { type: "redirect", redirect: { transform } },
          expectedError ?? GENERIC_TRANSFORM_ERROR,
          isSchemaError
        );

      // Maximum length of a UTL is 1048576 (network.standard-url.max-length).
      // Since URLs have other characters (separators), using VERY_LONG_STRING
      // anywhere in a transform should be rejected. Note that this is mainly
      // to verify that there is some bounds check on the URL. It is possible
      // to generate a transform that is borderline valid at validation time,
      // but invalid when applied to an existing longer URL.
      const VERY_LONG_STRING = "x".repeat(1048576);

      // An empty transformation is still valid.
      await testValidTransform({});

      // redirect.transform.scheme
      await testValidTransform({ scheme: "http" });
      await testValidTransform({ scheme: "https" });
      await testValidTransform({ scheme: "moz-extension" });
      await testInvalidTransform(
        { scheme: "HTTPS" },
        /scheme: Invalid enumeration value "HTTPS"/,
        /* isSchemaError */ true
      );
      await testInvalidTransform(
        { scheme: "javascript" },
        /scheme: Invalid enumeration value "javascript"/,
        /* isSchemaError */ true
      );
      // "ftp" is unsupported because support for it was dropped in Firefox.
      // Chrome documents "ftp" as a supported scheme, but in practice it does
      // not do anything useful, because it cannot handle ftp schemes either.
      await testInvalidTransform(
        { scheme: "ftp" },
        /scheme: Invalid enumeration value "ftp"/,
        /* isSchemaError */ true
      );

      // redirect.transform.host
      await testValidTransform({ host: "example.com" });
      await testValidTransform({ host: "example.com." });
      await testValidTransform({ host: "localhost" });
      await testValidTransform({ host: "127.0.0.1" });
      await testValidTransform({ host: "[::1]" });
      await testValidTransform({ host: "." });
      await testValidTransform({ host: "straß.de" });
      await testValidTransform({ host: "xn--stra-yna.de" });
      await testInvalidTransform({ host: "::1" }); // Invalid IPv6.
      await testInvalidTransform({ host: "[]" }); // Invalid IPv6.
      await testInvalidTransform({ host: "/" }); // Invalid host
      await testInvalidTransform({ host: " a" }); // Invalid host
      await testInvalidTransform({ host: "foo:1234" }); // Port not allowed.
      await testInvalidTransform({ host: "foo:" }); // Port sep not allowed.
      await testInvalidTransform({ host: "" }); // Host cannot be empty.
      await testInvalidTransform({ host: VERY_LONG_STRING });

      // redirect.transform.port
      await testValidTransform({ port: "" }); // empty = strip port.
      await testValidTransform({ port: "0" });
      await testValidTransform({ port: "0700" });
      await testValidTransform({ port: "65535" });
      const PORT_ERR = "redirect.transform.port should be empty or an integer";
      await testInvalidTransform({ port: "65536" }, GENERIC_TRANSFORM_ERROR);
      await testInvalidTransform({ port: " 0" }, PORT_ERR);
      await testInvalidTransform({ port: "0 " }, PORT_ERR);
      await testInvalidTransform({ port: "0." }, PORT_ERR);
      await testInvalidTransform({ port: "0x1" }, PORT_ERR);
      await testInvalidTransform({ port: "1.2" }, PORT_ERR);
      await testInvalidTransform({ port: "-1" }, PORT_ERR);
      await testInvalidTransform({ port: "a" }, PORT_ERR);
      // A naive implementation of `host = hostname + ":" + port` could be
      // misinterpreted as an IPv6 address. Verify that this is not the case.
      await testInvalidTransform({ host: "[::1", port: "2]" }, PORT_ERR);
      await testInvalidTransform({ port: VERY_LONG_STRING }, PORT_ERR);

      // redirect.transform.path
      await testValidTransform({ path: "" }); // empty = strip path.
      await testValidTransform({ path: "/slash" });
      await testValidTransform({ path: "/ref#ok" }); // # will be escaped.
      await testValidTransform({ path: "/\n\t\x00" }); // Will all be escaped.
      // A path should start with a '/', but the implementation works fine
      // without it, and Chrome doesn't require it either.
      await testValidTransform({ path: "noslash" });
      await testValidTransform({ path: "http://example.com/" });
      await testInvalidTransform({ path: VERY_LONG_STRING });

      // redirect.transform.query
      await testValidTransform({ query: "" }); // empty = strip query.
      await testValidTransform({ query: "?suffix" });
      await testValidTransform({ query: "?ref#ok" }); // # will be escaped.
      await testValidTransform({ query: "?\n\t\x00" }); // Will all be escaped.
      await testInvalidTransform(
        { query: "noquestionmark" },
        "redirect.transform.query should be empty or start with a '?'"
      );
      await testInvalidTransform({ query: "?" + VERY_LONG_STRING });

      // redirect.transform.queryTransform
      await testInvalidTransform(
        { query: "", queryTransform: {} },
        "redirect.transform.query and redirect.transform.queryTransform are mutually exclusive"
      );
      await testValidTransform({ queryTransform: {} });
      await testValidTransform({ queryTransform: { removeParams: [] } });
      await testValidTransform({ queryTransform: { removeParams: ["x"] } });
      await testValidTransform({ queryTransform: { addOrReplaceParams: [] } });
      await testValidTransform({
        queryTransform: {
          addOrReplaceParams: [{ key: "k", value: "v" }],
        },
      });
      await testValidTransform({
        queryTransform: {
          addOrReplaceParams: [{ key: "k", value: "v", replaceOnly: true }],
        },
      });
      await testInvalidTransform({
        queryTransform: {
          addOrReplaceParams: [{ key: "k", value: VERY_LONG_STRING }],
        },
      });
      await testInvalidTransform(
        {
          queryTransform: {
            addOrReplaceParams: [{ key: "k" }],
          },
        },
        /addOrReplaceParams\.0: Property "value" is required/,
        /* isSchemaError */ true
      );
      await testInvalidTransform(
        {
          queryTransform: {
            addOrReplaceParams: [{ value: "v" }],
          },
        },
        /addOrReplaceParams\.0: Property "key" is required/,
        /* isSchemaError */ true
      );

      // redirect.transform.fragment
      await testValidTransform({ fragment: "" }); // empty = strip fragment.
      await testValidTransform({ fragment: "#suffix" });
      await testValidTransform({ fragment: "#\n\t\x00" }); // will be escaped.
      await testInvalidTransform(
        { fragment: "nohash" },
        "redirect.transform.fragment should be empty or start with a '#'"
      );
      await testInvalidTransform({ fragment: "#" + VERY_LONG_STRING });

      // redirect.transform.username
      await testValidTransform({ username: "" }); // empty = strip username.
      await testValidTransform({ username: "username" });
      await testValidTransform({ username: "@:" }); // will be escaped.
      await testInvalidTransform({ username: VERY_LONG_STRING });

      // redirect.transform.password
      await testValidTransform({ password: "" }); // empty = strip password.
      await testValidTransform({ password: "pass" });
      await testValidTransform({ password: "@:" }); // will be escaped.
      await testInvalidTransform({ password: VERY_LONG_STRING });

      // All together:
      await testValidTransform({
        scheme: "http",
        username: "a",
        password: "b",
        host: "c",
        port: "12345",
        path: "/d",
        query: "?e",
        queryTransform: null,
        fragment: "#f",
      });

      browser.test.notifyPass();
    },
  });
});

add_task(async function session_rules_total_rule_limit() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { MAX_NUMBER_OF_SESSION_RULES } = browser.declarativeNetRequest;

      let inputRules = [];
      let nextRuleId = 1;
      for (let i = 0; i < MAX_NUMBER_OF_SESSION_RULES; ++i) {
        inputRules.push(dnrTestUtils.makeRuleInput(nextRuleId++));
      }
      let excessRule = dnrTestUtils.makeRuleInput(nextRuleId++);

      browser.test.log(`Should be able to add ${inputRules.length} rules.`);
      await browser.declarativeNetRequest.updateSessionRules({
        addRules: inputRules,
      });

      browser.test.assertEq(
        inputRules.length,
        (await browser.declarativeNetRequest.getSessionRules()).length,
        "Added up to MAX_NUMBER_OF_SESSION_RULES session rules"
      );

      await browser.test.assertRejects(
        browser.declarativeNetRequest.updateSessionRules({
          addRules: [excessRule],
        }),
        `Number of rules in ruleset "_session" exceeds MAX_NUMBER_OF_SESSION_RULES.`,
        "Should not accept more than MAX_NUMBER_OF_SESSION_RULES rules"
      );

      await browser.test.assertRejects(
        browser.declarativeNetRequest.updateSessionRules({
          removeRuleIds: [inputRules[0].id],
          addRules: [inputRules[0], excessRule],
        }),
        `Number of rules in ruleset "_session" exceeds MAX_NUMBER_OF_SESSION_RULES.`,
        "Removing one rule is not enough to make space for two rules"
      );

      browser.test.log("Should be able to replace one rule while at the limit");
      await browser.declarativeNetRequest.updateSessionRules({
        removeRuleIds: [inputRules[0].id],
        addRules: [excessRule],
      });

      browser.test.log("Should be able to remove many rules, even at quota");
      await browser.declarativeNetRequest.updateSessionRules({
        // Note: inputRules[0].id was already removed, but that's fine.
        removeRuleIds: inputRules.map(r => r.id),
      });

      browser.test.assertDeepEq(
        [dnrTestUtils.makeRuleOutput(excessRule.id)],
        await browser.declarativeNetRequest.getSessionRules(),
        "Expected one rule after removing all-but-one-rule"
      );

      await browser.test.assertRejects(
        browser.declarativeNetRequest.updateSessionRules({
          addRules: inputRules,
        }),
        `Number of rules in ruleset "_session" exceeds MAX_NUMBER_OF_SESSION_RULES.`,
        "Should not be able to add MAX_NUMBER_OF_SESSION_RULES when there is already a rule"
      );

      await browser.declarativeNetRequest.updateSessionRules({
        removeRuleIds: [excessRule.id],
      });
      browser.test.assertDeepEq(
        [],
        await browser.declarativeNetRequest.getSessionRules(),
        "Removed last remaining rule"
      );

      browser.test.notifyPass();
    },
  });
});
