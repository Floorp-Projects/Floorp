"use strict";

add_task(async function test_toolbar_field_hover() {
  const TOOLBAR_FIELD_BACKGROUND = "#FF00FF";
  const TOOLBAR_FIELD_COLOR = "#00FF00";
  const TOOLBAR_FOCUS_BACKGROUND = "#FF0000";
  const TOOLBAR_FOCUS_TEXT = "#9400FF";
  const TOOLBAR_FOCUS_BORDER = "#FFFFFF";

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        colors: {
          frame: "#FF0000",
          tab_background_color: "#ffffff",
          toolbar_field: TOOLBAR_FIELD_BACKGROUND,
          toolbar_field_text: TOOLBAR_FIELD_COLOR,
          toolbar_field_focus: TOOLBAR_FOCUS_BACKGROUND,
          toolbar_field_text_focus: TOOLBAR_FOCUS_TEXT,
          toolbar_field_border_focus: TOOLBAR_FOCUS_BORDER,
        },
      },
    },
  });

  await extension.startup();

  // Remove the `remotecontrol` attribute since it interferes with the urlbar styling.
  document.documentElement.removeAttribute("remotecontrol");
  registerCleanupFunction(() => {
    document.documentElement.setAttribute("remotecontrol", "true");
  });
  info("Checking toolbar field's focus color");

  let urlBar = document.getElementById("urlbar");
  urlBar.setAttribute("focused", "true");
  Assert.equal(
    window.getComputedStyle(urlBar).backgroundColor,
    `rgb(${hexToRGB(TOOLBAR_FOCUS_BACKGROUND).join(", ")})`,
    "Background Color is changed"
  );

  Assert.equal(
    window.getComputedStyle(urlBar).color,
    `rgb(${hexToRGB(TOOLBAR_FOCUS_TEXT).join(", ")})`,
    "Text Color is changed"
  );
  testBorderColor(urlBar, TOOLBAR_FOCUS_BORDER);

  urlBar.removeAttribute("focused");

  Assert.equal(
    window.getComputedStyle(urlBar).backgroundColor,
    `rgb(${hexToRGB(TOOLBAR_FIELD_BACKGROUND).join(", ")})`,
    "Background Color is set back to initial"
  );
  Assert.equal(
    window.getComputedStyle(urlBar).color,
    `rgb(${hexToRGB(TOOLBAR_FIELD_COLOR).join(", ")})`,
    "Text Color is set back to initial"
  );
  await extension.unload();
});
