/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SiteDataTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SiteDataTestUtils.sys.mjs"
);

let testRules = [
  // Top-level cookie rule.
  {
    id: "87815b2d-a840-4155-8713-f8a26d1f483a",
    click: {},
    cookies: {
      optIn: [
        {
          name: "foo",
          value: "bar",
        },
      ],
    },
    domains: [TEST_DOMAIN_B],
  },
  // Child click rule.
  {
    id: "d42bbaee-f96e-47e7-8e81-efc642518e97",
    click: {
      optOut: "#optOutBtn",
      presence: "#cookieBanner",
      runContext: "child",
    },
    cookies: {},
    domains: [TEST_DOMAIN_C],
  },
  // Top level click rule.
  {
    id: "19dd1f52-f3e6-4a24-a926-d77f553d1b15",
    click: {
      optOut: "#optOutBtn",
      presence: "#cookieBanner",
    },
    cookies: {},
    domains: [TEST_DOMAIN_A],
  },
];

/**
 * Insert an iframe and wait for it to load.
 * @param {BrowsingContext} parentBC - The BC the frame to insert under.
 * @param {string} uri - The URI to load in the frame.
 * @returns {Promise} - A Promise which resolves once the frame has loaded.
 */
function insertIframe(parentBC, uri) {
  return SpecialPowers.spawn(parentBC, [uri], async testURL => {
    let iframe = content.document.createElement("iframe");
    iframe.src = testURL;
    content.document.body.appendChild(iframe);
    await ContentTaskUtils.waitForEvent(iframe, "load");
    return iframe.browsingContext;
  });
}

add_setup(async function () {
  // Enable the service and insert the test rules. We only test
  // MODE_REJECT_OR_ACCEPT here as the other modes are covered by other tests
  // already and hasRuleForBrowsingContextTree mostly shares logic with other
  // service getters.
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode",
        Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      ],
      ["cookiebanners.listService.testSkipRemoteSettings", true],
      ["cookiebanners.listService.testRules", JSON.stringify(testRules)],
    ],
  });

  // Ensure the test rules have been applied before the first test starts.
  Services.cookieBanners.resetRules();

  // Visiting sites in this test can set cookies. Clean them up on test exit.
  registerCleanupFunction(async () => {
    await SiteDataTestUtils.clear();
  });
});

add_task(async function test_unsupported() {
  let unsupportedURIs = {
    "about:preferences": /NS_ERROR_FAILURE/,
    "about:blank": false,
  };

  for (let [key, value] of Object.entries(unsupportedURIs)) {
    await BrowserTestUtils.withNewTab(key, async browser => {
      if (typeof value == "object") {
        // It's an error code.
        Assert.throws(
          () => {
            Services.cookieBanners.hasRuleForBrowsingContextTree(
              browser.browsingContext
            );
          },
          value,
          `Should throw ${value} for hasRuleForBrowsingContextTree call for '${key}'.`
        );
      } else {
        is(
          Services.cookieBanners.hasRuleForBrowsingContextTree(
            browser.browsingContext
          ),
          value,
          `Should return expected value for hasRuleForBrowsingContextTree for '${key}'`
        );
      }
    });
  }
});

