"use strict";

add_setup(() => {
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
  Services.prefs.setBoolPref("extensions.dnr.enabled", true);
  Services.prefs.setBoolPref("extensions.dnr.feedback", true);

  // Don't turn warnings in errors, to make sure that the parameter validation
  // tests verify real-world behavior, instead of the stricter test-only mode.
  ExtensionTestUtils.failOnSchemaWarnings(false);
});

// This function is serialized and called in the context of the test extension's
// background page. dnrTestUtils is passed to the background function.
function makeDnrTestUtils() {
  const dnrTestUtils = {};
  const dnr = browser.declarativeNetRequest;
  function makeDummyAction(type) {
    switch (type) {
      case "redirect":
        return { type, redirect: { url: "https://example.com/dummy" } };
      case "modifyHeaders":
        return {
          type,
          responseHeaders: [{ operation: "append", header: "x", value: "y" }],
        };
      default:
        return { type };
    }
  }
  function makeDummyRequest() {
    // A value that matches the condition from makeDummyRule().
    return { url: "https://example.com/some-dummy-url", type: "main_frame" };
  }
  function makeDummyRule(id, actionType) {
    return {
      id,
      // condition matches makeDummyRequest().
      condition: { resourceTypes: ["main_frame"] },
      action: makeDummyAction(actionType),
    };
  }
  async function testMatchesRequest(request, ruleIds, description) {
    browser.test.assertDeepEq(
      ruleIds,
      (await dnr.testMatchOutcome(request)).matchedRules.map(mr => mr.ruleId),
      description
    );
  }
  async function testCanUseAction(type, canUse) {
    await dnr.updateSessionRules({ addRules: [makeDummyRule(1, type)] });
    await testMatchesRequest(
      makeDummyRequest(),
      canUse ? [1] : [],
      `${type} - should${canUse ? "" : " not"} match`
    );
    await dnr.updateSessionRules({ removeRuleIds: [1] });
  }
  Object.assign(dnrTestUtils, {
    makeDummyAction,
    makeDummyRequest,
    makeDummyRule,
    testMatchesRequest,
    testCanUseAction,
  });
  return dnrTestUtils;
}

async function runAsDNRExtension({
  background,
  manifest,
  unloadTestAtEnd = true,
}) {
  let extension = ExtensionTestUtils.loadExtension({
    background: `(${background})((${makeDnrTestUtils})())`,
    manifest: {
      manifest_version: 3,
      permissions: ["declarativeNetRequest", "declarativeNetRequestFeedback"],
      host_permissions: ["<all_urls>"],
      granted_host_permissions: true,
      ...manifest,
    },
    temporarilyInstalled: true, // <-- for granted_host_permissions
  });
  await extension.startup();
  await extension.awaitFinish();
  if (unloadTestAtEnd) {
    await extension.unload();
  }
  return extension;
}

add_task(async function validate_required_params() {
  await runAsDNRExtension({
    background: async () => {
      const testMatchOutcome = browser.declarativeNetRequest.testMatchOutcome;

      browser.test.assertThrows(
        () => testMatchOutcome({ type: "image" }),
        /Type error for parameter request \(Property "url" is required\)/,
        "url is required"
      );
      browser.test.assertThrows(
        () => testMatchOutcome({ url: "https://example.com/" }),
        /Type error for parameter request \(Property "type" is required\)/,
        "resource type is required"
      );

      browser.test.assertDeepEq(
        { matchedRules: [] },
        await testMatchOutcome({ url: "https://example.com/", type: "image" }),
        "testMatchOutcome with url and type succeeds"
      );

      browser.test.notifyPass();
    },
  });
});

add_task(async function resource_type_validation() {
  await runAsDNRExtension({
    background: async () => {
      const testMatchOutcome = browser.declarativeNetRequest.testMatchOutcome;

      const url = "https://example.com/some-dummy-url";

      browser.test.assertThrows(
        () => testMatchOutcome({ url, type: "MAIN_FRAME" }),
        /Error processing type: Invalid enumeration value "MAIN_FRAME"/,
        "testMatchOutcome should expects a lowercase type"
      );

      // Check that at least one ResourceType exists.
      browser.test.assertEq(
        "main_frame",
        browser.declarativeNetRequest.ResourceType.MAIN_FRAME,
        "ResourceType.MAIN_FRAME exists"
      );

      for (let type of Object.values(
        browser.declarativeNetRequest.ResourceType
      )) {
        browser.test.assertDeepEq(
          { matchedRules: [] },
          await testMatchOutcome({ url, type }),
          `testMatchOutcome for type=${type} is allowed`
        );
      }

      browser.test.notifyPass();
    },
  });
});

