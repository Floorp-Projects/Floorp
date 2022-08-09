"use strict";

let { ExtensionTestCommon } = ChromeUtils.import(
  "resource://testing-common/ExtensionTestCommon.jsm"
);

let bundle;
if (AppConstants.MOZ_APP_NAME == "thunderbird") {
  bundle = Services.strings.createBundle(
    "chrome://messenger/locale/addons.properties"
  );
} else {
  // For Android, these strings are only used in tests. In the actual UI, the
  // warnings are in Android-Components, as explained in bug 1671453.
  bundle = Services.strings.createBundle(
    "chrome://browser/locale/browser.properties"
  );
}
const DUMMY_APP_NAME = "Dummy brandName";

// nativeMessaging is in PRIVILEGED_PERMS on Android.
const IS_NATIVE_MESSAGING_PRIVILEGED = AppConstants.platform == "android";

const { createAppInfo } = AddonTestUtils;

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.usePrivilegedSignatures = id => id.startsWith("privileged");
createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);

async function getManifestPermissions(extensionData) {
  let extension = ExtensionTestCommon.generate(extensionData);
  // Some tests contain invalid permissions; ignore the warnings about their invalidity.
  ExtensionTestUtils.failOnSchemaWarnings(false);
  await extension.loadManifest();
  ExtensionTestUtils.failOnSchemaWarnings(true);
  let result = extension.manifestPermissions;

  if (extension.manifest.manifest_version >= 3) {
    // In MV3, host permissions are optional by default.
    deepEqual(result.origins, [], "No origins by default in MV3");
    let optional = extension.manifestOptionalPermissions;
    deepEqual(optional.permissions, [], "No tests use optional_permissions");
    result.origins = optional.origins;
  }

  await extension.cleanupGeneratedFile();
  return result;
}

function getPermissionWarnings(
  manifestPermissions,
  options,
  stringBundle = bundle
) {
  let info = {
    permissions: manifestPermissions,
    appName: DUMMY_APP_NAME,
  };
  let { msgs } = ExtensionData.formatPermissionStrings(
    info,
    stringBundle,
    options
  );
  return msgs;
}

async function getPermissionWarningsForUpdate(
  oldExtensionData,
  newExtensionData
) {
  let oldPerms = await getManifestPermissions(oldExtensionData);
  let newPerms = await getManifestPermissions(newExtensionData);
  let difference = Extension.comparePermissions(oldPerms, newPerms);
  return getPermissionWarnings(difference);
}

// Tests that the callers of ExtensionData.formatPermissionStrings can customize the
// mapping between the permission names and related localized strings.
add_task(async function customized_permission_keys_mapping() {
  const mockBundle = {
    // Mocked nsIStringBundle getStringFromName to returns a fake localized string.
    GetStringFromName: key => `Fake localized ${key}`,
    formatStringFromName: (name, params) => "Fake formatted string",
  };

  // Define a non-default mapping for permission names -> locale keys.
  const getKeyForPermission = perm => `customWebExtPerms.description.${perm}`;

  const manifest = {
    permissions: ["downloads", "proxy"],
  };
  const expectedWarnings = manifest.permissions.map(k =>
    mockBundle.GetStringFromName(getKeyForPermission(k))
  );
  const manifestPermissions = await getManifestPermissions({ manifest });

  // Pass the callback function for the non-default key mapping to
  // ExtensionData.formatPermissionStrings() and verify it being used.
  const warnings = getPermissionWarnings(
    manifestPermissions,
    { getKeyForPermission },
    mockBundle
  );
  deepEqual(
    warnings,
    expectedWarnings,
    "Got the expected string from customized permission mapping"
  );
});

