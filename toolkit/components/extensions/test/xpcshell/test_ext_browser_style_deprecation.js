"use strict";

AddonTestUtils.init(this);
// This test expects and checks deprecation warnings.
ExtensionTestUtils.failOnSchemaWarnings(false);

const PREF_SUPPORTED = "extensions.browser_style_mv3.supported";
const PREF_SAME_AS_MV2 = "extensions.browser_style_mv3.same_as_mv2";

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

async function checkBrowserStyleWithOpenInTabTrue({
  manifest_version = 3,
  browser_style_in_manifest = null,
  expected_browser_style,
}) {
  info(
    `Testing options_ui.open_in_tab=true + browser_style=${browser_style_in_manifest} for MV${manifest_version} extension`
  );
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version,
      options_ui: {
        page: "options.html",
        browser_style: browser_style_in_manifest,
        open_in_tab: true,
      },
    },
  });
  await extension.startup();
  checkBrowserStyleInManifestKey(
    extension,
    "options_ui",
    expected_browser_style
  );
  const warnings = extension.extension.warnings;
  await extension.unload();
  Assert.deepEqual(
    warnings,
    [],
    "Expected no warnings on extension with options_ui.open_in_tab true"
  );
}

async function repeatTestIndependentOfPref_browser_style_same_as_mv2(testFn) {
  for (let same_as_mv2 of [true, false]) {
    await runWithPrefs([[PREF_SAME_AS_MV2, same_as_mv2]], testFn);
  }
}
async function repeatTestIndependentOf_browser_style_deprecation_prefs(testFn) {
  for (let supported of [true, false]) {
    for (let same_as_mv2 of [true, false]) {
      await runWithPrefs(
        [
          [PREF_SUPPORTED, supported],
          [PREF_SAME_AS_MV2, same_as_mv2],
        ],
        testFn
      );
    }
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

    // When open_in_tab is true, browser_style is not used and its value does
    // not matter. Since we want the parsed value to be false in MV3, and the
    // implementation is simpler if consistently applied to MV2, browser_style
    // is false when open_in_tab is true (even if browser_style:true is set).
    await checkBrowserStyleWithOpenInTabTrue({
      manifest_version: 2,
      browser_style_in_manifest: null,
      expected_browser_style: false,
    });
    await checkBrowserStyleWithOpenInTabTrue({
      manifest_version: 2,
      browser_style_in_manifest: true,
      expected_browser_style: false,
    });
  }
  // Regardless of all potential test configurations, browser_style is never
  // deprecated in MV2.
  await repeatTestIndependentOf_browser_style_deprecation_prefs(
    check_browser_style_never_deprecated_in_MV2
  );
});

add_task(async function open_in_tab_implies_browser_style_false_MV3() {
  // Regardless of all potential test configurations, when
  // options_ui.open_in_tab is true, options_ui.browser_style should be false,
  // because it being true would print deprecation warnings in MV3, and
  // browser_style:true does not have any effect when open_in_tab is true.
  await repeatTestIndependentOf_browser_style_deprecation_prefs(async () => {
    await checkBrowserStyleWithOpenInTabTrue({
      manifest_version: 3,
      browser_style_in_manifest: null,
      expected_browser_style: false,
    });
    await checkBrowserStyleWithOpenInTabTrue({
      manifest_version: 3,
      browser_style_in_manifest: true,
      expected_browser_style: false,
    });
  });
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
