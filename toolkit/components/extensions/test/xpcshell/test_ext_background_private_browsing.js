/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_background_incognito() {
  info(
    "Test background page incognito value with permanent private browsing enabled"
  );

  Services.prefs.setBoolPref("extensions.allowPrivateBrowsingByDefault", false);
  Services.prefs.setBoolPref("browser.privatebrowsing.autostart", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.privatebrowsing.autostart");
    Services.prefs.clearUserPref("extensions.allowPrivateBrowsingByDefault");
  });

  let extension = ExtensionTestUtils.loadExtension({
    incognitoOverride: "spanning",
    async background() {
      browser.test.assertEq(
        window,
        browser.extension.getBackgroundPage(),
        "Caller should be able to access itself as a background page"
      );
      browser.test.assertEq(
        window,
        await browser.runtime.getBackgroundPage(),
        "Caller should be able to access itself as a background page"
      );

      browser.test.assertEq(
        browser.extension.inIncognitoContext,
        true,
        "inIncognitoContext is true for permanent private browsing"
      );

      browser.test.notifyPass("incognito");
    },
  });

  await extension.startup();

  await extension.awaitFinish("incognito");

  await extension.unload();
});
