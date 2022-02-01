/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

const BROWSER_PROPERTIES =
  AppConstants.MOZ_APP_NAME == "thunderbird"
    ? "chrome://messenger/locale/addons.properties"
    : "chrome://browser/locale/browser.properties";

// Lazily import ExtensionParent to allow AddonTestUtils.createAppInfo to
// override Services.appinfo.
ChromeUtils.defineModuleGetter(
  this,
  "ExtensionParent",
  "resource://gre/modules/ExtensionParent.jsm"
);

async function _test_manifest(manifest, expectedError) {
  ExtensionTestUtils.failOnSchemaWarnings(false);
  let normalized = await ExtensionTestUtils.normalizeManifest(
    manifest,
    "manifest.WebExtensionSitePermissionsManifest"
  );
  ExtensionTestUtils.failOnSchemaWarnings(true);

  if (expectedError) {
    ok(
      normalized.error.includes(expectedError),
      `The manifest error ${JSON.stringify(
        normalized.error
      )} must contain ${JSON.stringify(expectedError)}`
    );
  } else {
    equal(normalized.error, undefined, "Should not have an error");
  }
  equal(normalized.errors.length, 0, "Should have no warning");
}

add_task(async function test_manifest_site_permissions() {
  await _test_manifest({
    site_permissions: ["midi"],
    install_origins: ["http://example.com"],
  });
  await _test_manifest({
    site_permissions: ["midi-sysex"],
    install_origins: ["http://example.com"],
  });
  await _test_manifest(
    {
      site_permissions: ["unknown_site_permission"],
      install_origins: ["http://example.com"],
    },
    `Error processing site_permissions.0: Invalid enumeration value "unknown_site_permission"`
  );
  await _test_manifest(
    {
      site_permissions: ["unknown_site_permission"],
      install_origins: [],
    },
    `Error processing install_origins: Array requires at least 1 items;`
  );
  await _test_manifest(
    {
      site_permissions: ["unknown_site_permission"],
    },
    `Property "install_origins" is required`
  );
  await _test_manifest(
    {
      install_origins: ["http://example.com"],
    },
    `Property "site_permissions" is required`
  );
  // test any extra manifest entries not part of a site permissions addon will cause an error.
  await _test_manifest(
    {
      site_permissions: ["midi"],
      install_origins: ["http://example.com"],
      permissions: ["webRequest"],
    },
    `Unexpected property`
  );
});

add_task(async function test_sitepermission_telemetry() {
  await AddonTestUtils.promiseStartupManager();

  Services.telemetry.clearEvents();

  const addon_id = "webmidi@test";
  const origin = "https://example.com";
  const permName = "midi";

  let site_permission = {
    "manifest.json": {
      name: "test Site Permission",
      version: "1.0",
      manifest_version: 2,
      browser_specific_settings: {
        gecko: { id: addon_id },
      },
      install_origins: [origin],
      site_permissions: [permName],
    },
  };

  let [, { addon }] = await Promise.all([
    TestUtils.topicObserved("webextension-sitepermissions-startup"),
    AddonTestUtils.promiseInstallXPI(site_permission),
  ]);

  await addon.uninstall();

  await TelemetryTestUtils.assertEvents(
    [
      [
        "addonsManager",
        "install",
        "sitepermission",
        /.*/,
        {
          step: "started",
          addon_id,
        },
      ],
      [
        "addonsManager",
        "install",
        "sitepermission",
        /.*/,
        {
          step: "completed",
          addon_id,
        },
      ],
      ["addonsManager", "uninstall", "sitepermission", addon_id],
    ],
    {
      category: "addonsManager",
      method: /^install|uninstall$/,
    }
  );

  await AddonTestUtils.promiseShutdownManager();
});

async function _test_ext_site_permissions(site_permissions, install_origins) {
  ExtensionTestUtils.failOnSchemaWarnings(false);
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      install_origins,
      site_permissions,
    },
  });
  await extension.startup();
  await extension.unload();
  ExtensionTestUtils.failOnSchemaWarnings(true);
}

add_task(async function test_ext_site_permissions() {
  await _test_ext_site_permissions(["midi"], ["http://example.com"]);

  await _test_ext_site_permissions(
    ["midi"],
    ["http://example.com", "http://foo.com"]
  ).catch(e => {
    Assert.ok(
      e.message.includes(
        "Error processing install_origins: Array requires at most 1 items; you have 2"
      ),
      "Site permissions can only contain one install origin: "
    );
  });
});

