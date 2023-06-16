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

add_task(async function test_domain_preference() {
  info("Enabling cookie banner service with MODE_REJECT");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  let uri = Services.io.newURI("http://example.com");

  // Check no site preference at the beginning
  is(
    Services.cookieBanners.getDomainPref(uri, false),
    Ci.nsICookieBannerService.MODE_UNSET,
    "There should be no per site preference at the beginning."
  );

  // Check setting and getting a site preference.
  Services.cookieBanners.setDomainPref(
    uri,
    Ci.nsICookieBannerService.MODE_REJECT,
    false
  );

  is(
    Services.cookieBanners.getDomainPref(uri, false),
    Ci.nsICookieBannerService.MODE_REJECT,
    "Can get site preference for example.com with the correct value."
  );

  // Check site preference is shared between http and https.
  let uriHttps = Services.io.newURI("https://example.com");
  is(
    Services.cookieBanners.getDomainPref(uriHttps, false),
    Ci.nsICookieBannerService.MODE_REJECT,
    "Can get site preference for example.com in secure context."
  );

  // Check site preference in the other domain, example.org.
  let uriOther = Services.io.newURI("https://example.org");
  is(
    Services.cookieBanners.getDomainPref(uriOther, false),
    Ci.nsICookieBannerService.MODE_UNSET,
    "There should be no domain preference for example.org."
  );

  // Check setting site preference won't affect the other domain.
  Services.cookieBanners.setDomainPref(
    uriOther,
    Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
    false
  );

  is(
    Services.cookieBanners.getDomainPref(uriOther, false),
    Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
    "Can get domain preference for example.org with the correct value."
  );
  is(
    Services.cookieBanners.getDomainPref(uri, false),
    Ci.nsICookieBannerService.MODE_REJECT,
    "Can get site preference for example.com"
  );

  // Check nsICookieBannerService.setDomainPrefAndPersistInPrivateBrowsing().
  Services.cookieBanners.setDomainPrefAndPersistInPrivateBrowsing(
    uri,
    Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT
  );
  is(
    Services.cookieBanners.getDomainPref(uri, true),
    Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
    "Can get site preference for example.com"
  );

  // Check removing the site preference.
  Services.cookieBanners.removeDomainPref(uri, false);
  is(
    Services.cookieBanners.getDomainPref(uri, false),
    Ci.nsICookieBannerService.MODE_UNSET,
    "There should be no site preference for example.com."
  );

  // Check remove all site preferences.
  Services.cookieBanners.removeAllDomainPrefs(false);
  is(
    Services.cookieBanners.getDomainPref(uri, false),
    Ci.nsICookieBannerService.MODE_UNSET,
    "There should be no site preference for example.com."
  );
  is(
    Services.cookieBanners.getDomainPref(uriOther, false),
    Ci.nsICookieBannerService.MODE_UNSET,
    "There should be no site preference for example.org."
  );
});

add_task(async function test_domain_preference_dont_override_disable_pref() {
  info("Enabling cookie banner service with MODE_REJECT");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  info("Adding a domain preference for example.com");
  let uri = Services.io.newURI("https://example.com");

  // Set a domain preference.
  Services.cookieBanners.setDomainPref(
    uri,
    Ci.nsICookieBannerService.MODE_REJECT,
    false
  );

  info("Disabling the cookie banner service.");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_DISABLED],
      [
        "cookiebanners.service.mode.privateBrowsing",
        Ci.nsICookieBannerService.MODE_DISABLED,
      ],
    ],
  });

  info("Verifying if the cookie banner service is disabled.");
  Assert.throws(
    () => {
      Services.cookieBanners.getDomainPref(uri, false);
    },
    /NS_ERROR_NOT_AVAILABLE/,
    "Should have thrown NS_ERROR_NOT_AVAILABLE for getDomainPref."
  );

  info("Enable the service again in order to clear the domain prefs.");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });
  Services.cookieBanners.removeAllDomainPrefs(false);
});

/**
 * Test that domain preference is properly cleared when private browsing session
 * ends.
 */
add_task(async function test_domain_preference_cleared_PBM_ends() {
  info("Enabling cookie banner service with MODE_REJECT");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  info("Adding a domain preference for example.com in PBM");
  let uri = Services.io.newURI("https://example.com");

  info("Open a private browsing window.");
  let PBMWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  // Set a domain preference for PBM.
  Services.cookieBanners.setDomainPref(
    uri,
    Ci.nsICookieBannerService.MODE_DISABLED,
    true
  );

  info("Verifying if the cookie banner domain pref is set for PBM.");
  is(
    Services.cookieBanners.getDomainPref(uri, true),
    Ci.nsICookieBannerService.MODE_DISABLED,
    "The domain pref is properly set for PBM."
  );

  info("Trigger an ending of a private browsing window session");
  let PBMSessionEndsObserved = TestUtils.topicObserved(
    "last-pb-context-exited"
  );

  // Close the PBM window and wait until it finishes.
  await BrowserTestUtils.closeWindow(PBMWin);
  await PBMSessionEndsObserved;

  info("Verify if the private domain pref is cleared.");
  is(
    Services.cookieBanners.getDomainPref(uri, true),
    Ci.nsICookieBannerService.MODE_UNSET,
    "The domain pref is properly set for PBM."
  );
});

/**
 * Test that the persistent domain preference won't be cleared when private
 * browsing session ends.
 */
