"use strict";

// This test checks whether applied WebExtension themes that attempt to change
// the text color of the selected tab are applied properly.

add_task(async function test_support_tab_text_property_css_color() {
  const TAB_TEXT_COLOR = "#9400ff";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        images: {
          theme_frame: "image1.png",
        },
        colors: {
          frame: ACCENT_COLOR,
          tab_background_text: TEXT_COLOR,
          tab_text: TAB_TEXT_COLOR,
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
  Assert.equal(
    window.getComputedStyle(selectedTab).color,
    "rgb(" + hexToRGB(TAB_TEXT_COLOR).join(", ") + ")",
    "Selected tab text color should be set."
  );

  await extension.unload();
});

add_task(async function test_support_tab_text_chrome_array() {
  const TAB_TEXT_COLOR = [148, 0, 255];
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        images: {
          theme_frame: "image1.png",
        },
        colors: {
          frame: FRAME_COLOR,
          tab_background_text: TAB_BACKGROUND_TEXT_COLOR,
          tab_text: TAB_TEXT_COLOR,
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
  Assert.equal(
    window.getComputedStyle(selectedTab).color,
    "rgb(" + TAB_TEXT_COLOR.join(", ") + ")",
    "Selected tab text color should be set."
  );

  await extension.unload();
});
