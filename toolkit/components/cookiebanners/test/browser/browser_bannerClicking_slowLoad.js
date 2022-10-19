/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(clickTestSetup);

/**
 * Test the banner clicking with the case where the banner is added shortly after
 * page load, but the page load itself is delayed.
 */
add_task(async function test_clicking_with_delayed_banner() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  insertTestClickRules();

  // A test page that has a delayed load event.
  let TEST_PAGE = TEST_ORIGIN_A + TEST_PATH + "file_delayed_banner_load.html";
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE,
    visible: false,
    expected: "OptOut",
  });
});
