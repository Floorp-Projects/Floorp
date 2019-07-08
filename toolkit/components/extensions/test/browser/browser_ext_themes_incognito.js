"use strict";

add_task(async function test_theme_incognito_not_allowed() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.allowPrivateBrowsingByDefault", false]],
  });

  let windowExtension = ExtensionTestUtils.loadExtension({
    incognitoOverride: "spanning",
    async background() {
      const theme = {
        colors: {
          frame: "black",
          tab_background_text: "black",
        },
      };
      let window = await browser.windows.create({ incognito: true });
      browser.test.onMessage.addListener(async message => {
        if (message == "update") {
          browser.theme.update(window.id, theme);
          return;
        }
        await browser.windows.remove(window.id);
        browser.test.sendMessage("done");
      });
      browser.test.sendMessage("ready", window.id);
    },
    manifest: {
      permissions: ["theme"],
    },
  });
  await windowExtension.startup();
  let wId = await windowExtension.awaitMessage("ready");

  async function background(windowId) {
    const theme = {
      colors: {
        frame: "black",
        tab_background_text: "black",
      },
    };

    browser.theme.onUpdated.addListener(info => {
      browser.test.log("got theme onChanged");
      browser.test.fail("theme");
    });
    await browser.test.assertRejects(
      browser.theme.getCurrent(windowId),
      /Invalid window ID/,
      "API should reject getting window theme"
    );
    await browser.test.assertRejects(
      browser.theme.update(windowId, theme),
      /Invalid window ID/,
      "API should reject updating theme"
    );
    await browser.test.assertRejects(
      browser.theme.reset(windowId),
      /Invalid window ID/,
      "API should reject reseting theme on window"
    );

    browser.test.sendMessage("start");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: `(${background})(${wId})`,
    manifest: {
      permissions: ["theme"],
    },
  });

  await extension.startup();
  await extension.awaitMessage("start");
  windowExtension.sendMessage("update");

  windowExtension.sendMessage("close");
  await windowExtension.awaitMessage("done");
  await windowExtension.unload();
  await extension.unload();
});
