"use strict";

// This test checks whether browser.theme.getCurrent() works correctly when theme
// does not originate from extension querying the theme.

add_task(async function test_getcurrent() {
  const theme = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "images": {
          "headerURL": "image1.png",
        },
        "colors": {
          "accentcolor": ACCENT_COLOR,
          "textcolor": TEXT_COLOR,
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
    manifest: {
      permissions: ["theme"],
    },
  });

  await extension.startup();

  info("Testing getCurrent after static theme startup");
  let updatedPromise = extension.awaitMessage("theme-updated");
  await theme.startup();
  let receivedTheme = await updatedPromise;
  Assert.ok(receivedTheme.images.headerURL.includes("image1.png"),
            "getCurrent returns correct headerURL");
  Assert.equal(receivedTheme.colors.accentcolor, ACCENT_COLOR,
               "getCurrent returns correct accentcolor");
  Assert.equal(receivedTheme.colors.textcolor, TEXT_COLOR,
               "getCurrent returns correct textcolor");

  info("Testing getCurrent after static theme unload");
  updatedPromise = extension.awaitMessage("theme-updated");
  await theme.unload();
  receivedTheme = await updatedPromise;
  Assert.equal(Object.keys(receivedTheme), 0,
               "getCurrent returns empty theme");

  await extension.unload();
});
