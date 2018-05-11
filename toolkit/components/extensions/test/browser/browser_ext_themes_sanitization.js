"use strict";

// This test checks color sanitization in various situations

add_task(async function test_sanitization_invalid() {
  // This test checks that invalid values are sanitized
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "colors": {
          "accentcolor": ACCENT_COLOR,
          "textcolor": TEXT_COLOR,
          "toolbar_bottom_separator": "ntimsfavoriteblue",
        },
      },
    },
  });

  let docEl = document.documentElement;

  let transitionPromise = waitForTransition(docEl, "background-color");
  await extension.startup();
  await transitionPromise;

  let toolbox = document.querySelector("#navigator-toolbox");
  Assert.equal(
    window.getComputedStyle(toolbox, "::after").borderBottomColor,
    "rgb(0, 0, 0)",
    "All invalid values should always compute to black."
  );

  await extension.unload();
});

add_task(async function test_sanitization_css_variables() {
  // This test checks that CSS variables are sanitized
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "colors": {
          "accentcolor": ACCENT_COLOR,
          "textcolor": TEXT_COLOR,
          "toolbar_bottom_separator": "var(--arrowpanel-dimmed)",
        },
      },
    },
  });

  let docEl = document.documentElement;

  let transitionPromise = waitForTransition(docEl, "background-color");
  await extension.startup();
  await transitionPromise;

  let toolbox = document.querySelector("#navigator-toolbox");
  Assert.equal(
    window.getComputedStyle(toolbox, "::after").borderBottomColor,
    "rgb(0, 0, 0)",
    "All CSS variables should always compute to black."
  );

  await extension.unload();
});

add_task(async function test_sanitization_transparent() {
  // This test checks whether transparent values are applied properly
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "colors": {
          "accentcolor": ACCENT_COLOR,
          "textcolor": TEXT_COLOR,
          "toolbar_top_separator": "transparent",
          "toolbar_bottom_separator": "transparent",
        },
      },
    },
  });

  let docEl = document.documentElement;

  let transitionPromise = waitForTransition(docEl, "background-color");
  await extension.startup();
  await transitionPromise;

  let navbar = document.querySelector("#nav-bar");
  Assert.ok(
    window.getComputedStyle(navbar).boxShadow.includes("rgba(0, 0, 0, 0)"),
    "Top separator should be transparent"
  );

  let toolbox = document.querySelector("#navigator-toolbox");
  Assert.equal(
    window.getComputedStyle(toolbox, "::after").borderBottomColor,
    "rgba(0, 0, 0, 0)",
    "Bottom separator should be transparent"
  );

  await extension.unload();
});

add_task(async function test_sanitization_transparent_accentcolor() {
  // This test checks whether transparent accentcolor falls back to white.
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "colors": {
          "accentcolor": "transparent",
          "textcolor": TEXT_COLOR,
        },
      },
    },
  });

  let docEl = document.documentElement;

  let transitionPromise = waitForTransition(docEl, "background-color");
  await extension.startup();
  await transitionPromise;

  Assert.equal(
    window.getComputedStyle(docEl).backgroundColor,
    "rgb(255, 255, 255)",
    "Accent color should be white",
  );

  await extension.unload();
});

add_task(async function test_sanitization_transparent_textcolor() {
  // This test checks whether transparent textcolor falls back to black.
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "colors": {
          "accentcolor": ACCENT_COLOR,
          "textcolor": "transparent",
        },
      },
    },
  });

  let docEl = document.documentElement;

  let transitionPromise = waitForTransition(docEl, "background-color");
  await extension.startup();
  await transitionPromise;

  Assert.equal(
    window.getComputedStyle(docEl).color,
    "rgb(0, 0, 0)",
    "Text color should be black",
  );

  await extension.unload();
});
