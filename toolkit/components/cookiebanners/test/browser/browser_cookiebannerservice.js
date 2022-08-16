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
  Assert.throws(
    () => {
      Services.cookieBanners.lookupOrInsertRuleForDomain("example.com");
    },
    /NS_ERROR_NOT_AVAILABLE/,
    "Should have thrown NS_ERROR_NOT_AVAILABLE for rules lookupOrInsertRuleForDomain."
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

  let rule = Services.cookieBanners.lookupOrInsertRuleForDomain("example.com");
  ok(rule, "lookupOrInsertRuleForDomain returns rule");
  ok(
    rule instanceof Ci.nsICookieBannerRule,
    "Rule is of type nsICookieBannerRule"
  );

  info("Clearing preexisting cookies rules for example.com.");
  rule.clearCookies();

  rule.addCookie(true, "example.com", "foo", "bar");
  rule.addCookie(true, "example.com", "foobar", "barfoo");
  rule.addCookie(false, "foo.example.com", "foo", "bar", "/myPath", 3600);

  is(rule.cookiesOptOut.length, 2, "Should have two opt-out cookies.");
  is(rule.cookiesOptIn.length, 1, "Should have one opt-in cookie.");

  is(
    Services.cookieBanners.rules.length,
    1,
    "Cookie Banner Service has one rule."
  );

  let rule2 = Services.cookieBanners.lookupOrInsertRuleForDomain("example.org");
  info("Clearing preexisting cookies rules for example.org.");
  rule2.clearCookies();

  rule2.addCookie(false, "example.org", "foo2", "bar2");

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
});
