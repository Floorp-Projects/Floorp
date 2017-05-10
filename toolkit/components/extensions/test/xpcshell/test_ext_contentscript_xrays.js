"use strict";

Cu.import("resource://gre/modules/Preferences.jsm");

ExtensionTestUtils.mockAppInfo();

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;
const XRAY_PREF = "dom.allow_named_properties_object_for_xrays";

add_task(async function test_contentscript_xrays() {
  async function contentScript() {
    let deferred;
    browser.test.onMessage.addListener(msg => {
      if (msg === "pref-set") {
        deferred.resolve();
      }
    });
    function setPref(val) {
      browser.test.sendMessage("set-pref", val);
      return new Promise(resolve => { deferred = {resolve}; });
    }

    let unwrapped = window.wrappedJSObject;

    await setPref(0);
    browser.test.assertEq("object", typeof test, "Should have named X-ray property access with pref=0");
    browser.test.assertEq("object", typeof window.test, "Should have named X-ray property access with pref=0");
    browser.test.assertEq("object", typeof unwrapped.test, "Should always have non-X-ray named property access");

    await setPref(1);
    browser.test.assertEq("undefined", typeof test, "Should not have named X-ray property access with pref=1");
    browser.test.assertEq(undefined, window.test, "Should not have named X-ray property access with pref=1");
    browser.test.assertEq("object", typeof unwrapped.test, "Should always have non-X-ray named property access");

    await setPref(2);
    browser.test.assertEq("undefined", typeof test, "Should not have named X-ray property access with pref=2");
    browser.test.assertEq(undefined, window.test, "Should not have named X-ray property access with pref=2");
    browser.test.assertEq("object", typeof unwrapped.test, "Should always have non-X-ray named property access");

    browser.test.notifyPass("contentScriptXrays");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [{
        "matches": ["http://*/*/file_sample.html"],
        "js": ["content_script.js"],
      }],
    },

    files: {
      "content_script.js": contentScript,
    },
  });

  extension.onMessage("set-pref", value => {
    Preferences.set(XRAY_PREF, value);
    extension.sendMessage("pref-set");
  });

  equal(Preferences.get(XRAY_PREF), 1, "Should have pref=1 by default");

  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(`${BASE_URL}/file_sample.html`);

  await extension.awaitFinish("contentScriptXrays");

  await contentPage.close();
  await extension.unload();
});
