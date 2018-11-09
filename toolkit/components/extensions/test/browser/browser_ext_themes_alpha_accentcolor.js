"use strict";

add_task(async function test_alpha_accentcolor() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "images": {
          "headerURL": "image1.png",
        },
        "colors": {
          "accentcolor": "rgba(230, 128, 0, 0.1)",
          "textcolor": TEXT_COLOR,
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
  });

  await extension.startup();

  // Add the event listener before loading the extension
  let docEl = window.document.documentElement;
  let style = window.getComputedStyle(docEl);

  Assert.equal(style.backgroundColor, "rgb(230, 128, 0)",
               "Window background color should be opaque");

  await extension.unload();
});
