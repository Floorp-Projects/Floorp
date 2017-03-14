"use strict";

// This test checks whether applied WebExtension themes are persisted and applied
// on newly opened windows.

add_task(function* setup() {
  yield SpecialPowers.pushPrefEnv({
    set: [["extensions.webextensions.themes.enabled", true]],
  });
});

add_task(function* test_multiple_windows() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "images": {
          "headerURL": BACKGROUND,
        },
        "colors": {
          "accentcolor": ACCENT_COLOR,
          "textcolor": TEXT_COLOR,
        },
      },
    },
  });

  yield extension.startup();

  let docEl = window.document.documentElement;
  let style = window.getComputedStyle(docEl);

  Assert.ok(docEl.hasAttribute("lwtheme"), "LWT attribute should be set");
  Assert.equal(docEl.getAttribute("lwthemetextcolor"), "bright",
    "LWT text color attribute should be set");
  Assert.equal(style.backgroundImage, 'url("' + BACKGROUND.replace(/"/g, '\\"') + '")',
    "Expected background image");

  // Now we'll open a new window to see if the theme is also applied there.
  let window2 = yield BrowserTestUtils.openNewBrowserWindow();
  docEl = window2.document.documentElement;
  style = window2.getComputedStyle(docEl);

  Assert.ok(docEl.hasAttribute("lwtheme"), "LWT attribute should be set");
  Assert.equal(docEl.getAttribute("lwthemetextcolor"), "bright",
    "LWT text color attribute should be set");
  Assert.equal(style.backgroundImage, 'url("' + BACKGROUND.replace(/"/g, '\\"') + '")',
    "Expected background image");

  yield BrowserTestUtils.closeWindow(window2);
  yield extension.unload();
});
