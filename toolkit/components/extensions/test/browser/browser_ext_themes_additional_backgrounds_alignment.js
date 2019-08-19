"use strict";

// Case 1 - When there is a theme_frame image and additional_backgrounds_alignment is not specified.
// So background-position should default to "right top"
add_task(async function test_default_additional_backgrounds_alignment() {
  const RIGHT_TOP = "100% 0%";

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        images: {
          theme_frame: "image1.png",
          additional_backgrounds: ["image1.png", "image1.png"],
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

  let docEl = document.documentElement;
  let rootCS = window.getComputedStyle(docEl);

  Assert.equal(
    rootCS.getPropertyValue("background-position"),
    RIGHT_TOP,
    "root only contains theme_frame alignment property"
  );

  let toolbox = document.querySelector("#navigator-toolbox");
  let toolboxCS = window.getComputedStyle(toolbox);

  Assert.equal(
    toolboxCS.getPropertyValue("background-position"),
    RIGHT_TOP,
    toolbox.id +
      " only contains default additional backgrounds alignment property"
  );

  await extension.unload();
});

// Case 2 - When there is a theme_frame image and additional_backgrounds_alignment is specified.
add_task(async function test_additional_backgrounds_alignment() {
  const LEFT_BOTTOM = "0% 100%";
  const CENTER_CENTER = "50% 50%";
  const RIGHT_TOP = "100% 0%";

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        images: {
          theme_frame: "image1.png",
          additional_backgrounds: ["image1.png", "image1.png", "image1.png"],
        },
        colors: {
          frame: ACCENT_COLOR,
          tab_background_text: TEXT_COLOR,
        },
        properties: {
          additional_backgrounds_alignment: [
            "left bottom",
            "center center",
            "right top",
          ],
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
  });

  await extension.startup();

  let docEl = document.documentElement;
  let rootCS = window.getComputedStyle(docEl);

  Assert.equal(
    rootCS.getPropertyValue("background-position"),
    RIGHT_TOP,
    "root only contains theme_frame alignment property"
  );

  let toolbox = document.querySelector("#navigator-toolbox");
  let toolboxCS = window.getComputedStyle(toolbox);

  Assert.equal(
    toolboxCS.getPropertyValue("background-position"),
    LEFT_BOTTOM + ", " + CENTER_CENTER + ", " + RIGHT_TOP,
    toolbox.id + " contains additional backgrounds alignment properties"
  );

  await extension.unload();
});
