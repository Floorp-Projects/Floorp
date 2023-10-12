/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

const { BrowserUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/BrowserUtils.sys.mjs"
);

const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
  "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
);

const { Region } = ChromeUtils.importESModule(
  "resource://gre/modules/Region.sys.mjs"
);

const { updateAppInfo } = ChromeUtils.importESModule(
  "resource://testing-common/AppInfo.sys.mjs"
);

const { Preferences } = ChromeUtils.importESModule(
  "resource://gre/modules/Preferences.sys.mjs"
);

// Helper to run tests for specific regions
function setupRegions(home, current) {
  Region._setHomeRegion(home || "");
  Region._setCurrentRegion(current || "");
}

function setLanguage(language) {
  Services.locale.availableLocales = [language];
  Services.locale.requestedLocales = [language];
}

/**
 * Calls to this need to revert these changes by undoing them at the end of the test,
 * using:
 *
 *  await EnterprisePolicyTesting.setupPolicyEngineWithJson("");
 */
async function setupEnterprisePolicy() {
  // set app info name as it's needed to set a policy and is not defined by default
  // in xpcshell tests
  updateAppInfo({
    name: "XPCShell",
  });

  // set up an arbitrary enterprise policy
  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      EnableTrackingProtection: {
        Value: true,
      },
    },
  });
}

add_task(async function test_shouldShowVPNPromo() {
  function setPromoEnabled(enabled) {
    Services.prefs.setBoolPref("browser.vpn_promo.enabled", enabled);
  }

  const allowedRegion = "US";
  const disallowedRegion = "SY";
  const illegalRegion = "CN";
  const unsupportedRegion = "LY";
  const regionNotInDefaultPref = "QQ";

  // Show promo when enabled in allowed regions
  setupRegions(allowedRegion, allowedRegion);
  Assert.ok(BrowserUtils.shouldShowVPNPromo());

  // Don't show when not enabled
  setPromoEnabled(false);
  Assert.ok(!BrowserUtils.shouldShowVPNPromo());

  // Don't show in disallowed home regions, even when enabled
  setPromoEnabled(true);
  setupRegions(disallowedRegion);
  Assert.ok(!BrowserUtils.shouldShowVPNPromo());

  // Don't show in illegal home regions, even when enabled
  setupRegions(illegalRegion);
  Assert.ok(!BrowserUtils.shouldShowVPNPromo());

  // Don't show in disallowed current regions, even when enabled
  setupRegions(allowedRegion, disallowedRegion);
  Assert.ok(!BrowserUtils.shouldShowVPNPromo());

  // Don't show in illegal current regions, even when enabled
  setupRegions(allowedRegion, illegalRegion);
  Assert.ok(!BrowserUtils.shouldShowVPNPromo());

  // Show if home region is supported, even if current region is not supported (but isn't disallowed or illegal)
  setupRegions(allowedRegion, unsupportedRegion);
  Assert.ok(BrowserUtils.shouldShowVPNPromo());

  // Show VPN if current region is supported, even if home region is unsupported (but isn't disallowed or illegal)
  setupRegions(unsupportedRegion, allowedRegion); // revert changes to regions
  Assert.ok(BrowserUtils.shouldShowVPNPromo());

  // Make sure we are getting the list of allowed regions from the right
  // place.
  setupRegions(regionNotInDefaultPref);
  Services.prefs.setStringPref(
    "browser.contentblocking.report.vpn_regions",
    "qq"
  );
  Assert.ok(BrowserUtils.shouldShowVPNPromo());
  Services.prefs.clearUserPref("browser.contentblocking.report.vpn_regions");

  if (AppConstants.platform !== "android") {
    // Services.policies isn't shipped on Android
    // Don't show VPN if there's an active enterprise policy
    setupRegions(allowedRegion, allowedRegion);
    await setupEnterprisePolicy();

    Assert.ok(!BrowserUtils.shouldShowVPNPromo());

    // revert policy changes made earlier
    await EnterprisePolicyTesting.setupPolicyEngineWithJson("");
  }
});

add_task(async function test_sendToDeviceEmailsSupported() {
  const allowedLanguage = "en-US";
  const disallowedLanguage = "ar";

  // Return true if language is en-US
  setLanguage(allowedLanguage);
  Assert.ok(BrowserUtils.sendToDeviceEmailsSupported());

  // Return false if language is ar
  setLanguage(disallowedLanguage);
  Assert.ok(!BrowserUtils.sendToDeviceEmailsSupported());
});

