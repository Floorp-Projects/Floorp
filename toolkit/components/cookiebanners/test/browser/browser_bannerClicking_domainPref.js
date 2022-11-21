/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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
