"use strict";

add_task(async function test_support_backgrounds_position() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "images": {
          "theme_frame": "face1.png",
          "additional_backgrounds": ["face2.png", "face2.png", "face2.png"],
        },
        "colors": {
          "frame": `rgb(${FRAME_COLOR.join(",")})`,
          "tab_background_text": `rgb(${TAB_BACKGROUND_TEXT_COLOR.join(",")})`,
        },
        "properties": {
          "additional_backgrounds_alignment": ["left top", "center top", "right bottom"],
        },
      },
    },
    files: {
      "face1.png": imageBufferFromDataURI(ENCODED_IMAGE_DATA),
      "face2.png": imageBufferFromDataURI(ENCODED_IMAGE_DATA),
    },
  });

  await extension.startup();

  let docEl = window.document.documentElement;
  let toolbox = document.querySelector("#navigator-toolbox");

  Assert.ok(docEl.hasAttribute("lwtheme"), "LWT attribute should be set");
  Assert.equal(docEl.getAttribute("lwthemetextcolor"), "bright",
               "LWT text color attribute should be set");

  let toolboxCS = window.getComputedStyle(toolbox);
  let rootCS = window.getComputedStyle(docEl);
  let bgImage = toolboxCS.backgroundImage.split(",")[0].trim();

  checkThemeHeaderImage(window, `moz-extension://${extension.uuid}/face1.png`);
  Assert.equal(toolboxCS.backgroundImage, Array(3).fill(bgImage).join(", "),
               "The backgroundImage should use face2.png three times.");
  Assert.equal(toolboxCS.backgroundPosition, "0% 0%, 50% 0%, 100% 100%",
               "The backgroundPosition should use the three values provided.");
  Assert.equal(toolboxCS.backgroundRepeat, "no-repeat",
               "The backgroundPosition should use the default value.");

  await extension.unload();

  Assert.ok(!docEl.hasAttribute("lwtheme"), "LWT attribute should not be set");
  toolboxCS = window.getComputedStyle(toolbox);

  // Styles should've reverted to their initial values.
  Assert.equal(rootCS.backgroundImage, "none");
  Assert.equal(rootCS.backgroundPosition, "0% 0%");
  Assert.equal(rootCS.backgroundRepeat, "repeat");
  Assert.equal(toolboxCS.backgroundImage, "none");
  Assert.equal(toolboxCS.backgroundPosition, "0% 0%");
  Assert.equal(toolboxCS.backgroundRepeat, "repeat");
});

add_task(async function test_support_backgrounds_repeat() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "images": {
          "theme_frame": "face0.png",
          "additional_backgrounds": ["face1.png", "face2.png", "face3.png"],
        },
        "colors": {
          "frame": FRAME_COLOR,
          "tab_background_text": TAB_BACKGROUND_TEXT_COLOR,
        },
        "properties": {
          "additional_backgrounds_tiling": ["repeat-x", "repeat-y", "repeat"],
        },
      },
    },
    files: {
      "face0.png": imageBufferFromDataURI(ENCODED_IMAGE_DATA),
      "face1.png": imageBufferFromDataURI(ENCODED_IMAGE_DATA),
      "face2.png": imageBufferFromDataURI(ENCODED_IMAGE_DATA),
      "face3.png": imageBufferFromDataURI(ENCODED_IMAGE_DATA),
    },
  });

  await extension.startup();

  let docEl = window.document.documentElement;
  let toolbox = document.querySelector("#navigator-toolbox");

  Assert.ok(docEl.hasAttribute("lwtheme"), "LWT attribute should be set");
  Assert.equal(docEl.getAttribute("lwthemetextcolor"), "bright",
               "LWT text color attribute should be set");

  let rootCS = window.getComputedStyle(docEl);
  let toolboxCS = window.getComputedStyle(toolbox);
  checkThemeHeaderImage(window, `moz-extension://${extension.uuid}/face0.png`);
  Assert.equal([1, 2, 3].map(num => `url("moz-extension://${extension.uuid}/face${num}.png")`).join(", "),
               toolboxCS.backgroundImage, "The backgroundImage should use face.png three times.");
  Assert.equal(rootCS.backgroundPosition, "100% 0%",
               "The backgroundPosition should use the default value for root.");
  Assert.equal(toolboxCS.backgroundPosition, "100% 0%",
               "The backgroundPosition should use the default value for navigator-toolbox.");
  Assert.equal(rootCS.backgroundRepeat, "no-repeat",
               "The backgroundRepeat should use the default values for root.");
  Assert.equal(toolboxCS.backgroundRepeat, "repeat-x, repeat-y, repeat",
               "The backgroundRepeat should use the three values provided for navigator-toolbox.");

  await extension.unload();

  Assert.ok(!docEl.hasAttribute("lwtheme"), "LWT attribute should not be set");
});

add_task(async function test_additional_images_check() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "images": {
          "theme_frame": "face.png",
        },
        "colors": {
          "frame": FRAME_COLOR,
          "tab_background_text": TAB_BACKGROUND_TEXT_COLOR,
        },
        "properties": {
          "additional_backgrounds_tiling": ["repeat-x", "repeat-y", "repeat"],
        },
      },
    },
    files: {
      "face.png": imageBufferFromDataURI(ENCODED_IMAGE_DATA),
    },
  });

  await extension.startup();

  let docEl = window.document.documentElement;
  let toolbox = document.querySelector("#navigator-toolbox");

  Assert.ok(docEl.hasAttribute("lwtheme"), "LWT attribute should be set");
  Assert.equal(docEl.getAttribute("lwthemetextcolor"), "bright",
               "LWT text color attribute should be set");

  let rootCS = window.getComputedStyle(docEl);
  let toolboxCS = window.getComputedStyle(toolbox);
  checkThemeHeaderImage(window, `moz-extension://${extension.uuid}/face.png`);
  Assert.equal("none", toolboxCS.backgroundImage,
               "The backgroundImage should not use face.png.");
  Assert.equal(rootCS.backgroundPosition, "100% 0%",
               "The backgroundPosition should use the default value.");
  Assert.equal(rootCS.backgroundRepeat, "no-repeat",
               "The backgroundPosition should use only one (default) value.");

  await extension.unload();

  Assert.ok(!docEl.hasAttribute("lwtheme"), "LWT attribute should not be set");
});
