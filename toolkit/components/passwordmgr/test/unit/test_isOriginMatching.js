/*
 * Test LoginHelper.isOriginMatching
 */

"use strict";

Cu.import("resource://gre/modules/LoginHelper.jsm");

add_task(function test_isOriginMatching() {
  let testcases = [
    // Index 0 holds the expected return value followed by arguments to isOriginMatching.
    [true, "http://example.com", "http://example.com"],
    [true, "http://example.com:8080", "http://example.com:8080"],
    [true, "https://example.com", "https://example.com"],
    [true, "https://example.com:8443", "https://example.com:8443"],
    [false, "http://example.com", "http://mozilla.org"],
    [false, "http://example.com", "http://example.com:8080"],
    [false, "https://example.com", "http://example.com"],
    [false, "https://example.com", "https://mozilla.org"],
    [false, "http://example.com", "http://sub.example.com"],
    [false, "https://example.com", "https://sub.example.com"],
    [false, "http://example.com", "https://example.com:8443"],
    [false, "http://example.com:8080", "http://example.com:8081"],
    [false, "http://example.com", ""],
    [false, "", "http://example.com"],
    [true, "http://example.com", "https://example.com", { schemeUpgrades: true }],
    [true, "https://example.com", "https://example.com", { schemeUpgrades: true }],
    [true, "http://example.com:8080", "http://example.com:8080", { schemeUpgrades: true }],
    [true, "https://example.com:8443", "https://example.com:8443", { schemeUpgrades: true }],
    [false, "https://example.com", "http://example.com", { schemeUpgrades: true }], // downgrade
    [false, "http://example.com:8080", "https://example.com", { schemeUpgrades: true }], // port mismatch
    [false, "http://example.com", "https://example.com:8443", { schemeUpgrades: true }], // port mismatch
    [false, "http://sub.example.com", "http://example.com", { schemeUpgrades: true }],
  ];
  for (let tc of testcases) {
    let expected = tc.shift();
    Assert.strictEqual(LoginHelper.isOriginMatching(...tc), expected,
                       "Check " + JSON.stringify(tc));
  }
});
