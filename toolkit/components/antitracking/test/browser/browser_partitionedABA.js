/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A test to verify that ABA iframes partition at least localStorage and document.cookie
 */

"use strict";

add_setup(async function () {
  await setCookieBehaviorPref(
    BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    false
  );
});

add_task(async function runTest() {
  info("Creating the tab");
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tab;

  let browser = tab.linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser);

  info("Creating the third-party iframe");
  let ifrBC = await SpecialPowers.spawn(
    browser,
    [TEST_TOP_PAGE_7],
    async page => {
      let ifr = content.document.createElement("iframe");

      let loading = ContentTaskUtils.waitForEvent(ifr, "load");
      content.document.body.appendChild(ifr);
      ifr.src = page;
      await loading;

      return ifr.browsingContext;
    }
  );

  info("Creating the ABA iframe");
  let ifrABABC = await SpecialPowers.spawn(
    ifrBC,
    [TEST_TOP_PAGE],
    async page => {
      let ifr = content.document.createElement("iframe");

      let loading = ContentTaskUtils.waitForEvent(ifr, "load");
      content.document.body.appendChild(ifr);
      ifr.src = page;
      await loading;

      return ifr.browsingContext;
    }
  );

  info("Write cookie to the ABA third-party iframe");
  await SpecialPowers.spawn(ifrABABC, [], async _ => {
    content.document.cookie = "foo; SameSite=None; Secure; Partitioned";
  });

  let cookie = await SpecialPowers.spawn(browser, [], async () => {
    return content.document.cookie;
  });
  is(cookie, "", "Cookie is not in the top level");

  info("Write localstorage to the ABA third-party iframe");
  await SpecialPowers.spawn(ifrABABC, [], async _ => {
    content.localStorage.setItem("foo", "bar");
  });

  let storage = await SpecialPowers.spawn(browser, [], async () => {
    return content.localStorage.getItem("foo");
  });
  is(storage, null, "LocalStorage update is not in the top level");

  info("Clean up");
  BrowserTestUtils.removeTab(tab);
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, () =>
      resolve()
    );
  });
});
