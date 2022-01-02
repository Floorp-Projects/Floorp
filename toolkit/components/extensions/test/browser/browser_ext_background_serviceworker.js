/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* globals getBackgroundServiceWorkerRegistration, waitForServiceWorkerTerminated */

Services.scriptloader.loadSubScript(
  new URL("head_serviceworker.js", gTestPath).href,
  this
);

add_task(assert_background_serviceworker_pref_enabled);

add_task(async function test_serviceWorker_register_guarded_by_pref() {
  // Test with backgroundServiceWorkeEnable set to true and the
  // extensions.serviceWorkerRegist.allowed pref set to false.
  // NOTE: the scenario with backgroundServiceWorkeEnable set to false
  // is part of "browser_ext_background_serviceworker_pref_disabled.js".

  await SpecialPowers.pushPrefEnv({
    set: [["extensions.serviceWorkerRegister.allowed", false]],
  });

  let extensionData = {
    files: {
      "page.html": "<!DOCTYPE html><script src='page.js'></script>",
      "page.js": async function() {
        try {
          await navigator.serviceWorker.register("sw.js");
          browser.test.fail(
            `An extension page should not be able to register a serviceworker successfully`
          );
        } catch (err) {
          browser.test.assertEq(
            String(err),
            "SecurityError: The operation is insecure.",
            "Got the expected error on registering a service worker from a script"
          );
        }
        browser.test.sendMessage("test-serviceWorker-register-disallowed");
      },
      "sw.js": "",
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

  // Verify that an extension page can't register a moz-extension url
  // as a service worker.
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: `moz-extension://${extension.uuid}/page.html`,
    },
    async () => {
      await extension.awaitMessage("test-serviceWorker-register-disallowed");
    }
  );

  await extension.unload();

  await SpecialPowers.popPrefEnv();

  // Test again with the pref set to true.

  await SpecialPowers.pushPrefEnv({
    set: [["extensions.serviceWorkerRegister.allowed", true]],
  });

  extension = ExtensionTestUtils.loadExtension({
    files: {
      ...extensionData.files,
      "page.js": async function() {
        try {
          await navigator.serviceWorker.register("sw.js");
        } catch (err) {
          browser.test.fail(
            `Unexpected error on registering a service worker: ${err}`
          );
          throw err;
        } finally {
          browser.test.sendMessage("test-serviceworker-register-allowed");
        }
      },
    },
  });
  await extension.startup();

  // Verify that an extension page can register a moz-extension url
  // as a service worker if enabled by the related pref.
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: `moz-extension://${extension.uuid}/page.html`,
    },
    async () => {
      await extension.awaitMessage("test-serviceworker-register-allowed");
    }
  );

  await extension.unload();

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_cache_api_allowed() {
  // Verify that Cache API support for moz-extension url availability is
  // conditioned only by the extensions.backgroundServiceWorker.enabled pref.
  // NOTE: the scenario with backgroundServiceWorkeEnable set to false
  // is part of "browser_ext_background_serviceworker_pref_disabled.js".
  const extension = ExtensionTestUtils.loadExtension({
    async background() {
      try {
        let cache = await window.caches.open("test-cache-api");
        browser.test.assertTrue(
          await window.caches.has("test-cache-api"),
          "CacheStorage.has should resolve to true"
        );

        // Test that adding and requesting cached moz-extension urls
        // works as well.
        let url = browser.runtime.getURL("file.txt");
        await cache.add(url);
        const content = await cache.match(url).then(res => res.text());
        browser.test.assertEq(
          "file content",
          content,
          "Got the expected content from the cached moz-extension url"
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
    },
    files: {
      "file.txt": "file content",
    },
  });

  await extension.startup();
  await extension.awaitMessage("test-cache-api-allowed");
  await extension.unload();
});

function createTestSWScript({ postMessageReply }) {
  return `
    self.onmessage = msg => {
      dump("Background ServiceWorker - onmessage handler\\n");
      msg.ports[0].postMessage("${postMessageReply}");
      dump("Background ServiceWorker - postMessage\\n");
    };
    dump("Background ServiceWorker - executed\\n");
  `;
}

async function testServiceWorker({ extension, expectMessageReply }) {
  // Verify that the WebExtensions framework has successfully registered the
  // background service worker declared in the extension manifest.
  const swRegInfo = getBackgroundServiceWorkerRegistration(extension);

  // Activate the background service worker by exchanging a message
  // with it.
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: `moz-extension://${extension.uuid}/page.html`,
    },
    async browser => {
      let msgFromV1 = await SpecialPowers.spawn(
        browser,
        [swRegInfo.scriptURL],
        async url => {
          const { active } = await content.navigator.serviceWorker.ready;
          const { port1, port2 } = new content.MessageChannel();

          return new Promise(resolve => {
            port1.onmessage = msg => resolve(msg.data);
            active.postMessage("test", [port2]);
          });
        }
      );

      Assert.deepEqual(
        msgFromV1,
        expectMessageReply,
        "Got the expected reply from the extension service worker"
      );
    }
  );
}

function loadTestExtension({ version }) {
  const postMessageReply = `reply:sw-v${version}`;

  return ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      version,
      background: {
        service_worker: "sw.js",
      },
      applications: { gecko: { id: "test-bg-sw@mochi.test" } },
    },
    files: {
      "page.html": "<!DOCTYPE html><body></body>",
      "sw.js": createTestSWScript({ postMessageReply }),
    },
  });
}

async function assertWorkerIsRunningInExtensionProcess(extension) {
  // Activate the background service worker by exchanging a message
  // with it.
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: `moz-extension://${extension.uuid}/page.html`,
    },
    async browser => {
      const workerScriptURL = `moz-extension://${extension.uuid}/sw.js`;
      const workerDebuggerURLs = await SpecialPowers.spawn(
        browser,
        [workerScriptURL],
        async url => {
          await content.navigator.serviceWorker.ready;
          const wdm = Cc[
            "@mozilla.org/dom/workers/workerdebuggermanager;1"
          ].getService(Ci.nsIWorkerDebuggerManager);

          return Array.from(wdm.getWorkerDebuggerEnumerator())
            .map(wd => {
              return wd.url;
            })
            .filter(swURL => swURL == url);
        }
      );

      Assert.deepEqual(
        workerDebuggerURLs,
        [workerScriptURL],
        "The worker should be running in the extension child process"
      );
    }
  );
}

add_task(async function test_background_serviceworker_with_no_ext_apis() {
  const extensionV1 = loadTestExtension({ version: "1" });
  await extensionV1.startup();

  const swRegInfo = getBackgroundServiceWorkerRegistration(extensionV1);
  const { uuid } = extensionV1;

  await assertWorkerIsRunningInExtensionProcess(extensionV1);
  await testServiceWorker({
    extension: extensionV1,
    expectMessageReply: "reply:sw-v1",
  });

  // Load a new version of the same addon and verify that the
  // expected worker script is being executed.
  const extensionV2 = loadTestExtension({ version: "2" });
  await extensionV2.startup();
  is(extensionV2.uuid, uuid, "The extension uuid did not change as expected");

  await testServiceWorker({
    extension: extensionV2,
    expectMessageReply: "reply:sw-v2",
  });

  await Promise.all([
    extensionV2.unload(),
    // test extension v1 wrapper has to be unloaded explicitly, otherwise
    // will be detected as a failure by the test harness.
    extensionV1.unload(),
  ]);
  await waitForServiceWorkerTerminated(swRegInfo);
  await waitForServiceWorkerRegistrationsRemoved(extensionV2);
});
