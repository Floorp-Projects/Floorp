/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let testRules = [
  // Cookie rule with multiple domains.
  {
    id: "0e4cdbb8-b688-47e0-9c8b-4db620398dbd",
    click: {},
    cookies: {
      optIn: [
        {
          name: "foo",
          value: "bar",
        },
      ],
    },
    domains: [TEST_DOMAIN_A, TEST_DOMAIN_B],
  },
  // Click rule with single domain.
  {
    id: "0560e02c-a50f-4e7b-86e0-d6b7d258eb5f",
    click: {
      optOut: "#optOutBtn",
      presence: "#cookieBanner",
    },
    cookies: {},
    domains: [TEST_DOMAIN_C],
  },
];

add_setup(async function () {
  // Enable the service and insert the test rules.
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode",
        Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      ],
      ["cookiebanners.listService.testSkipRemoteSettings", true],
      ["cookiebanners.listService.testRules", JSON.stringify(testRules)],
      ["cookiebanners.listService.logLevel", "Debug"],
    ],
  });

  Services.cookieBanners.resetRules(true);
});

function ruleCountForDomain(domain) {
  return Services.cookieBanners.rules.filter(rule =>
    rule.domains.includes(domain)
  ).length;
}

/**
 * Tests that the rules getter does not return duplicate rules for rules with
 * multiple domains.
 */
add_task(async function test_rules_getter_no_duplicates() {
  // The rule import is async because it needs to fetch rules from
  // RemoteSettings. Wait for the test rules to be applied.
  // See CookieBannerListService#importAllRules.
  await BrowserTestUtils.waitForCondition(
    () => Services.cookieBanners.rules.length,
    "Waiting for test rules to be imported."
  );
  is(
    Services.cookieBanners.rules.length,
    2,
    "Rules getter should only return the two test rules."
  );
  is(
    ruleCountForDomain(TEST_DOMAIN_A),
    1,
    "There should only be one rule with TEST_DOMAIN_A."
  );
  is(
    ruleCountForDomain(TEST_DOMAIN_B),
    1,
    "There should only be one rule with TEST_DOMAIN_B."
  );
  is(
    ruleCountForDomain(TEST_DOMAIN_C),
    1,
    "There should only be one rule with TEST_DOMAIN_C."
  );
});
