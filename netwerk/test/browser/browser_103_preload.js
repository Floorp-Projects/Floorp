/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Services.prefs.setBoolPref("network.early-hints.enabled", true);
// Disable mixed-content upgrading as this test is expecting HTTP image loads
Services.prefs.setBoolPref(
  "security.mixed_content.upgrade_display_content",
  false
);

const { request_count_checking, test_preload_url, test_hint_preload } =
  ChromeUtils.importESModule(
    "resource://testing-common/early_hint_preload_test_helper.sys.mjs"
  );

// TODO testing:
//  * Abort main document load while early hint is still loading -> early hint should be aborted

// Test that with early hint config option disabled, no early hint requests are made
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

add_task(async function test_103_font_disabled() {
  let url =
    "https://example.com/browser/netwerk/test/browser/early_hint_asset_html.sjs?hinted=1&as=font";
  Services.prefs.setBoolPref("gfx.downloadable_fonts.enabled", false);
  await test_preload_url("font_loading_disabled", url, {
    hinted: 0,
    normal: 0,
  });
  Services.prefs.setBoolPref("gfx.downloadable_fonts.enabled", true);
  await test_preload_url("font_loading_enabled", url, {
    hinted: 1,
    normal: 0,
  });
  Services.prefs.clearUserPref("gfx.downloadable_fonts.enabled");
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

// Same preload from localhost to localhost should preload
add_task(async function test_103_preload_localhost_to_localhost() {
  await test_hint_preload(
    "test_103_preload_localhost_to_localhost",
    "http://127.0.0.1:8888",
    "http://127.0.0.1:8888/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 1, normal: 0 }
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
