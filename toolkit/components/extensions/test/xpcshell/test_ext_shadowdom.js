"use strict";

ChromeUtils.defineModuleGetter(this, "Preferences",
                               "resource://gre/modules/Preferences.jsm");

// ExtensionContent.jsm needs to know when it's running from xpcshell,
// to use the right timeout for content scripts executed at document_idle.
ExtensionTestUtils.mockAppInfo();

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

add_task(async function test_contentscript_shadowDOM() {
  const PREFS = {
    "dom.webcomponents.shadowdom.enabled": true,
  };

  // Set prefs to our initial values.
  for (let pref in PREFS) {
    Preferences.set(pref, PREFS[pref]);
  }

  registerCleanupFunction(() => {
    // Reset the prefs.
    for (let pref in PREFS) {
      Preferences.reset(pref);
    }
  });

  function backgroundScript() {
    browser.test.assertTrue("openOrClosedShadowRoot" in document.documentElement,
                            "Should have openOrClosedShadowRoot in Element in background script.");
  }

  function contentScript() {
    let host = document.getElementById("host");
    browser.test.assertTrue("openOrClosedShadowRoot" in host, "Should have openOrClosedShadowRoot in Element.");
    let shadowRoot = host.openOrClosedShadowRoot;
    browser.test.assertEq(shadowRoot.mode, "closed", "Should have closed ShadowRoot.");
    browser.test.sendMessage("contentScript");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [{
        "matches": ["http://*/*/file_shadowdom.html"],
        "js": ["content_script.js"],
      }],
    },
    background: backgroundScript,
    files: {
      "content_script.js": contentScript,
    },
  });

  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(`${BASE_URL}/file_shadowdom.html`);
  await extension.awaitMessage("contentScript");

  await contentPage.close();
  await extension.unload();
});
