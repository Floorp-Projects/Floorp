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

  function background() {
    browser.test.onMessage.addListener(async msg => {
      switch (msg) {
        case "set-storage":
          browser.test.log(`storing data in storage.local`);
          await browser.storage.local.set({ akey: "somevalue" });
          browser.test.log(`data stored in storage.local successfully`);
          break;
        case "has-storage": {
          browser.test.log(`checking data stored in storage.local`);
          const data = await browser.storage.local.get(["akey"]);
          browser.test.assertEq(
            data.akey,
            "somevalue",
            "Got storage.local data"
          );
          break;
        }
        default:
          browser.test.fail(`Unexpected test message: ${msg}`);
      }

      browser.test.sendMessage(`${msg}:done`);
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["unlimitedStorage", "storage"],
      applications: { gecko: { id } },
      version: "1",
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

  extension.sendMessage("set-storage");
  await extension.awaitMessage("set-storage:done");
  extension.sendMessage("has-storage");
  await extension.awaitMessage("has-storage:done");

  // Simulate an update which do not require the unlimitedStorage permission.
  await extension.upgrade({
    background,
    manifest: {
      permissions: ["storage"],
      applications: { gecko: { id } },
      version: "2",
    },
    useAddonManager: "permanent",
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

  // Verify that the previously stored data has not been
  // removed as a side effect of removing the unlimitedStorage
  // permission.
  extension.sendMessage("has-storage");
  await extension.awaitMessage("has-storage:done");

  await extension.unload();
});

add_task(async function test_unlimitedStorage_origin_attributes() {
  Services.prefs.setBoolPref("privacy.firstparty.isolate", true);

  const id = "test-unlimitedStorage-origin-attributes@mozilla";

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["unlimitedStorage"],
      applications: { gecko: { id } },
    },
  });

  await extension.startup();

  let policy = WebExtensionPolicy.getByID(extension.id);
  let principal = policy.extension.principal;

  ok(
    !principal.firstPartyDomain,
    "extension principal has no firstPartyDomain"
  );

  let perm = Services.perms.testExactPermissionFromPrincipal(
    principal,
    "persistent-storage"
  );
  equal(
    perm,
    Services.perms.ALLOW_ACTION,
    "Should have the correct permission without OAs"
  );

  await extension.unload();

  Services.prefs.clearUserPref("privacy.firstparty.isolate");
});
