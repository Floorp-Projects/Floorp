"use strict";

// This test checks whether applied WebExtension themes that attempt to change
// the background color of toolbars are applied properly.

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.webextensions.themes.enabled", true]],
  });
});

add_task(async function test_support_toolbar_property() {
  const TAB_TEXT_COLOR = "#9400ff";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "images": {
          "headerURL": "image1.png",
        },
        "colors": {
          "accentcolor": ACCENT_COLOR,
          "textcolor": TEXT_COLOR,
          "tab_text": TAB_TEXT_COLOR,
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
  });

  await extension.startup();

  info("Checking selected tab colors");
  let selectedTab = document.querySelector(".tabbrowser-tab[selected]");
  Assert.equal(window.getComputedStyle(selectedTab).color,
    "rgb(" + hexToRGB(TAB_TEXT_COLOR).join(", ") + ")", "Selected tab text color should be set.");

  await extension.unload();
});
