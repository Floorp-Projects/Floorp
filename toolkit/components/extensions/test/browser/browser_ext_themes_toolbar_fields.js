"use strict";

// This test checks whether applied WebExtension themes that attempt to change
// the background color and the color of the navbar text fields are applied properly.

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({set: [
    ["extensions.webextensions.themes.enabled", true],
    ["browser.search.widget.inNavBar", true],
  ]});
});

function testBorderColor(element, expected) {
  Assert.equal(window.getComputedStyle(element).borderLeftColor,
    "rgb(" + hexToRGB(expected).join(", ") + ")", "Field left border color should be set.");
  Assert.equal(window.getComputedStyle(element).borderRightColor,
    "rgb(" + hexToRGB(expected).join(", ") + ")", "Field right border color should be set.");
  Assert.equal(window.getComputedStyle(element).borderTopColor,
    "rgb(" + hexToRGB(expected).join(", ") + ")", "Field top border color should be set.");
  Assert.equal(window.getComputedStyle(element).borderBottomColor,
    "rgb(" + hexToRGB(expected).join(", ") + ")", "Field bottom border color should be set.");
}

add_task(async function test_support_toolbar_field_properties() {
  const TOOLBAR_FIELD_BACKGROUND = "#ff00ff";
  const TOOLBAR_FIELD_COLOR = "#00ff00";
  const TOOLBAR_FIELD_BORDER = "#aaaaff";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "images": {
          "headerURL": "image1.png",
        },
        "colors": {
          "accentcolor": ACCENT_COLOR,
          "textcolor": TEXT_COLOR,
          "toolbar_field": TOOLBAR_FIELD_BACKGROUND,
          "toolbar_field_text": TOOLBAR_FIELD_COLOR,
          "toolbar_field_border": TOOLBAR_FIELD_BORDER,
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
  });

  await extension.startup();

  // Remove the `remotecontrol` attribute since it interferes with the urlbar styling.
  document.documentElement.removeAttribute("remotecontrol");

  let toolbox = document.querySelector("#navigator-toolbox");
  let searchbar = document.querySelector("#searchbar");
  let fields = [
    toolbox.querySelector("#urlbar"),
    document.getAnonymousElementByAttribute(searchbar, "anonid", "searchbar-textbox"),
  ].filter(field => {
    let bounds = field.getBoundingClientRect();
    return bounds.width > 0 && bounds.height > 0;
  });

  Assert.equal(fields.length, 2, "Should be testing two elements");

  info(`Checking toolbar background colors and colors for ${fields.length} toolbar fields.`);
  for (let field of fields) {
    info(`Testing ${field.id || field.className}`);
    Assert.equal(window.getComputedStyle(field).backgroundColor,
      "rgb(" + hexToRGB(TOOLBAR_FIELD_BACKGROUND).join(", ") + ")", "Field background should be set.");
    Assert.equal(window.getComputedStyle(field).color,
      "rgb(" + hexToRGB(TOOLBAR_FIELD_COLOR).join(", ") + ")", "Field color should be set.");
    testBorderColor(field, TOOLBAR_FIELD_BORDER);
  }

  // Restore the `remotecontrol` attribute at the end of the test.
  document.documentElement.setAttribute("remotecontrol", "true");

  await extension.unload();
});
