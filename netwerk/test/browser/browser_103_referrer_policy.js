/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Services.prefs.setBoolPref("network.early-hints.enabled", true);

async function test_referrer_policy(input, expected_results) {
  // reset the count
  let headers = new Headers();
  headers.append("X-Early-Hint-Count-Start", "");
  await fetch(
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs",
    { headers }
  );

  await fetch(
    "https://example.com/browser/netwerk/test/browser/early_hint_referrer_policy_html.sjs?action=reset_referrer_results"
  );

  let requestUrl = `https://example.com/browser/netwerk/test/browser/early_hint_referrer_policy_html.sjs?as=${
    input.resource_type
  }&hinted=${input.hinted ? "1" : "0"}${
    input.header_referrer_policy
      ? "&header_referrer_policy=" + input.header_referrer_policy
      : ""
  }
    ${
      input.link_referrer_policy
        ? "&link_referrer_policy=" + input.link_referrer_policy
        : ""
    }`;

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

  // Retrieve the request referrer from the server
  let referrer_response = await fetch(
    "https://example.com/browser/netwerk/test/browser/early_hint_referrer_policy_html.sjs?action=get_request_referrer_results"
  ).then(response => response.text());

  Assert.ok(
    referrer_response === expected_results.referrer,
    "Request referrer matches expected - " + input.test_name
  );

  await Assert.deepEqual(
    gotRequestCount,
    { hinted: expected_results.hinted, normal: expected_results.normal },
    `${input.testName} (${input.resource_type}): Unexpected amount of requests made`
  );
}

add_task(async function test_103_referrer_policies() {
  let tests = [
    {
      input: {
        test_name: "image - no policies",
        resource_type: "image",
        header_referrer_policy: "",
        link_referrer_policy: "",
        hinted: true,
      },
      expected: {
        hinted: 1,
        normal: 0,
        referrer:
          "https://example.com/browser/netwerk/test/browser/early_hint_referrer_policy_html.sjs?as=image&hinted=1",
      },
    },
    {
      input: {
        test_name: "image - origin on header",
        resource_type: "image",
        header_referrer_policy: "origin",
        link_referrer_policy: "",
        hinted: true,
      },
      expected: { hinted: 1, normal: 0, referrer: "https://example.com/" },
    },
    {
      input: {
        test_name: "image - origin on link",
        resource_type: "image",
        header_referrer_policy: "",
        link_referrer_policy: "origin",
        hinted: true,
      },
      expected: { hinted: 1, normal: 0, referrer: "https://example.com/" },
    },
    {
      input: {
        test_name: "image - origin on both",
        resource_type: "image",
        header_referrer_policy: "origin",
        link_referrer_policy: "origin",
        hinted: true,
      },
      expected: { hinted: 1, normal: 0, referrer: "https://example.com/" },
    },
    {
      input: {
        test_name: "image - no-referrer on header",
        resource_type: "image",
        header_referrer_policy: "no-referrer",
        link_referrer_policy: "",
        hinted: true,
      },
      expected: { hinted: 1, normal: 0, referrer: "" },
    },
    {
      input: {
        test_name: "image - no-referrer on link",
        resource_type: "image",
        header_referrer_policy: "",
        link_referrer_policy: "no-referrer",
        hinted: true,
      },
      expected: { hinted: 1, normal: 0, referrer: "" },
    },
    {
      input: {
        test_name: "image - no-referrer on both",
        resource_type: "image",
        header_referrer_policy: "no-referrer",
        link_referrer_policy: "no-referrer",
        hinted: true,
      },
      expected: { hinted: 1, normal: 0, referrer: "" },
    },
    {
      // link referrer policy takes precedence
      input: {
        test_name: "image - origin on header, no-referrer on link",
        resource_type: "image",
        header_referrer_policy: "origin",
        link_referrer_policy: "no-referrer",
        hinted: true,
      },
      expected: { hinted: 1, normal: 0, referrer: "" },
    },
    {
      // link referrer policy takes precedence
      input: {
        test_name: "image - no-referrer on header, origin on link",
        resource_type: "image",
        header_referrer_policy: "no-referrer",
        link_referrer_policy: "origin",
        hinted: true,
      },
      expected: { hinted: 1, normal: 0, referrer: "https://example.com/" },
    },
  ];

  for (let test of tests) {
    await test_referrer_policy(test.input, test.expected);
  }
});
