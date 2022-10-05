/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  addGatedPermissionTypesForXpcShellTests,
  SITEPERMS_ADDON_PROVIDER_PREF,
  SITEPERMS_ADDON_TYPE,
} = ChromeUtils.importESModule(
  "resource://gre/modules/addons/siteperms-addon-utils.sys.mjs"
);

const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);
let ssm = Services.scriptSecurityManager;
const PRINCIPAL_COM = ssm.createContentPrincipalFromOrigin(
  "https://example.com"
);
const PRINCIPAL_ORG = ssm.createContentPrincipalFromOrigin(
  "https://example.org"
);
const PRINCIPAL_GITHUB = ssm.createContentPrincipalFromOrigin(
  "https://github.io"
);
const PRINCIPAL_UNSECURE = ssm.createContentPrincipalFromOrigin(
  "http://example.net"
);
const PRINCIPAL_IP = ssm.createContentPrincipalFromOrigin(
  "https://18.154.122.194"
);

const GATED_SITE_PERM1 = "test/gatedSitePerm";
const GATED_SITE_PERM2 = "test/anotherGatedSitePerm";
addGatedPermissionTypesForXpcShellTests([GATED_SITE_PERM1, GATED_SITE_PERM2]);
const NON_GATED_SITE_PERM = "test/nonGatedPerm";

add_setup(async () => {
  AddonTestUtils.init(this);

  Services.prefs.setBoolPref(SITEPERMS_ADDON_PROVIDER_PREF, true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(SITEPERMS_ADDON_PROVIDER_PREF);
  });

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  await promiseStartupManager();

  // The SitePermsAddonProvider does not register until the first content process
  // is launched, so we simulate that by firing this notification.
  Services.obs.notifyObservers(null, "ipc:first-content-process-created");
});

