"use strict";

add_task(async function test_theme_reset_api() {
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

      await browser.theme.update(theme);
      await browser.theme.reset();
      let reset_theme = await browser.theme.getCurrent();

      browser.test.assertEq(
        undefined,
        reset_theme.colors,
        "Should reset when extension had replaced the global theme"
      );

      let { id: winId } = await browser.windows.getCurrent();

      await browser.theme.update(winId, theme);
      let update_theme = await browser.theme.getCurrent(winId);
      await browser.theme.reset(winId);
      reset_theme = await browser.theme.getCurrent(winId);

      browser.test.assertEq(
        undefined,
        reset_theme.colors,
        "Should reset when window id is exist"
      );

      let { id: anotherWindId } = await browser.windows.create();
      await browser.windows.remove(anotherWindId);
      await browser.test.assertRejects(
        browser.theme.reset(anotherWindId),
        /Invalid window/,
        "Invalid window should throw"
      );

      browser.test.onMessage.addListener(async () => {
        reset_theme = await browser.theme.getCurrent(winId);
        browser.test.assertEq(
          update_theme.colors.frame,
          reset_theme.colors.frame,
          "Should not reset where another extension try reset"
        );

        browser.test.sendMessage("done");
      });

      await browser.theme.update(winId, theme);
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
