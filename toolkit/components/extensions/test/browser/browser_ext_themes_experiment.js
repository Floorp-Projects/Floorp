"use strict";

const { AddonSettings } = ChromeUtils.import(
  "resource://gre/modules/addons/AddonSettings.jsm"
);

// This test checks whether the theme experiments work
add_task(async function test_experiment_static_theme() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        colors: {
          some_color_property: "#ff00ff",
        },
        images: {
          some_image_property: "background.jpg",
        },
        properties: {
          some_random_property: "no-repeat",
        },
      },
      theme_experiment: {
        colors: {
          some_color_property: "--some-color-property",
        },
        images: {
          some_image_property: "--some-image-property",
        },
        properties: {
          some_random_property: "--some-random-property",
        },
      },
    },
  });

  const root = window.document.documentElement;

  is(
    root.style.getPropertyValue("--some-color-property"),
    "",
    "Color property should be unset"
  );
  is(
    root.style.getPropertyValue("--some-image-property"),
    "",
    "Image property should be unset"
  );
  is(
    root.style.getPropertyValue("--some-random-property"),
    "",
    "Generic Property should be unset."
  );

  await extension.startup();

  const testExperimentApplied = rootEl => {
    if (AddonSettings.EXPERIMENTS_ENABLED) {
      is(
        rootEl.style.getPropertyValue("--some-color-property"),
        hexToCSS("#ff00ff"),
        "Color property should be parsed and set."
      );
      ok(
        rootEl.style
          .getPropertyValue("--some-image-property")
          .startsWith("url("),
        "Image property should be parsed."
      );
      ok(
        rootEl.style
          .getPropertyValue("--some-image-property")
          .endsWith("background.jpg)"),
        "Image property should be set."
      );
      is(
        rootEl.style.getPropertyValue("--some-random-property"),
        "no-repeat",
        "Generic Property should be set."
      );
    } else {
      is(
        rootEl.style.getPropertyValue("--some-color-property"),
        "",
        "Color property should be unset"
      );
      is(
        rootEl.style.getPropertyValue("--some-image-property"),
        "",
        "Image property should be unset"
      );
      is(
        rootEl.style.getPropertyValue("--some-random-property"),
        "",
        "Generic Property should be unset."
      );
    }
  };

  info("Testing that current window updated with the experiment applied");
  testExperimentApplied(root);

  info("Testing that new window initialized with the experiment applied");
  const newWindow = await BrowserTestUtils.openNewBrowserWindow();
  const newWindowRoot = newWindow.document.documentElement;
  testExperimentApplied(newWindowRoot);

  await extension.unload();

  info("Testing that both windows unapplied the experiment");
  for (const rootEl of [root, newWindowRoot]) {
    is(
      rootEl.style.getPropertyValue("--some-color-property"),
      "",
      "Color property should be unset"
    );
    is(
      rootEl.style.getPropertyValue("--some-image-property"),
      "",
      "Image property should be unset"
    );
    is(
      rootEl.style.getPropertyValue("--some-random-property"),
      "",
      "Generic Property should be unset."
    );
  }
  await BrowserTestUtils.closeWindow(newWindow);
});

