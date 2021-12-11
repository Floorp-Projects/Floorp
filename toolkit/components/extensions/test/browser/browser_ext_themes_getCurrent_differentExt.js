"use strict";

// This test checks whether browser.theme.getCurrent() works correctly when theme
// does not originate from extension querying the theme.

add_task(async function test_getcurrent() {
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
      browser.theme.onUpdated.addListener(() => {
        browser.theme.getCurrent().then(theme => {
          browser.test.sendMessage("theme-updated", theme);
        });
      });
    },
  });

  await extension.startup();

  info("Testing getCurrent after static theme startup");
  let updatedPromise = extension.awaitMessage("theme-updated");
  await theme.startup();
  let receivedTheme = await updatedPromise;
  Assert.ok(
    receivedTheme.images.theme_frame.includes("image1.png"),
    "getCurrent returns correct theme_frame image"
  );
  Assert.equal(
    receivedTheme.colors.frame,
    ACCENT_COLOR,
    "getCurrent returns correct frame color"
  );
  Assert.equal(
    receivedTheme.colors.tab_background_text,
    TEXT_COLOR,
    "getCurrent returns correct tab_background_text color"
  );

  info("Testing getCurrent after static theme unload");
  updatedPromise = extension.awaitMessage("theme-updated");
  await theme.unload();
  receivedTheme = await updatedPromise;
  Assert.equal(
    JSON.stringify({ colors: null, images: null, properties: null }),
    JSON.stringify(receivedTheme),
    "getCurrent returns empty theme"
  );

  await extension.unload();
});