// Tests that the expected permission warnings are generated for various
// combinations of host permissions.
add_task(async function host_permissions() {
  let { PluralForm } = ChromeUtils.import(
    "resource://gre/modules/PluralForm.jsm"
  );

  let permissionTestCases = [
    {
      description: "Empty manifest without permissions",
      manifest: {},
      expectedOrigins: [],
      expectedWarnings: [],
    },
    {
      description: "Invalid match patterns",
      manifest: {
        permissions: [
          "https:///",
          "https://",
          "https://*",
          "about:ugh",
          "about:*",
          "about://*/",
          "resource://*/",
        ],
      },
      expectedOrigins: [],
      expectedWarnings: [],
    },
    {
      description: "moz-extension: permissions",
      manifest: {
        permissions: ["moz-extension://*/*", "moz-extension://uuid/"],
      },
      // moz-extension:-origin does not appear in the permission list,
      // but it is implicitly granted anyway.
      expectedOrigins: [],
      expectedWarnings: [],
    },
    {
      description: "*. host permission",
      manifest: {
        // This permission is rejected by the manifest and ignored.
        permissions: ["http://*./"],
      },
      expectedOrigins: [],
      expectedWarnings: [],
    },
    {
      description: "<all_urls> permission",
      manifest: {
        permissions: ["<all_urls>"],
      },
      expectedOrigins: ["<all_urls>"],
      expectedWarnings: [
        bundle.GetStringFromName("webextPerms.hostDescription.allUrls"),
      ],
    },
    {
      description: "file: permissions",
      manifest: {
        permissions: ["file://*/"],
      },
      expectedOrigins: ["file://*/"],
      expectedWarnings: [
        bundle.GetStringFromName("webextPerms.hostDescription.allUrls"),
      ],
    },
    {
      description: "http: permission",
      manifest: {
        permissions: ["http://*/"],
      },
      expectedOrigins: ["http://*/"],
      expectedWarnings: [
        bundle.GetStringFromName("webextPerms.hostDescription.allUrls"),
      ],
    },
    {
      description: "*://*/ permission",
      manifest: {
        permissions: ["*://*/"],
      },
      expectedOrigins: ["*://*/"],
      expectedWarnings: [
        bundle.GetStringFromName("webextPerms.hostDescription.allUrls"),
      ],
    },
    {
      description: "content_script[*].matches",
      manifest: {
        content_scripts: [
          {
            // This test uses the manifest file without loading the content script
            // file, so we can use a non-existing dummy file.
            js: ["dummy.js"],
            matches: ["https://*/"],
          },
        ],
      },
      expectedOrigins: ["https://*/"],
      expectedWarnings: [
        bundle.GetStringFromName("webextPerms.hostDescription.allUrls"),
      ],
    },
    {
      description: "A few host permissions",
      manifest: {
        permissions: ["http://a/", "http://*.b/", "http://c/*"],
      },
      expectedOrigins: ["http://a/", "http://*.b/", "http://c/*"],
      expectedWarnings: [
        // Wildcard hosts take precedence in the permission list.
        bundle.formatStringFromName("webextPerms.hostDescription.wildcard", [
          "b",
        ]),
        bundle.formatStringFromName("webextPerms.hostDescription.oneSite", [
          "a",
        ]),
        bundle.formatStringFromName("webextPerms.hostDescription.oneSite", [
          "c",
        ]),
      ],
    },
    {
      description: "many host permission",
      manifest: {
        permissions: [
          "http://a/",
          "http://b/",
          "http://c/",
          "http://d/",
          "http://e/*",
          "http://*.1/",
          "http://*.2/",
          "http://*.3/",
          "http://*.4/",
        ],
      },
      expectedOrigins: [
        "http://a/",
        "http://b/",
        "http://c/",
        "http://d/",
        "http://e/*",
        "http://*.1/",
        "http://*.2/",
        "http://*.3/",
        "http://*.4/",
      ],
      expectedWarnings: [
        // Wildcard hosts take precedence in the permission list.
        bundle.formatStringFromName("webextPerms.hostDescription.wildcard", [
          "1",
        ]),
        bundle.formatStringFromName("webextPerms.hostDescription.wildcard", [
          "2",
        ]),
        bundle.formatStringFromName("webextPerms.hostDescription.wildcard", [
          "3",
        ]),
        bundle.formatStringFromName("webextPerms.hostDescription.wildcard", [
          "4",
        ]),
        bundle.formatStringFromName("webextPerms.hostDescription.oneSite", [
          "a",
        ]),
        bundle.formatStringFromName("webextPerms.hostDescription.oneSite", [
          "b",
        ]),
        bundle.formatStringFromName("webextPerms.hostDescription.oneSite", [
          "c",
        ]),
        PluralForm.get(
          2,
          bundle.GetStringFromName("webextPerms.hostDescription.tooManySites")
        ).replace("#1", "2"),
      ],
      options: {
        collapseOrigins: true,
      },
    },
    {
      description:
        "many host permissions without item limit in the warning list",
      manifest: {
        permissions: [
          "http://a/",
          "http://b/",
          "http://c/",
          "http://d/",
          "http://e/*",
          "http://*.1/",
          "http://*.2/",
          "http://*.3/",
          "http://*.4/",
          "http://*.5/",
        ],
      },
      expectedOrigins: [
        "http://a/",
        "http://b/",
        "http://c/",
        "http://d/",
        "http://e/*",
        "http://*.1/",
        "http://*.2/",
        "http://*.3/",
        "http://*.4/",
        "http://*.5/",
      ],
      expectedWarnings: [
        bundle.formatStringFromName("webextPerms.hostDescription.wildcard", [
          "1",
        ]),
        bundle.formatStringFromName("webextPerms.hostDescription.wildcard", [
          "2",
        ]),
        bundle.formatStringFromName("webextPerms.hostDescription.wildcard", [
          "3",
        ]),
        bundle.formatStringFromName("webextPerms.hostDescription.wildcard", [
          "4",
        ]),
        bundle.formatStringFromName("webextPerms.hostDescription.wildcard", [
          "5",
        ]),
        bundle.formatStringFromName("webextPerms.hostDescription.oneSite", [
          "a",
        ]),
        bundle.formatStringFromName("webextPerms.hostDescription.oneSite", [
          "b",
        ]),
        bundle.formatStringFromName("webextPerms.hostDescription.oneSite", [
          "c",
        ]),
        bundle.formatStringFromName("webextPerms.hostDescription.oneSite", [
          "d",
        ]),
        bundle.formatStringFromName("webextPerms.hostDescription.oneSite", [
          "e",
        ]),
      ],
    },
  ];
  for (let manifest_version of [2, 3]) {
    for (let {
      description,
      manifest,
      expectedOrigins,
      expectedWarnings,
      options,
    } of permissionTestCases) {
      manifest = Object.assign({}, manifest, { manifest_version });
      if (manifest_version > 2) {
        manifest.host_permissions = manifest.permissions;
        manifest.permissions = [];
      }

      let manifestPermissions = await getManifestPermissions({ manifest });

      deepEqual(
        manifestPermissions.origins,
        expectedOrigins,
        `Expected origins (${description})`
      );
      deepEqual(
        manifestPermissions.permissions,
        [],
        `Expected no non-host permissions (${description})`
      );

      let warnings = getPermissionWarnings(manifestPermissions, options);
      deepEqual(
        warnings,
        expectedWarnings,
        `Expected warnings (${description})`
      );
    }
  }
});

