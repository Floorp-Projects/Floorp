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

add_setup(() => {
  // NOTE: Services.io.offline shouldn't be set to offline,
  // otherwise we would hit an unexpected behavior when
  // the extension worker tries to fetch from an
  // http url or cache an http url response, see Bug 1845317.
  Assert.ok(
    !Services.io.offline,
    "Services.io.offline should not be set to true while running this test"
  );
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
        await cache.match(BASE_URL).then(res => res?.text()),
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

  const background = async function () {
    const BASE_URL_OK = `http://example.com/dummy`;
    const BASE_URL_KO = `http://anotherdomain.com/dummy`;
    const worker = new SharedWorker("worker.js");
    const { data: resultOK } = await new Promise(resolve => {
      worker.port.onmessage = resolve;
      worker.port.postMessage(["worker-cacheapi-test-allowed", BASE_URL_OK]);
    });
    browser.test.assertDeepEq(
      ["worker-cacheapi-test-allowed:result", { success: true }],
      resultOK,
      "Got success result from extension worker for allowed host url"
    );
    const { data: resultKO } = await new Promise(resolve => {
      worker.port.onmessage = resolve;
      worker.port.postMessage(["worker-cacheapi-test-disallowed", BASE_URL_KO]);
    });
    browser.test.assertDeepEq(
      [
        "worker-cacheapi-test-disallowed:result",
        { error: "NetworkError when attempting to fetch resource." },
      ],
      resultKO,
      "Got result from extension worker for disallowed host url"
    );

    browser.test.assertTrue(
      await window.caches.has("test-cache-api"),
      "CacheStorage.has should resolve to true"
    );
    const cache = await window.caches.open("test-cache-api");
    browser.test.assertEq(
      "test_ext_cache_api.js",
      await cache.match(BASE_URL_OK).then(res => res?.text()),
      "Got the expected content from the cached http url"
    );
    browser.test.assertEq(
      true,
      await cache.match(BASE_URL_KO).then(res => res == undefined),
      "Got no match for the http url that isn't allowed by host permissions"
    );

    browser.test.sendMessage("test-cacheapi-sharedworker:done");
  };

  const extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: { permissions: ["http://example.com/*"] },
    files: {
      "worker.js": function () {
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
  await extension.awaitMessage("test-cacheapi-sharedworker:done");
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
        await cache.match(BASE_URL).then(res => res?.text()),
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

add_task(
  {
    // Pref used to allow to use the Cache WebAPI related to a page loaded from http
    // (otherwise Gecko will throw a SecurityError when trying to access the webpage
    // cache storage from the content script, unless the webpage is loaded from https).
    pref_set: [["dom.caches.testing.enabled", true]],
  },
  async function test_cache_put_from_contentscript() {
    const extension = ExtensionTestUtils.loadExtension({
      manifest: {
        content_scripts: [
          {
            matches: ["http://example.com/*"],
            js: ["content.js"],
          },
        ],
      },
      files: {
        "content.js": async function () {
          const cache = await caches.open("test-cachestorage");
          const request = "http://example.com";
          const response = await fetch(request);
          await cache.put(request, response).catch(err => {
            browser.test.sendMessage("cache-put-error", {
              name: err.name,
              message: err.message,
            });
          });
        },
      },
    });

    await extension.startup();

    const page = await ExtensionTestUtils.loadContentPage("http://example.com");
    const actualError = await extension.awaitMessage("cache-put-error");
    equal(
      actualError.name,
      "SecurityError",
      "Got a security error from cache.put call as expected"
    );
    ok(
      /Disallowed on WebExtension ContentScript Request/.test(
        actualError.message
      ),
      `Got the expected error message: ${actualError.message}`
    );

    await page.close();
    await extension.unload();
  }
);
