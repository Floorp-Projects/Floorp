"use strict";

add_task(async function test_alpha_frame_color() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        images: {
          theme_frame: "image1.png",
        },
        colors: {
          frame: "rgba(230, 128, 0, 0.1)",
          tab_background_text: TEXT_COLOR,
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
  });

  await extension.startup();

  Assert.equal(
    getToolboxBackgroundColor(),
    "rgb(230, 128, 0)",
    "Window background color should be opaque"
  );

  await extension.unload();
});
