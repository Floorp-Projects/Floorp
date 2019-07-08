"use strict";

// This test checks whether browser.theme.onUpdated works
// when a static theme is applied

add_task(async function test_on_updated() {
  const theme = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        images: {
          theme_frame: "image1.png",
        },
        colors: {
          frame: ACCENT_COLOR,
          tab_background_text: TEXT_COLOR,
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
  });

  const extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.theme.onUpdated.addListener(updateInfo => {
        browser.test.sendMessage("theme-updated", updateInfo);
      });
    },
  });

  await extension.startup();

  info("Testing update event on static theme startup");
  let updatedPromise = extension.awaitMessage("theme-updated");
  await theme.startup();
  const { theme: receivedTheme, windowId } = await updatedPromise;
  Assert.ok(!windowId, "No window id in static theme update event");
  Assert.ok(
    receivedTheme.images.theme_frame.includes("image1.png"),
    "Theme theme_frame image should be applied"
  );
  Assert.equal(
    receivedTheme.colors.frame,
    ACCENT_COLOR,
    "Theme frame color should be applied"
  );
  Assert.equal(
    receivedTheme.colors.tab_background_text,
    TEXT_COLOR,
    "Theme tab_background_text color should be applied"
  );

  info("Testing update event on static theme unload");
  updatedPromise = extension.awaitMessage("theme-updated");
  await theme.unload();
  const updateInfo = await updatedPromise;
  Assert.ok(!windowId, "No window id in static theme update event on unload");
  Assert.equal(
    Object.keys(updateInfo.theme),
    0,
    "unloading theme sends empty theme in update event"
  );

  await extension.unload();
});
