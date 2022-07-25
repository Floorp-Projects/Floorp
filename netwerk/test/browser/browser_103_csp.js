/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { lax_request_count_checking } = ChromeUtils.import(
  "resource://testing-common/early_hint_preload_test_helper.jsm"
);

// csp header with "img-src: 'none'" only on main html response, don't show the image on the page
add_task(async function test_preload_csp_imgsrc_none() {
  // reset the count
  let headers = new Headers();
  headers.append("X-Early-Hint-Count-Start", "");
  await fetch(
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs",
    { headers }
  );

  let requestUrl =
    "https://example.com/browser/netwerk/test/browser/103_preload_csp_imgsrc_none.html";

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: requestUrl,
      waitForLoad: true,
    },
    async function(browser) {
      let noImgLoaded = await SpecialPowers.spawn(browser, [], function() {
        let loadInfo = content.performance.getEntriesByName(
          "https://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs?1ac2a5e1-90c7-4171-b0f0-676f7d899af3"
        );
        return loadInfo.every(entry => entry.decodedBodySize === 0);
      });
      await Assert.ok(
        noImgLoaded,
        "test_preload_csp_imgsrc_none: Image dislpayed unexpectedly"
      );
    }
  );

  let gotRequestCount = await fetch(
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs"
  ).then(response => response.json());
  let expectedRequestCount = { hinted: 1, normal: 0 };

  // TODO: Switch to stricter counting method after fixing https://bugzilla.mozilla.org/show_bug.cgi?id=1753730#c11
  await lax_request_count_checking(
    "test_preload_csp_imgsrc_none",
    gotRequestCount,
    expectedRequestCount
  );
  /* stricter counting method:
    await Assert.deepEqual(
      gotRequestCount,
      { hinted: 1, normal: 0 },
      "test_preload_csp_imgsrc_none: Unexpected amount of requests made"
    );
    */

  Services.cache2.clear();
});
