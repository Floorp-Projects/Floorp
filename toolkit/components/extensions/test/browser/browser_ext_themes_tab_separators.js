"use strict";

add_task(async function test_support_tab_separators() {
  const TAB_SEPARATOR_COLOR = "#FF0000";

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "colors": {
          "accentcolor": "#000",
          "textcolor": "#9400ff",
          "tab_background_separator":  TAB_SEPARATOR_COLOR,
        },
      },
    },
  });
  await extension.startup();

  info("Checking background tab separator color");

  let tab = gBrowser.addTab("about:blank");

  Assert.equal(window.getComputedStyle(tab, "::before").borderLeftColor,
               `rgb(${hexToRGB(TAB_SEPARATOR_COLOR).join(", ")})`,
               "Left separator has right color.");

  Assert.equal(window.getComputedStyle(tab, "::after").borderLeftColor,
               `rgb(${hexToRGB(TAB_SEPARATOR_COLOR).join(", ")})`,
               "Right separator has right color.");

  gBrowser.removeTab(tab);

  await extension.unload();
});
