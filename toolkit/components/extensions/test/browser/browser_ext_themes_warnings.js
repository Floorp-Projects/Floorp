"use strict";

// This test checks that theme warnings are properly emitted.

function waitForConsole(task, message) {
  // eslint-disable-next-line no-async-promise-executor
  return new Promise(async resolve => {
    SimpleTest.monitorConsole(resolve, [
      {
        message: new RegExp(message),
      },
    ]);
    await task();
    SimpleTest.endMonitorConsole();
  });
}

add_task(async function setup() {
  SimpleTest.waitForExplicitFinish();
});

add_task(async function test_static_theme() {
  for (const property of ["colors", "images", "properties"]) {
    const extension = ExtensionTestUtils.loadExtension({
      manifest: {
        theme: {
          [property]: {
            such_property: "much_wow",
          },
        },
      },
    });
    await waitForConsole(
      extension.startup,
      `Unrecognized theme property found: ${property}.such_property`
    );
    await extension.unload();
  }
});

add_task(async function test_dynamic_theme() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["theme"],
    },
    background() {
      browser.test.onMessage.addListener((msg, details) => {
        if (msg === "update-theme") {
          browser.theme.update(details).then(() => {
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

  for (const property of ["colors", "images", "properties"]) {
    extension.sendMessage("update-theme", {
      [property]: {
        such_property: "much_wow",
      },
    });
    await waitForConsole(
      () => extension.awaitMessage("theme-updated"),
      `Unrecognized theme property found: ${property}.such_property`
    );
  }

  await extension.unload();
});

add_task(async function test_experiments_enabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.legacy.enabled", AppConstants.MOZ_ALLOW_LEGACY_EXTENSIONS],
    ],
  });

  info(
    "Testing that experiments are handled correctly when legacy pref is enabled"
  );

  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        properties: {
          such_property: "much_wow",
          unknown_property: "very_unknown",
        },
      },
      theme_experiment: {
        properties: {
          such_property: "--such-property",
        },
      },
    },
  });
  if (!AppConstants.MOZ_ALLOW_LEGACY_EXTENSIONS) {
    await waitForConsole(
      extension.startup,
      "This extension is not allowed to run theme experiments"
    );
  } else {
    await waitForConsole(
      extension.startup,
      "Unrecognized theme property found: properties.unknown_property"
    );
  }
  await extension.unload();

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_experiments_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.legacy.enabled", false]],
  });

  info(
    "Testing that experiments are handled correctly when legacy pref is disabled"
  );

  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        properties: {
          such_property: "much_wow",
        },
      },
      theme_experiment: {
        properties: {
          such_property: "--such-property",
        },
      },
    },
  });
  await waitForConsole(
    extension.startup,
    "This extension is not allowed to run theme experiments"
  );
  await extension.unload();
  await SpecialPowers.popPrefEnv();
});
