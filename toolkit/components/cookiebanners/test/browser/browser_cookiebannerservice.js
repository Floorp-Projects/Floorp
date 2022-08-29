/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function() {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("cookiebanners.service.mode");
    if (
      Services.prefs.getIntPref("cookiebanners.service.mode") !=
      Ci.nsICookieBannerService.MODE_DISABLED
    ) {
      // Restore original rules.
      Services.cookieBanners.resetRules(true);
    }
  });
});

add_task(async function test_enabled_pref() {
  info("Disabling cookie banner service.");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_DISABLED],
    ],
  });

  ok(Services.cookieBanners, "Services.cookieBanners is defined.");
  ok(
    Services.cookieBanners instanceof Ci.nsICookieBannerService,
    "Services.cookieBanners is nsICookieBannerService"
  );

  info(
    "Testing that methods throw NS_ERROR_NOT_AVAILABLE if the service is disabled."
  );

  Assert.throws(
    () => {
      Services.cookieBanners.rules;
    },
    /NS_ERROR_NOT_AVAILABLE/,
    "Should have thrown NS_ERROR_NOT_AVAILABLE for rules getter."
  );

  // Create a test rule to attempt to insert.
  let rule = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  rule.domain = "example.com";

  Assert.throws(
    () => {
      Services.cookieBanners.insertRule(rule);
    },
    /NS_ERROR_NOT_AVAILABLE/,
    "Should have thrown NS_ERROR_NOT_AVAILABLE for rules insertRule."
  );

  Assert.throws(
    () => {
      Services.cookieBanners.removeRuleForDomain("example.com");
    },
    /NS_ERROR_NOT_AVAILABLE/,
    "Should have thrown NS_ERROR_NOT_AVAILABLE for rules removeRuleForDomain."
  );

  Assert.throws(
    () => {
      Services.cookieBanners.getCookiesForURI(
        Services.io.newURI("https://example.com")
      );
    },
    /NS_ERROR_NOT_AVAILABLE/,
    "Should have thrown NS_ERROR_NOT_AVAILABLE for rules getCookiesForURI."
  );
  Assert.throws(
    () => {
      Services.cookieBanners.getClickRuleForDomain("example.com");
    },
    /NS_ERROR_NOT_AVAILABLE/,
    "Should have thrown NS_ERROR_NOT_AVAILABLE for rules getClickRuleForDomain."
  );

  info("Enabling cookie banner service. MODE_REJECT");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  let rules = Services.cookieBanners.rules;
  ok(
    Array.isArray(rules),
    "Rules getter should not throw but return an array."
  );

  info("Enabling cookie banner service. MODE_REJECT_OR_ACCEPT");
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode",
        Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      ],
    ],
  });

  rules = Services.cookieBanners.rules;
  ok(
    Array.isArray(rules),
    "Rules getter should not throw but return an array."
  );
});

