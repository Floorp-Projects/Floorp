"use strict";

add_task(async function theme_reset_by_extension() {
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
