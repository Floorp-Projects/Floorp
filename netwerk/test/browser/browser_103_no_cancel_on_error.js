/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Services.prefs.setBoolPref("network.early-hints.enabled", true);

const { request_count_checking } = ChromeUtils.importESModule(
  "resource://testing-common/early_hint_preload_test_helper.sys.mjs"
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

  let requestUrl = `https://example.com/browser/netwerk/test/browser/early_hint_asset_html.sjs?hinted=1&as=image&code=${httpCode}`;

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: requestUrl,
      waitForLoad: true,
    },
    async function () {}
  );

  let gotRequestCount = await fetch(
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs"
  ).then(response => response.json());

  await request_count_checking(`test_103_error_${httpCode}`, gotRequestCount, {
    hinted: 1,
    normal: 0,
  });
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