add_task(async function test_shouldShowFocusPromo() {
  const allowedRegion = "US";
  const disallowedRegion = "CN";

  // Show promo when neither region is disallowed
  setupRegions(allowedRegion, allowedRegion);
  Assert.ok(BrowserUtils.shouldShowPromo(BrowserUtils.PromoType.FOCUS));

  // Don't show when home region is disallowed
  setupRegions(disallowedRegion);
  Assert.ok(!BrowserUtils.shouldShowPromo(BrowserUtils.PromoType.FOCUS));

  setupRegions(allowedRegion, allowedRegion);

  // Don't show when there is an enterprise policy active
  if (AppConstants.platform !== "android") {
    // Services.policies isn't shipped on Android
    await setupEnterprisePolicy();

    Assert.ok(!BrowserUtils.shouldShowPromo(BrowserUtils.PromoType.FOCUS));

    // revert policy changes made earlier
    await EnterprisePolicyTesting.setupPolicyEngineWithJson("");
  }

  // Don't show when promo disabled by pref
  Preferences.set("browser.promo.focus.enabled", false);
  Assert.ok(!BrowserUtils.shouldShowPromo(BrowserUtils.PromoType.FOCUS));

  Preferences.resetBranch("browser.promo.focus");
});

add_task(async function test_shouldShowPinPromo() {
  Preferences.set("browser.promo.pin.enabled", true);
  // Show pin promo type by default when promo is enabled
  Assert.ok(BrowserUtils.shouldShowPromo(BrowserUtils.PromoType.PIN));

  // Don't show when there is an enterprise policy active
  if (AppConstants.platform !== "android") {
    // Services.policies isn't shipped on Android
    await setupEnterprisePolicy();

    Assert.ok(!BrowserUtils.shouldShowPromo(BrowserUtils.PromoType.PIN));

    // revert policy changes made earlier
    await EnterprisePolicyTesting.setupPolicyEngineWithJson("");
  }

  // Don't show when promo disabled by pref
  Preferences.set("browser.promo.pin.enabled", false);
  Assert.ok(!BrowserUtils.shouldShowPromo(BrowserUtils.PromoType.PIN));

  Preferences.resetBranch("browser.promo.pin");
});

add_task(async function test_shouldShowRelayPromo() {
  // This test assumes by default no uri is configured.
  Preferences.set("identity.fxaccounts.autoconfig.uri", "");
  Assert.ok(BrowserUtils.shouldShowPromo(BrowserUtils.PromoType.RELAY));

  // Don't show when there is an enterprise policy active
  if (AppConstants.platform !== "android") {
    // Services.policies isn't shipped on Android
    await setupEnterprisePolicy();

    Assert.ok(!BrowserUtils.shouldShowPromo(BrowserUtils.PromoType.RELAY));

    // revert policy changes made earlier
    await EnterprisePolicyTesting.setupPolicyEngineWithJson("");
  }

  // Don't show if a custom FxA instance is configured
  Preferences.set("identity.fxaccounts.autoconfig.uri", "https://x");
  Assert.ok(!BrowserUtils.shouldShowPromo(BrowserUtils.PromoType.RELAY));

  Preferences.reset("identity.fxaccounts.autoconfig.uri");
});

add_task(async function test_shouldShowCookieBannersPromo() {
  Preferences.set("browser.promo.cookiebanners.enabled", true);
  // Show cookie banners promo type by default when promo is enabled
  Assert.ok(
    BrowserUtils.shouldShowPromo(BrowserUtils.PromoType.COOKIE_BANNERS)
  );

  // Don't show when promo disabled by pref
  Preferences.set("browser.promo.cookiebanners.enabled", false);
  Assert.ok(
    !BrowserUtils.shouldShowPromo(BrowserUtils.PromoType.COOKIE_BANNERS)
  );

  Preferences.resetBranch("browser.promo.cookiebanners");
});

add_task(function test_getShareableURL() {
  // Some test suites, specifically android, don't have this setup properly -- so we add it manually
  if (!Preferences.get("services.sync.engine.tabs.filteredSchemes")) {
    Preferences.set(
      "services.sync.engine.tabs.filteredSchemes",
      "about|resource|chrome|file|blob|moz-extension|data"
    );
  }
  // Empty shouldn't be sendable
  Assert.ok(!BrowserUtils.getShareableURL(""));
  // Valid
  let good = Services.io.newURI("https://mozilla.org");
  Assert.ok(BrowserUtils.getShareableURL(good).equals(good));
  // Invalid
  Assert.ok(
    !BrowserUtils.getShareableURL(Services.io.newURI("file://path/to/pdf.pdf"))
  );

  // Invalid
  Assert.ok(
    !BrowserUtils.getShareableURL(
      Services.io.newURI(
        "data:application/json;base64,ewogICJ0eXBlIjogIm1haW4i=="
      )
    )
  );

  // Reader mode:
  if (AppConstants.platform !== "android") {
    let readerUrl = Services.io.newURI(
      "about:reader?url=" + encodeURIComponent("http://foo.com/")
    );
    Assert.equal(
      BrowserUtils.getShareableURL(readerUrl).spec,
      "http://foo.com/"
    );
  }
});
