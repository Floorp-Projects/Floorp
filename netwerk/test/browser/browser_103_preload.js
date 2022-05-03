"use strict";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

Services.prefs.setCharPref(
  "dom.securecontext.allowlist",
  "example.com,example.net"
);

Services.prefs.setBoolPref("network.early-hints.enabled", true);

async function test_hint_preload(
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
async function test_hint_preload_internal(
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

  // TODO: consider using http headers instead of query strings to pass the early hints
  //       to allow the baseUrl
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
    async function() {}
  );

  let gotRequestCount = await fetch(
    "http://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs"
  ).then(response => response.json());

  await Assert.deepEqual(
    gotRequestCount,
    expectedRequestCount,
    testName + ": Unexpected amount of requests made"
  );
}

// TODO testing:
//  * Abort main document load while early hint is still loading -> early hint should be aborted

// two early hint responses
add_task(async function test_103_two_preload_header() {
  await test_hint_preload_internal(
    "103_two_preload_header",
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
add_task(async function test_103_two_preload_header() {
  await test_hint_preload_internal(
    "103_two_preload_header",
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
add_task(async function test_103_two_preload_header() {
  await test_hint_preload_internal(
    "103_two_preload_header",
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
    "test_103_preload",
    "http://example.com",
    "http://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 1, normal: 0 },
    uuid
  );
  await test_hint_preload(
    "test_103_preload",
    "http://example.com",
    "http://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 0, normal: 0 },
    uuid
  );
});

// Test that with config option disabled, no early hint requests are made
add_task(async function test_103_preload() {
  Services.prefs.setBoolPref("network.early-hints.enabled", false);
  await test_hint_preload(
    "test_103_preload",
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

  await Assert.deepEqual(
    gotRequestCount,
    { hinted: 1, normal: 0 },
    "test_preload_csp_imgsrc_none: Unexpected amount of requests made"
  );
});

// 400 Bad Request
add_task(async function test_103_error_400() {
  await test_hint_preload(
    "test_103_error_400",
    "http://example.com",
    "http://example.com/browser/netwerk/test/browser/early_hint_error.sjs?400",
    { hinted: 1, normal: 1 }
  );
});

// 401 Unauthorized
add_task(async function test_103_error_401() {
  await test_hint_preload(
    "test_103_error_401",
    "http://example.com",
    "http://example.com/browser/netwerk/test/browser/early_hint_error.sjs?401",
    { hinted: 1, normal: 1 }
  );
});

// 403 Forbidden
add_task(async function test_103_error_403() {
  await test_hint_preload(
    "test_103_error_403",
    "http://example.com",
    "http://example.com/browser/netwerk/test/browser/early_hint_error.sjs?403",
    { hinted: 1, normal: 1 }
  );
});

// 404 Not Found
add_task(async function test_103_error_404() {
  await test_hint_preload(
    "test_103_error_404",
    "http://example.com",
    "http://example.com/browser/netwerk/test/browser/early_hint_error.sjs?404",
    { hinted: 1, normal: 1 }
  );
});

// 408 Request Timeout
add_task(async function test_103_error_408() {
  await test_hint_preload(
    "test_103_error_408",
    "http://example.com",
    "http://example.com/browser/netwerk/test/browser/early_hint_error.sjs?408",
    { hinted: 1, normal: 1 }
  );
});

// 410 Gone
add_task(async function test_103_error_410() {
  await test_hint_preload(
    "test_103_error_410",
    "http://example.com",
    "http://example.com/browser/netwerk/test/browser/early_hint_error.sjs?410",
    { hinted: 1, normal: 0 }
  );
});

// 429 Too Many Requests
add_task(async function test_103_error_429() {
  await test_hint_preload(
    "test_103_error_429",
    "http://example.com",
    "http://example.com/browser/netwerk/test/browser/early_hint_error.sjs?429",
    { hinted: 1, normal: 1 }
  );
});

// 500 Internal Server Error
add_task(async function test_103_error_500() {
  await test_hint_preload(
    "test_103_error_500",
    "http://example.com",
    "http://example.com/browser/netwerk/test/browser/early_hint_error.sjs?500",
    { hinted: 1, normal: 1 }
  );
});

// 502 Bad Gateway
add_task(async function test_103_error_502() {
  await test_hint_preload(
    "test_103_error_502",
    "http://example.com",
    "http://example.com/browser/netwerk/test/browser/early_hint_error.sjs?502",
    { hinted: 1, normal: 1 }
  );
});

// 503 Service Unavailable
add_task(async function test_103_error_503() {
  await test_hint_preload(
    "test_103_error_503",
    "http://example.com",
    "http://example.com/browser/netwerk/test/browser/early_hint_error.sjs?503",
    { hinted: 1, normal: 1 }
  );
});

// 504 Gateway Timeout
add_task(async function test_103_error_504() {
  await test_hint_preload(
    "test_103_error_504",
    "http://example.com",
    "http://example.com/browser/netwerk/test/browser/early_hint_error.sjs?504",
    { hinted: 1, normal: 1 }
  );
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

  await Assert.deepEqual(
    gotRequestCount,
    { hinted: 0, normal: 1 },
    "test_103_iframe: Unexpected amount of requests made"
  );
});
