/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const { BrowserUtils } = ChromeUtils.import(
  "resource://gre/modules/BrowserUtils.jsm"
);

const { EnterprisePolicyTesting } = ChromeUtils.import(
  "resource://testing-common/EnterprisePolicyTesting.jsm"
);

const { Region } = ChromeUtils.import("resource://gre/modules/Region.jsm");

const { updateAppInfo } = ChromeUtils.import(
  "resource://testing-common/AppInfo.jsm"
);

const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
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

add_task(async function test_shouldShowRallyPromo() {
  const allowedRegion = "US";
  const disallowedRegion = "CN";
  const allowedLanguage = "en-US";
  const disallowedLanguage = "fr";

  // Show promo when region is US and language is en-US
  setupRegions(allowedRegion, allowedRegion);
  setLanguage(allowedLanguage);
  Assert.ok(BrowserUtils.shouldShowRallyPromo());

  // Don't show when home region is not US
  setupRegions(disallowedRegion);
  Assert.ok(!BrowserUtils.shouldShowRallyPromo());

  // Don't show when langauge is not en-US, even if region is US
  setLanguage(disallowedLanguage);
  setupRegions(allowedRegion);
  Assert.ok(!BrowserUtils.shouldShowRallyPromo());

  // Don't show when home region is not US, even if language is en-US
  setupRegions(disallowedRegion);
  Assert.ok(!BrowserUtils.shouldShowRallyPromo());

  // Don't show when current region is not US, even if home region is US and langague is en-US
  setupRegions(allowedRegion, disallowedRegion);
  Assert.ok(!BrowserUtils.shouldShowRallyPromo());
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

add_task(function test_isShareableURL() {
  // Some test suites, specifically android, don't have this setup properly -- so we add it manually
  if (!Preferences.get("services.sync.engine.tabs.filteredSchemes")) {
    Preferences.set(
      "services.sync.engine.tabs.filteredSchemes",
      "about|resource|chrome|file|blob|moz-extension|data"
    );
  }
  // Empty shouldn't be sendable
  Assert.ok(!BrowserUtils.isShareableURL(""));
  // Valid
  Assert.ok(
    BrowserUtils.isShareableURL(Services.io.newURI("https://mozilla.org"))
  );
  // Invalid
  Assert.ok(
    !BrowserUtils.isShareableURL(Services.io.newURI("file://path/to/pdf.pdf"))
  );

  // Invalid
  Assert.ok(
    !BrowserUtils.isShareableURL(
      Services.io.newURI(
        "data:application/json;base64,ewogICJ0eXBlIjogIm1haW4i=="
      )
    )
  );
});