// Tests that the expected permission warnings are generated for a mix of host
// permissions and API permissions.
add_task(async function api_permissions() {
  let manifestPermissions = await getManifestPermissions({
    isPrivileged: IS_NATIVE_MESSAGING_PRIVILEGED,
    manifest: {
      permissions: [
        "activeTab",
        "webNavigation",
        "tabs",
        "nativeMessaging",
        "http://x/",
        "http://*.x/",
        "http://*.tld/",
      ],
    },
  });

  deepEqual(
    manifestPermissions,
    {
      origins: ["http://x/", "http://*.x/", "http://*.tld/"],
      permissions: ["activeTab", "webNavigation", "tabs", "nativeMessaging"],
    },
    "Expected origins and permissions"
  );

  deepEqual(
    getPermissionWarnings(manifestPermissions),
    [
      // Host permissions first, with wildcards on top.
      bundle.formatStringFromName("webextPerms.hostDescription.wildcard", [
        "x",
      ]),
      bundle.formatStringFromName("webextPerms.hostDescription.wildcard", [
        "tld",
      ]),
      bundle.formatStringFromName("webextPerms.hostDescription.oneSite", ["x"]),
      // nativeMessaging permission warning first of all permissions.
      bundle.formatStringFromName("webextPerms.description.nativeMessaging", [
        DUMMY_APP_NAME,
      ]),
      // Other permissions in alphabetical order.
      // Note: activeTab has no permission warning string.
      bundle.GetStringFromName("webextPerms.description.tabs"),
      bundle.GetStringFromName("webextPerms.description.webNavigation"),
    ],
    "Expected warnings"
  );
});

