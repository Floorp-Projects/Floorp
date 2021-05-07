/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

AddonTestUtils.init(this);
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

const server = createHttpServer({
  hosts: ["example.com", "anotherdomain.com"],
});
server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.write("test_ext_cache_api.js");
});

add_task(async function test_cache_api_http_resource_allowed() {
  async function background() {
    try {
      const BASE_URL = `http://example.com/dummy`;

      const cache = await window.caches.open("test-cache-api");
      browser.test.assertTrue(
        await window.caches.has("test-cache-api"),
        "CacheStorage.has should resolve to true"
      );

      // Test that adding and requesting cached http urls
      // works as well.
      await cache.add(BASE_URL);
      browser.test.assertEq(
        "test_ext_cache_api.js",
        await cache.match(BASE_URL).then(res => res.text()),
        "Got the expected content from the cached http url"
      );

      // Test that the http urls that the cache API is allowed
      // to fetch and cache are limited by the host permissions
      // associated to the extensions (same as when the extension
      // for fetch from those urls using fetch or XHR).
      await browser.test.assertRejects(
        cache.add(`http://anotherdomain.com/dummy`),
        "NetworkError when attempting to fetch resource.",
        "Got the expected rejection of requesting an http not allowed by host permissions"
      );

      // Test that deleting the cache storage works as expected.
      browser.test.assertTrue(
        await window.caches.delete("test-cache-api"),
        "Cache deleted successfully"
      );
      browser.test.assertTrue(
        !(await window.caches.has("test-cache-api")),
        "CacheStorage.has should resolve to false"
      );
    } catch (err) {
      browser.test.fail(`Unexpected error on using Cache API: ${err}`);
      throw err;
    } finally {
      browser.test.sendMessage("test-cache-api-allowed");
    }
  }

  // Verify that Cache API support for http urls is available
  // regardless of extensions.backgroundServiceWorker.enabled pref.
  const extension = ExtensionTestUtils.loadExtension({
    manifest: { permissions: ["http://example.com/*"] },
    background,
  });

  await extension.startup();

  await extension.awaitMessage("test-cache-api-allowed");
  await extension.unload();
});

add_task(async function test_cache_storage_evicted_on_addon_uninstalled() {
  async function background() {
    try {
      const BASE_URL = `http://example.com/dummy`;

      const cache = await window.caches.open("test-cache-api");
      browser.test.assertTrue(
        await window.caches.has("test-cache-api"),
        "CacheStorage.has should resolve to true"
      );

      // Test that adding and requesting cached http urls
      // works as well.
      await cache.add(BASE_URL);
      browser.test.assertEq(
        "test_ext_cache_api.js",
        await cache.match(BASE_URL).then(res => res.text()),
        "Got the expected content from the cached http url"
      );
    } catch (err) {
      browser.test.fail(`Unexpected error on using Cache API: ${err}`);
      throw err;
    } finally {
      browser.test.sendMessage("cache-storage-created");
    }
  }

  const extension = ExtensionTestUtils.loadExtension({
    manifest: { permissions: ["http://example.com/*"] },
    background,
    // Necessary to be sure the expected extension stored data cleanup callback
    // will be called when the extension is uninstalled from an AddonManager
    // perspective.
    useAddonManager: "temporary",
  });

  await AddonTestUtils.promiseStartupManager();
  await extension.startup();
  await extension.awaitMessage("cache-storage-created");

  const extURL = `moz-extension://${extension.extension.uuid}`;
  const extPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI(extURL),
    {}
  );
  let extCacheStorage = new CacheStorage("content", extPrincipal);

  ok(
    await extCacheStorage.has("test-cache-api"),
    "Got the expected extension cache storage"
  );

  await extension.unload();

  ok(
    !(await extCacheStorage.has("test-cache-api")),
    "The extension cache storage data should have been evicted on addon uninstall"
  );
});