add_task(async function test_insertAndGetRule() {
  info("Enabling cookie banner service with MODE_REJECT");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  info("Clear any preexisting rules");
  Services.cookieBanners.resetRules(false);

  is(
    Services.cookieBanners.rules.length,
    0,
    "Cookie banner service has no rules initially."
  );

  info("Test that we can't import rules with empty domain field.");
  let ruleInvalid = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  Assert.throws(
    () => {
      Services.cookieBanners.insertRule(ruleInvalid);
    },
    /NS_ERROR_FAILURE/,
    "Inserting an invalid rule missing a domain should throw."
  );

  let rule = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  rule.domain = "example.com";

  Services.cookieBanners.insertRule(rule);

  is(
    rule.cookiesOptOut.length,
    0,
    "Should not have any opt-out cookies initially"
  );
  is(
    rule.cookiesOptIn.length,
    0,
    "Should not have any opt-in cookies initially"
  );

  info("Clearing preexisting cookies rules for example.com.");
  rule.clearCookies();

  info("Adding cookies to the rule for example.com.");
  rule.addCookie(
    true,
    "foo",
    "bar",
    "example.com",
    "/",
    3600,
    "",
    false,
    false,
    false,
    0,
    0
  );
  rule.addCookie(
    true,
    "foobar",
    "barfoo",
    "example.com",
    "/",
    3600,
    "",
    false,
    false,
    false,
    0,
    0
  );
  rule.addCookie(
    false,
    "foo",
    "bar",
    "foo.example.com",
    "/myPath",
    3600,
    "",
    false,
    false,
    true,
    0,
    0
  );

  info("Adding a click rule to the rule for example.com.");
  rule.addClickRule("div#presence", "div#hide", "div#optOut", "div#optIn");

  is(rule.cookiesOptOut.length, 2, "Should have two opt-out cookies.");
  is(rule.cookiesOptIn.length, 1, "Should have one opt-in cookie.");

  is(
    Services.cookieBanners.rules.length,
    1,
    "Cookie Banner Service has one rule."
  );

  let rule2 = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  rule2.domain = "example.org";

  Services.cookieBanners.insertRule(rule2);
  info("Clearing preexisting cookies rules for example.org.");
  rule2.clearCookies();

  info("Adding a cookie to the rule for example.org.");
  rule2.addCookie(
    false,
    "foo2",
    "bar2",
    "example.org",
    "/",
    0,
    "",
    false,
    false,
    false,
    0,
    0
  );

  info("Adding a click rule to the rule for example.org.");
  rule2.addClickRule("div#presence", null, null, "div#optIn");

  is(
    Services.cookieBanners.rules.length,
    2,
    "Cookie Banner Service has two rules."
  );

  info("Getting cookies by URI for example.com.");
  let ruleArray = Services.cookieBanners.getCookiesForURI(
    Services.io.newURI("http://example.com")
  );
  ok(
    ruleArray && Array.isArray(ruleArray),
    "getCookiesForURI should return a rule array."
  );
  is(ruleArray.length, 2, "rule array should contain 2 rules.");
  ruleArray.every(rule => {
    ok(rule instanceof Ci.nsICookieRule, "Rule should have correct type.");
    is(rule.cookie.host, "example.com", "Rule should have correct host.");
  });

  info("Clearing cookies of rule.");
  rule.clearCookies();
  is(rule.cookiesOptOut.length, 0, "Should have no opt-out cookies.");
  is(rule.cookiesOptIn.length, 0, "Should have no opt-in cookies.");

  info("Getting the click rule for example.com.");
  let clickRule = Services.cookieBanners.getClickRuleForDomain("example.com");
  is(
    clickRule.presence,
    "div#presence",
    "Should have the correct presence selector."
  );
  is(clickRule.hide, "div#hide", "Should have the correct hide selector.");
  is(
    clickRule.optOut,
    "div#optOut",
    "Should have the correct optOut selector."
  );
  is(clickRule.optIn, "div#optIn", "Should have the correct optIn selector.");

  info("Getting cookies by URI for example.org.");
  let ruleArray2 = Services.cookieBanners.getCookiesForURI(
    Services.io.newURI("http://example.org")
  );
  ok(
    ruleArray2 && Array.isArray(ruleArray2),
    "getCookiesForURI should return a rule array."
  );
  is(
    ruleArray2.length,
    0,
    "rule array should contain no rules in MODE_REJECT (opt-out only)"
  );

  info("Getting the click rule for example.org.");
  let clickRule2 = Services.cookieBanners.getClickRuleForDomain("example.org");
  is(
    clickRule2.presence,
    "div#presence",
    "Should have the correct presence selector."
  );
  ok(!clickRule2.hide, "Should have no hide selector.");
  ok(!clickRule2.optOut, "Should have no target selector.");
  is(clickRule.optIn, "div#optIn", "Should have the correct optIn selector.");

  info("Switching cookiebanners.service.mode to MODE_REJECT_OR_ACCEPT.");
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode",
        Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      ],
    ],
  });

  ruleArray2 = Services.cookieBanners.getCookiesForURI(
    Services.io.newURI("http://example.org")
  );
  ok(
    ruleArray2 && Array.isArray(ruleArray2),
    "getCookiesForURI should return a rule array."
  );
  is(
    ruleArray2.length,
    1,
    "rule array should contain one rule in mode MODE_REJECT_OR_ACCEPT (opt-out or opt-in)"
  );

  info("Calling getCookiesForURI for unknown domain.");
  let ruleArrayUnknown = Services.cookieBanners.getCookiesForURI(
    Services.io.newURI("http://example.net")
  );
  ok(
    ruleArrayUnknown && Array.isArray(ruleArrayUnknown),
    "getCookiesForURI should return a rule array."
  );
  is(ruleArrayUnknown.length, 0, "rule array should contain no rules.");

  // Cleanup.
  Services.cookieBanners.resetRules(false);
});