add_task(async function nativeMessaging_permission() {
  let manifestPermissions = await getManifestPermissions({
    // isPrivileged: false, by default.
    manifest: {
      permissions: ["nativeMessaging"],
    },
  });

  if (IS_NATIVE_MESSAGING_PRIVILEGED) {
    // The behavior of nativeMessaging for unprivileged extensions on Android
    // is covered in
    // mobile/android/components/extensions/test/xpcshell/test_ext_native_messaging_permissions.js
    deepEqual(
      manifestPermissions,
      { origins: [], permissions: [] },
      "nativeMessaging perm ignored for unprivileged extensions on Android"
    );
  } else {
    deepEqual(
      manifestPermissions,
      { origins: [], permissions: ["nativeMessaging"] },
      "nativeMessaging permission recognized for unprivileged extensions"
    );
  }
});

add_task(async function declarativeNetRequest_unavailable_by_default() {
  let manifestPermissions = await getManifestPermissions({
    manifest: {
      manifest_version: 3,
      permissions: ["declarativeNetRequest"],
    },
  });
  deepEqual(
    manifestPermissions,
    { origins: [], permissions: [] },
    "Expected declarativeNetRequest permission to be ignored/stripped"
  );
});

add_task(
  { pref_set: [["extensions.dnr.enabled", true]] },
  async function declarativeNetRequest_permission_with_warning() {
    let manifestPermissions = await getManifestPermissions({
      manifest: {
        manifest_version: 3,
        permissions: ["declarativeNetRequest"],
      },
    });

    deepEqual(
      manifestPermissions,
      { origins: [], permissions: ["declarativeNetRequest"] },
      "Expected origins and permissions"
    );

    deepEqual(
      getPermissionWarnings(manifestPermissions),
      [
        bundle.GetStringFromName(
          "webextPerms.description.declarativeNetRequest"
        ),
      ],
      "Expected warnings"
    );
  }
);

add_task(
  { pref_set: [["extensions.dnr.enabled", true]] },
  async function declarativeNetRequest_permission_without_warning() {
    let manifestPermissions = await getManifestPermissions({
      manifest: {
        manifest_version: 3,
        permissions: ["declarativeNetRequestWithHostAccess"],
      },
    });

    deepEqual(
      manifestPermissions,
      { origins: [], permissions: ["declarativeNetRequestWithHostAccess"] },
      "Expected origins and permissions"
    );

    deepEqual(getPermissionWarnings(manifestPermissions), [], "No warnings");
  }
);

// Tests that the expected permission warnings are generated for a mix of host
// permissions and API permissions, for a privileged extension that uses the
// mozillaAddons permission.
add_task(async function privileged_with_mozillaAddons() {
  let manifestPermissions = await getManifestPermissions({
    isPrivileged: true,
    manifest: {
      permissions: [
        "mozillaAddons",
        "mozillaAddons",
        "mozillaAddons",
        "resource://x/*",
        "http://a/",
        "about:reader*",
      ],
    },
  });
  deepEqual(
    manifestPermissions,
    {
      origins: ["resource://x/*", "http://a/", "about:reader*"],
      permissions: ["mozillaAddons"],
    },
    "Expected origins and permissions for privileged add-on with mozillaAddons"
  );

  deepEqual(
    getPermissionWarnings(manifestPermissions),
    [bundle.GetStringFromName("webextPerms.hostDescription.allUrls")],
    "Expected warnings for privileged add-on with mozillaAddons permission."
  );
});

