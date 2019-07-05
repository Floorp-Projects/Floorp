"use strict";

const { ExtensionPermissions } = ChromeUtils.import(
  "resource://gre/modules/ExtensionPermissions.jsm"
);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

Services.prefs.setBoolPref(
  "extensions.webextensions.background-delayed-startup",
  false
);

const observer = {
  observe(subject, topic, data) {
    if (topic == "webextension-optional-permission-prompt") {
      let { resolve } = subject.wrappedJSObject;
      resolve(true);
    }
  },
};

// Look up the cached permissions, if any.
async function getCachedPermissions(extensionId) {
  const NotFound = Symbol("extension ID not found in permissions cache");
  try {
    return await ExtensionParent.StartupCache.permissions.get(
      extensionId,
      () => {
        // Throw error to prevent the key from being created.
        throw NotFound;
      }
    );
  } catch (e) {
    if (e === NotFound) {
      return null;
    }
    throw e;
  }
}

// Look up the permissions from the file. Internal methods are used to avoid
// inadvertently changing the permissions in the cache or JSON file.
async function getStoredPermissions(extensionId) {
  // The two _get calls that follow are expected to return the same object if
  // the entry exists in the JSON file, otherwise we expect two different
  // objects (with the same default properties).
  let perms1 = await ExtensionPermissions._get(extensionId);
  let perms2 = await ExtensionPermissions._get(extensionId);
  if (perms1 === perms2) {
    // There is an entry in the file.
    return perms1;
  }
  // Sanity check: The returned object should be empty.
  Assert.deepEqual(perms1, perms2, "Expected same permission values");
  Assert.deepEqual(
    perms1,
    { origins: [], permissions: [] },
    "Expected empty permissions"
  );
  return null;
}

add_task(async function setup() {
  Services.prefs.setBoolPref(
    "extensions.webextOptionalPermissionPrompts",
    true
  );
  Services.obs.addObserver(observer, "webextension-optional-permission-prompt");
  await AddonTestUtils.promiseStartupManager();
  registerCleanupFunction(async () => {
    await AddonTestUtils.promiseShutdownManager();
    Services.obs.removeObserver(
      observer,
      "webextension-optional-permission-prompt"
    );
    Services.prefs.clearUserPref("extensions.webextOptionalPermissionPrompts");
  });
});

// This test must run before any restart of the addonmanager so the
// uninstallObserver works.
add_task(async function test_permissions_removed() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      optional_permissions: ["idle"],
    },
    background() {
      browser.test.onMessage.addListener(async (msg, arg) => {
        if (msg == "request") {
          try {
            let result = await browser.permissions.request(arg);
            browser.test.sendMessage("request.result", result);
          } catch (err) {
            browser.test.sendMessage("request.result", err.message);
          }
        }
      });
    },
    useAddonManager: "temporary",
  });

  await extension.startup();

  await withHandlingUserInput(extension, async () => {
    extension.sendMessage("request", { permissions: ["idle"], origins: [] });
    let result = await extension.awaitMessage("request.result");
    equal(result, true, "request() for optional permissions succeeded");
  });

  let id = extension.id;
  let perms = await ExtensionPermissions.get(id);
  equal(perms.permissions.length, 1, "optional permission added");

  Assert.deepEqual(
    await getCachedPermissions(id),
    {
      permissions: ["idle"],
      origins: [],
    },
    "Optional permission added to cache"
  );
  Assert.deepEqual(
    await getStoredPermissions(id),
    {
      permissions: ["idle"],
      origins: [],
    },
    "Optional permission added to persistent file"
  );

  await extension.unload();

  // Directly read from the internals instead of using ExtensionPermissions.get,
  // because the latter will lazily cache the extension ID.
  Assert.deepEqual(
    await getCachedPermissions(id),
    null,
    "Cached permissions removed"
  );
  Assert.deepEqual(
    await getStoredPermissions(id),
    null,
    "Stored permissions removed"
  );

  perms = await ExtensionPermissions.get(id);
  equal(perms.permissions.length, 0, "no permissions after uninstall");
  equal(perms.origins.length, 0, "no origin permissions after uninstall");

  // The public ExtensionPermissions.get method should not store (empty)
  // permissions in the persistent JSON file. Polluting the cache is not ideal,
  // but acceptable since the cache will eventually be cleared, and non-test
  // code is not likely to call ExtensionPermissions.get() for non-installed
  // extensions anyway.
  Assert.deepEqual(await getCachedPermissions(id), perms, "Permissions cached");
  Assert.deepEqual(
    await getStoredPermissions(id),
    null,
    "Permissions not saved"
  );
});
