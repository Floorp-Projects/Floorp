"use strict";

// This test checks whether applied WebExtension themes that attempt to change
// the background color of toolbars are applied properly.

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.webextensions.themes.enabled", true]],
  });
});

add_task(async function test_support_toolbar_property() {
  const TOOLBAR_COLOR = "#ff00ff";
  const TOOLBAR_TEXT_COLOR = "#9400ff";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "images": {
          "headerURL": "image1.png",
        },
        "colors": {
          "accentcolor": ACCENT_COLOR,
          "textcolor": TEXT_COLOR,
          "toolbar": TOOLBAR_COLOR,
          "toolbar_text": TOOLBAR_TEXT_COLOR,
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
  });

  await extension.startup();

  let toolbox = document.querySelector("#navigator-toolbox");
  let toolbars = [...toolbox.querySelectorAll("toolbar:not(#TabsToolbar)")].filter(toolbar => {
    let bounds = toolbar.getBoundingClientRect();
    return bounds.width > 0 && bounds.height > 0;
  });

  info(`Checking toolbar colors for ${toolbars.length} toolbars.`);
  for (let toolbar of toolbars) {
    info(`Testing ${toolbar.id}`);
    Assert.equal(window.getComputedStyle(toolbar).backgroundColor,
      "rgb(" + hexToRGB(TOOLBAR_COLOR).join(", ") + ")", "Toolbar background color should be set.");
    Assert.equal(window.getComputedStyle(toolbar).color,
      "rgb(" + hexToRGB(TOOLBAR_TEXT_COLOR).join(", ") + ")", "Toolbar text color should be set.");
  }

  info("Checking selected tab colors");
  let selectedTab = document.querySelector(".tabbrowser-tab[selected]");
  Assert.equal(window.getComputedStyle(selectedTab).color,
    "rgb(" + hexToRGB(TOOLBAR_TEXT_COLOR).join(", ") + ")", "Selected tab text color should be set.");

  await extension.unload();
});
