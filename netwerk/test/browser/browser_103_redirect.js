/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Services.prefs.setBoolPref("network.early-hints.enabled", true);

const { test_hint_preload } = ChromeUtils.import(
  "resource://testing-common/early_hint_preload_test_helper.jsm"
);

// Early hint to redirect to same origin in secure context
add_task(async function test_103_redirect_same_origin() {
  await test_hint_preload(
    "test_103_redirect_same_origin",
    "https://example.com",
    "https://example.com/browser/netwerk/test/browser/early_hint_redirect.sjs?https://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 2, normal: 0 } // successful preload of redirect and resulting image
  );
});

// Early hint to redirect to cross origin in secure context
add_task(async function test_103_redirect_cross_origin() {
  await test_hint_preload(
    "test_103_redirect_cross_origin",
    "https://example.com",
    "https://example.com/browser/netwerk/test/browser/early_hint_redirect.sjs?https://example.net/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 2, normal: 0 } // successful load of redirect in preload, but image loaded via normal load
  );
});

// Early hint to redirect to cross origin in insecure context
add_task(async function test_103_redirect_insecure_cross_origin() {
  await test_hint_preload(
    "test_103_redirect_insecure_cross_origin",
    "https://example.com",
    "https://example.com/browser/netwerk/test/browser/early_hint_redirect.sjs?http://mochi.test:8888/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 1, normal: 1 }
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
