"use strict";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.webextensions.themes.enabled", true]],
  });
});

add_task(async function test_support_theme_frame() {
  const FRAME_COLOR = [71, 105, 91];
  const TAB_TEXT_COLOR = [207, 221, 192, .9];
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "images": {
          "theme_frame": "face.png",
        },
        "colors": {
          "frame": FRAME_COLOR,
          "background_tab_text": TAB_TEXT_COLOR,
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
  Assert.ok(style.backgroundImage.includes("face.png"),
            `The backgroundImage should use face.png. Actual value is: ${style.backgroundImage}`);
  Assert.equal(style.backgroundColor, "rgb(" + FRAME_COLOR.join(", ") + ")",
               "Expected correct background color");
  Assert.equal(style.color, "rgba(" + TAB_TEXT_COLOR.join(", ") + ")",
               "Expected correct text color");

  await extension.unload();

  Assert.ok(!docEl.hasAttribute("lwtheme"), "LWT attribute should not be set");
});
