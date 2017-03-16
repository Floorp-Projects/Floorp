"use strict";

XPCOMUtils.defineLazyGetter(this, "ExtensionManager", () => {
  const {ExtensionManager}
    = Cu.import("resource://gre/modules/ExtensionContent.jsm", {});
  return ExtensionManager;
});
Cu.import("resource://gre/modules/ExtensionPermissions.jsm");

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

// Find the DOMWindowUtils for the background page for the given
// extension (wrapper)
function findWinUtils(extension) {
  let extensionChild = ExtensionManager.extensions.get(extension.extension.id);
  let bgwin = null;
  for (let view of extensionChild.views) {
    if (view.viewType == "background") {
      bgwin = view.contentWindow;
    }
  }
  notEqual(bgwin, null, "Found background window for the test extension");
  return bgwin.QueryInterface(Ci.nsIInterfaceRequestor)
              .getInterface(Ci.nsIDOMWindowUtils);
}

add_task(async function test_permissions() {
  const REQUIRED_PERMISSIONS = ["downloads"];
  const REQUIRED_ORIGINS = ["*://site.com/", "*://*.domain.com/"];
  const OPTIONAL_PERMISSIONS = ["bookmarks", "history"];
  const OPTIONAL_ORIGINS = ["http://optionalsite.com/", "https://*.optionaldomain.com/"];

  let acceptPrompt = false;
  const observer = {
    observe(subject, topic, data) {
      if (topic == "webextension-optional-permission-prompt") {
        let {resolve} = subject.wrappedJSObject;
        resolve(acceptPrompt);
      }
    },
  };

  Services.prefs.setBoolPref("extensions.webextOptionalPermissionPrompts", true);
  Services.obs.addObserver(observer, "webextension-optional-permission-prompt", false);
  do_register_cleanup(() => {
    Services.obs.removeObserver(observer, "webextension-optional-permission-prompt");
    Services.prefs.clearUserPref("extensions.webextOptionalPermissionPrompts");
  });

  await AddonTestUtils.promiseStartupManager();

  function background() {
    browser.test.onMessage.addListener(async (method, arg) => {
      if (method == "getAll") {
        let perms = await browser.permissions.getAll();
        browser.test.sendMessage("getAll.result", perms);
      } else if (method == "contains") {
        let result = await browser.permissions.contains(arg);
        browser.test.sendMessage("contains.result", result);
      } else if (method == "request") {
        try {
          let result = await browser.permissions.request(arg);
          browser.test.sendMessage("request.result", {status: "success", result});
        } catch (err) {
          browser.test.sendMessage("request.result", {status: "error", message: err.message});
        }
      } else if (method == "remove") {
        let result = await browser.permissions.remove(arg);
        browser.test.sendMessage("remove.result", result);
      }
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: [...REQUIRED_PERMISSIONS, ...REQUIRED_ORIGINS],
      optional_permissions: [...OPTIONAL_PERMISSIONS, ...OPTIONAL_ORIGINS],
    },
    useAddonManager: "permanent",
  });

  await extension.startup();
  let winUtils = findWinUtils(extension);

  function call(method, arg) {
    extension.sendMessage(method, arg);
    return extension.awaitMessage(`${method}.result`);
  }

  let result = await call("getAll");
  deepEqual(result.permissions, REQUIRED_PERMISSIONS);
  deepEqual(result.origins, REQUIRED_ORIGINS);

  for (let perm of REQUIRED_PERMISSIONS) {
    result = await call("contains", {permissions: [perm]});
    equal(result, true, `contains() returns true for fixed permission ${perm}`);
  }
  for (let origin of REQUIRED_ORIGINS) {
    result = await call("contains", {origins: [origin]});
    equal(result, true, `contains() returns true for fixed origin ${origin}`);
  }

  // None of the optional permissions should be available yet
  for (let perm of OPTIONAL_PERMISSIONS) {
    result = await call("contains", {permissions: [perm]});
    equal(result, false, `contains() returns false for permission ${perm}`);
  }
  for (let origin of OPTIONAL_ORIGINS) {
    result = await call("contains", {origins: [origin]});
    equal(result, false, `conains() returns false for origin ${origin}`);
  }

  result = await call("contains", {
    permissions: [...REQUIRED_PERMISSIONS, ...OPTIONAL_PERMISSIONS],
  });
  equal(result, false, "contains() returns false for a mix of available and unavailable permissions");

  let perm = OPTIONAL_PERMISSIONS[0];
  result = await call("request", {permissions: [perm]});
  equal(result.status, "error", "request() fails if not called from an event handler");
  ok(/May only request permissions from a user input handler/.test(result.message),
     "error message for calling request() outside an event handler is reasonable");
  result = await call("contains", {permissions: [perm]});
  equal(result, false, "Permission requested outside an event handler was not granted");

  let userInputHandle = winUtils.setHandlingUserInput(true);

  result = await call("request", {permissions: ["notifications"]});
  equal(result.status, "error", "request() for permission not in optional_permissions should fail");
  ok(/since it was not declared in optional_permissions/.test(result.message),
     "error message for undeclared optional_permission is reasonable");

  // Check request() when the prompt is canceled.
  acceptPrompt = false;
  result = await call("request", {permissions: [perm]});
  equal(result.status, "success", "request() returned cleanly");
  equal(result.result, false, "request() returned false for rejected permission");

  result = await call("contains", {permissions: [perm]});
  equal(result, false, "Rejected permission was not granted");

  // Call request() and accept the prompt
  acceptPrompt = true;
  let allOptional = {
    permissions: OPTIONAL_PERMISSIONS,
    origins: OPTIONAL_ORIGINS,
  };
  result = await call("request", allOptional);
  equal(result.status, "success", "request() returned cleanly");
  equal(result.result, true, "request() returned true for accepted permissions");
  userInputHandle.destruct();

  let allPermissions = {
    permissions: [...REQUIRED_PERMISSIONS, ...OPTIONAL_PERMISSIONS],
    origins: [...REQUIRED_ORIGINS, ...OPTIONAL_ORIGINS],
  };

  result = await call("getAll");
  deepEqual(result, allPermissions, "getAll() returns required and runtime requested permissions");

  result = await call("contains", allPermissions);
  equal(result, true, "contains() returns true for runtime requested permissions");

  // Restart, verify permissions are still present
  await AddonTestUtils.promiseRestartManager();
  await extension.awaitStartup();

  result = await call("getAll");
  deepEqual(result, allPermissions, "Runtime requested permissions are still present after restart");

  // Check remove()
  result = await call("remove", {permissions: OPTIONAL_PERMISSIONS});
  equal(result, true, "remove() succeeded");

  let perms = {
    permissions: REQUIRED_PERMISSIONS,
    origins: [...REQUIRED_ORIGINS, ...OPTIONAL_ORIGINS],
  };
  result = await call("getAll");
  deepEqual(result, perms, "Expected permissions remain after removing some");

  result = await call("remove", {origins: OPTIONAL_ORIGINS});
  equal(result, true, "remove() succeeded");

  perms.origins = REQUIRED_ORIGINS;
  result = await call("getAll");
  deepEqual(result, perms, "Back to default permissions after removing more");

  await extension.unload();
});

