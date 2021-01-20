"use strict";

// This test checks whether applied WebExtension themes that attempt to change
// the color of the tab line are applied properly.

add_task(async function test_support_tab_line() {
  const TAB_LINE_COLOR = "#9400ff";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        colors: {
          frame: ACCENT_COLOR,
          tab_background_text: TEXT_COLOR,
          tab_line: TAB_LINE_COLOR,
        },
      },
    },
  });

  await extension.startup();

  info("Checking selected tab line color");
  let selectedTab = document.querySelector(".tabbrowser-tab[selected]");
  let line = selectedTab.querySelector(".tab-line");
  Assert.equal(
    window.getComputedStyle(line).backgroundColor,
    `rgb(${hexToRGB(TAB_LINE_COLOR).join(", ")})`,
    "Tab line should have theme color"
  );

  await extension.unload();
});
