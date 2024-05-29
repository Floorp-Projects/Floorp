/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A test to verify that same-site blob iframes' subresources have access to their unpartitioned cookies
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

  await SpecialPowers.spawn(browser, [], function () {
    content.document.cookie = "foo=bar; SameSite=Strict";
  });

  info("Creating the bloburl iframe");
  await SpecialPowers.spawn(
    browser,
    [`${TEST_DOMAIN + TEST_PATH}cookies.sjs`],
    async url => {
      let blobData = `
                      <html>
                      <body>
                      <script>
                        fetch("${url}")
                          .then((response) => response.text())
                          .then((text) => {
                            window.parent.postMessage(text);
                          });
                      </script>
                      </body>
                      </html>
                    `;
      let blobURL = content.URL.createObjectURL(new content.Blob([blobData]));
      let ifr = content.document.createElement("iframe");

      let iframeSubresourceCookiesEventPromise = ContentTaskUtils.waitForEvent(
        content.window,
        "message"
      );
      content.document.body.appendChild(ifr);
      ifr.src = blobURL;

      info("waiting for the iframe to get its http cookies");
      let iframeSubresourceCookiesEvent =
        await iframeSubresourceCookiesEventPromise;
      is(iframeSubresourceCookiesEvent.data, "cookie:foo=bar");
    }
  );
  info("Clean up");
  BrowserTestUtils.removeTab(tab);
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, () =>
      resolve()
    );
  });
});
