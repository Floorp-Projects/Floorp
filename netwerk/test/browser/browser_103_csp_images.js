/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Services.prefs.setBoolPref("network.early-hints.enabled", true);

// This verifies hints, requests server-side and client-side that the image actually loaded
async function test_image_preload_hint_request_loaded(
  input,
  expected_results,
  image_should_load
) {
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

  console.log("requestUrl: " + requestUrl);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: requestUrl,
      waitForLoad: true,
    },
    async function (browser) {
      let imageLoaded = await ContentTask.spawn(browser, [], function () {
        let image = content.document.getElementById("test_image");
        return image && image.complete && image.naturalHeight !== 0;
      });
      await Assert.ok(
        image_should_load == imageLoaded,
        "test_image_preload_hint_request_loaded: the image can be loaded as expected " +
          requestUrl
      );
    }
  );

  let gotRequestCount = await fetch(
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs"
  ).then(response => response.json());

  await Assert.deepEqual(gotRequestCount, expected_results, input.test_name);

  Services.cache2.clear();
}

// These tests verify whether or not the image actually loaded in the document
add_task(async function test_images_loaded_with_csp() {
  let tests = [
    {
      input: {
        test_name: "image loaded - no csp",
        resource_type: "image",
        csp: "",
        csp_in_early_hint: "",
        host: "",
        hinted: true,
      },
      expected: { hinted: 1, normal: 0 },
      image_should_load: true,
    },
    {
      input: {
        test_name: "image loaded - img-src none",
        resource_type: "image",
        csp: "img-src 'none';",
        csp_in_early_hint: "",
        host: "",
        hinted: true,
      },
      expected: { hinted: 1, normal: 0 },
      image_should_load: false,
    },
    {
      input: {
        test_name: "image loaded - img-src none in EH response",
        resource_type: "image",
        csp: "",
        csp_in_early_hint: "img-src 'none';",
        host: "",
        hinted: true,
      },
      expected: { hinted: 0, normal: 1 },
      image_should_load: true,
    },
    {
      input: {
        test_name: "image loaded - img-src none in both headers",
        resource_type: "image",
        csp: "img-src 'none';",
        csp_in_early_hint: "img-src 'none';",
        host: "",
        hinted: true,
      },
      expected: { hinted: 0, normal: 0 },
      image_should_load: false,
    },
    {
      input: {
        test_name: "image loaded - img-src self",
        resource_type: "image",
        csp: "img-src 'self';",
        csp_in_early_hint: "",
        host: "",
        hinted: true,
      },
      expected: { hinted: 1, normal: 0 },
      image_should_load: true,
    },
    {
      input: {
        test_name: "image loaded - img-src self in EH response",
        resource_type: "image",
        csp: "",
        csp_in_early_hint: "img-src 'self';",
        host: "",
        hinted: true,
      },
      expected: { hinted: 1, normal: 0 },
      image_should_load: true,
    },
    {
      input: {
        test_name: "image loaded - conflicting csp, early hint skipped",
        resource_type: "image",
        csp: "img-src 'self';",
        csp_in_early_hint: "img-src 'none';",
        host: "",
        hinted: true,
      },
      expected: { hinted: 0, normal: 1 },
      image_should_load: true,
    },
    {
      input: {
        test_name:
          "image loaded - conflicting csp, resource not loaded in document",
        resource_type: "image",
        csp: "img-src 'none';",
        csp_in_early_hint: "img-src 'self';",
        host: "",
        hinted: true,
      },
      expected: { hinted: 1, normal: 0 },
      image_should_load: false,
    },
  ];

  for (let test of tests) {
    await test_image_preload_hint_request_loaded(
      test.input,
      test.expected,
      test.image_should_load
    );
  }
});
