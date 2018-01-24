"use strict";

// This test checks applied WebExtension themes that attempt to change
// icon color properties

add_task(async function test_tint_properties() {
  const ICONS_COLOR = "#001b47";
  const ICONS_ATTENTION_COLOR = "#44ba77";

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "images": {
          "headerURL": "image1.png",
        },
        "colors": {
          "accentcolor": ACCENT_COLOR,
          "textcolor": TEXT_COLOR,
          "icons": ICONS_COLOR,
          "icons_attention": ICONS_ATTENTION_COLOR,
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
    starComputedStyle.getPropertyValue("--lwt-toolbarbutton-icon-fill-attention"),
    ICONS_ATTENTION_COLOR,
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
