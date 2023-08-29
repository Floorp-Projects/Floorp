"use strict";

add_task(async function test_support_tab_loading_filling() {
  const TAB_LOADING_COLOR = "#FF0000";

  // Make sure we use the animating loading icon
  await SpecialPowers.pushPrefEnv({
    set: [["ui.prefersReducedMotion", 0]],
  });

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        images: {
          theme_frame: "image1.png",
        },
        colors: {
          frame: "#000",
          toolbar: "#124455",
          tab_background_text: "#9400ff",
          tab_loading: TAB_LOADING_COLOR,
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
  });

  await extension.startup();

  info("Checking selected tab loading indicator colors");

  let selectedTab = document.querySelector(".tabbrowser-tab[visuallyselected]");

  selectedTab.setAttribute("busy", "true");
  selectedTab.setAttribute("progress", "true");

  let throbber = selectedTab.throbber;
  Assert.equal(
    window.getComputedStyle(throbber, "::before").fill,
    `rgb(${hexToRGB(TAB_LOADING_COLOR).join(", ")})`,
    "Throbber is filled with theme color"
  );

  selectedTab.removeAttribute("busy");
  selectedTab.removeAttribute("progress");
  await extension.unload();
});
