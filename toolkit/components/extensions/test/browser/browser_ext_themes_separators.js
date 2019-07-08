"use strict";

// This test checks whether applied WebExtension themes that attempt to change
// the separator colors are applied properly.

add_task(async function test_support_separator_properties() {
  const SEPARATOR_TOP_COLOR = "#ff00ff";
  const SEPARATOR_VERTICAL_COLOR = "#f0000f";
  const SEPARATOR_FIELD_COLOR = "#9400ff";
  const SEPARATOR_BOTTOM_COLOR = "#3366cc";

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        images: {
          theme_frame: "image1.png",
        },
        colors: {
          frame: ACCENT_COLOR,
          tab_background_text: TEXT_COLOR,
          toolbar_top_separator: SEPARATOR_TOP_COLOR,
          toolbar_vertical_separator: SEPARATOR_VERTICAL_COLOR,
          toolbar_field_separator: SEPARATOR_FIELD_COLOR,
          toolbar_bottom_separator: SEPARATOR_BOTTOM_COLOR,
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
  });

  await extension.startup();

  let navbar = document.querySelector("#nav-bar");
  Assert.ok(
    window
      .getComputedStyle(navbar)
      .boxShadow.includes(`rgb(${hexToRGB(SEPARATOR_TOP_COLOR).join(", ")})`),
    "Top separator color properly set"
  );

  let mainWin = document.querySelector("#main-window");
  Assert.equal(
    window
      .getComputedStyle(mainWin)
      .getPropertyValue("--urlbar-separator-color"),
    `rgb(${hexToRGB(SEPARATOR_FIELD_COLOR).join(", ")})`,
    "Toolbar field separator color properly set"
  );

  let panelUIButton = document.querySelector("#PanelUI-button");
  Assert.ok(
    window
      .getComputedStyle(panelUIButton)
      .getPropertyValue("border-image-source")
      .includes(`rgb(${hexToRGB(SEPARATOR_VERTICAL_COLOR).join(", ")})`),
    "Vertical separator color properly set"
  );

  let toolbox = document.querySelector("#navigator-toolbox");
  Assert.equal(
    window.getComputedStyle(toolbox).borderBottomColor,
    `rgb(${hexToRGB(SEPARATOR_BOTTOM_COLOR).join(", ")})`,
    "Bottom separator color properly set"
  );

  await extension.unload();
});
