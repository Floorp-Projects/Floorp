/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.usePrivilegedSignatures = id => id.startsWith("privileged");
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

add_task(async function setup() {
  await ExtensionTestUtils.startAddonManager();
});

// This test checks whether the theme experiments work for privileged static themes
// and are ignored for unprivileged static themes.
async function test_experiment_static_theme({ privileged }) {
  let extensionManifest = {
    theme: {
      colors: {},
      images: {},
      properties: {},
    },
    theme_experiment: {
      colors: {},
      images: {},
      properties: {},
    },
  };

  const addonId = `${
    privileged ? "privileged" : "unprivileged"
  }-static-theme@test-extension`;
  const themeFiles = {
    "manifest.json": {
      name: "test theme",
      version: "1.0",
      manifest_version: 2,
      browser_specific_settings: {
        gecko: { id: addonId },
      },
      ...extensionManifest,
    },
  };

  const promiseThemeUpdated = TestUtils.topicObserved(
    "lightweight-theme-styling-update"
  );

  let themeAddon;
  const { messages } = await AddonTestUtils.promiseConsoleOutput(async () => {
    let { addon } = await AddonTestUtils.promiseInstallXPI(themeFiles);
    // Enable the newly installed static theme.
    await addon.enable();
    themeAddon = addon;
  });

  const themeExperimentNotAllowed = {
    message: /This extension is not allowed to run theme experiments/,
  };
  AddonTestUtils.checkMessages(messages, {
    forbidden: privileged ? [themeExperimentNotAllowed] : [],
    expected: privileged ? [] : [themeExperimentNotAllowed],
  });

  if (privileged) {
    // ext-theme.js Theme class constructor doesn't call Theme.prototype.load
    // if the static theme includes theme_experiment but isn't allowed to.
    info("Wait for theme updated observer service topic to be notified");
    const [topicSubject] = await promiseThemeUpdated;
    let themeData = topicSubject.wrappedJSObject;
    ok(
      themeData.experiment,
      "Expect theme experiment property to be defined in theme update data"
    );
  }

  const policy = WebExtensionPolicy.getByID(themeAddon.id);
  equal(
    policy.extension.isPrivileged,
    privileged,
    `The static theme should be ${privileged ? "privileged" : "unprivileged"}`
  );

  await themeAddon.uninstall();
}

add_task(function test_privileged_theme() {
  return test_experiment_static_theme({ privileged: true });
});

add_task(
  {
    // Some builds (e.g. thunderbird) have experiments enabled by default.
    pref_set: [["extensions.experiments.enabled", false]],
  },
  function test_unprivileged_theme() {
    return test_experiment_static_theme({ privileged: false });
  }
);