add_task(async function rule_priority_and_action_type_precedence() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const dnr = browser.declarativeNetRequest;
      const { makeDummyRule, makeDummyRequest } = dnrTestUtils;

      await dnr.updateSessionRules({
        addRules: [
          makeDummyRule(1, "allow"),
          makeDummyRule(2, "allowAllRequests"),
          makeDummyRule(3, "block"),
          makeDummyRule(4, "upgradeScheme"),
          makeDummyRule(5, "redirect"),
          makeDummyRule(6, "modifyHeaders"),
          { ...makeDummyRule(7, "modifyHeaders"), priority: 2 },
          { ...makeDummyRule(8, "allow"), priority: 2 },
          { ...makeDummyRule(9, "block"), priority: 2 },
          // Repeat rules so that we can verify that the outcome is due to the
          // rule action, instead of the rule ID / input order.
          makeDummyRule(11, "allow"),
          makeDummyRule(12, "allowAllRequests"),
          makeDummyRule(13, "block"),
          makeDummyRule(14, "upgradeScheme"),
          makeDummyRule(15, "redirect"),
          makeDummyRule(16, "modifyHeaders"),
          { ...makeDummyRule(17, "modifyHeaders"), priority: 2 },
        ],
      });
      async function testAndRemove(ruleId, expectedRuleIds, description) {
        browser.test.assertDeepEq(
          expectedRuleIds.map(ruleId => ({ ruleId, rulesetId: "_session" })),
          (await dnr.testMatchOutcome(makeDummyRequest())).matchedRules,
          description
        );
        await dnr.updateSessionRules({ removeRuleIds: [ruleId] });
      }

      await testAndRemove(8, [8], "highest-prio allow wins");
      await testAndRemove(9, [9], "highest-prio block wins");
      // after this point, we only have same-prio rules and two higher-prio
      // modifyHeaders rules (7 & 17).

      await testAndRemove(
        1,
        [1, 7, 17],
        "1st allow ignores other rules, except for higher-prio modifyHeaders"
      );
      await testAndRemove(
        11,
        [11, 7, 17],
        "2nd allow ignores other rules, except for higher-prio modifyHeaders"
      );

      await testAndRemove(
        2,
        [2, 7, 17],
        "1st allowAllRequests ignores other rules, except for higher-prio modifyHeaders"
      );
      await testAndRemove(
        12,
        [12, 7, 17],
        "2nd allowAllRequests ignores other rules, except for higher-prio modifyHeaders"
      );

      await testAndRemove(3, [3], "1st block > all other actions");
      await testAndRemove(13, [13], "2nd block > all other actions");

      await testAndRemove(4, [4], "1st upgradeScheme > redirect");
      await testAndRemove(14, [14], "2nd upgradeScheme > redirect");

      await testAndRemove(5, [5], "1st redirect > modifyHeaders");
      await testAndRemove(15, [15], "2nd redirect > modifyHeaders");

      await testAndRemove(
        6,
        [7, 17, 6, 16],
        "All modifyHeaders match if there is no other action"
      );

      // Verify that a new rule takes precedence again.
      await dnr.updateSessionRules({
        addRules: [makeDummyRule(11, "allow")],
      });
      await testAndRemove(
        11,
        [11, 7, 17],
        "After adding an allow rule, only higher-prio modifyHeaders are shown"
      );

      browser.test.assertDeepEq(
        [7, 16, 17],
        (await dnr.getSessionRules()).map(r => r.id),
        "Remaining rules at end of test"
      );

      browser.test.notifyPass();
    },
  });
});

