"use strict";

/* globals InspectorUtils */

// This test checks whether applied WebExtension themes that attempt to change
// the button background color properties are applied correctly.

add_task(async function test_button_background_properties() {
  const BUTTON_BACKGROUND_ACTIVE = "#FFFFFF";
  const BUTTON_BACKGROUND_HOVER = "#59CBE8";

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        images: {
          theme_frame: "image1.png",
        },
        colors: {
          frame: ACCENT_COLOR,
          tab_background_text: TEXT_COLOR,
          button_background_active: BUTTON_BACKGROUND_ACTIVE,
          button_background_hover: BUTTON_BACKGROUND_HOVER,
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
  });

  await extension.startup();

  let toolbarButton = document.querySelector("#home-button");
  let toolbarButtonIcon = toolbarButton.icon;
  let toolbarButtonIconCS = window.getComputedStyle(toolbarButtonIcon);

  InspectorUtils.addPseudoClassLock(toolbarButton, ":hover");

  Assert.equal(
    toolbarButtonIconCS.getPropertyValue("background-color"),
    `rgb(${hexToRGB(BUTTON_BACKGROUND_HOVER).join(", ")})`,
    "Toolbar button hover background is set."
  );

  InspectorUtils.addPseudoClassLock(toolbarButton, ":active");

  Assert.equal(
    toolbarButtonIconCS.getPropertyValue("background-color"),
    `rgb(${hexToRGB(BUTTON_BACKGROUND_ACTIVE).join(", ")})`,
    "Toolbar button active background is set!"
  );

  InspectorUtils.clearPseudoClassLocks(toolbarButton);

  await extension.unload();
});