add_task(async function() {
  let addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
  Assert.equal(addons.length, 0, "There's no addons");

  info("Add a gated permission");
  PermissionTestUtils.add(
    PRINCIPAL_COM,
    GATED_SITE_PERM1,
    Services.perms.ALLOW_ACTION
  );
  addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
  Assert.equal(addons.length, 1, "A siteperm addon is now available");
  const comAddon = await promiseAddonByID(addons[0].id);
  Assert.equal(addons[0], comAddon, "getAddonByID returns the expected addon");

  Assert.deepEqual(
    comAddon.sitePermissions,
    [GATED_SITE_PERM1],
    "addon has expected sitePermissions"
  );
  Assert.equal(comAddon.type, SITEPERMS_ADDON_TYPE, "addon has expected type");
  Assert.equal(
    comAddon.name,
    `Site Permissions for example.com`,
    "addon has expected name"
  );

  info("Add another gated permission");
  PermissionTestUtils.add(
    PRINCIPAL_COM,
    GATED_SITE_PERM2,
    Services.perms.ALLOW_ACTION
  );
  addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
  Assert.equal(
    addons.length,
    1,
    "There is no new siteperm addon after adding a permission to the same principal..."
  );
  Assert.deepEqual(
    comAddon.sitePermissions,
    [GATED_SITE_PERM1, GATED_SITE_PERM2],
    "...but the new permission is reported by addon.sitePermissions"
  );

  info("Add a non-gated permission");
  PermissionTestUtils.add(
    PRINCIPAL_COM,
    NON_GATED_SITE_PERM,
    Services.perms.ALLOW_ACTION
  );
  Assert.equal(
    addons.length,
    1,
    "There is no new siteperm addon after adding a non gated permission to the same principal..."
  );
  Assert.deepEqual(
    comAddon.sitePermissions,
    [GATED_SITE_PERM1, GATED_SITE_PERM2],
    "...and the new permission is not reported by addon.sitePermissions"
  );

  info("Adding a gated permission to another principal");
  PermissionTestUtils.add(
    PRINCIPAL_ORG,
    GATED_SITE_PERM1,
    Services.perms.ALLOW_ACTION
  );
  addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
  Assert.equal(addons.length, 2, "A new siteperm addon is now available");
  const orgAddon = await promiseAddonByID(addons[1].id);
  Assert.equal(addons[1], orgAddon, "getAddonByID returns the expected addon");

  Assert.deepEqual(
    orgAddon.sitePermissions,
    [GATED_SITE_PERM1],
    "new addon only has a single sitePermission"
  );

  info("Passing null or undefined to getAddonsByTypes returns all the addons");
  addons = await promiseAddonsByTypes(null);
  // We can't do an exact check on all the returned addons as we get other type
  // of addons from other providers.
  Assert.deepEqual(
    addons.filter(a => a.type == SITEPERMS_ADDON_TYPE).map(a => a.id),
    [comAddon.id, orgAddon.id],
    "Got site perms addons when passing null"
  );

  addons = await promiseAddonsByTypes();
  Assert.deepEqual(
    addons.filter(a => a.type == SITEPERMS_ADDON_TYPE).map(a => a.id),
    [comAddon.id, orgAddon.id],
    "Got site perms addons when passing undefined"
  );

  info("Remove a gated permission");
  PermissionTestUtils.remove(PRINCIPAL_COM, GATED_SITE_PERM2);
  addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
  Assert.equal(
    addons.length,
    2,
    "Removing a permission did not removed the addon has it has another permission"
  );
  Assert.deepEqual(
    comAddon.sitePermissions,
    [GATED_SITE_PERM1],
    "addon has expected sitePermissions"
  );

  info("Remove last gated permission on PRINCIPAL_COM");
  const promisePrincipalComUninstalling = AddonTestUtils.promiseAddonEvent(
    "onUninstalling",
    addon => {
      return addon.id === comAddon.id;
    }
  );
  const promisePrincipalComUninstalled = AddonTestUtils.promiseAddonEvent(
    "onUninstalled",
    addon => {
      return addon.id === comAddon.id;
    }
  );
  PermissionTestUtils.remove(PRINCIPAL_COM, GATED_SITE_PERM1);
  info("Wait for onUninstalling addon listener call");
  await promisePrincipalComUninstalling;
  info("Wait for onUninstalled addon listener call");
  await promisePrincipalComUninstalled;

  addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
  Assert.equal(
    addons.length,
    1,
    "Removing the last gated permission removed the addon"
  );
  Assert.equal(addons[0], orgAddon);

  info("Uninstall org addon");
  orgAddon.uninstall();
  addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
  Assert.equal(addons.length, 0, "org addon is removed");
  Assert.equal(
    PermissionTestUtils.testExactPermission(PRINCIPAL_ORG, GATED_SITE_PERM1),
    false,
    "Permission was removed when the addon was uninstalled"
  );

  info("Adding a permission to a public etld");
  PermissionTestUtils.add(
    PRINCIPAL_GITHUB,
    GATED_SITE_PERM1,
    Services.perms.ALLOW_ACTION
  );
  addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
  Assert.equal(
    addons.length,
    0,
    "Adding a gated permission to a public etld shouldn't add a new addon"
  );
  // Cleanup
  PermissionTestUtils.remove(PRINCIPAL_GITHUB, GATED_SITE_PERM1);

  info("Adding a permission to a non secure principal");
  PermissionTestUtils.add(
    PRINCIPAL_UNSECURE,
    GATED_SITE_PERM1,
    Services.perms.ALLOW_ACTION
  );
  addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
  Assert.equal(
    addons.length,
    0,
    "Adding a gated permission to an unsecure principal shouldn't add a new addon"
  );

  info("Call installSitePermsAddonFromWebpage without proper principal");
  await Assert.rejects(
    AddonManager.installSitePermsAddonFromWebpage(
      null,
      // principal
      null,
      GATED_SITE_PERM1
    ),
    /aInstallingPrincipal must be a nsIPrincipal/,
    "installSitePermsAddonFromWebpage rejected when called without a principal"
  );

  info(
    "Call installSitePermsAddonFromWebpage with non-null, non-element browser"
  );
  await Assert.rejects(
    AddonManager.installSitePermsAddonFromWebpage(
      "browser",
      PRINCIPAL_COM,
      GATED_SITE_PERM1
    ),
    /aBrowser must be an Element, or null/,
    "installSitePermsAddonFromWebpage rejected when called  with a non-null, non-element browser"
  );

  info("Call installSitePermsAddonFromWebpage with unsecure principal");
  await Assert.rejects(
    AddonManager.installSitePermsAddonFromWebpage(
      null,
      PRINCIPAL_UNSECURE,
      GATED_SITE_PERM1
    ),
    /SitePermsAddons can only be installed from secure origins/,
    "installSitePermsAddonFromWebpage rejected when called with unsecure principal"
  );

  info(
    "Call installSitePermsAddonFromWebpage for public principal and gated permission"
  );
  await Assert.rejects(
    AddonManager.installSitePermsAddonFromWebpage(
      null,
      PRINCIPAL_GITHUB,
      GATED_SITE_PERM1
    ),
    /SitePermsAddon can\'t be installed from public eTLDs/,
    "installSitePermsAddonFromWebpage rejected when called with public principal"
  );

  addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
  Assert.equal(addons.length, 0, "there was no added addon...");
  Assert.equal(
    PermissionTestUtils.testExactPermission(PRINCIPAL_GITHUB, GATED_SITE_PERM1),
    false,
    "...and no new permission either"
  );

  info(
    "Call installSitePermsAddonFromWebpage for plain-ip principal and gated permission"
  );
  await Assert.rejects(
    AddonManager.installSitePermsAddonFromWebpage(
      null,
      PRINCIPAL_IP,
      GATED_SITE_PERM1
    ),
    /SitePermsAddons install disallowed when the host is an IP address/,
    "installSitePermsAddonFromWebpage rejected when called with plain-ip principal"
  );

  addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
  Assert.equal(addons.length, 0, "there was no added addon...");
  Assert.equal(
    PermissionTestUtils.testExactPermission(PRINCIPAL_IP, GATED_SITE_PERM1),
    false,
    "...and no new permission either"
  );

  info(
    "Call installSitePermsAddonFromWebpage for authorized principal and non-gated permission"
  );
  await Assert.rejects(
    AddonManager.installSitePermsAddonFromWebpage(
      null,
      PRINCIPAL_COM,
      NON_GATED_SITE_PERM
    ),
    new RegExp(`"${NON_GATED_SITE_PERM}" is not a gated permission`),
    "installSitePermsAddonFromWebpage rejected for non-gated permission"
  );

  addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
  Assert.equal(
    addons.length,
    0,
    "installSitePermsAddonFromWebpage with non-gated permission should not add the addon"
  );

  info(
    "Call installSitePermsAddonFromWebpage for authorized principal and gated permission"
  );
  // Observers to bypass panels and continue install.
  TestUtils.topicObserved("addon-install-blocked").then(([subject]) => {
    let installInfo = subject.wrappedJSObject;
    info("==== test got addon-install-blocked, calling `install`");
    installInfo.install();
  });
  TestUtils.topicObserved("webextension-permission-prompt").then(
    ([subject]) => {
      info("==== test webextension-permission-prompt, calling `resolve`");
      subject.wrappedJSObject.info.resolve();
    }
  );
  await AddonManager.installSitePermsAddonFromWebpage(
    null,
    PRINCIPAL_COM,
    GATED_SITE_PERM1
  );
  addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
  Assert.equal(
    addons.length,
    1,
    "installSitePermsAddonFromWebpage should add the addon..."
  );
  Assert.equal(
    PermissionTestUtils.testExactPermission(PRINCIPAL_COM, GATED_SITE_PERM1),
    true,
    "...and set the permission"
  );
});