add_task(async function declarativeNetRequest_and_host_permissions() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { testCanUseAction } = dnrTestUtils;

      // Unlocked by declarativeNetRequest permission:
      await testCanUseAction("allow", true);
      await testCanUseAction("allowAllRequests", true);
      await testCanUseAction("block", true);
      await testCanUseAction("upgradeScheme", true);
      // Unlocked by host permissions:
      await testCanUseAction("redirect", true);
      await testCanUseAction("modifyHeaders", true);

      browser.test.notifyPass();
    },
  });
});

add_task(async function declarativeNetRequest_permission_only() {
  await runAsDNRExtension({
    manifest: {
      host_permissions: [],
    },
    background: async dnrTestUtils => {
      const { testCanUseAction } = dnrTestUtils;

      // Unlocked by declarativeNetRequest permission:
      await testCanUseAction("allow", true);
      await testCanUseAction("allowAllRequests", true);
      await testCanUseAction("block", true);
      await testCanUseAction("upgradeScheme", true);
      // These require host permissions, which we don't have:
      await testCanUseAction("redirect", false);
      await testCanUseAction("modifyHeaders", false);

      browser.test.notifyPass();
    },
  });
});

add_task(async function declarativeNetRequestWithHostAccess_only() {
  await runAsDNRExtension({
    manifest: {
      permissions: [
        "declarativeNetRequestWithHostAccess",
        "declarativeNetRequestFeedback",
      ],
      host_permissions: [],
    },
    background: async dnrTestUtils => {
      const { testCanUseAction } = dnrTestUtils;

      // declarativeNetRequestWithHostAccess requires host permissions,
      // which we don't have. So none of the rules should match:
      await testCanUseAction("allow", false);
      await testCanUseAction("allowAllRequests", false);
      await testCanUseAction("block", false);
      await testCanUseAction("upgradeScheme", false);
      await testCanUseAction("redirect", false);
      await testCanUseAction("modifyHeaders", false);

      browser.test.notifyPass();
    },
  });
});

add_task(async function declarativeNetRequestWithHostAccess_only() {
  await runAsDNRExtension({
    manifest: {
      permissions: [
        "declarativeNetRequestWithHostAccess",
        "declarativeNetRequestFeedback",
      ],
      // Origin used by makeDummyRequest() & makeDummyRule():
      host_permissions: ["https://example.com/"],
    },
    background: async dnrTestUtils => {
      const { testCanUseAction } = dnrTestUtils;

      // declarativeNetRequestWithHostAccess + host permissions allows all:
      await testCanUseAction("allow", true);
      await testCanUseAction("allowAllRequests", true);
      await testCanUseAction("block", true);
      await testCanUseAction("upgradeScheme", true);
      await testCanUseAction("redirect", true);
      await testCanUseAction("modifyHeaders", true);

      browser.test.notifyPass();
    },
  });
});

// Tests: resourceTypes, excludedResourceTypes
// Tests: requestMethods, excludedRequestMethods
add_task(async function match_condition_types_and_methods() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const dnr = browser.declarativeNetRequest;
      const { makeDummyAction, testMatchesRequest } = dnrTestUtils;

      // "modifyHeaders" is the only action that allows multiple rule matches.
      const action = makeDummyAction("modifyHeaders");

      await dnr.updateSessionRules({
        addRules: [
          {
            id: 1,
            condition: {
              resourceTypes: ["xmlhttprequest"],
              requestMethods: ["put"],
            },
            action,
          },
          {
            id: 2,
            condition: {
              excludedResourceTypes: ["sub_frame"],
              excludedRequestMethods: ["post"],
            },
            action,
          },
          {
            id: 3,
            condition: {
              // resourceTypes not specified should imply all-minus-main_frame.
              requestMethods: ["get", "post"],
            },
            action,
          },
          {
            id: 4,
            condition: {
              resourceTypes: ["main_frame", "xmlhttprequest"],
              excludedRequestMethods: ["get"],
            },
            action,
          },
        ],
      });

      const url = "https://example.com/some-dummy-url";
      await testMatchesRequest(
        { url, type: "main_frame" },
        [2],
        "main_frame + GET"
      );

      await testMatchesRequest(
        { url, type: "xmlhttprequest" },
        [2, 3],
        "xmlhttprequest + GET"
      );

      await testMatchesRequest(
        { url, type: "xmlhttprequest", method: "put" },
        [1, 2, 4],
        "xmlhttprequest + PUT"
      );

      await testMatchesRequest(
        { url, type: "sub_frame", method: "post" },
        [3],
        "sub_frame + POST"
      );

      await testMatchesRequest(
        { url, type: "sub_frame", method: "post" },
        [3],
        "sub_frame + POST"
      );

      browser.test.notifyPass();
    },
  });
});

