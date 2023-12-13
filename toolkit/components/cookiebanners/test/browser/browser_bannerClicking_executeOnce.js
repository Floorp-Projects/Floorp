/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let { ForgetAboutSite } = ChromeUtils.importESModule(
  "resource://gre/modules/ForgetAboutSite.sys.mjs"
);

add_setup(async function () {
  await clickTestSetup();

  // Enable the pref to once execute cookie banner handling once per domain.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.bannerClicking.executeOnce", true],
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  // Clean the records.
  Services.cookieBanners.removeAllExecutedRecords(false);
  Services.cookieBanners.removeAllExecutedRecords(true);
});

// Ensure the banner clicking doesn't execute at the second load if the
// executeOnce feature is enabled.
add_task(async function testExecuteOnce() {
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

  // Open the domain and ensure the banner is clicked.
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  // Open the domain again and the banner shouldn't be hidden nor the banner is
  // clicked.
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: true,
    expected: "NoClick",
  });

  // Clear the record for the normal window.
  Services.cookieBanners.removeAllExecutedRecords(false);

  // Open the domain again after clearing the record and ensure the banner is
  // clicked.
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  // Test in the private browsing mode.
  let pbmWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  // Open the domain in PBM and ensure the banner is clicked.
  await openPageAndVerify({
    win: pbmWindow,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  // Open the domain in PBM again and the banner shouldn't be hidden nor the
  // banner is clicked.
  await openPageAndVerify({
    win: pbmWindow,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: true,
    expected: "NoClick",
  });

  // Clear the record for the private window.
  Services.cookieBanners.removeAllExecutedRecords(true);

  // Open the domain again in PBM after clearing the record, and ensure the
  // banner is clicked.
  await openPageAndVerify({
    win: pbmWindow,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  await BrowserTestUtils.closeWindow(pbmWindow);

  // Clean the records.
  Services.cookieBanners.removeAllExecutedRecords(false);
  Services.cookieBanners.removeAllExecutedRecords(true);
});

// Ensure that ForgetAboutSite clears the handled record properly.
add_task(async function testForgetAboutSiteWithExecuteOnce() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  insertTestClickRules();

  // Open the domain and ensure the banner is clicked.
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  // Open the domain again and the banner shouldn't be hidden nor the banner is
  // clicked.
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: true,
    expected: "NoClick",
  });

  // Call ForgetAboutSite for the domain.
  await ForgetAboutSite.removeDataFromDomain(TEST_DOMAIN_A);

  // Open the domain again after ForgetAboutSite and the clicking should work
  // again.
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  // Clean the records.
  Services.cookieBanners.removeAllExecutedRecords(false);
  Services.cookieBanners.removeAllExecutedRecords(true);
});

// Ensure the ClearDataService clears the handled record properly.
add_task(async function testClearDataServiceWithExecuteOnce() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  insertTestClickRules();

  // Open the domain initially and ensure the banner is clicked.
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  // Invoke deleteDataFromBaseDomain.
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromBaseDomain(
      TEST_DOMAIN_A,
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_COOKIE_BANNER_EXECUTED_RECORD,
      aResolve
    );
  });

  // Ensure the record is cleared. The banner clicking should work after clean.
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  // Invoke deleteDataFromHost.
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromHost(
      TEST_DOMAIN_A,
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_COOKIE_BANNER_EXECUTED_RECORD,
      aResolve
    );
  });

  // Ensure the record is cleared. The banner clicking should work after clean.
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  // Invoke deleteDataFromPrincipal.
  let principal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "https://" + TEST_DOMAIN_A
    );
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromPrincipal(
      principal,
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_COOKIE_BANNER_EXECUTED_RECORD,
      aResolve
    );
  });

  // Ensure the record is cleared. The banner clicking should work after clean.
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  // Invoke deleteData.
  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_COOKIE_BANNER_EXECUTED_RECORD,
      aResolve
    );
  });

  // Ensure the record is cleared. The banner clicking should work after clean.
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  // Clean the records.
  Services.cookieBanners.removeAllExecutedRecords(false);
  Services.cookieBanners.removeAllExecutedRecords(true);
});
