/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function assert_background_serviceworker_pref_disabled() {
  is(
    WebExtensionPolicy.backgroundServiceWorkerEnabled,
    false,
    "Expect extensions.backgroundServiceWorker.enabled to be false"
  );
});

add_task(async function test_background_serviceworker_disallowed() {
  const id = "test-disallowed-worker@test";

  const extensionData = {
    manifest: {
      background: {
        service_worker: "sw.js",
      },
      applicantions: { gecko: { id } },
      useAddonManager: "temporary",
    },
  };

  SimpleTest.waitForExplicitFinish();
  let waitForConsole = new Promise(resolve => {
    SimpleTest.monitorConsole(resolve, [
      {
        message: /Reading manifest: Error processing background: background.service_worker is currently disabled/,
      },
    ]);
  });

  const extension = ExtensionTestUtils.loadExtension(extensionData);
  await Assert.rejects(
    extension.startup(),
    /startup failed/,
    "Startup failed with background.service_worker while disabled by pref"
  );

  SimpleTest.endMonitorConsole();
  await waitForConsole;
});

add_task(async function test_serviceWorker_register_disallowed() {
  // Verify that setting extensions.serviceWorkerRegist.allowed pref to false
  // doesn't allow serviceWorker.register if backgroundServiceWorkeEnable is
  // set to false

  await SpecialPowers.pushPrefEnv({
    set: [["extensions.serviceWorkerRegister.allowed", true]],
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
});

add_task(async function test_cache_api_disallowed() {
  // Verify that Cache API support for moz-extension url availability is also
  // conditioned by the extensions.backgroundServiceWorker.enabled pref.
  const extension = ExtensionTestUtils.loadExtension({
    async background() {
      try {
        await window.caches.open("test-cache-api");
        browser.test.fail(
          `An extension page should not be allowed to use the Cache API successfully`
        );
      } catch (err) {
        browser.test.assertEq(
          String(err),
          "SecurityError: The operation is insecure.",
          "Got the expected error on registering a service worker from a script"
        );
      } finally {
        browser.test.sendMessage("test-cache-api-disallowed");
      }
    },
  });

  await extension.startup();
  await extension.awaitMessage("test-cache-api-disallowed");
  await extension.unload();
});