// Tests: requestDomains, excludedRequestDomains
add_task(async function match_request_domains() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const dnr = browser.declarativeNetRequest;
      const { makeDummyAction, testMatchesRequest } = dnrTestUtils;

      // "modifyHeaders" is the only action that allows multiple rule matches.
      const action = makeDummyAction("modifyHeaders");

      await dnr.updateSessionRules({
        addRules: [
          {
            id: 1,
            condition: {
              requestDomains: ["a.com", "www.b.com"],
            },
            action,
          },
          {
            id: 2,
            condition: {
              excludedRequestDomains: ["a.com", "www.b.com", "127.0.0.1"],
            },
            action,
          },
          {
            id: 3,
            condition: {
              requestDomains: ["one.net"],
              excludedRequestDomains: ["sub.one.net"],
            },
            action,
          },
          {
            id: 4,
            condition: {
              // This can never match.
              requestDomains: ["sub.one.net"],
              excludedRequestDomains: ["one.net"],
            },
            action,
          },
          {
            id: 5,
            condition: {
              requestDomains: ["127.0.0.1", "[::1]"],
            },
            action,
          },
          {
            id: 6,
            condition: {
              requestDomains: [
                "~b.com", // "~" should not be interpreted as pattern negation.
              ],
            },
            action,
          },
          {
            id: 7,
            condition: {
              // A canonical domain does not start with a ".". Domains filters
              // starting with a "." are therefore not matching anything.
              requestDomains: [".a.com"],
            },
            action,
          },
        ],
      });

      const type = "sub_frame";
      // Tests related to a.com:
      await testMatchesRequest(
        { url: "https://a.com:1234/path", type },
        [1],
        "a.com: url's domain is equal to a.com"
      );
      await testMatchesRequest(
        { url: "http://sub.a.com/", type },
        [1],
        "sub.a.com: url is subdomain of a.com"
      );
      await testMatchesRequest(
        { url: "http://nota.com/a.com?a.com#a.com", type },
        [2],
        "nota.com: url's domain does not match a.com"
      );
      await testMatchesRequest(
        { url: "http://a.com.not/a.com?a.com#a.com", type },
        [2],
        "a.com.not: url's domain does not match a.com"
      );
      await testMatchesRequest(
        { url: "http://a.com./a.com?a.com#a.com", type },
        [2],
        "a.com.: url's domain (ending with dot) does not match a.com"
      );

      // Tests related to www.b.com:
      await testMatchesRequest(
        { url: "http://www.b.com/", type },
        [1],
        "www.b.com: url's domain is equal to www.b.com"
      );
      await testMatchesRequest(
        { url: "http://sub.www.b.com", type },
        [1],
        "sub.www.b.com: url's domain is a subdomain of www.b.com"
      );
      await testMatchesRequest(
        { url: "http://b.com/", type },
        [2],
        "b.com: url's domain is a superdomain, NOT a subdomain of www.b.com"
      );

      // Tests related to sub.one.net / one.net
      await testMatchesRequest(
        { url: "http://one.net/", type },
        [2, 3],
        "one.net: url's domain matches one.net, but not sub.one.net"
      );
      await testMatchesRequest(
        { url: "http://sub.one.net/", type },
        [2], // Rule 4 was a candidate, but excluded anyway.
        "sub.one.net: url's domain matches sub.one.net, but excluded by one.net"
      );

      // Tests related to IP addresses
      await testMatchesRequest(
        { url: "http://127.0.0.1:8080/", type },
        [5],
        "127.0.0.1: IP address is exact match for 127.0.0.1"
      );
      await testMatchesRequest(
        { url: "http://8.8.8.8/", type },
        [2],
        "8.8.8.8: not matched by any of the domains"
      );
      await testMatchesRequest(
        { url: "http://9.127.0.0.1/", type },
        [5],
        "9.127.0.0.1: while not a valid IP, it looks like a subdomain"
      );
      await testMatchesRequest(
        { url: "http://[::1]/", type },
        [2, 5],
        "[::1]: IPv6 matches with bracket"
      );

      // For completeness, verify that the non-resolving domain "~b.com"
      // matches the input, so that we know that "~" was not given special
      // treatment. In filter list syntax, "~" before the domain negates the
      // meaning, but that should not be supported in DNR.
      await testMatchesRequest(
        { url: "http://~b.com/", type },
        [2, 6],
        "~b.com: Although a non-resolving domain, it matches the pattern"
      );

      // match_initiator_domains has more tests; here we just confirm that
      // requestDomains rules don't match initiator.
      await testMatchesRequest(
        { url: "http://url.does.not.match/", type, initiator: "http://a.com/" },
        [2],
        "requestDomains should not match initiator URL"
      );

      browser.test.notifyPass();
    },
  });
});