add_task(async function test_hasRuleForBCTree() {
  info("Test with top level A");
  await BrowserTestUtils.withNewTab(TEST_ORIGIN_A, async browser => {
    let bcTop = browser.browsingContext;

    ok(
      Services.cookieBanners.hasRuleForBrowsingContextTree(bcTop),
      "Should have rule when called with top BC for A"
    );

    info("inserting frame with TEST_ORIGIN_A");
    let bcChildA = await insertIframe(bcTop, TEST_ORIGIN_A);
    ok(
      Services.cookieBanners.hasRuleForBrowsingContextTree(bcTop),
      "Should still have rule when called with top BC for A."
    );
    ok(
      !Services.cookieBanners.hasRuleForBrowsingContextTree(bcChildA),
      "Should not have rule when called with child BC for A, because A has no child click-rule."
    );
  });

  info("Test with top level C");
  await BrowserTestUtils.withNewTab(TEST_ORIGIN_C, async browser => {
    let bcTop = browser.browsingContext;

    ok(
      !Services.cookieBanners.hasRuleForBrowsingContextTree(bcTop),
      "Should have no rule when called with top BC for C, because C only has a child click rule."
    );

    info("inserting frame with TEST_ORIGIN_C");
    let bcChildC = await insertIframe(bcTop, TEST_ORIGIN_C);

    info("inserting unrelated frames");
    await insertIframe(bcTop, "https://itisatracker.org");
    await insertIframe(bcChildC, "https://itisatracker.org");

    ok(
      Services.cookieBanners.hasRuleForBrowsingContextTree(bcTop),
      "Should have rule when called with top BC for C, because frame C has a child click rule."
    );
    ok(
      Services.cookieBanners.hasRuleForBrowsingContextTree(bcChildC),
      "Should have rule when called with child BC for C, because it has a child click rule."
    );
  });

  info("Test with unrelated top level");
  await BrowserTestUtils.withNewTab("http://mochi.test:8888", async browser => {
    let bcTop = browser.browsingContext;

    ok(
      !Services.cookieBanners.hasRuleForBrowsingContextTree(bcTop),
      "Should not have rule for unrelated site."
    );

    info("inserting frame with TEST_ORIGIN_A");
    let bcChildA = await insertIframe(bcTop, TEST_ORIGIN_A);
    ok(
      !Services.cookieBanners.hasRuleForBrowsingContextTree(bcTop),
      "Should still have no rule when called with top BC for A, because click rule for A only applies top-level."
    );
    ok(
      !Services.cookieBanners.hasRuleForBrowsingContextTree(bcChildA),
      "Should have no rule when called with child BC for A."
    );

    info("inserting frame with TEST_ORIGIN_B");
    let bcChildB = await insertIframe(bcTop, TEST_ORIGIN_B);
    ok(
      !Services.cookieBanners.hasRuleForBrowsingContextTree(bcTop),
      "Should still have no rule when called with top BC for A, because cookie rule for B only applies top-level."
    );
    ok(
      !Services.cookieBanners.hasRuleForBrowsingContextTree(bcChildA),
      "Should have no rule when called with child BC for A."
    );
    ok(
      !Services.cookieBanners.hasRuleForBrowsingContextTree(bcChildB),
      "Should have no rule when called with child BC for B."
    );

    info("inserting nested frame with TEST_ORIGIN_C");
    let bcChildC = await insertIframe(bcChildB, TEST_ORIGIN_C);
    ok(
      Services.cookieBanners.hasRuleForBrowsingContextTree(bcTop),
      "Should have rule when called with top level BC because rule for nested iframe C applies."
    );
    ok(
      !Services.cookieBanners.hasRuleForBrowsingContextTree(bcChildA),
      "Should have no rule when called with child BC for A."
    );
    ok(
      Services.cookieBanners.hasRuleForBrowsingContextTree(bcChildB),
      "Should have rule when called with child BC for B, because C rule for nested iframe C applies."
    );
    ok(
      Services.cookieBanners.hasRuleForBrowsingContextTree(bcChildC),
      "Should have rule when called with child BC for C, because C rule for nested iframe C applies."
    );
  });
});

/**
 * Tests that domain prefs are not considered when evaluating whether the
 * service has an applicable rule for the given BrowsingContext.
 */
add_task(async function test_hasRuleForBCTree_ignoreDomainPrefs() {
  info("Test with top level A");
  await BrowserTestUtils.withNewTab(TEST_ORIGIN_A, async browser => {
    let bcTop = browser.browsingContext;

    ok(
      Services.cookieBanners.hasRuleForBrowsingContextTree(bcTop),
      "Should have rule when called with top BC for A"
    );

    // Disable for current site per domain pref.
    Services.cookieBanners.setDomainPref(
      browser.currentURI,
      Ci.nsICookieBannerService.MODE_DISABLED,
      false
    );
    ok(
      Services.cookieBanners.hasRuleForBrowsingContextTree(bcTop),
      "Should have rule when called with top BC for A, even if mechanism is disabled for A."
    );

    // Change mode via domain pref.
    Services.cookieBanners.setDomainPref(
      browser.currentURI,
      Ci.nsICookieBannerService.MODE_REJECT,
      false
    );
    ok(
      Services.cookieBanners.hasRuleForBrowsingContextTree(bcTop),
      "Should still have rule when called with top BC for A, even with custom mode for A"
    );

    // Cleanup.
    Services.cookieBanners.removeAllDomainPrefs(false);
  });

  info("Test with top level B");
  await BrowserTestUtils.withNewTab(TEST_ORIGIN_B, async browser => {
    let bcTop = browser.browsingContext;

    ok(
      Services.cookieBanners.hasRuleForBrowsingContextTree(bcTop),
      "Should have rule when called with top BC for B"
    );

    // Change mode via domain pref.
    Services.cookieBanners.setDomainPref(
      browser.currentURI,
      Ci.nsICookieBannerService.MODE_REJECT,
      false
    );
    // Rule for B has no opt-out option. Since the mode is overridden to
    // MODE_REJECT for B we don't have any applicable rule for it. This should
    // however not be considered for the hasRule getter, it should ignore
    // per-domain preferences and evaluate based on the global service mode
    // instead.
    ok(
      Services.cookieBanners.hasRuleForBrowsingContextTree(bcTop),
      "Should still have rule when called with top BC for B, even with custom mode for B"
    );

    // Cleanup.
    Services.cookieBanners.removeAllDomainPrefs(false);
  });
});