// Similar to the privileged_with_mozillaAddons test, except the test extension
// is unprivileged and not allowed to use the mozillaAddons permission.
add_task(async function unprivileged_with_mozillaAddons() {
  let manifestPermissions = await getManifestPermissions({
    manifest: {
      permissions: [
        "mozillaAddons",
        "mozillaAddons",
        "mozillaAddons",
        "resource://x/*",
        "http://a/",
        "about:reader*",
      ],
    },
  });
  deepEqual(
    manifestPermissions,
    {
      origins: ["http://a/"],
      permissions: [],
    },
    "Expected origins and permissions for unprivileged add-on with mozillaAddons"
  );

  deepEqual(
    getPermissionWarnings(manifestPermissions),
    [bundle.formatStringFromName("webextPerms.hostDescription.oneSite", ["a"])],
    "Expected warnings for unprivileged add-on with mozillaAddons permission."
  );
});

// Tests that an update with less permissions has no warning.
add_task(async function update_drop_permission() {
  let warnings = await getPermissionWarningsForUpdate(
    {
      manifest: {
        permissions: ["<all_urls>", "https://a/", "http://b/"],
      },
    },
    {
      manifest: {
        permissions: [
          "https://a/",
          "http://b/",
          "ftp://host_matching_all_urls/",
        ],
      },
    }
  );
  deepEqual(
    warnings,
    [],
    "An update with fewer permissions should not have any warnings"
  );
});

// Tests that an update that switches from "*://*/*" to "<all_urls>" does not
// result in additional permission warnings.
add_task(async function update_all_urls_permission() {
  let warnings = await getPermissionWarningsForUpdate(
    {
      manifest: {
        permissions: ["*://*/*"],
      },
    },
    {
      manifest: {
        permissions: ["<all_urls>"],
      },
    }
  );
  deepEqual(
    warnings,
    [],
    "An update from a wildcard host to <all_urls> should not have any warnings"
  );
});

// Tests that an update where a new permission whose domain overlaps with
// an existing permission does not result in additional permission warnings.
add_task(async function update_change_permissions() {
  let warnings = await getPermissionWarningsForUpdate(
    {
      manifest: {
        permissions: ["https://a/", "http://*.b/", "http://c/", "http://f/"],
      },
    },
    {
      manifest: {
        permissions: [
          // (no new warning) Unchanged permission from old extension.
          "https://a/",
          // (no new warning) Different schemes, host should match "*.b" wildcard.
          "ftp://ftp.b/",
          "ws://ws.b/",
          "wss://wss.b",
          "https://https.b/",
          "http://http.b/",
          "*://*.b/",
          "http://b/",

          // (expect warning) Wildcard was added.
          "http://*.c/",
          // (no new warning) file:-scheme, but host "f" is same as "http://f/".
          "file://f/",
          // (expect warning) New permission was added.
          "proxy",
        ],
      },
    }
  );
  deepEqual(
    warnings,
    [
      bundle.formatStringFromName("webextPerms.hostDescription.wildcard", [
        "c",
      ]),
      bundle.formatStringFromName("webextPerms.description.proxy", [
        DUMMY_APP_NAME,
      ]),
    ],
    "Expected permission warnings for new permissions only"
  );
});

// Tests that a privileged extension with the mozillaAddons permission can be
// updated without errors.
add_task(async function update_privileged_with_mozillaAddons() {
  let warnings = await getPermissionWarningsForUpdate(
    {
      isPrivileged: true,
      manifest: {
        permissions: ["mozillaAddons", "resource://a/"],
      },
    },
    {
      isPrivileged: true,
      manifest: {
        permissions: ["mozillaAddons", "resource://a/", "resource://b/"],
      },
    }
  );
  deepEqual(
    warnings,
    [bundle.formatStringFromName("webextPerms.hostDescription.oneSite", ["b"])],
    "Expected permission warnings for new host only"
  );
});

