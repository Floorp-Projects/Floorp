/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Services.prefs.setBoolPref("network.early-hints.enabled", true);

const {
  test_hint_preload,
  test_hint_preload_internal,
  request_count_checking,
} = ChromeUtils.importESModule(
  "resource://testing-common/early_hint_preload_test_helper.sys.mjs"
);

// two early hint responses
add_task(async function test_103_two_preload_responses() {
  await test_hint_preload_internal(
    "103_two_preload_responses",
    "https://example.com",
    [
      [
        "https://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
        Services.uuid.generateUUID().toString(),
      ],
      ["", "new_response"], // indicate new early hint response
      [
        "https://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
        Services.uuid.generateUUID().toString(),
      ],
    ],
    { hinted: 1, normal: 1 }
  );
});

// two link header in one early hint response
add_task(async function test_103_two_link_header() {
  await test_hint_preload_internal(
    "103_two_link_header",
    "https://example.com",
    [
      [
        "https://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
        Services.uuid.generateUUID().toString(),
      ],
      ["", ""], // indicate new link header in same reponse
      [
        "https://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
        Services.uuid.generateUUID().toString(),
      ],
    ],
    { hinted: 2, normal: 0 }
  );
});

// two links in one early hint link header
add_task(async function test_103_two_links() {
  await test_hint_preload_internal(
    "103_two_links",
    "https://example.com",
    [
      [
        "https://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
        Services.uuid.generateUUID().toString(),
      ],
      [
        "https://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
        Services.uuid.generateUUID().toString(),
      ],
    ],
    { hinted: 2, normal: 0 }
  );
});

// two early hint responses, only second one has a link header
add_task(async function test_103_two_links() {
  await test_hint_preload_internal(
    "103_two_links",
    "https://example.com",
    [
      ["", "non_link_header"], // indicate non-link related header
      ["", "new_response"], // indicate new early hint response
      [
        "https://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
        Services.uuid.generateUUID().toString(),
      ],
    ],
    { hinted: 1, normal: 0 }
  );
});

// Preload twice same origin in secure context
add_task(async function test_103_preload_twice() {
  // pass two times the same uuid so that on the second request, the response is
  // already in the cache
  let uuid = Services.uuid.generateUUID();
  await test_hint_preload(
    "test_103_preload_twice_1",
    "https://example.com",
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 1, normal: 0 },
    uuid
  );
  await test_hint_preload(
    "test_103_preload_twice_2",
    "https://example.com",
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 0, normal: 0 },
    uuid
  );
});

// Test that preloads in iframes don't get triggered
add_task(async function test_103_iframe() {
  // reset the count
  let headers = new Headers();
  headers.append("X-Early-Hint-Count-Start", "");
  await fetch(
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs",
    { headers }
  );

  let iframeUri =
    "https://example.com/browser/netwerk/test/browser/103_preload_iframe.html";

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: iframeUri,
      waitForLoad: true,
    },
    async function () {}
  );

  let gotRequestCount = await fetch(
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs"
  ).then(response => response.json());
  let expectedRequestCount = { hinted: 0, normal: 1 };

  await request_count_checking(
    "test_103_iframe",
    gotRequestCount,
    expectedRequestCount
  );

  Services.cache2.clear();
});

// Test that anchors are parsed
add_task(async function test_103_anchor() {
  // reset the count
  let headers = new Headers();
  headers.append("X-Early-Hint-Count-Start", "");
  await fetch(
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs",
    { headers }
  );

  let anchorUri =
    "https://example.com/browser/netwerk/test/browser/103_preload_anchor.html";

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: anchorUri,
      waitForLoad: true,
    },
    async function () {}
  );

  let gotRequestCount = await fetch(
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs"
  ).then(response => response.json());

  await request_count_checking("test_103_anchor", gotRequestCount, {
    hinted: 0,
    normal: 1,
  });
});
