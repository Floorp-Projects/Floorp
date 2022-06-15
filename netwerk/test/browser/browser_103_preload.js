/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Services.prefs.setCharPref(
  "dom.securecontext.allowlist",
  "example.com,example.net"
);

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
    "http://example.com",
    [
      [
        "http://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
        Services.uuid.generateUUID().toString(),
      ],
      ["", "new_response"], // empty string to indicate new early hint response
      [
        "http://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
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
    "http://example.com",
    [
      [
        "http://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
        Services.uuid.generateUUID().toString(),
      ],
      ["", ""], // empty string to indicate new early hint response
      [
        "http://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
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
    "http://example.com",
    [
      [
        "http://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
        Services.uuid.generateUUID().toString(),
      ],
      [
        "http://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
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
    "http://example.com",
    "http://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 1, normal: 0 },
    uuid
  );
  await test_hint_preload(
    "test_103_preload_twice_2",
    "http://example.com",
    "http://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 0, normal: 0 },
    uuid
  );
});

// Test that with config option disabled, no early hint requests are made
add_task(async function test_103_preload_disabled() {
  Services.prefs.setBoolPref("network.early-hints.enabled", false);
  await test_hint_preload(
    "test_103_preload_disabled",
    "http://example.com",
    "http://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
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
    "http://example.com",
    "http://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 1, normal: 0 }
  );
});

// Cross origin preload in secure context
add_task(async function test_103_preload_cor() {
  await test_hint_preload(
    "test_103_preload_cor",
    "http://example.com",
    "http://example.net/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 0, normal: 1 }
  );
});

// Cross origin preload in insecure context
add_task(async function test_103_preload_insecure_cor() {
  await test_hint_preload(
    "test_103_preload_insecure_cor",
    "http://example.com",
    "http://mochi.test:8888/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 0, normal: 1 }
  );
});

// Same origin request with relative url
add_task(async function test_103_relative_preload() {
  await test_hint_preload(
    "test_103_relative_preload",
    "http://example.com",
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

// Early hint to redirect to same origin in secure context
add_task(async function test_103_redirect_same_origin() {
  await test_hint_preload(
    "test_103_redirect_same_origin",
    "http://example.com",
    "http://example.com/browser/netwerk/test/browser/early_hint_redirect.sjs?http://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 2, normal: 0 } // successful preload of redirect and resulting image
  );
});

// Early hint to redirect to cross origin in secure context
add_task(async function test_103_redirect_cross_origin() {
  await test_hint_preload(
    "test_103_redirect_cross_origin",
    "http://example.com",
    "http://example.com/browser/netwerk/test/browser/early_hint_redirect.sjs?http://example.net/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 1, normal: 1 } // successful load of redirect in preload, but image loaded via normal load
  );
});

// Early hint to redirect to cross origin in insecure context
add_task(async function test_103_redirect_insecure_cross_origin() {
  await test_hint_preload(
    "test_103_redirect_insecure_cross_origin",
    "http://example.com",
    "http://example.com/browser/netwerk/test/browser/early_hint_redirect.sjs?http://mochi.test:8888/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 1, normal: 1 }
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

// Cross origin preload from secure context to redirected insecure context on same domain
add_task(async function test_103_preload_redirect_mixed_content() {
  await test_hint_preload(
    "test_103_preload_redirect_mixed_content",
    "https://example.org",
    "https://example.org/browser/netwerk/test/browser/early_hint_redirect.sjs?http://example.org/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 1, normal: 1 }
  );
});

// Relative url, correct file for requested uri
add_task(async function test_103_preload_only_file() {
  await test_hint_preload(
    "test_103_preload_only_file",
    "http://example.com",
    "early_hint_pixel.sjs",
    { hinted: 1, normal: 0 }
  );
});

// csp header with "img-src: 'none'" only on main html response, don't show the image on the page
add_task(async function test_preload_csp_imgsrc_none() {
  // reset the count
  let headers = new Headers();
  headers.append("X-Early-Hint-Count-Start", "");
  await fetch(
    "http://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs",
    { headers }
  );

  let requestUrl =
    "http://example.com/browser/netwerk/test/browser/103_preload_csp_imgsrc_none.html";

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: requestUrl,
      waitForLoad: true,
    },
    async function(browser) {
      let noImgLoaded = await SpecialPowers.spawn(browser, [], function() {
        let loadInfo = content.performance.getEntriesByName(
          "http://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs?1ac2a5e1-90c7-4171-b0f0-676f7d899af3"
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
    "http://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs"
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

// Test that preloads in iframes don't get triggered
add_task(async function test_103_iframe() {
  // reset the count
  let headers = new Headers();
  headers.append("X-Early-Hint-Count-Start", "");
  await fetch(
    "http://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs",
    { headers }
  );

  let iframeUri =
    "http://example.com/browser/netwerk/test/browser/103_preload_iframe.html";

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: iframeUri,
      waitForLoad: true,
    },
    async function() {}
  );

  let gotRequestCount = await fetch(
    "http://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs"
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

// - testName is just there to be printed during Asserts when failing
// - asset is the asset type, see early_hint_asset_html.sjs for possible values
// - hinted: when true, the server reponds with "103 Early Hints"-header
async function test_hint_asset(testName, asset, hinted) {
  // reset the count
  let headers = new Headers();
  headers.append("X-Early-Hint-Count-Start", "");
  await fetch(
    "http://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs",
    { headers }
  );

  let requestUrl = `http://example.com/browser/netwerk/test/browser/early_hint_asset_html.sjs?as=${asset}&hinted=${
    hinted ? "1" : "0"
  }`;

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: requestUrl,
      waitForLoad: true,
    },
    async function() {}
  );

  let gotRequestCount = await fetch(
    "http://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs"
  ).then(response => response.json());

  await Assert.deepEqual(
    gotRequestCount,
    hinted ? { hinted: 1, normal: 0 } : { hinted: 0, normal: 1 },
    `${testName} (${asset}): Unexpected amount of requests made`
  );
}

// preload css
add_task(async function test_103_asset_style() {
  await test_hint_asset("test_103_asset_hinted", "style", true);
  await test_hint_asset("test_103_asset_normal", "style", false);
});

// preload javascript
add_task(async function test_103_asset_javascript() {
  await test_hint_asset("test_103_asset_hinted", "script", true);
  await test_hint_asset("test_103_asset_normal", "script", false);
});

// preload fetch
add_task(async function test_103_asset_fetch() {
  await test_hint_asset("test_103_asset_hinted", "fetch", true);
  await test_hint_asset("test_103_asset_normal", "fetch", false);
});

// preload font
add_task(async function test_103_asset_font() {
  await test_hint_asset("test_103_asset_hinted", "font", true);
  await test_hint_asset("test_103_asset_normal", "font", false);
});
