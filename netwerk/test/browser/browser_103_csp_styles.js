/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Services.prefs.setBoolPref("network.early-hints.enabled", true);

const { test_preload_hint_and_request } = ChromeUtils.importESModule(
  "resource://testing-common/early_hint_preload_test_helper.sys.mjs"
);

add_task(async function test_preload_styles_csp_in_response() {
  let tests = [
    {
      input: {
        test_name: "style - no csp",
        resource_type: "style",
        csp: "",
        csp_in_early_hint: "",
        host: "",
        hinted: true,
      },
      expected: { hinted: 1, normal: 0 },
    },
    {
      input: {
        test_name: "style style-src 'self';",
        resource_type: "style",
        csp: "",
        csp_in_early_hint: "style-src 'self';",
        host: "",
        hinted: true,
      },
      expected: { hinted: 1, normal: 0 },
    },
    {
      input: {
        test_name: "style style-src self; same host provided",
        resource_type: "style",
        csp: "",
        csp_in_early_hint: "style-src 'self';",
        host: "https://example.com/browser/netwerk/test/browser/",
        hinted: true,
      },
      expected: { hinted: 1, normal: 0 },
    },
    {
      input: {
        test_name: "style style-src 'self'; other host provided",
        resource_type: "style",
        csp: "",
        csp_in_early_hint: "style-src 'self';",
        host: "https://example.org/browser/netwerk/test/browser/",
        hinted: true,
      },
      expected: { hinted: 0, normal: 1 },
    },
    {
      input: {
        test_name: "style style-src 'none';",
        resource_type: "style",
        csp: "",
        csp_in_early_hint: "style-src 'none';",
        host: "",
        hinted: true,
      },
      expected: { hinted: 0, normal: 1 },
    },
    {
      input: {
        test_name: "style style-src 'none'; other host provided",
        resource_type: "style",
        csp: "",
        csp_in_early_hint: "style-src 'none';",
        host: "https://example.org/browser/netwerk/test/browser/",
        hinted: true,
      },
      expected: { hinted: 0, normal: 1 },
    },
  ];

  for (let test of tests) {
    await test_preload_hint_and_request(test.input, test.expected);
  }
});
