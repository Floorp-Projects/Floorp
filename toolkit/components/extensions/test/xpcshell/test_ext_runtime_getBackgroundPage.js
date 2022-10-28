"use strict";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "43"
);

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();

  Services.prefs.setBoolPref("dom.serviceWorkers.testing.enabled", true);

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("dom.serviceWorkers.testing.enabled");
    Services.prefs.clearUserPref("dom.serviceWorkers.idle_timeout");
  });
});

add_task(async function test_getBackgroundPage_noBackground() {
  async function testBackground() {
    let page = await browser.runtime.getBackgroundPage();
    browser.test.assertEq(
      page,
      null,
      "getBackgroundPage returned null as expected"
    );
    browser.test.sendMessage("page-ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    files: {
      "page.html": `
      <!DOCTYPE html>
      <html>
      <head><meta charset="utf-8"></head>
      <body>
      <script src="page.js"></script>
      </body></html>
      `,

      "page.js": testBackground,
    },
  });
  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    `moz-extension://${extension.uuid}//page.html`
  );
  await extension.awaitMessage("page-ready");
  await contentPage.close();

  await extension.unload();
});

add_task(
  {
    pref_set: [["extensions.eventPages.enabled", true]],
    skip_if: () =>
      Services.prefs.getBoolPref(
        "extensions.backgroundServiceWorker.forceInTestExtension",
        false
      ),
  },
  async function test_getBackgroundPage_eventpage() {
    async function wakeupBackground() {
      let page = await browser.runtime.getBackgroundPage();
      page.hello();
      browser.test.sendMessage("page-ready");
    }

    let extension = ExtensionTestUtils.loadExtension({
      useAddonManager: "temporary", // To automatically show sidebar on load.
      manifest: {
        background: { persistent: false },
      },

      files: {
        "page.html": `
      <!DOCTYPE html>
      <html>
      <head><meta charset="utf-8"></head>
      <body>
      <script src="page.js"></script>
      </body></html>
      `,

        "page.js": wakeupBackground,
      },
      async background() {
        // eslint-disable-next-line no-unused-vars
        window.hello = () => {
          browser.test.sendMessage("hello");
        };

        browser.test.sendMessage("ready");
      },
    });

    await extension.startup();
    await extension.awaitMessage("ready");

    await extension.terminateBackground();

    // wake up the background
    let contentPage = await ExtensionTestUtils.loadContentPage(
      `moz-extension://${extension.uuid}//page.html`
    );
    await extension.awaitMessage("ready");
    await extension.awaitMessage("hello");
    await extension.awaitMessage("page-ready");
    await contentPage.close();

    ok(true, "getBackgroundPage wakes up background");

    await extension.unload();
  }
);

add_task(
  {
    skip_if: () => {
      return !WebExtensionPolicy.backgroundServiceWorkerEnabled;
    },
  },
  async function test_getBackgroundPage_serviceWorker() {
    async function testBackground() {
      let page = await browser.runtime.getBackgroundPage();
      browser.test.assertEq(
        page,
        null,
        "getBackgroundPage returned null as expected"
      );
      browser.test.sendMessage("page-ready");
    }

    let extension = ExtensionTestUtils.loadExtension({
      useAddonManager: "temporary",
      manifest: {
        version: "1.0",
        background: {
          service_worker: "sw.js",
        },
        browser_specific_settings: { gecko: { id: "test-bg-sw@mochi.test" } },
      },

      files: {
        "sw.js": "dump('Background ServiceWorker - executed\\n');",
        "page.html": `
      <!DOCTYPE html>
      <html>
      <head><meta charset="utf-8"></head>
      <body>
      <script src="page.js"></script>
      </body></html>
      `,

        "page.js": testBackground,
      },
    });
    await extension.startup();
    let contentPage = await ExtensionTestUtils.loadContentPage(
      `moz-extension://${extension.uuid}//page.html`
    );
    await extension.awaitMessage("page-ready");
    await contentPage.close();

    await extension.unload();
  }
);