add_task(async function test_persistent_domain_preference_remain_PBM_ends() {
  info("Enabling cookie banner service with MODE_REJECT");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  info("Adding a domain preference for example.com in PBM");
  let uri = Services.io.newURI("https://example.com");

  info("Open a private browsing window.");
  let PBMWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  // Set a domain preference for PBM.
  Services.cookieBanners.setDomainPref(
    uri,
    Ci.nsICookieBannerService.MODE_DISABLED,
    true
  );

  info("Verifying if the cookie banner domain pref is set for PBM.");
  is(
    Services.cookieBanners.getDomainPref(uri, true),
    Ci.nsICookieBannerService.MODE_DISABLED,
    "The domain pref is properly set for PBM."
  );

  info("Adding a persistent domain preference for example.org in PBM");
  let uriPersistent = Services.io.newURI("https://example.org");

  // Set a persistent domain preference for PBM.
  Services.cookieBanners.setDomainPrefAndPersistInPrivateBrowsing(
    uriPersistent,
    Ci.nsICookieBannerService.MODE_DISABLED
  );

  info("Trigger an ending of a private browsing window session");
  let PBMSessionEndsObserved = TestUtils.topicObserved(
    "last-pb-context-exited"
  );

  // Close the PBM window and wait until it finishes.
  await BrowserTestUtils.closeWindow(PBMWin);
  await PBMSessionEndsObserved;

  info("Verify if the private domain pref is cleared.");
  is(
    Services.cookieBanners.getDomainPref(uri, true),
    Ci.nsICookieBannerService.MODE_UNSET,
    "The domain pref is properly set for PBM."
  );

  info("Verify if the persistent private domain pref remains.");
  is(
    Services.cookieBanners.getDomainPref(uriPersistent, true),
    Ci.nsICookieBannerService.MODE_DISABLED,
    "The persistent domain pref remains for PBM after private session ends."
  );
});

add_task(async function test_remove_persistent_domain_pref_in_PBM() {
  info("Enabling cookie banner service with MODE_REJECT");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  info("Adding a domain preference for example.com in PBM");
  let uri = Services.io.newURI("https://example.com");

  info("Open a private browsing window.");
  let PBMWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  // Set a persistent domain preference for PBM.
  Services.cookieBanners.setDomainPrefAndPersistInPrivateBrowsing(
    uri,
    Ci.nsICookieBannerService.MODE_DISABLED
  );

  info("Verifying if the cookie banner domain pref is set for PBM.");
  is(
    Services.cookieBanners.getDomainPref(uri, true),
    Ci.nsICookieBannerService.MODE_DISABLED,
    "The domain pref is properly set for PBM."
  );

  info("Remove the persistent domain pref.");
  Services.cookieBanners.removeDomainPref(uri, true);

  info("Trigger an ending of a private browsing window session");
  let PBMSessionEndsObserved = TestUtils.topicObserved(
    "last-pb-context-exited"
  );

  // Close the PBM window and wait until it finishes.
  await BrowserTestUtils.closeWindow(PBMWin);
  await PBMSessionEndsObserved;

  info(
    "Verify if the private domain pref is no longer persistent and cleared."
  );
  is(
    Services.cookieBanners.getDomainPref(uri, true),
    Ci.nsICookieBannerService.MODE_UNSET,
    "The domain pref is properly set for PBM."
  );
});

/**
 * Test that the persistent state of a domain pref in PMB can be override by new
 * call without persistent state.
 */
add_task(async function test_override_persistent_state_in_PBM() {
  info("Enabling cookie banner service with MODE_REJECT");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  info("Adding a domain preference for example.com in PBM");
  let uri = Services.io.newURI("https://example.com");

  info("Open a private browsing window.");
  let PBMWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  // Set a persistent domain preference for PBM.
  Services.cookieBanners.setDomainPrefAndPersistInPrivateBrowsing(
    uri,
    Ci.nsICookieBannerService.MODE_DISABLED
  );

  info("Trigger an ending of a private browsing window session");
  let PBMSessionEndsObserved = TestUtils.topicObserved(
    "last-pb-context-exited"
  );

  // Close the PBM window and wait until it finishes.
  await BrowserTestUtils.closeWindow(PBMWin);
  await PBMSessionEndsObserved;

  info("Verify if the persistent private domain pref remains.");
  is(
    Services.cookieBanners.getDomainPref(uri, true),
    Ci.nsICookieBannerService.MODE_DISABLED,
    "The persistent domain pref remains for PBM after private session ends."
  );

  info("Open a private browsing window again.");
  PBMWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  info("Override the persistent domain pref with non-persistent domain pref.");
  Services.cookieBanners.setDomainPref(
    uri,
    Ci.nsICookieBannerService.MODE_DISABLED,
    true
  );

  info("Trigger an ending of a private browsing window session again");
  PBMSessionEndsObserved = TestUtils.topicObserved("last-pb-context-exited");

  // Close the PBM window and wait until it finishes.
  await BrowserTestUtils.closeWindow(PBMWin);
  await PBMSessionEndsObserved;

  info("Verify if the private domain pref is cleared.");
  is(
    Services.cookieBanners.getDomainPref(uri, true),
    Ci.nsICookieBannerService.MODE_UNSET,
    "The domain pref is properly set for PBM."
  );
});
