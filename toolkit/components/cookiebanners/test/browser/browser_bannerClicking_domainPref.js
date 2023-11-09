/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let { ForgetAboutSite } = ChromeUtils.importESModule(
  "resource://gre/modules/ForgetAboutSite.sys.mjs"
);

add_setup(clickTestSetup);

/**
 * Test that domain preference takes precedence over pref settings.
 */
add_task(async function test_domain_preference() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
      [
        "cookiebanners.service.mode.privateBrowsing",
        Ci.nsICookieBannerService.MODE_REJECT,
      ],
    ],
  });

  insertTestClickRules();

  // Clear executed records before testing for private and normal browsing.
  Services.cookieBanners.removeAllExecutedRecords(true);
  Services.cookieBanners.removeAllExecutedRecords(false);

  for (let testPBM of [false, true]) {
    let testWin = window;
    if (testPBM) {
      testWin = await BrowserTestUtils.openNewBrowserWindow({
        private: true,
      });
    }

    await testClickResultTelemetry({});

    info(
      "Make sure the example.org follows the pref setting when there is no domain preference."
    );
    await openPageAndVerify({
      win: testWin,
      domain: TEST_DOMAIN_B,
      testURL: TEST_PAGE_B,
      visible: true,
      expected: "NoClick",
    });

    await testClickResultTelemetry(
      {
        fail: 1,
        fail_no_rule_for_mode: 1,
      },
      false
    );

    Services.cookieBanners.removeAllExecutedRecords(testPBM);

    info("Set the domain preference of example.org to MODE_REJECT_OR_ACCEPT");
    let uri = Services.io.newURI(TEST_ORIGIN_B);
    Services.cookieBanners.setDomainPref(
      uri,
      Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      testPBM
    );

    info(
      "Verify if domain preference takes precedence over then the pref setting for example.org"
    );
    await openPageAndVerify({
      win: testWin,
      domain: TEST_DOMAIN_B,
      testURL: TEST_PAGE_B,
      visible: false,
      expected: "OptIn",
    });

    Services.cookieBanners.removeAllDomainPrefs(testPBM);

    if (testPBM) {
      await BrowserTestUtils.closeWindow(testWin);
    }

    await testClickResultTelemetry({
      fail: 1,
      fail_no_rule_for_mode: 1,
      success: 1,
      success_dom_content_loaded: 1,
    });
  }
});

/**
 * Test that domain preference works on the top-level domain.
 */
add_task(async function test_domain_preference_iframe() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
      [
        "cookiebanners.service.mode.privateBrowsing",
        Ci.nsICookieBannerService.MODE_REJECT,
      ],
    ],
  });

  insertTestClickRules();

  // Clear executed records before testing for private and normal browsing.
  Services.cookieBanners.removeAllExecutedRecords(true);
  Services.cookieBanners.removeAllExecutedRecords(false);

  await testClickResultTelemetry({});

  for (let testPBM of [false, true]) {
    let testWin = window;
    if (testPBM) {
      testWin = await BrowserTestUtils.openNewBrowserWindow({
        private: true,
      });
    }

    info(
      "Make sure the example.org follows the pref setting when there is no domain preference for the top-level example.net."
    );
    await openIframeAndVerify({
      win: testWin,
      domain: TEST_DOMAIN_B,
      testURL: TEST_PAGE_B,
      visible: true,
      expected: "NoClick",
    });

    await testClickResultTelemetry(
      {
        fail: 1,
        fail_no_rule_for_mode: 1,
      },
      false
    );

    Services.cookieBanners.removeAllExecutedRecords(testPBM);

    info(
      "Set the domain preference of the top-level domain to MODE_REJECT_OR_ACCEPT"
    );
    let uri = Services.io.newURI(TEST_ORIGIN_C);
    Services.cookieBanners.setDomainPref(
      uri,
      Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      testPBM
    );

    info(
      "Verify if domain preference takes precedence over then the pref setting for top-level example.net"
    );
    await openIframeAndVerify({
      win: testWin,
      domain: TEST_DOMAIN_B,
      testURL: TEST_PAGE_B,
      visible: false,
      expected: "OptIn",
    });

    Services.cookieBanners.removeAllDomainPrefs(testPBM);

    if (testPBM) {
      await BrowserTestUtils.closeWindow(testWin);
    }

    await testClickResultTelemetry({
      fail: 1,
      fail_no_rule_for_mode: 1,
      success: 1,
      success_dom_content_loaded: 1,
    });
  }
});

