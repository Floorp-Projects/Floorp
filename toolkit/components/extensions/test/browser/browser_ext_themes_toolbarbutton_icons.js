"use strict";

// This test checks applied WebExtension themes that attempt to change
// icon color properties

add_task(async function setup_home_button() {
  if (CustomizableUI.protonToolbarEnabled) {
    CustomizableUI.addWidgetToArea("home-button", "nav-bar");
    registerCleanupFunction(() =>
      CustomizableUI.removeWidgetFromArea("home-button")
    );
  }
});

add_task(async function test_icons_properties() {
  const ICONS_COLOR = "#001b47";
  const ICONS_ATTENTION_COLOR = "#44ba77";

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        images: {
          theme_frame: "image1.png",
        },
        colors: {
          frame: ACCENT_COLOR,
          tab_background_text: TEXT_COLOR,
          icons: ICONS_COLOR,
          icons_attention: ICONS_ATTENTION_COLOR,
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
  });

  await extension.startup();

  let toolbarbutton = document.querySelector("#home-button");
  Assert.equal(
    window.getComputedStyle(toolbarbutton).getPropertyValue("fill"),
    `rgb(${hexToRGB(ICONS_COLOR).join(", ")})`,
    "Buttons fill color set!"
  );

  let starButton = document.querySelector("#star-button");
  starButton.setAttribute("starred", "true");

  let starComputedStyle = window.getComputedStyle(starButton);
  Assert.equal(
    starComputedStyle.getPropertyValue(
      "--lwt-toolbarbutton-icon-fill-attention"
    ),
    `rgb(${hexToRGB(ICONS_ATTENTION_COLOR).join(", ")})`,
    "Variable is properly set"
  );
  Assert.equal(
    starComputedStyle.getPropertyValue("fill"),
    `rgb(${hexToRGB(ICONS_ATTENTION_COLOR).join(", ")})`,
    "Starred icon fill is properly set"
  );

  starButton.removeAttribute("starred");

  await extension.unload();
});

add_task(async function test_no_icons_properties() {
  let extension = ExtensionTestUtils.loadExtension({
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

  await extension.startup();

  let toolbarbutton = document.querySelector("#home-button");
  let toolbarbuttonCS = window.getComputedStyle(toolbarbutton);
  Assert.equal(
    toolbarbuttonCS.getPropertyValue("--lwt-toolbarbutton-icon-fill"),
    "",
    "Icon fill should not be set when the value is not specified in the manifest."
  );
  let currentColor = toolbarbuttonCS.getPropertyValue("color");
  Assert.equal(
    window.getComputedStyle(toolbarbutton).getPropertyValue("fill"),
    currentColor,
    "Button fill color should be currentColor when no icon color specified."
  );

  let starButton = document.querySelector("#star-button");
  starButton.setAttribute("starred", "true");
  let starComputedStyle = window.getComputedStyle(starButton);
  Assert.equal(
    starComputedStyle.getPropertyValue(
      "--lwt-toolbarbutton-icon-fill-attention"
    ),
    "",
    "Icon attention fill should not be set when the value is not specified in the manifest."
  );
  starButton.removeAttribute("starred");

  await extension.unload();
});
