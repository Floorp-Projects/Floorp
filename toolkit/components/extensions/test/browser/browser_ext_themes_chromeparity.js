"use strict";

add_task(async function test_support_theme_frame() {
  const FRAME_COLOR = [71, 105, 91];
  const TAB_TEXT_COLOR = [0, 0, 0];
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        images: {
          theme_frame: "face.png",
        },
        colors: {
          frame: FRAME_COLOR,
          tab_background_text: TAB_TEXT_COLOR,
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

  Assert.ok(
    docEl.hasAttribute("lwtheme-image"),
    "LWT image attribute should be set"
  );

  Assert.equal(
    docEl.getAttribute("lwthemetextcolor"),
    "dark",
    "LWT text color attribute should be set"
  );

  let toolbox = document.querySelector("#navigator-toolbox");
  let toolboxCS = window.getComputedStyle(toolbox);

  if (backgroundColorSetOnRoot()) {
    let rootCS = window.getComputedStyle(docEl);
    Assert.ok(
      rootCS.backgroundImage.includes("face.png"),
      `The backgroundImage should use face.png. Actual value is: ${toolboxCS.backgroundImage}`
    );
    Assert.equal(
      rootCS.backgroundColor,
      "rgb(" + FRAME_COLOR.join(", ") + ")",
      "Expected correct background color"
    );
  } else {
    Assert.ok(
      toolboxCS.backgroundImage.includes("face.png"),
      `The backgroundImage should use face.png. Actual value is: ${toolboxCS.backgroundImage}`
    );
    Assert.equal(
      toolboxCS.backgroundColor,
      "rgb(" + FRAME_COLOR.join(", ") + ")",
      "Expected correct background color"
    );
  }
  Assert.equal(
    toolboxCS.color,
    "rgb(" + TAB_TEXT_COLOR.join(", ") + ")",
    "Expected correct text color"
  );

  await extension.unload();

  Assert.ok(!docEl.hasAttribute("lwtheme"), "LWT attribute should not be set");

  Assert.ok(
    !docEl.hasAttribute("lwtheme-image"),
    "LWT image attribute should not be set"
  );

  Assert.ok(
    !docEl.hasAttribute("lwthemetextcolor"),
    "LWT text color attribute should not be set"
  );
});

add_task(async function test_support_theme_frame_inactive() {
  const FRAME_COLOR = [71, 105, 91];
  const FRAME_COLOR_INACTIVE = [255, 0, 0];
  const TAB_TEXT_COLOR = [207, 221, 192];
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        images: {
          theme_frame: "image1.png",
        },
        colors: {
          frame: FRAME_COLOR,
          frame_inactive: FRAME_COLOR_INACTIVE,
          tab_background_text: TAB_TEXT_COLOR,
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
  });

  await extension.startup();

  let docEl = window.document.documentElement;
  let toolbox = document.querySelector("#navigator-toolbox");
  let toolboxCS = window.getComputedStyle(toolbox);

  if (backgroundColorSetOnRoot()) {
    let rootCS = window.getComputedStyle(docEl);
    Assert.equal(
      rootCS.backgroundColor,
      "rgb(" + FRAME_COLOR.join(", ") + ")",
      "Window background is set to the colors.frame property"
    );
  } else {
    Assert.equal(
      toolboxCS.backgroundColor,
      "rgb(" + FRAME_COLOR.join(", ") + ")",
      "Window background is set to the colors.frame property"
    );
  }

  // Now we'll open a new window to see if the inactive browser accent color changed
  let window2 = await BrowserTestUtils.openNewBrowserWindow();
  if (backgroundColorSetOnRoot()) {
    let rootCS = window.getComputedStyle(docEl);
    Assert.equal(
      rootCS.backgroundColor,
      "rgb(" + FRAME_COLOR_INACTIVE.join(", ") + ")",
      `Inactive window root background color should be ${FRAME_COLOR_INACTIVE}`
    );
  } else {
    Assert.equal(
      toolboxCS.backgroundColor,
      "rgb(" + FRAME_COLOR_INACTIVE.join(", ") + ")",
      `Inactive window background color should be ${FRAME_COLOR_INACTIVE}`
    );
  }

  await BrowserTestUtils.closeWindow(window2);
  await extension.unload();

  Assert.ok(!docEl.hasAttribute("lwtheme"), "LWT attribute should not be set");
});

add_task(async function test_lack_of_theme_frame_inactive() {
  const FRAME_COLOR = [71, 105, 91];
  const TAB_TEXT_COLOR = [207, 221, 192];
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        images: {
          theme_frame: "image1.png",
        },
        colors: {
          frame: FRAME_COLOR,
          tab_background_text: TAB_TEXT_COLOR,
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
  });

  await extension.startup();

  let docEl = window.document.documentElement;
  let toolbox = document.querySelector("#navigator-toolbox");
  let toolboxCS = window.getComputedStyle(toolbox);

  if (backgroundColorSetOnRoot()) {
    let rootCS = window.getComputedStyle(docEl);
    Assert.equal(
      rootCS.backgroundColor,
      "rgb(" + FRAME_COLOR.join(", ") + ")",
      "Window background is set to the colors.frame property"
    );
  } else {
    Assert.equal(
      toolboxCS.backgroundColor,
      "rgb(" + FRAME_COLOR.join(", ") + ")",
      "Window background is set to the colors.frame property"
    );
  }

  // Now we'll open a new window to make sure the inactive browser accent color stayed the same
  let window2 = await BrowserTestUtils.openNewBrowserWindow();
  if (backgroundColorSetOnRoot()) {
    let rootCS = window.getComputedStyle(docEl);
    Assert.equal(
      rootCS.backgroundColor,
      "rgb(" + FRAME_COLOR.join(", ") + ")",
      "Inactive window background should not change if colors.frame_inactive isn't set"
    );
  } else {
    Assert.equal(
      toolboxCS.backgroundColor,
      "rgb(" + FRAME_COLOR.join(", ") + ")",
      "Inactive window background should not change if colors.frame_inactive isn't set"
    );
  }

  await BrowserTestUtils.closeWindow(window2);
  await extension.unload();

  Assert.ok(!docEl.hasAttribute("lwtheme"), "LWT attribute should not be set");
});
