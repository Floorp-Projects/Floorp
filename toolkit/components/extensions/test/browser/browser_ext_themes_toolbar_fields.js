"use strict";

// This test checks whether applied WebExtension themes that attempt to change
// the background color and the color of the navbar text fields are applied properly.

const { CustomizableUITestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/CustomizableUITestUtils.sys.mjs"
);
let gCUITestUtils = new CustomizableUITestUtils(window);

add_setup(async function () {
  await gCUITestUtils.addSearchBar();
  registerCleanupFunction(() => {
    gCUITestUtils.removeSearchBar();
  });
});

add_task(async function test_support_toolbar_field_properties() {
  const TOOLBAR_FIELD_BACKGROUND = "#ff00ff";
  const TOOLBAR_FIELD_COLOR = "#00ff00";
  const TOOLBAR_FIELD_BORDER = "#aaaaff";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        images: {
          theme_frame: "image1.png",
        },
        colors: {
          frame: ACCENT_COLOR,
          tab_background_text: TEXT_COLOR,
          toolbar_field: TOOLBAR_FIELD_BACKGROUND,
          toolbar_field_text: TOOLBAR_FIELD_COLOR,
          toolbar_field_border: TOOLBAR_FIELD_BORDER,
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
  });

  await extension.startup();

  let root = document.documentElement;
  // Remove the `remotecontrol` attribute since it interferes with the urlbar styling.
  root.removeAttribute("remotecontrol");
  registerCleanupFunction(() => {
    root.setAttribute("remotecontrol", "true");
  });

  let fields = [
    document.querySelector("#urlbar-background"),
    BrowserSearch.searchBar,
  ].filter(field => {
    let bounds = field.getBoundingClientRect();
    return bounds.width > 0 && bounds.height > 0;
  });

  Assert.equal(fields.length, 2, "Should be testing two elements");

  info(
    `Checking toolbar background colors and colors for ${fields.length} toolbar fields.`
  );
  for (let field of fields) {
    info(`Testing ${field.id || field.className}`);
    Assert.equal(
      window.getComputedStyle(field).backgroundColor,
      hexToCSS(TOOLBAR_FIELD_BACKGROUND),
      "Field background should be set."
    );
    Assert.equal(
      window.getComputedStyle(field).color,
      hexToCSS(TOOLBAR_FIELD_COLOR),
      "Field color should be set."
    );
    testBorderColor(field, TOOLBAR_FIELD_BORDER);
  }

  await extension.unload();
});

add_task(async function test_support_toolbar_field_brighttext() {
  let root = document.documentElement;
  // Remove the `remotecontrol` attribute since it interferes with the urlbar styling.
  root.removeAttribute("remotecontrol");
  registerCleanupFunction(() => {
    root.setAttribute("remotecontrol", "true");
  });
  let urlbar = gURLBar.textbox;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        colors: {
          frame: ACCENT_COLOR,
          tab_background_text: TEXT_COLOR,
          toolbar_field: "#fff",
          toolbar_field_text: "#000",
        },
      },
    },
  });

  await extension.startup();

  Assert.equal(
    window.getComputedStyle(urlbar).color,
    hexToCSS("#000000"),
    "Color has been set"
  );
  Assert.equal(
    root.getAttribute("lwt-toolbar-field"),
    "light",
    "Should be light"
  );

  await extension.unload();

  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        colors: {
          frame: ACCENT_COLOR,
          tab_background_text: TEXT_COLOR,
          toolbar_field: "#000",
          toolbar_field_text: "#fff",
        },
      },
    },
  });

  await extension.startup();

  Assert.equal(
    window.getComputedStyle(urlbar).color,
    hexToCSS("#ffffff"),
    "Color has been set"
  );
  Assert.equal(
    root.getAttribute("lwt-toolbar-field"),
    "dark",
    "Should be dark"
  );

  await extension.unload();
});

// Verifies that we apply the lwt-toolbar-field="dark" attribute when
// toolbar fields are dark text on a dark background.
add_task(async function test_support_toolbar_field_brighttext_dark_on_dark() {
  let root = document.documentElement;
  // Remove the `remotecontrol` attribute since it interferes with the urlbar styling.
  root.removeAttribute("remotecontrol");
  registerCleanupFunction(() => {
    root.setAttribute("remotecontrol", "true");
  });
  let urlbar = gURLBar.textbox;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        colors: {
          frame: ACCENT_COLOR,
          tab_background_text: TEXT_COLOR,
          toolbar_field: "#000",
          toolbar_field_text: "#111111",
        },
      },
    },
  });

  await extension.startup();

  Assert.equal(
    window.getComputedStyle(urlbar).color,
    hexToCSS("#111111"),
    "Color has been set"
  );
  Assert.equal(
    root.getAttribute("lwt-toolbar-field"),
    "dark",
    "toolbar-field color-scheme should be dark"
  );

  await extension.unload();
});

add_task(async function test_no_explicit_toolbar_field_on_dark_toolbar() {
  let root = document.documentElement;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        colors: {
          frame: "#000",
          tab_background_text: "#fff",
          // Explicitly unset toolbar fields, but they default to light.
        },
      },
    },
  });

  await extension.startup();

  Assert.equal(
    root.getAttribute("lwt-toolbar-field"),
    "light",
    "toolbar-field color-scheme should be set and light"
  );

  await extension.unload();
});
