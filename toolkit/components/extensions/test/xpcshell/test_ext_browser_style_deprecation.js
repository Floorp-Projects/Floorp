"use strict";

AddonTestUtils.init(this);
// This test expects and checks deprecation warnings.
ExtensionTestUtils.failOnSchemaWarnings(false);

const PREF_SUPPORTED = "extensions.browser_style_mv3.supported";
const PREF_SAME_AS_MV2 = "extensions.browser_style_mv3.same_as_mv2";

// Set the prefs to the defaults at the end of the deprecation process.
// TODO bug 1830711: remove these two lines.
Services.prefs.setBoolPref(PREF_SUPPORTED, false);
Services.prefs.setBoolPref(PREF_SAME_AS_MV2, false);

function checkBrowserStyleInManifestKey(extension, key, expected) {
  let actual = extension.extension.manifest[key].browser_style;
  Assert.strictEqual(actual, expected, `Expected browser_style of "${key}"`);
}

const BROWSER_STYLE_MV2_DEFAULTS = "BROWSER_STYLE_MV2_DEFAULTS";
async function checkBrowserStyle({
  manifest_version = 3,
  browser_style_in_manifest = null,
  expected_browser_style,
  expected_warnings,
}) {
  const actionKey = manifest_version === 2 ? "browser_action" : "action";
  // sidebar_action is implemented in browser/ and therefore only available to
  // Firefox desktop and not other toolkit apps such as Firefox for Android,
  // Thunderbird, etc.
  const IS_SIDEBAR_SUPPORTED = AppConstants.MOZ_BUILD_APP === "browser";
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version,
      options_ui: {
        page: "options.html",
        browser_style: browser_style_in_manifest,
      },
      [actionKey]: {
        browser_style: browser_style_in_manifest,
      },
      page_action: {
        browser_style: browser_style_in_manifest,
      },
      sidebar_action: {
        default_panel: "sidebar.html",
        browser_style: browser_style_in_manifest,
      },
    },
  });
  await extension.startup();
  if (expected_browser_style === BROWSER_STYLE_MV2_DEFAULTS) {
    checkBrowserStyleInManifestKey(extension, "options_ui", true);
    checkBrowserStyleInManifestKey(extension, actionKey, false);
    checkBrowserStyleInManifestKey(extension, "page_action", false);
    if (IS_SIDEBAR_SUPPORTED) {
      checkBrowserStyleInManifestKey(extension, "sidebar_action", true);
    }
  } else {
    let value = expected_browser_style;
    checkBrowserStyleInManifestKey(extension, "options_ui", value);
    checkBrowserStyleInManifestKey(extension, actionKey, value);
    checkBrowserStyleInManifestKey(extension, "page_action", value);
    if (IS_SIDEBAR_SUPPORTED) {
      checkBrowserStyleInManifestKey(extension, "sidebar_action", value);
    }
  }
  if (!IS_SIDEBAR_SUPPORTED) {
    expected_warnings = expected_warnings.filter(
      msg => !msg.includes("sidebar_action")
    );
    expected_warnings.unshift(
      `Reading manifest: Warning processing sidebar_action: An unexpected property was found in the WebExtension manifest.`
    );
  }
  const warnings = extension.extension.warnings;
  await extension.unload();
  Assert.deepEqual(
    warnings,
    expected_warnings,
    `Got expected warnings for MV${manifest_version} extension with browser_style:${browser_style_in_manifest}.`
  );
}

async function repeatTestIndependentOfPref_browser_style_same_as_mv2(testFn) {
  for (let same_as_mv2 of [true, false]) {
    await runWithPrefs([[PREF_SAME_AS_MV2, same_as_mv2]], testFn);
  }
}

add_task(async function browser_style_never_deprecated_in_MV2() {
  async function check_browser_style_never_deprecated_in_MV2() {
    await checkBrowserStyle({
      manifest_version: 2,
      browser_style_in_manifest: true,
      expected_browser_style: true,
      expected_warnings: [],
    });
    await checkBrowserStyle({
      manifest_version: 2,
      browser_style_in_manifest: false,
      expected_browser_style: false,
      expected_warnings: [],
    });
    await checkBrowserStyle({
      manifest_version: 2,
      browser_style_in_manifest: null,
      expected_browser_style: "BROWSER_STYLE_MV2_DEFAULTS",
      expected_warnings: [],
    });
  }
  // Regardless of all potential test configurations, browser_style is never
  // deprecated in MV2.
  for (let supported of [true, false]) {
    for (let same_as_mv2 of [true, false]) {
      await runWithPrefs(
        [
          [PREF_SUPPORTED, supported],
          [PREF_SAME_AS_MV2, same_as_mv2],
        ],
        check_browser_style_never_deprecated_in_MV2
      );
    }
  }
});

