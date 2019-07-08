"use strict";

const {
  createAppInfo,
  promiseStartupManager,
  promiseRestartManager,
  promiseWebExtensionStartup,
} = AddonTestUtils;

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "42", "42");

const STORAGE_SITE_PERMISSIONS = [
  "WebExtensions-unlimitedStorage",
  "indexedDB",
  "persistent-storage",
];

function checkSitePermissions(principal, expectedPermAction, assertMessage) {
  for (const permName of STORAGE_SITE_PERMISSIONS) {
    const actualPermAction = Services.perms.testPermissionFromPrincipal(
      principal,
      permName
    );

    equal(
      actualPermAction,
      expectedPermAction,
      `The extension "${permName}" SitePermission ${assertMessage} as expected`
    );
  }
}

add_task(async function test_unlimitedStorage_restored_on_app_startup() {
  const id = "test-unlimitedStorage-removed-on-app-shutdown@mozilla";

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["unlimitedStorage"],
      applications: {
        gecko: { id },
      },
    },

    useAddonManager: "permanent",
  });

  await promiseStartupManager();
  await extension.startup();

  const policy = WebExtensionPolicy.getByID(extension.id);
  const principal = policy.extension.principal;

  checkSitePermissions(
    principal,
    Services.perms.ALLOW_ACTION,
    "has been allowed"
  );

  // Remove site permissions as it would happen if Firefox is shutting down
  // with the "clear site permissions" setting.

  Services.perms.removeFromPrincipal(
    principal,
    "WebExtensions-unlimitedStorage"
  );
  Services.perms.removeFromPrincipal(principal, "indexedDB");
  Services.perms.removeFromPrincipal(principal, "persistent-storage");

  checkSitePermissions(principal, Services.perms.UNKNOWN_ACTION, "is not set");

  const onceExtensionStarted = promiseWebExtensionStartup(id);
  await promiseRestartManager();
  await onceExtensionStarted;

  // The site permissions should have been granted again.
  checkSitePermissions(
    principal,
    Services.perms.ALLOW_ACTION,
    "has been allowed"
  );

  await extension.unload();
});

add_task(async function test_unlimitedStorage_removed_on_update() {
  const id = "test-unlimitedStorage-removed-on-update@mozilla";

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["unlimitedStorage"],
      applications: { gecko: { id } },
    },
    useAddonManager: "permanent",
  });

  await extension.startup();

  const policy = WebExtensionPolicy.getByID(extension.id);
  const principal = policy.extension.principal;

  checkSitePermissions(
    principal,
    Services.perms.ALLOW_ACTION,
    "has been allowed"
  );

  // Simulate an update which do not require the unlimitedStorage permission.
  await extension.upgrade({
    manifest: {
      applications: { gecko: { id } },
    },
  });

  const newPolicy = WebExtensionPolicy.getByID(extension.id);
  const newPrincipal = newPolicy.extension.principal;

  equal(
    principal.URI.spec,
    newPrincipal.URI.spec,
    "upgraded extension has the expected principal"
  );

  checkSitePermissions(
    principal,
    Services.perms.UNKNOWN_ACTION,
    "has been cleared"
  );

  await extension.unload();
});
