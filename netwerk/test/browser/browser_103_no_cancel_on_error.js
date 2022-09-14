/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Services.prefs.setBoolPref("network.early-hints.enabled", true);

const { lax_request_count_checking } = ChromeUtils.import(
  "resource://testing-common/early_hint_preload_test_helper.jsm"
);

// - httpCode is the response code we're testing for. This file mostly covers 400 and 500 responses
async function test_hint_completion_on_error(httpCode) {
  // reset the count
  let headers = new Headers();
  headers.append("X-Early-Hint-Count-Start", "");
  await fetch(
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs",
    { headers }
  );

  let requestUrl = `https://example.com/browser/netwerk/test/browser/early_hint_asset_html.sjs?as=image&code=${httpCode}`;

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: requestUrl,
      waitForLoad: true,
    },
    async function() {}
  );

  let gotRequestCount = await fetch(
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs"
  ).then(response => response.json());

  // TODO: Switch to stricter counting method after fixing https://bugzilla.mozilla.org/show_bug.cgi?id=1753730#c11
  await lax_request_count_checking(
    `test_103_error_${httpCode}`,
    gotRequestCount,
    {
      hinted: 1,
      normal: 0,
    }
  );
  /*
   await Assert.deepEqual(
     gotRequestCount,
     hinted ? { hinted: 1, normal: 0 } : { hinted: 0, normal: 1 },
     `${testName} (${asset}): Unexpected amount of requests made`
   );
   */
}

// 400 Bad Request
add_task(async function test_complete_103_on_400() {
  await test_hint_completion_on_error(400);
});
add_task(async function test_complete_103_on_401() {
  await test_hint_completion_on_error(401);
});
add_task(async function test_complete_103_on_402() {
  await test_hint_completion_on_error(402);
});
add_task(async function test_complete_103_on_403() {
  await test_hint_completion_on_error(403);
});
add_task(async function test_complete_103_on_500() {
  await test_hint_completion_on_error(500);
});
add_task(async function test_complete_103_on_501() {
  await test_hint_completion_on_error(501);
});
add_task(async function test_complete_103_on_502() {
  await test_hint_completion_on_error(502);
});
