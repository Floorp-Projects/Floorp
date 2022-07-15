/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head.js */

/*
 * Bug 1759496 - A test to verify if the console message of partitioned storage
 *               was sent correctly.
 */

"use strict";

add_setup(async function() {
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

  let consolePromise = new Promise(resolve => {
    let consoleListener = {
      observe(msg) {
        if (
          msg
            .QueryInterface(Ci.nsIScriptError)
            .category.startsWith("cookiePartitioned")
        ) {
          Services.console.unregisterListener(consoleListener);
          resolve(msg.QueryInterface(Ci.nsIScriptError).errorMessage);
        }
      },
    };

    Services.console.registerListener(consoleListener);
  });

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

  info("Write cookie to the third-party iframe to ensure the console message");
  await SpecialPowers.spawn(ifrBC, [], async _ => {
    content.document.cookie = "foo";
  });

  let msg = await consolePromise;

  ok(
    msg.startsWith("Partitioned cookie or storage access was provided to"),
    "The partitioned console message was sent correctly"
  );

  info("Clean up");
  BrowserTestUtils.removeTab(tab);
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});
