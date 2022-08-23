/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_DOMAIN_A = "example.com";
const TEST_DOMAIN_B = "example.org";

const TEST_ORIGIN_A = "https://" + TEST_DOMAIN_A;
const TEST_ORIGIN_B = "https://" + TEST_DOMAIN_B;

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  ""
);

const TEST_PAGE_A = TEST_ORIGIN_A + TEST_PATH + "file_banner.html";
const TEST_PAGE_B = TEST_ORIGIN_B + TEST_PATH + "file_banner.html";

/**
 * A helper function returns a promise which resolves when the banner clicking
 * is finished.
 */
function promiseBannerClickingFinish() {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer() {
      Services.obs.removeObserver(
        observer,
        "cookie-banner-test-clicking-finish"
      );
      resolve();
    }, "cookie-banner-test-clicking-finish");
  });
}

/**
 * A helper function to open the test page and verify the banner state.
 *
 * @param {Window} win the window object.
 * @param {String} page the url of the testing page.
 * @param {boolean} visible if the banner should be visible.
 * @param {boolean} expected the expected banner click state.
 */
async function openPageAndVerify(win, page, visible, expected) {
  info(`Opening ${page}`);
  let promise = promiseBannerClickingFinish();

  let tab = await BrowserTestUtils.openNewForegroundTab(win.gBrowser, page);

  await promise;

  info("Verify the cookie banner state.");
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [visible, expected],
    (visible, expected) => {
      let banner = content.document.getElementById("banner");

      is(
        banner.checkVisibility({
          checkOpacity: true,
          checkVisibilityCSS: true,
        }),
        visible,
        `The banner element should be ${visible ? "visible" : "hidden"}`
      );

      let result = content.document.getElementById("result");

      is(result.textContent, expected, "The build click state is correct.");
    }
  );

  BrowserTestUtils.removeTab(tab);
}

/**
 * A helper function to insert testing rules.
 */
function insertTestRules() {
  info("Clearing existing rules");
  Services.cookieBanners.resetRules(false);

  info("Inserting test rules.");

  let ruleA = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  ruleA.domain = TEST_DOMAIN_A;

  ruleA.addClickRule("div#banner", null, "button#optOut", "button#optIn");
  Services.cookieBanners.insertRule(ruleA);

  // An opt-in click rule for DOMAIN_B.
  let ruleB = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  ruleB.domain = TEST_DOMAIN_B;

  ruleB.addClickRule("div#banner", null, null, "button#optIn");
  Services.cookieBanners.insertRule(ruleB);
}

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.bannerClicking.testing", true],
      ["cookiebanners.bannerClicking.timeout", 500],
      ["cookiebanners.bannerClicking.enabled", true],
    ],
  });

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

/**
 * Test that the banner clicking won't click banner if the service is disabled.
 */
add_task(async function test_cookie_banner_service_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_DISABLED],
    ],
  });

  await openPageAndVerify(window, TEST_PAGE_A, true, "NoClick");
});

/**
 * Test that the banner clicking won't click banner if there is no rule.
 */
add_task(async function test_no_rules() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  info("Clearing existing rules");
  Services.cookieBanners.resetRules(false);

  await openPageAndVerify(window, TEST_PAGE_A, true, "NoClick");
});

/**
 * Test the banner clicking with MODE_REJECT.
 */
add_task(async function test_clicking_mode_reject() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  insertTestRules();

  await openPageAndVerify(window, TEST_PAGE_A, false, "OptOut");

  // No opt out rule for the example.org, the banner shouldn't be clicked.
  await openPageAndVerify(window, TEST_PAGE_B, true, "NoClick");
});

/**
 * Test the banner clicking with MODE_REJECT_OR_ACCEPT.
 */
add_task(async function test_clicking_mode_reject_or_accept() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode",
        Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      ],
    ],
  });

  insertTestRules();

  await openPageAndVerify(window, TEST_PAGE_A, false, "OptOut");

  await openPageAndVerify(window, TEST_PAGE_B, false, "OptIn");
});

/**
 * Test the banner clicking with the case where the banner is added after
 * page loads and with a short amount of delay.
 */
add_task(async function test_clicking_with_delayed_banner() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  insertTestRules();

  let TEST_PAGE =
    TEST_ORIGIN_A + TEST_PATH + "file_delayed_banner.html?delay=100";
  await openPageAndVerify(window, TEST_PAGE, false, "OptOut");
});

/**
 * Test that the banner clicking doesn't apply to iframe.
 */
add_task(async function test_embedded_iframe() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  insertTestRules();

  await BrowserTestUtils.withNewTab(TEST_ORIGIN_A, async browser => {
    let bc = await SpecialPowers.spawn(browser, [TEST_PAGE_A], async page => {
      let iframe = content.document.createElement("iframe");
      iframe.src = page;
      content.document.body.appendChild(iframe);
      await ContentTaskUtils.waitForEvent(iframe, "load");

      return iframe.browsingContext;
    });

    await SpecialPowers.spawn(bc, [], async () => {
      let banner = content.document.getElementById("banner");

      ok(
        ContentTaskUtils.is_visible(banner),
        "The banner element should be visible"
      );

      let result = content.document.getElementById("result");

      is(result.textContent, "NoClick", "No button has been clicked.");
    });
  });
});

/**
 * Test banner clicking with the private browsing window.
 */
add_task(async function test_pbm() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  insertTestRules();

  let pbmWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  await openPageAndVerify(pbmWindow, TEST_PAGE_A, false, "OptOut");

  await BrowserTestUtils.closeWindow(pbmWindow);
});
