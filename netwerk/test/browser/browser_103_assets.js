/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Services.prefs.setBoolPref("network.early-hints.enabled", true);

const { request_count_checking } = ChromeUtils.import(
  "resource://testing-common/early_hint_preload_test_helper.jsm"
);

// - testName is just there to be printed during Asserts when failing
// - asset is the asset type, see early_hint_asset_html.sjs for possible values
//   for the asset type fetch see test_hint_fetch due to timing issues
// - hinted: when true, the server reponds with "103 Early Hints"-header
async function test_hint_asset(testName, asset, hinted) {
  // reset the count
  let headers = new Headers();
  headers.append("X-Early-Hint-Count-Start", "");
  await fetch(
    "http://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs",
    { headers }
  );

  let requestUrl = `https://example.com/browser/netwerk/test/browser/early_hint_asset_html.sjs?as=${asset}&hinted=${
    hinted ? "1" : "0"
  }`;

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

  await request_count_checking(
    `${testName} (${asset})`,
    gotRequestCount,
    hinted ? { hinted: 1, normal: 0 } : { hinted: 0, normal: 1 }
  );
}

// preload image
add_task(async function test_103_asset_image() {
  await test_hint_asset("test_103_asset_normal", "image", false);
  await test_hint_asset("test_103_asset_hinted", "image", true);
});

// preload css
add_task(async function test_103_asset_style() {
  await test_hint_asset("test_103_asset_normal", "style", false);
  await test_hint_asset("test_103_asset_hinted", "style", true);
});

// preload javascript
add_task(async function test_103_asset_javascript() {
  await test_hint_asset("test_103_asset_normal", "script", false);
  await test_hint_asset("test_103_asset_hinted", "script", true);
});

// preload javascript module
/* TODO(Bug 1798319): enable this test case
add_task(async function test_103_asset_module() {
  await test_hint_asset("test_103_asset_normal", "module", false);
  await test_hint_asset("test_103_asset_hinted", "module", true);
});
*/

// preload font
add_task(async function test_103_asset_font() {
  await test_hint_asset("test_103_asset_normal", "font", false);
  await test_hint_asset("test_103_asset_hinted", "font", true);
});

// - testName is just there to be printed during Asserts when failing
// - asset is the asset type, see early_hint_asset_html.sjs for possible values
// - hinted: when true, the server reponds with "103 Early Hints"-header
async function test_hint_fetch(testName, hinted) {
  // reset the count
  let headers = new Headers();
  headers.append("X-Early-Hint-Count-Start", "");
  await fetch(
    "http://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs",
    { headers }
  );

  let requestUrl = `https://example.com/browser/netwerk/test/browser/early_hint_asset_html.sjs?as=fetch&hinted=${
    hinted ? "1" : "0"
  }`;

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: requestUrl,
      waitForLoad: true,
    },
    async function(browser) {
      // wait until the fetch is complete
      await TestUtils.waitForCondition(_ => {
        return SpecialPowers.spawn(browser, [], _ => {
          return (
            content.document.getElementsByTagName("h2")[0] != undefined &&
            content.document.getElementsByTagName("h2")[0].textContent !==
              "Fetching..." // default text set by early_hint_asset_html.sjs
          );
        });
      });
    }
  );

  let gotRequestCount = await fetch(
    "http://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs"
  ).then(response => response.json());

  await request_count_checking(
    `${testName} (fetch)`,
    gotRequestCount,
    hinted ? { hinted: 1, normal: 0 } : { hinted: 0, normal: 1 }
  );
}

// preload fetch
add_task(async function test_103_asset_fetch() {
  await test_hint_fetch("test_103_asset_normal", false);
  await test_hint_fetch("test_103_asset_hinted", true);
});
