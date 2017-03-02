"use strict";

const ENCODED_IMAGE_DATA = "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";

function imageBufferFromDataURI(encodedImageData) {
  let decodedImageData = atob(encodedImageData);
  return Uint8Array.from(decodedImageData, byte => byte.charCodeAt(0)).buffer;
}

add_task(function* setup() {
  yield SpecialPowers.pushPrefEnv({
    set: [["extensions.webextensions.themes.enabled", true]],
  });
});

add_task(function* test_support_theme_frame() {
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
          "tab_text": TAB_TEXT_COLOR,
        },
      },
    },
    files: {
      "face.png": imageBufferFromDataURI(ENCODED_IMAGE_DATA),
    },
  });

  yield extension.startup();

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

  yield extension.unload();

  Assert.ok(!docEl.hasAttribute("lwtheme"), "LWT attribute should not be set");
});
