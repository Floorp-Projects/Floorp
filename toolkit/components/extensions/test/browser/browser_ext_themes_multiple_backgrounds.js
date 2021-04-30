"use strict";

add_task(async function test_support_backgrounds_position() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        images: {
          theme_frame: "face1.png",
          additional_backgrounds: ["face2.png", "face2.png", "face2.png"],
        },
        colors: {
          frame: `rgb(${FRAME_COLOR.join(",")})`,
          tab_background_text: `rgb(${TAB_BACKGROUND_TEXT_COLOR.join(",")})`,
        },
        properties: {
          additional_backgrounds_alignment: [
            "left top",
            "center top",
            "right bottom",
          ],
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
  Assert.equal(
    docEl.getAttribute("lwthemetextcolor"),
    "bright",
    "LWT text color attribute should be set"
  );

  let toolboxCS = window.getComputedStyle(toolbox);
  let toolboxBgImage = toolboxCS.backgroundImage.split(",")[0].trim();
  if (backgroundColorSetOnRoot()) {
    let rootCS = window.getComputedStyle(docEl);
    let rootBgImage = rootCS.backgroundImage.split(",")[0].trim();
    Assert.ok(
      rootBgImage.includes("face1.png"),
      `The backgroundImage should use face1.png. Actual value is: ${rootBgImage}`
    );
    Assert.equal(
      toolboxCS.backgroundImage,
      Array(3)
        .fill(toolboxBgImage)
        .join(", "),
      "The backgroundImage should use face2.png three times."
    );
    Assert.equal(
      toolboxCS.backgroundPosition,
      "0% 0%, 50% 0%, 100% 100%",
      "The backgroundPosition should use the three values provided."
    );
  } else {
    Assert.equal(
      toolboxCS.backgroundImage,
      [1, 2, 2, 2]
        .map(num => toolboxBgImage.replace(/face[\d]*/, `face${num}`))
        .join(", "),
      "The backgroundImage should use face1.png once and face2.png three times."
    );
    Assert.equal(
      toolboxCS.backgroundPosition,
      "100% 0%, 0% 0%, 50% 0%, 100% 100%",
      "The backgroundPosition should use the three values provided, preceded by the default for theme_frame."
    );
    /**
     * We expect duplicate background-repeat values because we apply `no-repeat`
     * once for theme_frame, and again as the default value of
     * --lwt-background-tiling.
     */
    Assert.equal(
      toolboxCS.backgroundRepeat,
      "no-repeat, no-repeat",
      "The backgroundPosition should use the default value."
    );
  }

  await extension.unload();

  Assert.ok(!docEl.hasAttribute("lwtheme"), "LWT attribute should not be set");
  toolboxCS = window.getComputedStyle(toolbox);

  // Styles should've reverted to their initial values.
  if (backgroundColorSetOnRoot()) {
    let rootCS = window.getComputedStyle(docEl);
    Assert.equal(rootCS.backgroundImage, "none");
    Assert.equal(rootCS.backgroundPosition, "0% 0%");
    Assert.equal(rootCS.backgroundRepeat, "repeat");
  }
  Assert.equal(toolboxCS.backgroundImage, "none");
  Assert.equal(toolboxCS.backgroundPosition, "0% 0%");
  Assert.equal(toolboxCS.backgroundRepeat, "repeat");
});