add_task(async function test_removeRule() {
  info("Enabling cookie banner service with MODE_REJECT");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  info("Clear any preexisting rules");
  Services.cookieBanners.resetRules(false);

  is(
    Services.cookieBanners.rules.length,
    0,
    "Cookie banner service has no rules initially."
  );

  let rule = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  rule.domain = "example.com";

  Services.cookieBanners.insertRule(rule);

  let rule2 = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  rule2.domain = "example.org";

  Services.cookieBanners.insertRule(rule2);

  is(
    Services.cookieBanners.rules.length,
    2,
    "Cookie banner service two rules after insert."
  );

  info("Removing rule for non existent example.net");
  Services.cookieBanners.removeRuleForDomain("example.net");

  is(
    Services.cookieBanners.rules.length,
    2,
    "Cookie banner service still has two rules."
  );

  info("Removing rule for non example.com");
  Services.cookieBanners.removeRuleForDomain("example.com");

  is(
    Services.cookieBanners.rules.length,
    1,
    "Cookie banner service should have one rule left after remove."
  );

  is(
    Services.cookieBanners.rules[0].domain,
    "example.org",
    "It should be the example.org rule."
  );

  // Cleanup.
  Services.cookieBanners.resetRules(false);
});

add_task(async function test_overwriteRule() {
  info("Enabling cookie banner service with MODE_REJECT");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  info("Clear any preexisting rules");
  Services.cookieBanners.resetRules(false);

  is(
    Services.cookieBanners.rules.length,
    0,
    "Cookie banner service has no rules initially."
  );

  let rule = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  rule.domain = "example.com";

  info("Adding a cookie so we can detect if the rule updates.");
  rule.addCookie(
    true,
    "foo",
    "original",
    "example.com",
    "/",
    3600,
    "",
    false,
    false,
    false,
    0,
    0
  );

  info("Adding a click rule so we can detect if the rule updates.");
  rule.addClickRule("div#original");

  Services.cookieBanners.insertRule(rule);

  let { cookie } = Services.cookieBanners.rules[0].cookiesOptOut[0];

  is(cookie.name, "foo", "Should have set the correct cookie name.");
  is(cookie.value, "original", "Should have set the correct cookie value.");

  info("Add a new rule with the same domain. It should be overwritten.");

  let ruleNew = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  ruleNew.domain = "example.com";

  ruleNew.addCookie(
    true,
    "foo",
    "new",
    "example.com",
    "/",
    3600,
    "",
    false,
    false,
    false,
    0,
    0
  );

  ruleNew.addClickRule("div#new");

  Services.cookieBanners.insertRule(ruleNew);

  let { cookie: cookieNew } = Services.cookieBanners.rules[0].cookiesOptOut[0];
  is(cookieNew.name, "foo", "Should have set the original cookie name.");
  is(cookieNew.value, "new", "Should have set the updated cookie value.");

  let { presence: presenceNew } = Services.cookieBanners.rules[0].clickRule;
  is(presenceNew, "div#new", "Should have set the updated presence value");

  // Cleanup.
  Services.cookieBanners.resetRules(false);
});
