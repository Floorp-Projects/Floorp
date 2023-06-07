/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Services.prefs.setBoolPref("network.early-hints.enabled", true);

registerCleanupFunction(function () {
  Services.prefs.clearUserPref("network.early-hints.enabled");
});

const { request_count_checking } = ChromeUtils.importESModule(
  "resource://testing-common/early_hint_preload_test_helper.sys.mjs"
);

// Test steps:
// 1. Install an extension to observe the tabId.
// 2. Load early_hint_asset_html.sjs?as=image, so we should see a hinted
//    request to early_hint_asset.sjs?as=image.
// 3. Check if the hinted request has the same tabId as
//    early_hint_asset_html.sys.
add_task(async function test_103_asset_with_extension() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: [
        "webRequest",
        "webRequestBlocking",
        "https://example.com/*",
      ],
    },
    background() {
      let { browser } = this;
      browser.webRequest.onBeforeRequest.addListener(
        details => {
          browser.test.sendMessage("request", {
            url: details.url,
            tabId: details.tabId,
          });
          return undefined;
        },
        { urls: ["https://example.com/*"] },
        ["blocking"]
      );
    },
  });

  await extension.startup();

  // reset the count
  let headers = new Headers();
  headers.append("X-Early-Hint-Count-Start", "");
  await fetch(
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs",
    { headers }
  );

  let baseUrl = "https://example.com/browser/netwerk/test/browser/";
  let requestUrl = `${baseUrl}early_hint_asset_html.sjs?as=image&hinted=1`;
  let assetUrl = `${baseUrl}early_hint_asset.sjs?as=image`;

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: requestUrl,
      waitForLoad: true,
    },
    async function () {}
  );

  const result1 = await extension.awaitMessage("request");
  Assert.equal(result1.url, requestUrl);
  let tabId = result1.tabId;

  const result2 = await extension.awaitMessage("request");
  Assert.equal(result2.url, assetUrl);
  Assert.equal(result2.tabId, tabId);

  await extension.unload();
});
