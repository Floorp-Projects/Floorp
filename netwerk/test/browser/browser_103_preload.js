/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Services.prefs.setBoolPref("network.early-hints.enabled", true);

const {
  lax_request_count_checking,
  test_hint_preload_internal,
  test_hint_preload,
} = ChromeUtils.import(
  "resource://testing-common/early_hint_preload_test_helper.jsm"
);

// TODO testing:
//  * Abort main document load while early hint is still loading -> early hint should be aborted

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
      ["", "new_response"], // empty string to indicate new early hint response
      [
        "https://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
        Services.uuid.generateUUID().toString(),
      ],
    ],
    { hinted: 2, normal: 0 }
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
      ["", ""], // empty string to indicate new early hint response
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

// Test that with config option disabled, no early hint requests are made
add_task(async function test_103_preload_disabled() {
  Services.prefs.setBoolPref("network.early-hints.enabled", false);
  await test_hint_preload(
    "test_103_preload_disabled",
    "https://example.com",
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 0, normal: 1 }
  );
  Services.prefs.setBoolPref("network.early-hints.enabled", true);
});

// Preload with same origin in secure context with mochitest http proxy
add_task(async function test_103_preload_https() {
  await test_hint_preload(
    "test_103_preload_https",
    "https://example.org",
    "/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 1, normal: 0 }
  );
});

// Preload with same origin in secure context
add_task(async function test_103_preload() {
  await test_hint_preload(
    "test_103_preload",
    "https://example.com",
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 1, normal: 0 }
  );
});

// Cross origin preload in secure context
add_task(async function test_103_preload_cor() {
  await test_hint_preload(
    "test_103_preload_cor",
    "https://example.com",
    "https://example.net/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 1, normal: 0 }
  );
});

// Cross origin preload in insecure context
add_task(async function test_103_preload_insecure_cor() {
  await test_hint_preload(
    "test_103_preload_insecure_cor",
    "https://example.com",
    "http://mochi.test:8888/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 0, normal: 1 }
  );
});

// Same origin request with relative url
add_task(async function test_103_relative_preload() {
  await test_hint_preload(
    "test_103_relative_preload",
    "https://example.com",
    "/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 1, normal: 0 }
  );
});

// Early hint from insecure context
add_task(async function test_103_insecure_preload() {
  await test_hint_preload(
    "test_103_insecure_preload",
    "http://mochi.test:8888",
    "/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 0, normal: 1 }
  );
});

// Cross origin preload from secure context to insecure context on same domain
add_task(async function test_103_preload_mixed_content() {
  await test_hint_preload(
    "test_103_preload_mixed_content",
    "https://example.org",
    "http://example.org/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 0, normal: 1 }
  );
});

// Relative url, correct file for requested uri
add_task(async function test_103_preload_only_file() {
  await test_hint_preload(
    "test_103_preload_only_file",
    "https://example.com",
    "early_hint_pixel.sjs",
    { hinted: 1, normal: 0 }
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
    async function() {}
  );

  let gotRequestCount = await fetch(
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs"
  ).then(response => response.json());
  let expectedRequestCount = { hinted: 0, normal: 1 };

  // TODO: Switch to stricter counting method after fixing https://bugzilla.mozilla.org/show_bug.cgi?id=1753730#c11
  await lax_request_count_checking(
    "test_103_iframe",
    gotRequestCount,
    expectedRequestCount
  );
  /* stricter counting method:
  await Assert.deepEqual(
    gotRequestCount,
    { hinted: 0, normal: 1 },
    "test_103_iframe: Unexpected amount of requests made"
  );
  */

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
    async function() {}
  );

  let gotRequestCount = await fetch(
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs"
  ).then(response => response.json());

  // TODO: Switch to stricter counting method after fixing Bug 1771867
  await lax_request_count_checking("test_103_anchor", gotRequestCount, {
    hinted: 1,
    normal: 0,
  });
  /* stricter counting method:
  await Assert.deepEqual(
    gotRequestCount,
    { hinted: 1, normal: 0 },
    "test_103_anchor: Unexpected amount of requests made"
  );
  */
});