add_task(async function test_startup() {
  async function background() {
    browser.test.onMessage.addListener(async (perms) => {
      await browser.permissions.request(perms);
      browser.test.sendMessage("requested");
    });

    let all = await browser.permissions.getAll();
    browser.test.sendMessage("perms", all);
  }

  const PERMS1 = {
    permissions: ["history", "tabs"],
  };
  const PERMS2 = {
    origins: ["https://site2.com/"],
  };

  let extension1 = ExtensionTestUtils.loadExtension({
    background,
    manifest: {optional_permissions: PERMS1.permissions},
    useAddonManager: "permanent",
  });
  let extension2 = ExtensionTestUtils.loadExtension({
    background,
    manifest: {optional_permissions: PERMS2.origins},
    useAddonManager: "permanent",
  });

  await extension1.startup();
  await extension2.startup();

  let perms = await extension1.awaitMessage("perms");
  dump(`perms1 ${JSON.stringify(perms)}\n`);
  perms = await extension2.awaitMessage("perms");
  dump(`perms2 ${JSON.stringify(perms)}\n`);

  let winUtils = findWinUtils(extension1);
  let handle = winUtils.setHandlingUserInput(true);
  extension1.sendMessage(PERMS1);
  await extension1.awaitMessage("requested");
  handle.destruct();

  winUtils = findWinUtils(extension2);
  handle = winUtils.setHandlingUserInput(true);
  extension2.sendMessage(PERMS2);
  await extension2.awaitMessage("requested");
  handle.destruct();

  // Restart everything, and force the permissions store to be
  // re-read on startup
  ExtensionPermissions._uninit();
  await AddonTestUtils.promiseRestartManager();
  await extension1.awaitStartup();
  await extension2.awaitStartup();

  async function checkPermissions(extension, permissions) {
    perms = await extension.awaitMessage("perms");
    let expect = Object.assign({permissions: [], origins: []}, permissions);
    deepEqual(perms, expect, "Extension got correct permissions on startup");
  }

  await checkPermissions(extension1, PERMS1);
  await checkPermissions(extension2, PERMS2);

  await extension1.unload();
  await extension2.unload();
});
