"use strict";

add_task(async function setup() {
  // Remove the `remotecontrol` attribute since it interferes with the urlbar styling.
  document.documentElement.removeAttribute("remotecontrol");
  registerCleanupFunction(() => {
    document.documentElement.setAttribute("remotecontrol", "true");
  });
});

add_task(async function test_toolbar_field_focus() {
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

  info("Checking toolbar field's focus color");

  let urlBar = document.querySelector("#urlbar-background");
  gURLBar.textbox.setAttribute("focused", "true");

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

  gURLBar.textbox.removeAttribute("focused");

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

add_task(async function test_toolbar_field_focus_low_alpha() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        colors: {
          frame: "#FF0000",
          tab_background_color: "#ffffff",
          toolbar_field: "#FF00FF",
          toolbar_field_text: "#00FF00",
          toolbar_field_focus: "rgba(0, 0, 255, 0.4)",
          toolbar_field_text_focus: "red",
          toolbar_field_border_focus: "#FFFFFF",
        },
      },
    },
  });

  await extension.startup();
  gURLBar.textbox.setAttribute("focused", "true");

  let urlBar = document.querySelector("#urlbar-background");
  Assert.equal(
    window.getComputedStyle(urlBar).backgroundColor,
    `rgba(0, 0, 255, 0.9)`,
    "Background color has minimum opacity enforced"
  );
  Assert.equal(
    window.getComputedStyle(urlBar).color,
    `rgb(255, 255, 255)`,
    "Text color has been overridden to match background"
  );

  gURLBar.textbox.removeAttribute("focused");
  await extension.unload();
});