add_task(async function test_sitepermission_type() {
  await AddonTestUtils.promiseStartupManager();

  // Test more than one perm to make sure both are added.
  // While this is allowed, midi-sysex overrides.
  let perms = ["midi", "midi-sysex"];
  let id = "@test-permission";
  let origin = "http://example.com";
  let uri = Services.io.newURI(origin);
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );

  // give the site some other permission (geo)
  Services.perms.addFromPrincipal(
    principal,
    "geo",
    Services.perms.ALLOW_ACTION,
    Services.perms.EXPIRE_NEVER
  );

  let assertGeo = () => {
    Assert.equal(
      Services.perms.testExactPermissionFromPrincipal(principal, "geo"),
      Ci.nsIPermissionManager.ALLOW_ACTION,
      "site still has geo permission"
    );
  };

  let checkPerms = (perms, action, msg) => {
    for (let permName of perms) {
      let permission = Services.perms.testExactPermissionFromPrincipal(
        principal,
        permName
      );
      Assert.equal(permission, action, `${permName}: ${msg}`);
    }
  };

  checkPerms(
    perms,
    Ci.nsIPermissionManager.UNKNOWN_ACTION,
    "no permission for site"
  );

  let site_permission = {
    "manifest.json": {
      name: "test Site Permission",
      version: "1.0",
      manifest_version: 2,
      browser_specific_settings: {
        gecko: {
          id,
        },
      },
      install_origins: [origin],
      site_permissions: perms,
    },
  };

  let [, { addon }] = await Promise.all([
    TestUtils.topicObserved("webextension-sitepermissions-startup"),
    AddonTestUtils.promiseInstallXPI(site_permission),
  ]);

  checkPerms(
    perms,
    Ci.nsIPermissionManager.ALLOW_ACTION,
    "extension enabled permission for site"
  );
  assertGeo();

  // Test the permission is retained on restart.
  await AddonTestUtils.promiseRestartManager();
  addon = await AddonManager.getAddonByID(id);

  checkPerms(
    perms,
    Ci.nsIPermissionManager.ALLOW_ACTION,
    "extension enabled permission for site"
  );
  assertGeo();

  // Test that a removed permission is added on restart
  Services.perms.removeFromPrincipal(principal, perms[0]);
  await AddonTestUtils.promiseRestartManager();
  addon = await AddonManager.getAddonByID(id);

  checkPerms(
    perms,
    Ci.nsIPermissionManager.ALLOW_ACTION,
    "extension enabled permission for site"
  );
  assertGeo();

  // Test that a changed permission is not changed on restart
  Services.perms.addFromPrincipal(
    principal,
    perms[0],
    Services.perms.DENY_ACTION,
    Services.perms.EXPIRE_NEVER
  );

  await AddonTestUtils.promiseRestartManager();
  addon = await AddonManager.getAddonByID(id);

  checkPerms(
    [perms[0]],
    Ci.nsIPermissionManager.DENY_ACTION,
    "extension enabled permission for site"
  );
  checkPerms(
    [perms[1]],
    Ci.nsIPermissionManager.ALLOW_ACTION,
    "extension enabled permission for site"
  );
  assertGeo();

  // Test permission removal when addon disabled
  await addon.disable();

  checkPerms(
    perms,
    Ci.nsIPermissionManager.UNKNOWN_ACTION,
    "no permission for site"
  );
  assertGeo();

  // Enabling an addon will always force ALLOW_ACTION
  await addon.enable();

  checkPerms(
    perms,
    Ci.nsIPermissionManager.ALLOW_ACTION,
    "extension enabled permission for site"
  );
  assertGeo();

  // Test permission removal when addon uninstalled
  await addon.uninstall();

  checkPerms(
    perms,
    Ci.nsIPermissionManager.UNKNOWN_ACTION,
    "no permission for site"
  );
  assertGeo();
});

add_task(async function test_site_permissions_have_localization_strings() {
  await ExtensionParent.apiManager.lazyInit();
  const SCHEMA_SITE_PERMISSIONS = Schemas.getPermissionNames([
    "SitePermission",
  ]);
  ok(SCHEMA_SITE_PERMISSIONS.length, "we have site permissions");

  const bundle = Services.strings.createBundle(BROWSER_PROPERTIES);

  for (const perm of SCHEMA_SITE_PERMISSIONS) {
    try {
      const str = bundle.GetStringFromName(
        `webextSitePerms.description.${perm}`
      );

      ok(str.length, `Found localization string for '${perm}' site permission`);
    } catch (e) {
      ok(false, `Site permission missing '${perm}'`);
    }
  }
});
