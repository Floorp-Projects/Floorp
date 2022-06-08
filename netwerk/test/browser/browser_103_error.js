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

// 400 Bad Request
add_task(async function test_103_error_400() {
  await test_hint_preload(
    "test_103_error_400",
    "https://example.com",
    "https://example.com/browser/netwerk/test/browser/early_hint_error.sjs?400",
    { hinted: 1, normal: 1 }
  );
});

// 401 Unauthorized
add_task(async function test_103_error_401() {
  await test_hint_preload(
    "test_103_error_401",
    "https://example.com",
    "https://example.com/browser/netwerk/test/browser/early_hint_error.sjs?401",
    { hinted: 1, normal: 1 }
  );
});

// 403 Forbidden
add_task(async function test_103_error_403() {
  await test_hint_preload(
    "test_103_error_403",
    "https://example.com",
    "https://example.com/browser/netwerk/test/browser/early_hint_error.sjs?403",
    { hinted: 1, normal: 1 }
  );
});

// 404 Not Found
add_task(async function test_103_error_404() {
  await test_hint_preload(
    "test_103_error_404",
    "https://example.com",
    "https://example.com/browser/netwerk/test/browser/early_hint_error.sjs?404",
    { hinted: 1, normal: 1 }
  );
});

// 408 Request Timeout
add_task(async function test_103_error_408() {
  await test_hint_preload(
    "test_103_error_408",
    "https://example.com",
    "https://example.com/browser/netwerk/test/browser/early_hint_error.sjs?408",
    { hinted: 1, normal: 1 }
  );
});

// 410 Gone
add_task(async function test_103_error_410() {
  await test_hint_preload(
    "test_103_error_410",
    "https://example.com",
    "https://example.com/browser/netwerk/test/browser/early_hint_error.sjs?410",
    { hinted: 1, normal: 0 }
  );
});

// 429 Too Many Requests
add_task(async function test_103_error_429() {
  await test_hint_preload(
    "test_103_error_429",
    "https://example.com",
    "https://example.com/browser/netwerk/test/browser/early_hint_error.sjs?429",
    { hinted: 1, normal: 1 }
  );
});

// 500 Internal Server Error
add_task(async function test_103_error_500() {
  await test_hint_preload(
    "test_103_error_500",
    "https://example.com",
    "https://example.com/browser/netwerk/test/browser/early_hint_error.sjs?500",
    { hinted: 1, normal: 1 }
  );
});

// 502 Bad Gateway
add_task(async function test_103_error_502() {
  await test_hint_preload(
    "test_103_error_502",
    "https://example.com",
    "https://example.com/browser/netwerk/test/browser/early_hint_error.sjs?502",
    { hinted: 1, normal: 1 }
  );
});

// 503 Service Unavailable
add_task(async function test_103_error_503() {
  await test_hint_preload(
    "test_103_error_503",
    "https://example.com",
    "https://example.com/browser/netwerk/test/browser/early_hint_error.sjs?503",
    { hinted: 1, normal: 1 }
  );
});

// 504 Gateway Timeout
add_task(async function test_103_error_504() {
  await test_hint_preload(
    "test_103_error_504",
    "https://example.com",
    "https://example.com/browser/netwerk/test/browser/early_hint_error.sjs?504",
    { hinted: 1, normal: 1 }
  );
});