add_task(async function test_support_backgrounds_repeat() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        images: {
          theme_frame: "face0.png",
          additional_backgrounds: ["face1.png", "face2.png", "face3.png"],
        },
        colors: {
          frame: FRAME_COLOR,
          tab_background_text: TAB_BACKGROUND_TEXT_COLOR,
        },
        properties: {
          additional_backgrounds_tiling: ["repeat-x", "repeat-y", "repeat"],
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
  Assert.equal(
    docEl.getAttribute("lwthemetextcolor"),
    "bright",
    "LWT text color attribute should be set"
  );

  let toolboxCS = window.getComputedStyle(toolbox);
  if (backgroundColorSetOnRoot()) {
    let rootCS = window.getComputedStyle(docEl);
    let rootImage = rootCS.backgroundImage.split(",")[0].trim();
    Assert.ok(
      rootImage.includes("face0.png"),
      `The backgroundImage should use face.png. Actual value is: ${rootImage}`
    );
    Assert.equal(
      [1, 2, 3]
        .map(num => rootImage.replace(/face[\d]*/, `face${num}`))
        .join(", "),
      toolboxCS.backgroundImage,
      "The backgroundImage should use face.png three times."
    );
    Assert.equal(
      rootCS.backgroundPosition,
      "100% 0%, 100% 0%",
      "The backgroundPosition should use the default value for root."
    );
    Assert.equal(
      toolboxCS.backgroundPosition,
      "100% 0%",
      "The backgroundPosition should use the default value for navigator-toolbox."
    );
    Assert.equal(
      rootCS.backgroundRepeat,
      "no-repeat, repeat-x, repeat-y, repeat",
      "The backgroundRepeat should use the default values for root."
    );
    Assert.equal(
      toolboxCS.backgroundRepeat,
      "repeat-x, repeat-y, repeat",
      "The backgroundRepeat should use the three values provided for navigator-toolbox."
    );
  } else {
    let toolboxImage = toolboxCS.backgroundImage.split(",")[0].trim();
    Assert.equal(
      [0, 1, 2, 3]
        .map(num => toolboxImage.replace(/face[\d]*/, `face${num}`))
        .join(", "),
      toolboxCS.backgroundImage,
      "The backgroundImage should use face.png four times."
    );
    /**
     * We expect duplicate background-position values because we apply `right top`
     * once for theme_frame, and again as the default value of
     * --lwt-background-alignment.
     */
    Assert.equal(
      toolboxCS.backgroundPosition,
      "100% 0%, 100% 0%",
      "The backgroundPosition should use the default value for navigator-toolbox."
    );
    Assert.equal(
      toolboxCS.backgroundRepeat,
      "no-repeat, repeat-x, repeat-y, repeat",
      "The backgroundRepeat should use the three values provided for --lwt-background-tiling, preceeded by the default for theme_frame."
    );
  }

  await extension.unload();

  Assert.ok(!docEl.hasAttribute("lwtheme"), "LWT attribute should not be set");
});

add_task(async function test_additional_images_check() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        images: {
          theme_frame: "face.png",
        },
        colors: {
          frame: FRAME_COLOR,
          tab_background_text: TAB_BACKGROUND_TEXT_COLOR,
        },
        properties: {
          additional_backgrounds_tiling: ["repeat-x", "repeat-y", "repeat"],
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
  Assert.equal(
    docEl.getAttribute("lwthemetextcolor"),
    "bright",
    "LWT text color attribute should be set"
  );

  let toolboxCS = window.getComputedStyle(toolbox);
  if (backgroundColorSetOnRoot()) {
    let rootCS = window.getComputedStyle(docEl);
    let bgImage = rootCS.backgroundImage.split(",")[0].trim();
    Assert.ok(
      bgImage.includes("face.png"),
      `The backgroundImage should use face.png. Actual value is: ${bgImage}`
    );
    Assert.equal(
      "none",
      toolboxCS.backgroundImage,
      "The backgroundImage should not use face.png."
    );
    Assert.equal(
      rootCS.backgroundPosition,
      "100% 0%, 100% 0%",
      "The backgroundPosition should use the default value."
    );
    Assert.equal(
      rootCS.backgroundRepeat,
      "no-repeat, no-repeat",
      "The backgroundPosition should use only one (default) value for the header and the default additional images."
    );
  } else {
    let bgImage = toolboxCS.backgroundImage.split(",")[0].trim();
    Assert.ok(
      bgImage.includes("face.png"),
      `The backgroundImage should use face.png. Actual value is: ${bgImage}`
    );
    Assert.ok(
      bgImage.includes("face.png"),
      `The backgroundImage should use face.png. Actual value is: ${bgImage}`
    );
    Assert.equal(
      toolboxCS.backgroundPosition,
      "100% 0%, 100% 0%",
      "The backgroundPosition should use the default value."
    );
    Assert.equal(
      toolboxCS.backgroundRepeat,
      "no-repeat, no-repeat",
      "The backgroundRepeat should use the default value."
    );
  }

  await extension.unload();

  Assert.ok(!docEl.hasAttribute("lwtheme"), "LWT attribute should not be set");
});
