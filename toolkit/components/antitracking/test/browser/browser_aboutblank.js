/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_aboutblankInIframe() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "network.cookie.cookieBehavior",
        BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      ],
      [
        "network.cookie.cookieBehavior.pbmode",
        BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      ],
    ],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TOP_PAGE
  );
  let browser = tab.linkedBrowser;

  await SpecialPowers.spawn(browser, [], async function () {
    let ifr = content.document.createElement("iframe");
    let loading = new content.Promise(resolve => {
      ifr.onload = resolve;
    });
    ifr.src = "about:blank";
    content.document.body.appendChild(ifr);
    await loading;

    await SpecialPowers.spawn(ifr, [], async function () {
      ok(
        content.navigator.cookieEnabled,
        "Cookie should be enabled in about blank"
      );
    });
  });

  BrowserTestUtils.removeTab(tab);
});
