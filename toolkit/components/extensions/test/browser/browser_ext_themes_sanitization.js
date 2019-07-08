"use strict";

// This test checks color sanitization in various situations

add_task(async function test_sanitization_invalid() {
  // This test checks that invalid values are sanitized
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        colors: {
          frame: ACCENT_COLOR,
          tab_background_text: TEXT_COLOR,
          bookmark_text: "ntimsfavoriteblue",
        },
      },
    },
  });

  await extension.startup();

  let navbar = document.querySelector("#nav-bar");
  Assert.equal(
    window.getComputedStyle(navbar).color,
    "rgb(0, 0, 0)",
    "All invalid values should always compute to black."
  );

  await extension.unload();
});

add_task(async function test_sanitization_css_variables() {
  // This test checks that CSS variables are sanitized
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        colors: {
          frame: ACCENT_COLOR,
          tab_background_text: TEXT_COLOR,
          bookmark_text: "var(--arrowpanel-dimmed)",
        },
      },
    },
  });

  await extension.startup();

  let navbar = document.querySelector("#nav-bar");
  Assert.equal(
    window.getComputedStyle(navbar).color,
    "rgb(0, 0, 0)",
    "All CSS variables should always compute to black."
  );

  await extension.unload();
});

add_task(async function test_sanitization_important() {
  // This test checks that the sanitizer cannot be fooled with !important
  let stylesheetAttr = `href="data:text/css,*{color:red!important}" type="text/css"`;
  let stylesheet = document.createProcessingInstruction(
    "xml-stylesheet",
    stylesheetAttr
  );
  let load = BrowserTestUtils.waitForEvent(stylesheet, "load");
  document.insertBefore(stylesheet, document.documentElement);
  await load;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        colors: {
          frame: ACCENT_COLOR,
          tab_background_text: TEXT_COLOR,
          bookmark_text: "green",
        },
      },
    },
  });

  await extension.startup();

  let navbar = document.querySelector("#nav-bar");
  Assert.equal(
    window.getComputedStyle(navbar).color,
    "rgb(255, 0, 0)",
    "Sheet applies"
  );

  stylesheet.remove();

  Assert.equal(
    window.getComputedStyle(navbar).color,
    "rgb(0, 128, 0)",
    "Shouldn't be able to fool the color sanitizer with !important"
  );

  await extension.unload();
});

add_task(async function test_sanitization_transparent() {
  // This test checks whether transparent values are applied properly
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        colors: {
          frame: ACCENT_COLOR,
          tab_background_text: TEXT_COLOR,
          toolbar_top_separator: "transparent",
        },
      },
    },
  });

  await extension.startup();

  let navbar = document.querySelector("#nav-bar");
  Assert.ok(
    window.getComputedStyle(navbar).boxShadow.includes("rgba(0, 0, 0, 0)"),
    "Top separator should be transparent"
  );

  await extension.unload();
});

add_task(async function test_sanitization_transparent_frame_color() {
  // This test checks whether transparent frame color falls back to white.
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        colors: {
          frame: "transparent",
          tab_background_text: TEXT_COLOR,
        },
      },
    },
  });

  await extension.startup();

  let docEl = document.documentElement;
  Assert.equal(
    window.getComputedStyle(docEl).backgroundColor,
    "rgb(255, 255, 255)",
    "Accent color should be white"
  );

  await extension.unload();
});

add_task(
  async function test_sanitization_transparent_tab_background_text_color() {
    // This test checks whether transparent textcolor falls back to black.
    let extension = ExtensionTestUtils.loadExtension({
      manifest: {
        theme: {
          colors: {
            frame: ACCENT_COLOR,
            tab_background_text: "transparent",
          },
        },
      },
    });

    await extension.startup();

    let docEl = document.documentElement;
    Assert.equal(
      window.getComputedStyle(docEl).color,
      "rgb(0, 0, 0)",
      "Text color should be black"
    );

    await extension.unload();
  }
);
