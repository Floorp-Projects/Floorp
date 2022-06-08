Services.prefs.setBoolPref("network.early-hints.enabled", true);

// - testName is just there to be printed during Asserts when failing
// - asset is the asset type, see early_hint_asset_html.sjs for possible values
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

  await Assert.deepEqual(
    gotRequestCount,
    hinted ? { hinted: 1, normal: 0 } : { hinted: 0, normal: 1 },
    `${testName} (${asset}): Unexpected amount of requests made`
  );
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

// preload fetch
add_task(async function test_103_asset_fetch() {
  await test_hint_asset("test_103_asset_hinted", "fetch", true);
  await test_hint_asset("test_103_asset_normal", "fetch", false);
});

// preload font
add_task(async function test_103_asset_font() {
  await test_hint_asset("test_103_asset_hinted", "font", true);
  await test_hint_asset("test_103_asset_normal", "font", false);
});
