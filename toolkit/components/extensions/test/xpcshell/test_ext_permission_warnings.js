"use strict";

let {ExtensionTestCommon} = ChromeUtils.import("resource://testing-common/ExtensionTestCommon.jsm", {});

const bundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");
const DUMMY_APP_NAME = "Dummy brandName";

async function getManifestPermissions(extensionData) {
  let extension = ExtensionTestCommon.generate(extensionData);
  await extension.loadManifest();
  return extension.manifestPermissions;
}

function getPermissionWarnings(manifestPermissions) {
  let info = {
    permissions: manifestPermissions,
    appName: DUMMY_APP_NAME,
  };
  let {msgs} = ExtensionData.formatPermissionStrings(info, bundle);
  return msgs;
}

// Tests that the expected permission warnings are generated for various
// combinations of host permissions.
add_task(async function host_permissions() {
  let {PluralForm} = ChromeUtils.import("resource://gre/modules/PluralForm.jsm", {});

  let permissionTestCases = [{
    description: "Empty manifest without permissions",
    manifest: {},
    expectedOrigins: [],
    expectedWarnings: [],
  }, {
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
  }, {
    description: "moz-extension: permissions",
    manifest: {
      permissions: ["moz-extension://*/*", "moz-extension://uuid/"],
    },
    // moz-extension:-origin does not appear in the permission list,
    // but it is implicitly granted anyway.
    expectedOrigins: [],
    expectedWarnings: [],
  }, {
    description: "*. host permission",
    manifest: {
      // This permission is rejected by the manifest and ignored.
      permissions: ["http://*./"],
    },
    expectedOrigins: [],
    expectedWarnings: [],
  }, {
    description: "<all_urls> permission",
    manifest: {
      permissions: ["<all_urls>"],
    },
    expectedOrigins: ["<all_urls>"],
    expectedWarnings: [
      bundle.GetStringFromName("webextPerms.hostDescription.allUrls"),
    ],
  }, {
    description: "file: permissions",
    manifest: {
      permissions: ["file://*/"],
    },
    expectedOrigins: ["file://*/"],
    expectedWarnings: [
      bundle.GetStringFromName("webextPerms.hostDescription.allUrls"),
    ],
  }, {
    description: "http: permission",
    manifest: {
      permissions: ["http://*/"],
    },
    expectedOrigins: ["http://*/"],
    expectedWarnings: [
      bundle.GetStringFromName("webextPerms.hostDescription.allUrls"),
    ],
  }, {
    description: "*://*/ permission",
    manifest: {
      permissions: ["*://*/"],
    },
    expectedOrigins: ["*://*/"],
    expectedWarnings: [
      bundle.GetStringFromName("webextPerms.hostDescription.allUrls"),
    ],
  }, {
    description: "content_script[*].matches",
    manifest: {
      content_scripts: [{
        // This test uses the manifest file without loading the content script
        // file, so we can use a non-existing dummy file.
        js: ["dummy.js"],
        matches: ["https://*/"],
      }],
    },
    expectedOrigins: ["https://*/"],
    expectedWarnings: [
      bundle.GetStringFromName("webextPerms.hostDescription.allUrls"),
    ],
  }, {
    description: "A few host permissions",
    manifest: {
      permissions: ["http://a/", "http://*.b/", "http://c/*"],
    },
    expectedOrigins: ["http://a/", "http://*.b/", "http://c/*"],
    expectedWarnings: [
      // Wildcard hosts take precedence in the permission list.
      bundle.formatStringFromName("webextPerms.hostDescription.wildcard", ["b"], 1),
      bundle.formatStringFromName("webextPerms.hostDescription.oneSite", ["a"], 1),
      bundle.formatStringFromName("webextPerms.hostDescription.oneSite", ["c"], 1),
    ],
  }, {
    description: "many host permission",
    manifest: {
      permissions: ["http://a/", "http://b/", "http://c/", "http://d/", "http://e/*",
                    "http://*.1/", "http://*.2/", "http://*.3/", "http://*.4/"],
    },
    expectedOrigins: ["http://a/", "http://b/", "http://c/", "http://d/", "http://e/*",
                      "http://*.1/", "http://*.2/", "http://*.3/", "http://*.4/"],
    expectedWarnings: [
      // Wildcard hosts take precedence in the permission list.
      bundle.formatStringFromName("webextPerms.hostDescription.wildcard", ["1"], 1),
      bundle.formatStringFromName("webextPerms.hostDescription.wildcard", ["2"], 1),
      bundle.formatStringFromName("webextPerms.hostDescription.wildcard", ["3"], 1),
      bundle.formatStringFromName("webextPerms.hostDescription.wildcard", ["4"], 1),
      bundle.formatStringFromName("webextPerms.hostDescription.oneSite", ["a"], 1),
      bundle.formatStringFromName("webextPerms.hostDescription.oneSite", ["b"], 1),
      bundle.formatStringFromName("webextPerms.hostDescription.oneSite", ["c"], 1),
      PluralForm.get(2, bundle.GetStringFromName("webextPerms.hostDescription.tooManySites")).replace("#1", "2"),
    ],
  }];
  for (let {description, manifest, expectedOrigins, expectedWarnings} of permissionTestCases) {
    let manifestPermissions = await getManifestPermissions({
      manifest,
    });

    deepEqual(manifestPermissions.origins, expectedOrigins, `Expected origins (${description})`);
    deepEqual(manifestPermissions.permissions, [], `Expected no non-host permissions (${description})`);

    let warnings = getPermissionWarnings(manifestPermissions);
    deepEqual(warnings, expectedWarnings, `Expected warnings (${description})`);
  }
});