// Tests that an unprivileged extension cannot get privileged permissions
// through an update.
add_task(async function update_unprivileged_with_mozillaAddons() {
  // Unprivileged
  let warnings = await getPermissionWarningsForUpdate(
    {
      manifest: {
        permissions: ["mozillaAddons", "resource://a/"],
      },
    },
    {
      manifest: {
        permissions: ["mozillaAddons", "resource://a/", "resource://b/"],
      },
    }
  );
  deepEqual(
    warnings,
    [],
    "resource:-scheme is unsupported for unprivileged extensions"
  );
});

// Tests that invalid permission warning for privileged permissions requested
// are not emitted for privileged extensions, only for unprivileged extensions.
add_task(
  async function test_invalid_permission_warning_on_privileged_permission() {
    await AddonTestUtils.promiseStartupManager();

    const MANIFEST_WARNINGS = [
      "Reading manifest: Invalid extension permission: mozillaAddons",
      "Reading manifest: Invalid extension permission: resource://x/",
      "Reading manifest: Invalid extension permission: about:reader*",
    ];

    async function testInvalidPermissionWarning({ isPrivileged }) {
      let id = isPrivileged
        ? "privileged-addon@mochi.test"
        : "nonprivileged-addon@mochi.test";

      let expectedWarnings = isPrivileged ? [] : MANIFEST_WARNINGS;

      const ext = ExtensionTestUtils.loadExtension({
        useAddonManager: "permanent",
        manifest: {
          permissions: ["mozillaAddons", "resource://x/", "about:reader*"],
          applications: { gecko: { id } },
        },
        background() {},
      });

      await ext.startup();
      const { warnings } = ext.extension;
      Assert.deepEqual(
        warnings,
        expectedWarnings,
        `Got the expected warning for ${id}`
      );
      await ext.unload();
    }

    await testInvalidPermissionWarning({ isPrivileged: false });
    await testInvalidPermissionWarning({ isPrivileged: true });

    info("Test invalid permission warning on ExtensionData instance");
    // Generate an extension (just to be able to reuse its rootURI for the
    // ExtensionData instance created below).
    let generatedExt = ExtensionTestCommon.generate({
      manifest: {
        permissions: ["mozillaAddons", "resource://x/", "about:reader*"],
        applications: { gecko: { id: "extension-data@mochi.test" } },
      },
    });

    // Verify that XPIInstall.jsm will not collect the warning for the
    // privileged permission as expected.
    async function getWarningsFromExtensionData({ isPrivileged }) {
      let extData;
      if (typeof isPrivileged == "function") {
        // isPrivileged expected to be computed asynchronously.
        extData = await ExtensionData.constructAsync({
          rootURI: generatedExt.rootURI,
          checkPrivileged: isPrivileged,
        });
      } else {
        extData = new ExtensionData(generatedExt.rootURI, isPrivileged);
      }
      await extData.loadManifest();

      // This assertion is just meant to prevent the test to pass if there were
      // no warnings because some errors prevented the warnings to be
      // collected).
      Assert.deepEqual(
        extData.errors,
        [],
        "No errors collected by the ExtensionData instance"
      );
      return extData.warnings;
    }

    Assert.deepEqual(
      await getWarningsFromExtensionData({ isPrivileged: undefined }),
      MANIFEST_WARNINGS,
      "Got warnings about privileged permissions by default"
    );

    Assert.deepEqual(
      await getWarningsFromExtensionData({ isPrivileged: false }),
      MANIFEST_WARNINGS,
      "Got warnings about privileged permissions for non-privileged extensions"
    );

    Assert.deepEqual(
      await getWarningsFromExtensionData({ isPrivileged: true }),
      [],
      "No warnings about privileged permissions on privileged extensions"
    );

    Assert.deepEqual(
      await getWarningsFromExtensionData({ isPrivileged: async () => false }),
      MANIFEST_WARNINGS,
      "Got warnings about privileged permissions for non-privileged extensions (async)"
    );

    Assert.deepEqual(
      await getWarningsFromExtensionData({ isPrivileged: async () => true }),
      [],
      "No warnings about privileged permissions on privileged extensions (async)"
    );

    // Cleanup the generated xpi file.
    await generatedExt.cleanupGeneratedFile();

    await AddonTestUtils.promiseShutdownManager();
  }
);
