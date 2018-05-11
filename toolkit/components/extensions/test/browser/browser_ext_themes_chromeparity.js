"use strict";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.webextensions.themes.enabled", true]],
  });
});

add_task(async function test_support_theme_frame() {
  const FRAME_COLOR = [71, 105, 91];
  const TAB_TEXT_COLOR = [207, 221, 192];
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "images": {
          "theme_frame": "face.png",
        },
        "colors": {
          "frame": FRAME_COLOR,
          "tab_background_text": TAB_TEXT_COLOR,
        },
      },
    },
    files: {
      "face.png": imageBufferFromDataURI(ENCODED_IMAGE_DATA),
    },
  });

  let docEl = window.document.documentElement;
  let transitionPromise = waitForTransition(docEl, "background-color");
  await extension.startup();
  await transitionPromise;

  Assert.ok(docEl.hasAttribute("lwtheme"), "LWT attribute should be set");
  Assert.equal(docEl.getAttribute("lwthemetextcolor"), "bright",
               "LWT text color attribute should be set");

  let style = window.getComputedStyle(docEl);
  Assert.ok(style.backgroundImage.includes("face.png"),
            `The backgroundImage should use face.png. Actual value is: ${style.backgroundImage}`);
  Assert.equal(style.backgroundColor, "rgb(" + FRAME_COLOR.join(", ") + ")",
               "Expected correct background color");
  Assert.equal(style.color, "rgb(" + TAB_TEXT_COLOR.join(", ") + ")",
               "Expected correct text color");

  await extension.unload();

  Assert.ok(!docEl.hasAttribute("lwtheme"), "LWT attribute should not be set");
});

add_task(async function test_support_theme_frame_inactive() {
  const FRAME_COLOR = [71, 105, 91];
  const FRAME_COLOR_INACTIVE = [255, 0, 0];
  const TAB_TEXT_COLOR = [207, 221, 192];
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "images": {
          "theme_frame": "image1.png",
        },
        "colors": {
          "frame": FRAME_COLOR,
          "frame_inactive": FRAME_COLOR_INACTIVE,
          "tab_background_text": TAB_TEXT_COLOR,
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
  });

  let docEl = window.document.documentElement;
  let transitionPromise = waitForTransition(docEl, "background-color");
  await extension.startup();
  await transitionPromise;

  let style = window.getComputedStyle(docEl);

  Assert.equal(style.backgroundColor, "rgb(" + FRAME_COLOR.join(", ") + ")",
               "Window background is set to the colors.frame property");

  // Now we'll open a new window to see if the inactive browser accent color changed
  transitionPromise = waitForTransition(docEl, "background-color");
  let window2 = await BrowserTestUtils.openNewBrowserWindow();
  await transitionPromise;

  Assert.equal(style.backgroundColor, "rgb(" + FRAME_COLOR_INACTIVE.join(", ") + ")",
               `Inactive window background color should be ${FRAME_COLOR_INACTIVE}`);

  await BrowserTestUtils.closeWindow(window2);
  await extension.unload();

  Assert.ok(!docEl.hasAttribute("lwtheme"), "LWT attribute should not be set");
});

add_task(async function test_lack_of_theme_frame_inactive() {
  const FRAME_COLOR = [71, 105, 91];
  const TAB_TEXT_COLOR = [207, 221, 192];
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "images": {
          "theme_frame": "image1.png",
        },
        "colors": {
          "frame": FRAME_COLOR,
          "tab_background_text": TAB_TEXT_COLOR,
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
  });

  let docEl = window.document.documentElement;
  let transitionPromise = waitForTransition(docEl, "background-color");
  await extension.startup();
  await transitionPromise;

  let style = window.getComputedStyle(docEl);

  Assert.equal(style.backgroundColor, "rgb(" + FRAME_COLOR.join(", ") + ")",
               "Window background is set to the colors.frame property");

  // Now we'll open a new window to make sure the inactive browser accent color stayed the same
  let window2 = await BrowserTestUtils.openNewBrowserWindow();

  Assert.equal(style.backgroundColor, "rgb(" + FRAME_COLOR.join(", ") + ")",
               "Inactive window background should not change if colors.frame_inactive isn't set");

  await BrowserTestUtils.closeWindow(window2);
  await extension.unload();

  Assert.ok(!docEl.hasAttribute("lwtheme"), "LWT attribute should not be set");
});
