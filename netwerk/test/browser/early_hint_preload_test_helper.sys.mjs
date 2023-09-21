/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Assert } from "resource://testing-common/Assert.sys.mjs";
import { BrowserTestUtils } from "resource://testing-common/BrowserTestUtils.sys.mjs";

const { gBrowser } = Services.wm.getMostRecentWindow("navigator:browser");

export async function request_count_checking(testName, got, expected) {
  // stringify to pretty print assert output
  let g = JSON.stringify(got);
  let e = JSON.stringify(expected);
  // each early hint request can starts one hinted request, but doesn't yet
  // complete the early hint request during the test case
  Assert.ok(
    got.hinted == expected.hinted,
    `${testName}: unexpected amount of hinted request made expected ${expected.hinted} (${e}), got ${got.hinted} (${g})`
  );
  // when the early hint request doesn't complete fast enough, another request
  // is currently sent from the main document
  let expected_normal = expected.normal;
  Assert.ok(
    got.normal == expected_normal,
    `${testName}: unexpected amount of normal request made expected ${expected_normal} (${e}), got ${got.normal} (${g})`
  );
}

export async function test_hint_preload(
  testName,
  requestFrom,
  imgUrl,
  expectedRequestCount,
  uuid = undefined
) {
  // generate a uuid if none were passed
  if (uuid == undefined) {
    uuid = Services.uuid.generateUUID();
  }
  await test_hint_preload_internal(
    testName,
    requestFrom,
    [[imgUrl, uuid.toString()]],
    expectedRequestCount
  );
}

// - testName is just there to be printed during Asserts when failing
// - the baseUrl can't have query strings, because they are currently used to pass
//   the early hint the server responds with
// - urls are in the form [[url1, uuid1], ...]. The uuids are there to make each preload
//   unique and not available in the cache from other test cases
// - expectedRequestCount is the sum of all requested objects { normal: count, hinted: count }
export async function test_hint_preload_internal(
  testName,
  requestFrom,
  imgUrls,
  expectedRequestCount
) {
  // reset the count
  let headers = new Headers();
  headers.append("X-Early-Hint-Count-Start", "");
  await fetch(
    "http://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs",
    { headers }
  );

  let requestUrl =
    requestFrom +
    "/browser/netwerk/test/browser/early_hint_main_html.sjs?" +
    new URLSearchParams(imgUrls).toString(); // encode the hinted images as query string

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: requestUrl,
      waitForLoad: true,
    },
    async function () {}
  );

  let gotRequestCount = await fetch(
    "http://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs"
  ).then(response => response.json());

  await request_count_checking(testName, gotRequestCount, expectedRequestCount);
}

// Verify that CSP policies in both the 103 response as well as the main response are respected.
// e.g.
// 103 Early Hint
// Content-Security-Policy: style-src: self;
// Link: </style.css>; rel=preload; as=style
// 200 OK
// Content-Security-Policy: style-src: none;
// Link: </font.ttf>; rel=preload; as=font

// Server-side we verify that:
//  - the hinted preload request was made as expected
//  - the load request request was made as expected
// Client-side, we verify that the image was loaded or not loaded, depending on the scenario

// This verifies preload hints and requests
export async function test_preload_hint_and_request(input, expected_results) {
  // reset the count
  let headers = new Headers();
  headers.append("X-Early-Hint-Count-Start", "");
  await fetch(
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs",
    { headers }
  );

  let requestUrl = `https://example.com/browser/netwerk/test/browser/early_hint_csp_options_html.sjs?as=${
    input.resource_type
  }&hinted=${input.hinted ? "1" : "0"}${input.csp ? "&csp=" + input.csp : ""}${
    input.csp_in_early_hint
      ? "&csp_in_early_hint=" + input.csp_in_early_hint
      : ""
  }${input.host ? "&host=" + input.host : ""}`;

  await BrowserTestUtils.openNewForegroundTab(gBrowser, requestUrl, true);

  let gotRequestCount = await fetch(
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs"
  ).then(response => response.json());

  await Assert.deepEqual(gotRequestCount, expected_results, input.test_name);

  gBrowser.removeCurrentTab();
  Services.cache2.clear();
}

// simple loading of one url and then checking the request count against the
// passed expected count
export async function test_preload_url(testName, url, expectedRequestCount) {
  // reset the count
  let headers = new Headers();
  headers.append("X-Early-Hint-Count-Start", "");
  await fetch(
    "http://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs",
    { headers }
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
      waitForLoad: true,
    },
    async function () {}
  );

  let gotRequestCount = await fetch(
    "http://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs"
  ).then(response => response.json());

  await request_count_checking(testName, gotRequestCount, expectedRequestCount);
  Services.cache2.clear();
}