add_task(async function test_experiment_dynamic_theme() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["theme"],
      theme_experiment: {
        colors: {
          some_color_property: "--some-color-property",
        },
        images: {
          some_image_property: "--some-image-property",
        },
        properties: {
          some_random_property: "--some-random-property",
        },
      },
    },
    background() {
      const theme = {
        colors: {
          some_color_property: "#ff00ff",
        },
        images: {
          some_image_property: "background.jpg",
        },
        properties: {
          some_random_property: "no-repeat",
        },
      };
      browser.test.onMessage.addListener(msg => {
        if (msg === "update-theme") {
          browser.theme.update(theme).then(() => {
            browser.test.sendMessage("theme-updated");
          });
        } else {
          browser.theme.reset().then(() => {
            browser.test.sendMessage("theme-reset");
          });
        }
      });
    },
  });

  await extension.startup();

  const root = window.document.documentElement;

  is(
    root.style.getPropertyValue("--some-color-property"),
    "",
    "Color property should be unset"
  );
  is(
    root.style.getPropertyValue("--some-image-property"),
    "",
    "Image property should be unset"
  );
  is(
    root.style.getPropertyValue("--some-random-property"),
    "",
    "Generic Property should be unset."
  );

  extension.sendMessage("update-theme");
  await extension.awaitMessage("theme-updated");

  const testExperimentApplied = rootEl => {
    if (AddonSettings.EXPERIMENTS_ENABLED) {
      is(
        rootEl.style.getPropertyValue("--some-color-property"),
        hexToCSS("#ff00ff"),
        "Color property should be parsed and set."
      );
      ok(
        rootEl.style
          .getPropertyValue("--some-image-property")
          .startsWith("url("),
        "Image property should be parsed."
      );
      ok(
        rootEl.style
          .getPropertyValue("--some-image-property")
          .endsWith("background.jpg)"),
        "Image property should be set."
      );
      is(
        rootEl.style.getPropertyValue("--some-random-property"),
        "no-repeat",
        "Generic Property should be set."
      );
    } else {
      is(
        rootEl.style.getPropertyValue("--some-color-property"),
        "",
        "Color property should be unset"
      );
      is(
        rootEl.style.getPropertyValue("--some-image-property"),
        "",
        "Image property should be unset"
      );
      is(
        rootEl.style.getPropertyValue("--some-random-property"),
        "",
        "Generic Property should be unset."
      );
    }
  };
  testExperimentApplied(root);

  const newWindow = await BrowserTestUtils.openNewBrowserWindow();
  const newWindowRoot = newWindow.document.documentElement;

  testExperimentApplied(newWindowRoot);

  extension.sendMessage("reset-theme");
  await extension.awaitMessage("theme-reset");

  for (const rootEl of [root, newWindowRoot]) {
    is(
      rootEl.style.getPropertyValue("--some-color-property"),
      "",
      "Color property should be unset"
    );
    is(
      rootEl.style.getPropertyValue("--some-image-property"),
      "",
      "Image property should be unset"
    );
    is(
      rootEl.style.getPropertyValue("--some-random-property"),
      "",
      "Generic Property should be unset."
    );
  }

  extension.sendMessage("update-theme");
  await extension.awaitMessage("theme-updated");

  testExperimentApplied(root);
  testExperimentApplied(newWindowRoot);

  await extension.unload();

  for (const rootEl of [root, newWindowRoot]) {
    is(
      rootEl.style.getPropertyValue("--some-color-property"),
      "",
      "Color property should be unset"
    );
    is(
      rootEl.style.getPropertyValue("--some-image-property"),
      "",
      "Image property should be unset"
    );
    is(
      rootEl.style.getPropertyValue("--some-random-property"),
      "",
      "Generic Property should be unset."
    );
  }

  await BrowserTestUtils.closeWindow(newWindow);
});

add_task(async function test_experiment_stylesheet() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        colors: {
          menu_button_background: "#ff00ff",
        },
      },
      theme_experiment: {
        stylesheet: "experiment.css",
        colors: {
          menu_button_background: "--menu-button-background",
        },
      },
    },
    files: {
      "experiment.css": `#PanelUI-menu-button {
        background-color: var(--menu-button-background);
        fill: white;
      }`,
    },
  });

  const root = window.document.documentElement;
  const menuButton = document.getElementById("PanelUI-menu-button");
  const computedStyle = window.getComputedStyle(menuButton);
  const expectedColor = hexToCSS("#ff00ff");
  const expectedFill = hexToCSS("#ffffff");

  is(
    root.style.getPropertyValue("--menu-button-background"),
    "",
    "Variable should be unset"
  );
  isnot(
    computedStyle.backgroundColor,
    expectedColor,
    "Menu button should not have custom background"
  );
  isnot(
    computedStyle.fill,
    expectedFill,
    "Menu button should not have stylesheet fill"
  );

  await extension.startup();

  if (AddonSettings.EXPERIMENTS_ENABLED) {
    // Wait for stylesheet load.
    await BrowserTestUtils.waitForCondition(
      () => computedStyle.fill === expectedFill
    );

    is(
      root.style.getPropertyValue("--menu-button-background"),
      expectedColor,
      "Variable should be parsed and set."
    );
    is(
      computedStyle.backgroundColor,
      expectedColor,
      "Menu button should be have correct background"
    );
    is(
      computedStyle.fill,
      expectedFill,
      "Menu button should be have correct fill"
    );
  } else {
    is(
      root.style.getPropertyValue("--menu-button-background"),
      "",
      "Variable should be unset"
    );
    isnot(
      computedStyle.backgroundColor,
      expectedColor,
      "Menu button should not have custom background"
    );
    isnot(
      computedStyle.fill,
      expectedFill,
      "Menu button should not have stylesheet fill"
    );
  }

  await extension.unload();

  is(
    root.style.getPropertyValue("--menu-button-background"),
    "",
    "Variable should be unset"
  );
  isnot(
    computedStyle.backgroundColor,
    expectedColor,
    "Menu button should not have custom background"
  );
  isnot(
    computedStyle.fill,
    expectedFill,
    "Menu button should not have stylesheet fill"
  );
});

add_task(async function cleanup() {
  await SpecialPowers.popPrefEnv();
});
