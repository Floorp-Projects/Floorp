"use strict";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.webextensions.themes.enabled", true]],
  });
});

add_task(async function test_support_backgrounds_position() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "images": {
          "headerURL": "face.png",
          "additional_backgrounds": ["face.png", "face.png", "face.png"],
        },
        "colors": {
          "accentcolor": `rgb(${FRAME_COLOR.join(",")})`,
          "textcolor": `rgb(${BACKGROUND_TAB_TEXT_COLOR.join(",")})`,
        },
        "properties": {
          "additional_backgrounds_alignment": ["left top", "center top", "right bottom"],
        },
      },
    },
    files: {
      "face.png": imageBufferFromDataURI(ENCODED_IMAGE_DATA),
    },
  });

  await extension.startup();

  let docEl = window.document.documentElement;

  Assert.ok(docEl.hasAttribute("lwtheme"), "LWT attribute should be set");
  Assert.equal(docEl.getAttribute("lwthemetextcolor"), "bright",
               "LWT text color attribute should be set");

  let style = window.getComputedStyle(docEl);
  let bgImage = style.backgroundImage.split(",")[0].trim();
  Assert.ok(bgImage.includes("face.png"),
            `The backgroundImage should use face.png. Actual value is: ${bgImage}`);
  Assert.equal(Array(4).fill(bgImage).join(", "), style.backgroundImage,
               "The backgroundImage should use face.png four times.");
  Assert.equal(style.backgroundPosition, "100% 0%, 0% 0%, 50% 0%, 100% 100%",
               "The backgroundPosition should use the four values provided.");
  Assert.equal(style.backgroundRepeat, "no-repeat",
               "The backgroundPosition should use the default value.");

  await extension.unload();

  Assert.ok(!docEl.hasAttribute("lwtheme"), "LWT attribute should not be set");
  style = window.getComputedStyle(docEl);
  // Styles should've reverted to their initial values.
  Assert.equal(style.backgroundImage, "none");
  Assert.equal(style.backgroundPosition, "0% 0%");
  Assert.equal(style.backgroundRepeat, "repeat");
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
          "background_tab_text": BACKGROUND_TAB_TEXT_COLOR,
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

  Assert.ok(docEl.hasAttribute("lwtheme"), "LWT attribute should be set");
  Assert.equal(docEl.getAttribute("lwthemetextcolor"), "bright",
               "LWT text color attribute should be set");

  let style = window.getComputedStyle(docEl);
  let bgImage = style.backgroundImage.split(",")[0].trim();
  Assert.ok(bgImage.includes("face0.png"),
            `The backgroundImage should use face.png. Actual value is: ${bgImage}`);
  Assert.equal([0, 1, 2, 3].map(num => bgImage.replace(/face[\d]*/, `face${num}`)).join(", "),
               style.backgroundImage, "The backgroundImage should use face.png four times.");
  Assert.equal(style.backgroundPosition, "100% 0%",
               "The backgroundPosition should use the default value.");
  Assert.equal(style.backgroundRepeat, "no-repeat, repeat-x, repeat-y, repeat",
               "The backgroundPosition should use the four values provided.");

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
          "background_tab_text": BACKGROUND_TAB_TEXT_COLOR,
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

  Assert.ok(docEl.hasAttribute("lwtheme"), "LWT attribute should be set");
  Assert.equal(docEl.getAttribute("lwthemetextcolor"), "bright",
               "LWT text color attribute should be set");

  let style = window.getComputedStyle(docEl);
  let bgImage = style.backgroundImage.split(",")[0];
  Assert.ok(bgImage.includes("face.png"),
            `The backgroundImage should use face.png. Actual value is: ${bgImage}`);
  Assert.equal(bgImage + ", none", style.backgroundImage,
               "The backgroundImage should use face.png only once.");
  Assert.equal(style.backgroundPosition, "100% 0%",
               "The backgroundPosition should use the default value.");
  Assert.equal(style.backgroundRepeat, "no-repeat",
               "The backgroundPosition should use only one (default) value.");

  await extension.unload();

  Assert.ok(!docEl.hasAttribute("lwtheme"), "LWT attribute should not be set");
});
