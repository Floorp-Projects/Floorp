"use strict";

const DEFAULT_THEME_BG_COLOR = "rgb(255, 255, 255)";
const DEFAULT_THEME_TEXT_COLOR = "rgb(0, 0, 0)";

add_task(async function test_deprecated_LWT_properties_ignored() {
  // This test uses deprecated theme properties, so warnings are expected.
  ExtensionTestUtils.failOnSchemaWarnings(false);

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        images: {
          headerURL: "image1.png",
        },
        colors: {
          accentcolor: ACCENT_COLOR,
          textcolor: TEXT_COLOR,
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
  });

  await extension.startup();

  let docEl = window.document.documentElement;
  let docStyle = window.getComputedStyle(docEl);
  Assert.ok(docEl.hasAttribute("lwtheme"), "LWT attribute should be set");
  Assert.ok(
    !docEl.hasAttribute("lwtheme-image"),
    "LWT image attribute should not be set on deprecated headerURL alias"
  );
  Assert.ok(
    !docEl.getAttribute("lwtheme-brighttext"),
    "LWT text color attribute should not be set on deprecated textcolor alias"
  );

  Assert.equal(
    getToolboxBackgroundColor(),
    DEFAULT_THEME_BG_COLOR,
    "Expected default theme background color"
  );

  Assert.equal(
    docStyle.color,
    DEFAULT_THEME_TEXT_COLOR,
    "Expected default theme text color"
  );

  await extension.unload();

  Assert.ok(!docEl.hasAttribute("lwtheme"), "LWT attribute should not be set");
});
