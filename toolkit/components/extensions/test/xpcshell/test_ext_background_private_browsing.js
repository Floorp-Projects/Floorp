/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Cu.import("resource://gre/modules/Preferences.jsm");

function* testBackgroundPage(expected) {
  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      browser.test.assertEq(window, browser.extension.getBackgroundPage(),
                            "Caller should be able to access itself as a background page");
      browser.test.assertEq(window, await browser.runtime.getBackgroundPage(),
                            "Caller should be able to access itself as a background page");

      browser.test.sendMessage("incognito", browser.extension.inIncognitoContext);
    },
  });

  yield extension.startup();

  let incognito = yield extension.awaitMessage("incognito");
  equal(incognito, expected.incognito, "Expected incognito value");

  yield extension.unload();
}

add_task(function* test_background_incognito() {
  do_print("Test background page incognito value with permanent private browsing disabled");

  yield testBackgroundPage({incognito: false});

  do_print("Test background page incognito value with permanent private browsing enabled");

  Preferences.set("browser.privatebrowsing.autostart", true);
  do_register_cleanup(() => {
    Preferences.reset("browser.privatebrowsing.autostart");
  });

  yield testBackgroundPage({incognito: true});
});
