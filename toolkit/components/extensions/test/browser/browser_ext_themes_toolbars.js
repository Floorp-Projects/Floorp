"use strict";

// This test checks whether applied WebExtension themes that attempt to change
// the background color of toolbars are applied properly.

add_task(async function test_support_toolbar_property() {
  const TOOLBAR_COLOR = "#ff00ff";
  const TOOLBAR_TEXT_COLOR = "#9400ff";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        colors: {
          frame: ACCENT_COLOR,
          tab_background_text: TEXT_COLOR,
          toolbar: TOOLBAR_COLOR,
          toolbar_text: TOOLBAR_TEXT_COLOR,
        },
      },
    },
  });

  let toolbox = document.querySelector("#navigator-toolbox");
  let toolbars = [
    ...toolbox.querySelectorAll("toolbar:not(#TabsToolbar)"),
  ].filter(toolbar => {
    let bounds = toolbar.getBoundingClientRect();
    return bounds.width > 0 && bounds.height > 0;
  });

  let transitionPromise = waitForTransition(toolbars[0], "background-color");
  await extension.startup();
  await transitionPromise;

  info(`Checking toolbar colors for ${toolbars.length} toolbars.`);
  for (let toolbar of toolbars) {
    info(`Testing ${toolbar.id}`);
    Assert.equal(
      window.getComputedStyle(toolbar).backgroundColor,
      hexToCSS(TOOLBAR_COLOR),
      "Toolbar background color should be set."
    );
    Assert.equal(
      window.getComputedStyle(toolbar).color,
      hexToCSS(TOOLBAR_TEXT_COLOR),
      "Toolbar text color should be set."
    );
  }

  info("Checking selected tab colors");
  let selectedTab = document.querySelector(".tabbrowser-tab[selected]");
  Assert.equal(
    window.getComputedStyle(selectedTab).color,
    hexToCSS(TOOLBAR_TEXT_COLOR),
    "Selected tab text color should be set."
  );

  await extension.unload();
});

add_task(async function test_bookmark_text_property() {
  const TOOLBAR_COLOR = [255, 0, 255];
  const TOOLBAR_TEXT_COLOR = [48, 0, 255];
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        colors: {
          frame: ACCENT_COLOR,
          tab_background_text: TEXT_COLOR,
          toolbar: TOOLBAR_COLOR,
          bookmark_text: TOOLBAR_TEXT_COLOR,
        },
      },
    },
  });

  await extension.startup();

  let toolbox = document.querySelector("#navigator-toolbox");
  let toolbars = [
    ...toolbox.querySelectorAll("toolbar:not(#TabsToolbar)"),
  ].filter(toolbar => {
    let bounds = toolbar.getBoundingClientRect();
    return bounds.width > 0 && bounds.height > 0;
  });

  info(`Checking toolbar colors for ${toolbars.length} toolbars.`);
  for (let toolbar of toolbars) {
    info(`Testing ${toolbar.id}`);
    Assert.equal(
      window.getComputedStyle(toolbar).color,
      rgbToCSS(TOOLBAR_TEXT_COLOR),
      "bookmark_text should be an alias for toolbar_text"
    );
  }

  info("Checking selected tab colors");
  let selectedTab = document.querySelector(".tabbrowser-tab[selected]");
  Assert.equal(
    window.getComputedStyle(selectedTab).color,
    rgbToCSS(TOOLBAR_TEXT_COLOR),
    "Selected tab text color should be set."
  );

  await extension.unload();
});
