/**
 * Test for LoginHelper.getLoginOrigin
 */

"use strict";

const TESTCASES = [
  ["javascript:void(0);", null],
  ["javascript:void(0);", "javascript:", true],
  ["chrome://MyAccount", "chrome://myaccount"],
  ["data:text/html,example", null],
  [
    "http://username:password@example.com:80/foo?bar=baz#fragment",
    "http://example.com",
    true,
  ],
  ["http://127.0.0.1:80/foo", "http://127.0.0.1"],
  ["http://[::1]:80/foo", "http://[::1]"],
  ["http://example.com:8080/foo", "http://example.com:8080"],
  ["http://127.0.0.1:8080/foo", "http://127.0.0.1:8080", true],
  ["http://[::1]:8080/foo", "http://[::1]:8080"],
  ["https://example.com:443/foo", "https://example.com"],
  ["https://[::1]:443/foo", "https://[::1]"],
  ["https://[::1]:8443/foo", "https://[::1]:8443"],
  ["ftp://username:password@[::1]:2121/foo", "ftp://[::1]:2121"],
  [
    "moz-proxy://username:password@123.123.123.123:12345/foo",
    "moz-proxy://123.123.123.123:12345",
  ],
];

for (let [input, expected, allowJS] of TESTCASES) {
  let actual = LoginHelper.getLoginOrigin(input, allowJS);
  Assert.strictEqual(actual, expected, "Checking: " + input);
}