// Tests that the expected permission warnings are generated for a mix of host
// permissions and API permissions.
add_task(async function api_permissions() {
  let manifestPermissions = await getManifestPermissions({
    manifest: {
      permissions: [
        "activeTab", "webNavigation", "tabs", "nativeMessaging",
        "http://x/", "http://*.x/", "http://*.tld/",
      ],
    },
  });
  deepEqual(manifestPermissions, {
    origins: ["http://x/", "http://*.x/", "http://*.tld/"],
    permissions: ["activeTab", "webNavigation", "tabs", "nativeMessaging"],
  }, "Expected origins and permissions");

  deepEqual(getPermissionWarnings(manifestPermissions), [
    // Host permissions first, with wildcards on top.
    bundle.formatStringFromName("webextPerms.hostDescription.wildcard", ["x"], 1),
    bundle.formatStringFromName("webextPerms.hostDescription.wildcard", ["tld"], 1),
    bundle.formatStringFromName("webextPerms.hostDescription.oneSite", ["x"], 1),
    // nativeMessaging permission warning first of all permissions.
    bundle.formatStringFromName("webextPerms.description.nativeMessaging", [DUMMY_APP_NAME], 1),
    // Other permissions in alphabetical order.
    // Note: activeTab has no permission warning string.
    bundle.GetStringFromName("webextPerms.description.tabs"),
    bundle.GetStringFromName("webextPerms.description.webNavigation"),
  ], "Expected warnings");
});

// Tests that the expected permission warnings are generated for a mix of host
// permissions and API permissions, for a privileged extension that uses the
// mozillaAddons permission.
add_task(async function privileged_with_mozillaAddons() {
  let manifestPermissions = await getManifestPermissions({
    isPrivileged: true,
    manifest: {
      permissions: ["mozillaAddons", "mozillaAddons", "mozillaAddons", "resource://*/*", "http://a/"],
    },
  });
  deepEqual(manifestPermissions, {
    origins: ["resource://*/*", "http://a/"],
    permissions: ["mozillaAddons"],
  }, "Expected origins and permissions for privileged add-on with mozillaAddons");

  deepEqual(getPermissionWarnings(manifestPermissions), [
    bundle.GetStringFromName("webextPerms.hostDescription.allUrls"),
  ], "Expected warnings for privileged add-on with mozillaAddons permission.");
});

// Similar to the privileged_with_mozillaAddons test, except the test extension
// is unprivileged and not allowed to use the mozillaAddons permission.
add_task(async function unprivileged_with_mozillaAddons() {
  let manifestPermissions = await getManifestPermissions({
    manifest: {
      permissions: ["mozillaAddons", "mozillaAddons", "mozillaAddons", "resource://*/*", "http://a/"],
    },
  });
  deepEqual(manifestPermissions, {
    origins: ["http://a/"],
    permissions: [],
  }, "Expected origins and permissions for unprivileged add-on with mozillaAddons");

  deepEqual(getPermissionWarnings(manifestPermissions), [
    bundle.formatStringFromName("webextPerms.hostDescription.oneSite", ["a"], 1),
  ], "Expected warnings for unprivileged add-on with mozillaAddons permission.");
});

