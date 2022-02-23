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

// This test is similar to `test_cache_api_http_resource_allowed` but it does
// exercise the Cache API from a moz-extension shared worker.
// We expect the cache API calls to be successfull when it is being used to
// cache an HTTP url that is allowed for the extensions based on its host
// permission, but to fail if the extension doesn't have the required host
// permission to fetch data from that url.
add_task(async function test_cache_api_from_ext_shared_worker() {
  if (!WebExtensionPolicy.useRemoteWebExtensions) {
    // Ensure RemoteWorkerService has been initialized in the main
    // process.
    Services.obs.notifyObservers(null, "profile-after-change");
  }

  const background = async function() {
    const BASE_URL_OK = `http://example.com/dummy`;
    const BASE_URL_KO = `http://anotherdomain.com/dummy`;
    const worker = new SharedWorker("worker.js");
    const { data: resultOK } = await new Promise(resolve => {
      worker.port.onmessage = resolve;
      worker.port.postMessage(["worker-cacheapi-test-allowed", BASE_URL_OK]);
    });
    browser.test.log(
      `Got result from extension worker for allowed host url: ${JSON.stringify(
        resultOK
      )}`
    );
    const { data: resultKO } = await new Promise(resolve => {
      worker.port.onmessage = resolve;
      worker.port.postMessage(["worker-cacheapi-test-disallowed", BASE_URL_KO]);
    });
    browser.test.log(
      `Got result from extension worker for disallowed host url: ${JSON.stringify(
        resultKO
      )}`
    );

    browser.test.assertTrue(
      await window.caches.has("test-cache-api"),
      "CacheStorage.has should resolve to true"
    );
    const cache = await window.caches.open("test-cache-api");
    browser.test.assertEq(
      "test_ext_cache_api.js",
      await cache.match(BASE_URL_OK).then(res => res.text()),
      "Got the expected content from the cached http url"
    );
    browser.test.assertEq(
      true,
      await cache.match(BASE_URL_KO).then(res => res == undefined),
      "Got no match for the http url that isn't allowed by host permissions"
    );

    browser.test.sendMessage("test-cacheapi-sharedworker:done", {
      expectAllowed: resultOK,
      expectDisallowed: resultKO,
    });
  };

  const extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: { permissions: ["http://example.com/*"] },
    files: {
      "worker.js": function() {
        self.onconnect = evt => {
          const port = evt.ports[0];
          port.onmessage = async evt => {
            let result = {};
            let message;
            try {
              const [msg, BASE_URL] = evt.data;
              message = msg;
              const cache = await self.caches.open("test-cache-api");
              await cache.add(BASE_URL);
              result.success = true;
            } catch (err) {
              result.error = err.message;
              throw err;
            } finally {
              port.postMessage([`${message}:result`, result]);
            }
          };
        };
      },
    },
  });

  await extension.startup();
  const { expectAllowed, expectDisallowed } = await extension.awaitMessage(
    "test-cacheapi-sharedworker:done"
  );
  // Increase the chance to have the error message related to an unexpected
  // failure to be explicitly mention in the failure message.
  Assert.deepEqual(
    expectAllowed,
    ["worker-cacheapi-test-allowed:result", { success: true }],
    "Expect worker result to be successfull with the required host permission"
  );
  Assert.deepEqual(
    expectDisallowed,
    [
      "worker-cacheapi-test-disallowed:result",
      { error: "NetworkError when attempting to fetch resource." },
    ],
    "Expect worker result to be unsuccessfull without the required host permission"
  );

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
