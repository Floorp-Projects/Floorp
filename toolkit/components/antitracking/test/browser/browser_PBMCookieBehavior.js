"use strict";

// This test will run all combinations of CookieBehavior. So, request a longer
// timeout here
requestLongerTimeout(3);

const COOKIE_BEHAVIORS = [
  Ci.nsICookieService.BEHAVIOR_ACCEPT,
  Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN,
  Ci.nsICookieService.BEHAVIOR_REJECT,
  Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN,
  Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
  Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
];

async function verifyCookieBehavior(browser, expected) {
  await SpecialPowers.spawn(
    browser,
    [{ expected, page: TEST_3RD_PARTY_PAGE }],
    async obj => {
      is(
        content.document.cookieJarSettings.cookieBehavior,
        obj.expected,
        "The tab in the window has the expected CookieBehavior."
      );

      // Create an 3rd party iframe and check the cookieBehavior.
      let ifr = content.document.createElement("iframe");
      let loading = new content.Promise(resolve => {
        ifr.onload = resolve;
      });
      content.document.body.appendChild(ifr);
      ifr.src = obj.page;
      await loading;

      await SpecialPowers.spawn(
        ifr.browsingContext,
        [obj.expected],
        async expected => {
          is(
            content.document.cookieJarSettings.cookieBehavior,
            expected,
            "The iframe in the window has the expected CookieBehavior."
          );
        }
      );
    }
  );
}

add_task(async function() {
  for (let regularCookieBehavior of COOKIE_BEHAVIORS) {
    for (let PBMCookieBehavior of COOKIE_BEHAVIORS) {
      await SpecialPowers.flushPrefEnv();
      await SpecialPowers.pushPrefEnv({
        set: [
          ["network.cookie.cookieBehavior", regularCookieBehavior],
          ["network.cookie.cookieBehavior.pbmode", PBMCookieBehavior],
        ],
      });

      info(
        ` Start testing with regular cookieBehavior(${regularCookieBehavior}) and PBM cookieBehavior(${PBMCookieBehavior})`
      );

      info(" Open a tab in regular window.");
      let tab = await BrowserTestUtils.openNewForegroundTab(
        gBrowser,
        TEST_TOP_PAGE
      );

      info(
        " Verify if the tab in regular window has the expected cookieBehavior."
      );
      await verifyCookieBehavior(tab.linkedBrowser, regularCookieBehavior);
      BrowserTestUtils.removeTab(tab);

      info(" Open a tab in private window.");
      let pb_win = await BrowserTestUtils.openNewBrowserWindow({
        private: true,
      });

      tab = await BrowserTestUtils.openNewForegroundTab(
        pb_win.gBrowser,
        TEST_TOP_PAGE
      );

      let expectPBMCookieBehavior = PBMCookieBehavior;

      // The private cookieBehavior will mirror the regular pref if the regular
      // pref has a user value and the private pref doesn't have a user pref.
      if (
        Services.prefs.prefHasUserValue("network.cookie.cookieBehavior") &&
        !Services.prefs.prefHasUserValue("network.cookie.cookieBehavior.pbmode")
      ) {
        expectPBMCookieBehavior = regularCookieBehavior;
      }

      info(
        " Verify if the tab in private window has the expected cookieBehavior."
      );
      await verifyCookieBehavior(tab.linkedBrowser, expectPBMCookieBehavior);
      BrowserTestUtils.removeTab(tab);
      await BrowserTestUtils.closeWindow(pb_win);
    }
  }
});
