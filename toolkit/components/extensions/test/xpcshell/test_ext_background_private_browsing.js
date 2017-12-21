/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Cu.import("resource://gre/modules/Preferences.jsm");

async function testBackgroundPage(expected) {
  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      browser.test.assertEq(window, browser.extension.getBackgroundPage(),
                            "Caller should be able to access itself as a background page");
      browser.test.assertEq(window, await browser.runtime.getBackgroundPage(),
                            "Caller should be able to access itself as a background page");

      browser.test.sendMessage("incognito", browser.extension.inIncognitoContext);
    },
  });

  await extension.startup();

  let incognito = await extension.awaitMessage("incognito");
  equal(incognito, expected.incognito, "Expected incognito value");

  await extension.unload();
}

add_task(async function test_background_incognito() {
  info("Test background page incognito value with permanent private browsing disabled");

  await testBackgroundPage({incognito: false});

  info("Test background page incognito value with permanent private browsing enabled");

  Preferences.set("browser.privatebrowsing.autostart", true);
  registerCleanupFunction(() => {
    Preferences.reset("browser.privatebrowsing.autostart");
  });

  await testBackgroundPage({incognito: true});
});
