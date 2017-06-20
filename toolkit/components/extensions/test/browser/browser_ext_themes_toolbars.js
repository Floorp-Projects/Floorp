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
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
  });

  await extension.startup();

  gFindBar.onFindCommand();
  let promise = gFindBar._startFindDeferred ? gFindBar._startFindDeferred.promise : Promise.resolve();
  await promise;

  let toolbars = [gFindBar, ...document.getElementsByTagName("toolbar")].filter(toolbar => {
    let bounds = toolbar.getBoundingClientRect();
    return bounds.width > 0 && bounds.height > 0;
  });

  info(`Checking toolbar background colors for ${toolbars.length} toolbars.`);
  for (let toolbar of toolbars) {
    Assert.equal(window.getComputedStyle(toolbar).backgroundColor,
      "rgb(" + hexToRGB(TOOLBAR_COLOR).join(", ") + ")", "Toolbar color should be set.");
  }

  await extension.unload();
});