add_task(async function match_request_domains_punycode() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const dnr = browser.declarativeNetRequest;
      const { makeDummyAction, testMatchesRequest } = dnrTestUtils;

      // "modifyHeaders" is the only action that allows multiple rule matches.
      const action = makeDummyAction("modifyHeaders");

      // Note that the non-punycode domains are rejected by schema validation,
      // and checked by test validate_domains in test_ext_dnr_session_rules.js.

      await dnr.updateSessionRules({
        addRules: [
          {
            id: 1,
            condition: {
              // straß.de
              requestDomains: ["xn--stra-yna.de"],
            },
            action,
          },
          {
            id: 2,
            condition: {
              // IDNA2003 converted ß to ss. But IDNA2008 requires punycode.
              requestDomains: ["strass.de", "stras.de"],
            },
            action,
          },
        ],
      });

      const type = "sub_frame";

      await testMatchesRequest(
        { url: "https://straß.de/", type },
        [1],
        "straß.de matches"
      );
      await testMatchesRequest(
        { url: "https://xn--stra-yna.de/", type },
        [1],
        "xn--stra-yna.de matches"
      );
      await testMatchesRequest(
        { url: "https://strass.de/", type },
        [2],
        "strass.de does not match the punycode pattern of straß"
      );
      await testMatchesRequest(
        { url: "https://stras.de/", type },
        [2],
        "stras.de does not match the punycode pattern of straß"
      );

      browser.test.notifyPass();
    },
  });
});

// Tests: initiatorDomains, excludedInitiatorDomains
add_task(async function match_initiator_domains() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const dnr = browser.declarativeNetRequest;
      const { makeDummyAction, testMatchesRequest } = dnrTestUtils;

      // "modifyHeaders" is the only action that allows multiple rule matches.
      const action = makeDummyAction("modifyHeaders");

      // The validation of initiatorDomains and requestDomains are shared.
      // The match_request_domains and match_request_domains_punycode tests
      // already verify semantics; this test just tests that the conditional
      // logic works as expected, plus coverage for initiator being void.

      await dnr.updateSessionRules({
        addRules: [
          {
            id: 1,
            condition: {
              initiatorDomains: ["a.com"],
            },
            action,
          },
          {
            id: 2,
            condition: {
              excludedInitiatorDomains: ["a.com"],
            },
            action,
          },
          {
            id: 3,
            condition: {
              initiatorDomains: ["c.com"],
              excludedInitiatorDomains: ["c.com"],
            },
            action,
          },
          {
            id: 4, // To verify that it does not match a void initiator.
            condition: {
              initiatorDomains: ["null"],
            },
            action,
          },
          {
            id: 5,
            condition: {
              excludedInitiatorDomains: ["null", "undefined"],
            },
            action,
          },
          {
            id: 6, // To verify that it does not match a void initiator.
            condition: {
              initiatorDomains: ["undefined"],
            },
            action,
          },
        ],
      });

      const url = "https://do.not.look.here/look_at_initator_instead";
      const type = "image";
      await testMatchesRequest(
        { url, type, initiator: "http://a.com/" },
        [1, 5],
        "initiatorDomains matches"
      );
      await testMatchesRequest(
        { url, type, initiator: "http://b.com/" },
        [2, 5],
        "excludedInitiatorDomains does not match, so request matched"
      );
      await testMatchesRequest(
        { url, type, initiator: "http://c.com/" },
        [2, 5], // 3 is not here, despite containing "c.com".
        "excludedInitiatorDomains takes precedence over initiatorDomains"
      );
      // When initiator is not specified, rules with initiatorDomains should not
      // match, and rules with excludedInitiatorDomains may match.
      await testMatchesRequest(
        { url, type },
        [2, 5],
        "request without initiator matches every excludedInitiatorDomains"
      );
      // http://null is unlikely to exist in practice. Regardless, verify that
      // it won't match a void initiators.
      await testMatchesRequest(
        { url, type, initiator: "http://null/" },
        [2, 4],
        "http://null is matched by the 'null' domain"
      );
      await testMatchesRequest(
        { url, type, initiator: "http://undefined/" },
        [2, 6],
        "http://null is matched by the 'undefined' domain"
      );
      await testMatchesRequest(
        { url: "http://a.com/", type },
        [2, 5],
        "initiatorDomains should not match the request URL (initiator=null)"
      );

      browser.test.notifyPass();
    },
  });
});

