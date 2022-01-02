"use strict";

add_task(async function theme_reset_global_static_theme() {
  let global_theme_extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        colors: {
          frame: "#123456",
          tab_background_text: "#fedcba",
        },
      },
    },
  });

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["theme"],
    },
    async background() {
      await browser.theme.reset();
      let theme_after_reset = await browser.theme.getCurrent();

      browser.test.assertEq(
        "#123456",
        theme_after_reset.colors.frame,
        "Theme from other extension should not be cleared upon reset()"
      );

      let theme = {
        colors: {
          frame: "#CF723F",
        },
      };

      await browser.theme.update(theme);
      await browser.theme.reset();
      let final_reset_theme = await browser.theme.getCurrent();

      browser.test.assertEq(
        JSON.stringify({ colors: null, images: null, properties: null }),
        JSON.stringify(final_reset_theme),
        "Should reset when extension had replaced the global theme"
      );
      browser.test.sendMessage("done");
    },
  });
  await global_theme_extension.startup();
  await extension.startup();
  await extension.awaitMessage("done");

  await global_theme_extension.unload();
  await extension.unload();
});

add_task(async function theme_reset_by_windowId() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["theme"],
    },
    async background() {
      let theme = {
        colors: {
          frame: "#CF723F",
        },
      };

      let { id: winId } = await browser.windows.getCurrent();
      await browser.theme.update(winId, theme);
      let update_theme = await browser.theme.getCurrent(winId);

      browser.test.onMessage.addListener(async () => {
        let current_theme = await browser.theme.getCurrent(winId);
        browser.test.assertEq(
          update_theme.colors.frame,
          current_theme.colors.frame,
          "Should not be reset by a reset(windowId) call from another extension"
        );

        browser.test.sendMessage("done");
      });

      browser.test.sendMessage("ready", winId);
    },
  });

  let anotherExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["theme"],
    },
    background() {
      browser.test.onMessage.addListener(async winId => {
        await browser.theme.reset(winId);
        browser.test.sendMessage("done");
      });
    },
  });

  await extension.startup();
  let winId = await extension.awaitMessage("ready");

  await anotherExtension.startup();

  // theme.reset should be ignored if the theme was set by another extension.
  anotherExtension.sendMessage(winId);
  await anotherExtension.awaitMessage("done");

  extension.sendMessage();
  await extension.awaitMessage("done");

  await anotherExtension.unload();
  await extension.unload();
});
