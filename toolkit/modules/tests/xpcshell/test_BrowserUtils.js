/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

const { BrowserUtils } = ChromeUtils.import(
  "resource://gre/modules/BrowserUtils.jsm"
);

const { Region } = ChromeUtils.import("resource://gre/modules/Region.jsm");

const initialHomeRegion = Region._home;
const initialCurrentRegion = Region._current;

// Helper to run tests for specific regions
function setupRegions(home, current) {
  Region._setHomeRegion(home || "");
  Region._setCurrentRegion(current || "");
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

  setupRegions(initialHomeRegion, initialCurrentRegion); // revert changes to regions
});

add_task(async function test_shouldShowRallyPromo() {
  const initialLanguage = Services.locale.appLocaleAsBCP47;
  const allowedRegion = "US";
  const disallowedRegion = "CN";
  const allowedLanguage = "en-US";
  const disallowedLanguage = "fr";

  function setLanguage(language) {
    Services.locale.availableLocales = [language];
    Services.locale.requestedLocales = [language];
  }

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

  setupRegions(initialHomeRegion, initialCurrentRegion); // revert changes to regions
  setLanguage(initialLanguage); // revert changes to language
});