// Tests: urlFilter. For more comprehensive tests, see
// toolkit/components/extensions/test/xpcshell/test_ext_dnr_urlFilter.js
add_task(async function match_urlFilter() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const dnr = browser.declarativeNetRequest;
      const { makeDummyAction, testMatchesRequest } = dnrTestUtils;

      // "modifyHeaders" is the only action that allows multiple rule matches.
      const action = makeDummyAction("modifyHeaders");

      await dnr.updateSessionRules({
        addRules: [
          // Some patterns that match literally everything:
          { id: 1, condition: { urlFilter: "*" }, action },
          { id: 2, condition: { urlFilter: "^" }, action },
          { id: 3, condition: { urlFilter: "|" }, action },
          // Patterns that match the test URLs
          { id: 4, condition: { urlFilter: "https://example.com" }, action },
          {
            // urlFilter matches, requestDomains matches.
            id: 5,
            condition: { urlFilter: "*", requestDomains: ["example.com"] },
            action,
          },
          {
            // urlFilter matches, requestDomains does not match.
            id: 6,
            condition: { urlFilter: "*", requestDomains: ["notexample.com"] },
            action,
          },
          {
            // urlFilter does not match, requestDomains matches.
            id: 7,
            condition: { urlFilter: "notm", requestDomains: ["example.com"] },
            action,
          },
        ],
      });

      await testMatchesRequest(
        { url: "https://example.com/file.txt", type: "font" },
        [1, 2, 3, 4, 5],
        "urlFilter should match when needed, and correctly with requestDomains"
      );

      browser.test.notifyPass();
    },
  });
});

// Tests: tabIds, excludedTabIds
add_task(async function match_tabIds() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const dnr = browser.declarativeNetRequest;
      const { makeDummyAction, testMatchesRequest } = dnrTestUtils;

      // "modifyHeaders" is the only action that allows multiple rule matches.
      const action = makeDummyAction("modifyHeaders");

      await dnr.updateSessionRules({
        addRules: [
          {
            id: 1,
            condition: {
              excludedTabIds: [-1, Number.MAX_SAFE_INTEGER],
            },
            action,
          },
          {
            id: 2,
            condition: {
              tabIds: [1, Number.MAX_SAFE_INTEGER],
            },
            action,
          },
          {
            id: 3,
            condition: {
              tabIds: [-1],
            },
            action,
          },
        ],
      });

      const url = "https://example.com/some-dummy-url";
      const type = "font";
      await testMatchesRequest({ url, type }, [3], "tabId defaults to -1");
      await testMatchesRequest({ url, type, tabId: -1 }, [3], "tabId -1");
      await testMatchesRequest({ url, type, tabId: 1 }, [1, 2], "tabId 1");
      await testMatchesRequest(
        {
          url,
          type,
          tabId: Number.MAX_SAFE_INTEGER,
        },
        [2],
        `tabId high number (MAX_SAFE_INTEGER=${Number.MAX_SAFE_INTEGER})`
      );

      // tabId -2 is invalid and not encountered in practice, but technically
      // it matches the first rule.
      await testMatchesRequest({ url, type, tabId: -2 }, [1], "bad tabId -2");

      browser.test.notifyPass();
    },
  });
});