// Verify that the ForgetAboutSite clears the domain preference properly.
add_task(async function test_domain_preference_forgetAboutSite() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
      [
        "cookiebanners.service.mode.privateBrowsing",
        Ci.nsICookieBannerService.MODE_REJECT,
      ],
    ],
  });

  insertTestClickRules();

  // Clear executed records before testing for private and normal browsing.
  Services.cookieBanners.removeAllExecutedRecords(true);
  Services.cookieBanners.removeAllExecutedRecords(false);

  info("Set the domain preference of example.org to MODE_REJECT_OR_ACCEPT");
  let uri = Services.io.newURI(TEST_ORIGIN_B);
  Services.cookieBanners.setDomainPref(
    uri,
    Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
    false
  );

  info(
    "Verify if domain preference takes precedence over then the pref setting for example.org"
  );
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_B,
    testURL: TEST_PAGE_B,
    visible: false,
    expected: "OptIn",
  });

  // Call ForgetAboutSite for the domain.
  await ForgetAboutSite.removeDataFromDomain(TEST_DOMAIN_B);

  info("Ensure the domain preference is cleared.");
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_B,
    testURL: TEST_PAGE_B,
    visible: true,
    expected: "NoClick",
  });
});

add_task(async function test_domain_preference_clearDataService() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
      [
        "cookiebanners.service.mode.privateBrowsing",
        Ci.nsICookieBannerService.MODE_REJECT,
      ],
    ],
  });

  insertTestClickRules();

  // Clear executed records before testing or private and normal browsing.
  Services.cookieBanners.removeAllExecutedRecords(true);
  Services.cookieBanners.removeAllExecutedRecords(false);

  info("Set the domain preference of example.org to MODE_REJECT_OR_ACCEPT");
  let uri = Services.io.newURI(TEST_ORIGIN_B);
  Services.cookieBanners.setDomainPref(
    uri,
    Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
    false
  );

  info(
    "Verify if domain preference takes precedence over then the pref setting for example.org"
  );
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_B,
    testURL: TEST_PAGE_B,
    visible: false,
    expected: "OptIn",
  });

  info("Call ClearDataService.deleteDataFromBaseDomain for the domain.");
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromBaseDomain(
      TEST_DOMAIN_B,
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_COOKIE_BANNER_EXCEPTION,
      aResolve
    );
  });

  info("Ensure the domain preference is cleared.");
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_B,
    testURL: TEST_PAGE_B,
    visible: true,
    expected: "NoClick",
  });

  info("Set the domain preference of example.org to MODE_REJECT_OR_ACCEPT");
  Services.cookieBanners.setDomainPref(
    uri,
    Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
    false
  );

  info("Call ClearDataService.deleteDataFromHost for the domain.");
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromHost(
      TEST_DOMAIN_B,
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_COOKIE_BANNER_EXCEPTION,
      aResolve
    );
  });

  info("Ensure the domain preference is cleared.");
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_B,
    testURL: TEST_PAGE_B,
    visible: true,
    expected: "NoClick",
  });

  info("Set the domain preference of example.org to MODE_REJECT_OR_ACCEPT");
  Services.cookieBanners.setDomainPref(
    uri,
    Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
    false
  );

  info("Call ClearDataService.deleteDataFromPrincipal for the domain.");
  let principal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "https://" + TEST_DOMAIN_B
    );
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromPrincipal(
      principal,
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_COOKIE_BANNER_EXCEPTION,
      aResolve
    );
  });

  info("Ensure the domain preference is cleared.");
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_B,
    testURL: TEST_PAGE_B,
    visible: true,
    expected: "NoClick",
  });

  info("Set the domain preference of example.org to MODE_REJECT_OR_ACCEPT");
  Services.cookieBanners.setDomainPref(
    uri,
    Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
    false
  );

  info("Call ClearDataService.deleteData for the domain.");
  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_COOKIE_BANNER_EXCEPTION,
      aResolve
    );
  });

  info("Ensure the domain preference is cleared.");
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_B,
    testURL: TEST_PAGE_B,
    visible: true,
    expected: "NoClick",
  });
});
