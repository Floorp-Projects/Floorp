/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A test to verify that A(B->A) navigated iframes have unparitioned
 * cookies and storage access.
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
      ifr.id = "iframe";

      let loading = ContentTaskUtils.waitForEvent(ifr, "load");
      content.document.body.appendChild(ifr);
      ifr.src = page;
      await loading;

      return ifr.browsingContext;
    }
  );

  is(
    ifrBC.currentWindowGlobal.documentStoragePrincipal.originAttributes
      .partitionKey,
    "(http,example.net)",
    "partitioned after load"
  );

  info("Navigating the AB iframe to AA");
  await SpecialPowers.spawn(browser, [TEST_TOP_PAGE], async page => {
    let ifr = content.document.getElementById("iframe");
    let loading = ContentTaskUtils.waitForEvent(ifr, "load");
    ifr.src = page;
    await loading;
  });

  is(
    ifrBC.currentWindowGlobal.documentStoragePrincipal.originAttributes
      .partitionKey,
    "",
    "unpartitioned after navigation"
  );

  info("Clean up");
  BrowserTestUtils.removeTab(tab);
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, () =>
      resolve()
    );
  });
});