add_task(async function action_precedence_between_extensions() {
  // This test is structured as follows:
  // - otherExtension registers rules for several numeric conditions (tabId).
  // - otherExtensionNonBlockAndModifyHeaders adds allowAllRequests and
  //   modifyHeaders to all requests.
  // - otherExtensionModifyHeaders adds modifyHeaders rules to all requests.
  // - the main test extension also registers rules, and then simulates requests
  //   with testMatchOutcome for each tabId, and checks the result.

  let otherExtension = await runAsDNRExtension({
    manifest: { browser_specific_settings: { gecko: { id: "other@ext" } } },
    background: async dnrTestUtils => {
      const { makeDummyAction } = dnrTestUtils;

      // Dummy condition for testing requests in this test.
      const c = tabId => ({ resourceTypes: ["main_frame"], tabIds: [tabId] });

      await browser.declarativeNetRequest.updateSessionRules({
        addRules: [
          { id: 11, condition: c(1), action: makeDummyAction("allow") },
          { id: 12, condition: c(2), action: makeDummyAction("block") },
          { id: 13, condition: c(3), action: makeDummyAction("redirect") },
          { id: 14, condition: c(4), action: makeDummyAction("upgradeScheme") },
          {
            id: 15,
            condition: c(5),
            action: makeDummyAction("allowAllRequests"),
          },
          {
            id: 16,
            condition: c(6),
            action: makeDummyAction("allowAllRequests"),
          },
        ],
      });
      // Notify to continue. We don't exit yet due to unloadTestAtEnd:false
      browser.test.notifyPass();
    },
    unloadTestAtEnd: false,
  });

  let otherExtensionNonBlockAndModifyHeaders = await runAsDNRExtension({
    manifest: { browser_specific_settings: { gecko: { id: "other@ext2" } } },
    background: async dnrTestUtils => {
      const { makeDummyAction } = dnrTestUtils;

      // Matches all requests from this test.
      const condition = { resourceTypes: ["main_frame"] };

      await browser.declarativeNetRequest.updateSessionRules({
        addRules: [
          {
            id: 1000,
            condition,
            action: makeDummyAction("modifyHeaders"),
            // Same-or-lower priority "modifyHeaders" actions are ignored when
            // an "allowAllRequests" action exists within the same extension.
            // Since we have such a rule (ID 1001), this modifyHeaders rule must
            // have "priority: 2" to avoid being ignored.
            priority: 2,
          },
          { id: 1001, condition, action: makeDummyAction("allowAllRequests") },
          {
            id: 1002,
            condition,
            action: makeDummyAction("modifyHeaders"),
            priority: 2, // necessary as explained above at rule ID 1000.
          },
          // should never appear because the first allowAllRequests rule should
          // take precedence:
          { id: 1003, condition, action: makeDummyAction("allowAllRequests") },
        ],
      });

      // Notify to continue. We don't exit yet due to unloadTestAtEnd:false
      browser.test.notifyPass();
    },
    unloadTestAtEnd: false,
  });

  // |otherExtensionModifyHeaders| and |otherExtensionNonBlockAndModifyHeaders|
  // both have "modifyHeaders" rules. The documented order of rules is for
  // the most recently installed extension to take precedence when applying
  // modifyHeaders actions. The "priority" key is extension-specific, so even
  // though |otherExtensionNonBlockAndModifyHeaders| defines "priority: 2" for
  // modifyHeaders action (ID 1001), the modifyHeaders below (ID 1337) takes
  // precedence because the extension was installed later.
  let otherExtensionModifyHeaders = await runAsDNRExtension({
    manifest: { browser_specific_settings: { gecko: { id: "other@ext3" } } },
    background: async dnrTestUtils => {
      const { makeDummyAction } = dnrTestUtils;
      await browser.declarativeNetRequest.updateSessionRules({
        addRules: [
          {
            id: 1337,
            // Matches all requests from this test.
            condition: { resourceTypes: ["main_frame"] },
            action: makeDummyAction("modifyHeaders"),
            // Note: no "priority" key set, so defaults to 1.
          },
        ],
      });
      // Notify to continue. We don't exit yet due to unloadTestAtEnd:false
      browser.test.notifyPass();
    },
    unloadTestAtEnd: false,
  });

  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const dnr = browser.declarativeNetRequest;
      const { makeDummyAction } = dnrTestUtils;

      // Dummy condition for testing requests in this test.
      const c = tabId => ({ resourceTypes: ["main_frame"], tabIds: [tabId] });

      await dnr.updateSessionRules({
        addRules: [
          { id: 91, condition: c(1), action: makeDummyAction("block") },
          { id: 92, condition: c(2), action: makeDummyAction("allow") },
          { id: 93, condition: c(3), action: makeDummyAction("block") },
          { id: 94, condition: c(4), action: makeDummyAction("block") },
          { id: 95, condition: c(5), action: makeDummyAction("allow") },
          {
            id: 96,
            condition: c(6),
            action: makeDummyAction("allowAllRequests"),
          },
        ],
      });

      const url = "https://example.com/dummy-url";
      const type = "main_frame";
      const options = { includeOtherExtensions: true };
      browser.test.assertDeepEq(
        [{ ruleId: 91, rulesetId: "_session" }],
        (await dnr.testMatchOutcome({ url, type, tabId: 1 }, options))
          .matchedRules,
        "block takes precedence over allow (from other extension)"
      );

      browser.test.assertDeepEq(
        [{ ruleId: 12, rulesetId: "_session", extensionId: "other@ext" }],
        (await dnr.testMatchOutcome({ url, type, tabId: 2 }, options))
          .matchedRules,
        "block (from other extension) takes precedence over allow"
      );
      browser.test.assertDeepEq(
        [{ ruleId: 93, rulesetId: "_session" }],
        (await dnr.testMatchOutcome({ url, type, tabId: 3 }, options))
          .matchedRules,
        "block takes precedence over redirect (from other extension)"
      );
      browser.test.assertDeepEq(
        [{ ruleId: 94, rulesetId: "_session" }],
        (await dnr.testMatchOutcome({ url, type, tabId: 4 }, options))
          .matchedRules,
        "block takes precedence over upgradeScheme (from other extension)"
      );
      browser.test.assertDeepEq(
        [
          // allow:
          { ruleId: 95, rulesetId: "_session" },
          // allowAllRequests (newest install first):
          { ruleId: 1001, rulesetId: "_session", extensionId: "other@ext2" },
          { ruleId: 15, rulesetId: "_session", extensionId: "other@ext" },
          // modifyHeaders (see comment at otherExtensionModifyHeaders):
          { ruleId: 1337, rulesetId: "_session", extensionId: "other@ext3" },
          { ruleId: 1000, rulesetId: "_session", extensionId: "other@ext2" },
          { ruleId: 1002, rulesetId: "_session", extensionId: "other@ext2" },
        ],
        (await dnr.testMatchOutcome({ url, type, tabId: 5 }, options))
          .matchedRules,
        "When allow matches, allowAllRequests from other extension matches too"
      );
      browser.test.assertDeepEq(
        [
          // allowAllRequests (newest install first):
          { ruleId: 96, rulesetId: "_session" },
          { ruleId: 1001, rulesetId: "_session", extensionId: "other@ext2" },
          { ruleId: 16, rulesetId: "_session", extensionId: "other@ext" },
          // modifyHeaders (see comment at otherExtensionModifyHeaders):
          { ruleId: 1337, rulesetId: "_session", extensionId: "other@ext3" },
          { ruleId: 1000, rulesetId: "_session", extensionId: "other@ext2" },
          { ruleId: 1002, rulesetId: "_session", extensionId: "other@ext2" },
        ],
        (await dnr.testMatchOutcome({ url, type, tabId: 6 }, options))
          .matchedRules,
        "allowAllRequests from all other extensions are matched"
      );

      browser.test.notifyPass();
    },
  });

  await otherExtension.unload();
  await otherExtensionNonBlockAndModifyHeaders.unload();
  await otherExtensionModifyHeaders.unload();
});
