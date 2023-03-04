/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Services.prefs.setBoolPref("network.early-hints.enabled", true);

const { test_preload_hint_and_request } = ChromeUtils.importESModule(
  "resource://testing-common/early_hint_preload_test_helper.sys.mjs"
);

add_task(async function test_preload_images_csp_in_early_hints_response() {
  let tests = [
    {
      input: {
        test_name: "image - no csp",
        resource_type: "image",
        csp: "",
        csp_in_early_hint: "",
        host: "",
        hinted: true,
      },
      expected: { hinted: 1, normal: 0 },
    },
    {
      input: {
        test_name: "image img-src 'self';",
        resource_type: "image",
        csp: "",
        csp_in_early_hint: "img-src 'self';",
        host: "",
        hinted: true,
      },
      expected: { hinted: 1, normal: 0 },
    },
    {
      input: {
        test_name: "image img-src 'self'; same host provided",
        resource_type: "image",
        csp: "",
        csp_in_early_hint: "img-src 'self';",
        host: "https://example.com/browser/netwerk/test/browser/",
        hinted: true,
      },
      expected: { hinted: 1, normal: 0 },
    },
    {
      input: {
        test_name: "image img-src 'self'; other host provided",
        resource_type: "image",
        csp: "",
        csp_in_early_hint: "img-src 'self';",
        host: "https://example.org/browser/netwerk/test/browser/",
        hinted: true,
      },
      expected: { hinted: 0, normal: 1 },
    },
    {
      input: {
        test_name: "image img-src 'none';",
        resource_type: "image",
        csp: "",
        csp_in_early_hint: "img-src 'none';",
        host: "",
        hinted: true,
      },
      expected: { hinted: 0, normal: 1 },
    },
    {
      input: {
        test_name: "image img-src 'none'; same host provided",
        resource_type: "image",
        csp: "",
        csp_in_early_hint: "img-src 'none';",
        host: "https://example.com/browser/netwerk/test/browser/",
        hinted: true,
      },
      expected: { hinted: 0, normal: 1 },
    },
  ];

  for (let test of tests) {
    await test_preload_hint_and_request(test.input, test.expected);
  }
});
