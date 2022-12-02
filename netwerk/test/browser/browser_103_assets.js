/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Services.prefs.setBoolPref("network.early-hints.enabled", true);

const { lax_request_count_checking } = ChromeUtils.import(
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

  // TODO: Switch to stricter counting method after fixing https://bugzilla.mozilla.org/show_bug.cgi?id=1753730#c11
  await lax_request_count_checking(
    `${testName} (${asset})`,
    gotRequestCount,
    hinted ? { hinted: 1, normal: 0 } : { hinted: 0, normal: 1 }
  );
  /*
  await Assert.deepEqual(
    gotRequestCount,
    hinted ? { hinted: 1, normal: 0 } : { hinted: 0, normal: 1 },
    `${testName} (${asset}): Unexpected amount of requests made`
  );
  */
}

// preload image
add_task(async function test_103_asset_style() {
  await test_hint_asset("test_103_asset_hinted", "image", true);
  await test_hint_asset("test_103_asset_normal", "image", false);
});

// preload css
add_task(async function test_103_asset_style() {
  await test_hint_asset("test_103_asset_hinted", "style", true);
  await test_hint_asset("test_103_asset_normal", "style", false);
});

// preload javascript
add_task(async function test_103_asset_javascript() {
  await test_hint_asset("test_103_asset_hinted", "script", true);
  await test_hint_asset("test_103_asset_normal", "script", false);
});

// preload font
add_task(async function test_103_asset_font() {
  await test_hint_asset("test_103_asset_hinted", "font", true);
  await test_hint_asset("test_103_asset_normal", "font", false);
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

  // TODO: Switch to stricter counting method after fixing https://bugzilla.mozilla.org/show_bug.cgi?id=1753730#c11
  await lax_request_count_checking(
    `${testName} (fetch)`,
    gotRequestCount,
    hinted ? { hinted: 1, normal: 0 } : { hinted: 0, normal: 1 }
  );
  /*
  await Assert.deepEqual(
    gotRequestCount,
    hinted ? { hinted: 1, normal: 0 } : { hinted: 0, normal: 1 },
    `${testName} (fetch): Unexpected amount of requests made`
  );
  */
}

// preload fetch
add_task(async function test_103_asset_fetch() {
  await test_hint_fetch("test_103_asset_hinted", true);
  await test_hint_fetch("test_103_asset_normal", false);
});
