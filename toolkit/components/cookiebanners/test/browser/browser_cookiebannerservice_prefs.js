/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function () {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("cookiebanners.service.mode");
    Services.prefs.clearUserPref("cookiebanners.service.mode.privateBrowsing");
    if (
      Services.prefs.getIntPref("cookiebanners.service.mode") !=
        Ci.nsICookieBannerService.MODE_DISABLED ||
      Services.prefs.getIntPref("cookiebanners.service.mode.privateBrowsing") !=
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
      [
        "cookiebanners.service.mode.privateBrowsing",
        Ci.nsICookieBannerService.MODE_DISABLED,
      ],
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
  rule.id = genUUID();
  rule.domains = ["example.com"];

  Assert.throws(
    () => {
      Services.cookieBanners.insertRule(rule);
    },
    /NS_ERROR_NOT_AVAILABLE/,
    "Should have thrown NS_ERROR_NOT_AVAILABLE for insertRule."
  );

  Assert.throws(
    () => {
      Services.cookieBanners.removeRule(rule);
    },
    /NS_ERROR_NOT_AVAILABLE/,
    "Should have thrown NS_ERROR_NOT_AVAILABLE for removeRule."
  );

  Assert.throws(
    () => {
      Services.cookieBanners.getCookiesForURI(
        Services.io.newURI("https://example.com"),
        false
      );
    },
    /NS_ERROR_NOT_AVAILABLE/,
    "Should have thrown NS_ERROR_NOT_AVAILABLE for rules getCookiesForURI."
  );
  Assert.throws(
    () => {
      Services.cookieBanners.getClickRulesForDomain("example.com", true);
    },
    /NS_ERROR_NOT_AVAILABLE/,
    "Should have thrown NS_ERROR_NOT_AVAILABLE for rules getClickRuleForDomain."
  );
  let uri = Services.io.newURI("https://example.com");
  Assert.throws(
    () => {
      Services.cookieBanners.getDomainPref(uri, false);
    },
    /NS_ERROR_NOT_AVAILABLE/,
    "Should have thrown NS_ERROR_NOT_AVAILABLE for getDomainPref."
  );
  Assert.throws(
    () => {
      Services.cookieBanners.setDomainPref(
        uri,
        Ci.nsICookieBannerService.MODE_REJECT,
        false
      );
    },
    /NS_ERROR_NOT_AVAILABLE/,
    "Should have thrown NS_ERROR_NOT_AVAILABLE for setDomainPref."
  );
  Assert.throws(
    () => {
      Services.cookieBanners.setDomainPrefAndPersistInPrivateBrowsing(
        uri,
        Ci.nsICookieBannerService.MODE_REJECT
      );
    },
    /NS_ERROR_NOT_AVAILABLE/,
    "Should have thrown NS_ERROR_NOT_AVAILABLE for setDomainPrefAndPersistInPrivateBrowsing."
  );
  Assert.throws(
    () => {
      Services.cookieBanners.removeDomainPref(uri, false);
    },
    /NS_ERROR_NOT_AVAILABLE/,
    "Should have thrown NS_ERROR_NOT_AVAILABLE for removeDomainPref."
  );
  Assert.throws(
    () => {
      Services.cookieBanners.removeAllDomainPrefs(false);
    },
    /NS_ERROR_NOT_AVAILABLE/,
    "Should have thrown NS_ERROR_NOT_AVAILABLE for removeAllSitePref."
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

/**
 * Test both service mode pref combinations to ensure the cookie banner service
 * is (un-)initialized correctly.
 */
add_task(async function test_enabled_pref_pbm_combinations() {
  const MODES = [
    Ci.nsICookieBannerService.MODE_DISABLED,
    Ci.nsICookieBannerService.MODE_REJECT,
    Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
  ];

  // Test all pref combinations
  MODES.forEach(modeNormal => {
    MODES.forEach(modePrivate => {
      info(
        `cookiebanners.service.mode=${modeNormal}; cookiebanners.service.mode.privateBrowsing=${modePrivate}`
      );
      Services.prefs.setIntPref("cookiebanners.service.mode", modeNormal);
      Services.prefs.setIntPref(
        "cookiebanners.service.mode.privateBrowsing",
        modePrivate
      );

      if (
        modeNormal == Ci.nsICookieBannerService.MODE_DISABLED &&
        modePrivate == Ci.nsICookieBannerService.MODE_DISABLED
      ) {
        Assert.throws(
          () => {
            Services.cookieBanners.rules;
          },
          /NS_ERROR_NOT_AVAILABLE/,
          "Cookie banner service should be disabled. Should throw NS_ERROR_NOT_AVAILABLE for rules getter."
        );
      } else {
        ok(
          Services.cookieBanners.rules,
          "Cookie banner service should be enabled, rules getter should not throw."
        );
      }
    });
  });

  // Cleanup.
  Services.prefs.clearUserPref("cookiebanners.service.mode");
  Services.prefs.clearUserPref("cookiebanners.service.mode.privateBrowsing");
});
