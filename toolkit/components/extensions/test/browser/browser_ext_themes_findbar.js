"use strict";

// This test checks whether applied WebExtension themes that attempt to change
// the toolbar and toolbar_field properties also theme the findbar.

add_task(async function test_support_toolbar_properties_on_findbar() {
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
  await gBrowser.getFindBar();

  let findbar_button = document.getAnonymousElementByAttribute(gFindBar, "anonid", "highlight");

  info("Checking findbar background is set as toolbar color");
  Assert.equal(window.getComputedStyle(gFindBar).backgroundColor,
               hexToCSS(TOOLBAR_COLOR),
               "Findbar background color should be the same as toolbar background color.");

  info("Checking findbar and button text color is set as toolbar text color");
  Assert.equal(window.getComputedStyle(gFindBar).color,
               hexToCSS(TOOLBAR_TEXT_COLOR),
               "Findbar text color should be the same as toolbar text color.");
  Assert.equal(window.getComputedStyle(findbar_button).color,
               hexToCSS(TOOLBAR_TEXT_COLOR),
               "Findbar button text color should be the same as toolbar text color.");

  await extension.unload();
});

add_task(async function test_support_toolbar_field_properties_on_findbar() {
  const TOOLBAR_FIELD_COLOR = "#ff00ff";
  const TOOLBAR_FIELD_TEXT_COLOR = "#9400ff";
  const TOOLBAR_FIELD_BORDER_COLOR = "#ffffff";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "images": {
          "headerURL": "image1.png",
        },
        "colors": {
          "accentcolor": ACCENT_COLOR,
          "textcolor": TEXT_COLOR,
          "toolbar_field": TOOLBAR_FIELD_COLOR,
          "toolbar_field_text": TOOLBAR_FIELD_TEXT_COLOR,
          "toolbar_field_border": TOOLBAR_FIELD_BORDER_COLOR,
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
  });

  await extension.startup();
  await gBrowser.getFindBar();

  let findbar_textbox =
    document.getAnonymousElementByAttribute(gFindBar, "anonid", "findbar-textbox");

  let findbar_prev_button =
    document.getAnonymousElementByAttribute(gFindBar, "anonid", "find-previous");

  let findbar_next_button =
    document.getAnonymousElementByAttribute(gFindBar, "anonid", "find-next");

  info("Checking findbar textbox background is set as toolbar field background color");
  Assert.equal(window.getComputedStyle(findbar_textbox).backgroundColor,
               hexToCSS(TOOLBAR_FIELD_COLOR),
               "Findbar textbox background color should be the same as toolbar field color.");

  info("Checking findbar textbox color is set as toolbar field text color");
  Assert.equal(window.getComputedStyle(findbar_textbox).color,
               hexToCSS(TOOLBAR_FIELD_TEXT_COLOR),
               "Findbar textbox text color should be the same as toolbar field text color.");
  testBorderColor(findbar_textbox, TOOLBAR_FIELD_BORDER_COLOR);
  testBorderColor(findbar_prev_button, TOOLBAR_FIELD_BORDER_COLOR);
  testBorderColor(findbar_next_button, TOOLBAR_FIELD_BORDER_COLOR);

  await extension.unload();
});