// Disable browser_style:true - bug 1830711.
add_task(async function unsupported_and_browser_style_true() {
  await checkBrowserStyle({
    manifest_version: 3,
    browser_style_in_manifest: true,
    expected_browser_style: false,
    expected_warnings: [
      // TODO bug 1830712: Update warnings when max_manifest_version:2 is used.
      `Reading manifest: Warning processing options_ui.browser_style: "browser_style:true" is no longer supported in Manifest Version 3.`,
      `Reading manifest: Warning processing action.browser_style: "browser_style:true" is no longer supported in Manifest Version 3.`,
      `Reading manifest: Warning processing page_action.browser_style: "browser_style:true" is no longer supported in Manifest Version 3.`,
      `Reading manifest: Warning processing sidebar_action.browser_style: "browser_style:true" is no longer supported in Manifest Version 3.`,
    ],
  });
});

add_task(async function unsupported_and_browser_style_false() {
  await checkBrowserStyle({
    manifest_version: 3,
    browser_style_in_manifest: false,
    expected_browser_style: false,
    // TODO bug 1830712: Add warnings when max_manifest_version:2 is used.
    expected_warnings: [],
  });
});

add_task(async function unsupported_and_browser_style_default() {
  await checkBrowserStyle({
    manifest_version: 3,
    browser_style_in_manifest: null,
    expected_browser_style: false,
    expected_warnings: [],
  });
});

add_task(
  { pref_set: [[PREF_SUPPORTED, true]] },
  async function supported_with_browser_style_true() {
    await repeatTestIndependentOfPref_browser_style_same_as_mv2(async () => {
      await checkBrowserStyle({
        manifest_version: 3,
        browser_style_in_manifest: true,
        expected_browser_style: true,
        expected_warnings: [
          `Reading manifest: Warning processing options_ui.browser_style: "browser_style:true" has been deprecated in Manifest Version 3 and will be unsupported in the near future.`,
          `Reading manifest: Warning processing action.browser_style: "browser_style:true" has been deprecated in Manifest Version 3 and will be unsupported in the near future.`,
          `Reading manifest: Warning processing page_action.browser_style: "browser_style:true" has been deprecated in Manifest Version 3 and will be unsupported in the near future.`,
          `Reading manifest: Warning processing sidebar_action.browser_style: "browser_style:true" has been deprecated in Manifest Version 3 and will be unsupported in the near future.`,
        ],
      });
    });
  }
);

add_task(
  { pref_set: [[PREF_SUPPORTED, true]] },
  async function supported_with_browser_style_false() {
    await repeatTestIndependentOfPref_browser_style_same_as_mv2(async () => {
      await checkBrowserStyle({
        manifest_version: 3,
        browser_style_in_manifest: false,
        expected_browser_style: false,
        expected_warnings: [],
      });
    });
  }
);

// Initial prefs - warn only - https://bugzilla.mozilla.org/show_bug.cgi?id=1827910#c1
add_task(
  {
    pref_set: [
      [PREF_SUPPORTED, true],
      [PREF_SAME_AS_MV2, true],
    ],
  },
  async function supported_with_mv2_defaults() {
    const makeWarning = manifestKey =>
      `Reading manifest: Warning processing ${manifestKey}.browser_style: "browser_style:true" has been deprecated in Manifest Version 3 and will be unsupported in the near future. While "${manifestKey}.browser_style" was not explicitly specified in manifest.json, its default value was true. Its default will change to false in Manifest Version 3 starting from Firefox 115.`;
    await checkBrowserStyle({
      manifest_version: 3,
      browser_style_in_manifest: null,
      expected_browser_style: "BROWSER_STYLE_MV2_DEFAULTS",
      expected_warnings: [
        makeWarning("options_ui"),
        makeWarning("sidebar_action"),
      ],
    });
  }
);

// Deprecation + change defaults - bug 1830710.
add_task(
  {
    pref_set: [
      [PREF_SUPPORTED, true],
      [PREF_SAME_AS_MV2, false],
    ],
  },
  async function supported_with_browser_style_default_false() {
    const makeWarning = manifestKey =>
      `Reading manifest: Warning processing ${manifestKey}.browser_style: "browser_style:true" has been deprecated in Manifest Version 3 and will be unsupported in the near future. While "${manifestKey}.browser_style" was not explicitly specified in manifest.json, its default value was true. The default value of "${manifestKey}.browser_style" has changed from true to false in Manifest Version 3.`;
    await checkBrowserStyle({
      manifest_version: 3,
      browser_style_in_manifest: null,
      expected_browser_style: false,
      expected_warnings: [
        makeWarning("options_ui"),
        makeWarning("sidebar_action"),
      ],
    });
  }
);

// While we are not planning to set this pref combination, users can do so if
// they desire.
add_task(
  {
    pref_set: [
      [PREF_SUPPORTED, false],
      [PREF_SAME_AS_MV2, true],
    ],
  },
  async function unsupported_with_mv2_defaults() {
    const makeWarning = manifestKey =>
      `Reading manifest: Warning processing ${manifestKey}.browser_style: "browser_style:true" is no longer supported in Manifest Version 3. While "${manifestKey}.browser_style" was not explicitly specified in manifest.json, its default value was true. Its default will change to false in Manifest Version 3 starting from Firefox 115.`;
    await checkBrowserStyle({
      manifest_version: 3,
      browser_style_in_manifest: null,
      expected_browser_style: false,
      expected_warnings: [
        makeWarning("options_ui"),
        makeWarning("sidebar_action"),
      ],
    });
  }
);
