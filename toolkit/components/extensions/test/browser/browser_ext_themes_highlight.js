"use strict";

// This test checks whether applied WebExtension themes that attempt to change
// the color of the font and background in a selection are applied properly.
ChromeUtils.import(
  "resource://testing-common/CustomizableUITestUtils.jsm",
  this
);
let gCUITestUtils = new CustomizableUITestUtils(window);
add_task(async function setup() {
  await gCUITestUtils.addSearchBar();
  registerCleanupFunction(() => {
    gCUITestUtils.removeSearchBar();
  });
});

add_task(async function test_support_selection() {
  const HIGHLIGHT_TEXT_COLOR = "#9400FF";
  const HIGHLIGHT_COLOR = "#F89919";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        colors: {
          toolbar_field_highlight: HIGHLIGHT_COLOR,
          toolbar_field_highlight_text: HIGHLIGHT_TEXT_COLOR,
        },
      },
    },
  });

  await extension.startup();

  let fields = [
    gURLBar.inputField,
    document.querySelector("#searchbar .searchbar-textbox"),
  ].filter(field => {
    let bounds = field.getBoundingClientRect();
    return bounds.width > 0 && bounds.height > 0;
  });

  Assert.equal(fields.length, 2, "Should be testing two elements");

  info(
    `Checking background colors and colors for ${fields.length} toolbar input fields.`
  );
  for (let field of fields) {
    info(`Testing ${field.id || field.className}`);
    Assert.equal(
      window.getComputedStyle(field, "::selection").backgroundColor,
      hexToCSS(HIGHLIGHT_COLOR),
      "Input selection background should be set."
    );
    Assert.equal(
      window.getComputedStyle(field, "::selection").color,
      hexToCSS(HIGHLIGHT_TEXT_COLOR),
      "Input selection color should be set."
    );
  }

  await extension.unload();
});
