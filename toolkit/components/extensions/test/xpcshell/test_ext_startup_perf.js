/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const STARTUP_APIS = ["backgroundPage"];

const STARTUP_MODULES = new Set([
  "resource://gre/modules/Extension.sys.mjs",
  "resource://gre/modules/ExtensionCommon.sys.mjs",
  "resource://gre/modules/ExtensionParent.sys.mjs",
  // FIXME: This is only loaded at startup for new extension installs.
  // Otherwise the data comes from the startup cache. We should test for
  // this.
  "resource://gre/modules/ExtensionPermissions.sys.mjs",
  "resource://gre/modules/ExtensionProcessScript.sys.mjs",
  "resource://gre/modules/ExtensionUtils.sys.mjs",
  "resource://gre/modules/ExtensionTelemetry.sys.mjs",
]);

if (!Services.prefs.getBoolPref("extensions.webextensions.remote")) {
  STARTUP_MODULES.add("resource://gre/modules/ExtensionChild.sys.mjs");
  STARTUP_MODULES.add("resource://gre/modules/ExtensionPageChild.sys.mjs");
}

if (AppConstants.MOZ_APP_NAME == "thunderbird") {
  // Imported via mail/components/extensions/processScript.js.
  STARTUP_MODULES.add("resource://gre/modules/ExtensionChild.sys.mjs");
  STARTUP_MODULES.add("resource://gre/modules/ExtensionContent.sys.mjs");
  STARTUP_MODULES.add("resource://gre/modules/ExtensionPageChild.sys.mjs");
}

AddonTestUtils.init(this);

// Tests that only the minimal set of API scripts and modules are loaded at
// startup for a simple extension.
add_task(async function test_loaded_scripts() {
  await ExtensionTestUtils.startAddonManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    background() {},
    manifest: {},
  });

  await extension.startup();

  const { apiManager } = ExtensionParent;

  const loadedAPIs = Array.from(apiManager.modules.values())
    .filter(m => m.loaded || m.asyncLoaded)
    .map(m => m.namespaceName);

  deepEqual(
    loadedAPIs.sort(),
    STARTUP_APIS,
    "No extra APIs should be loaded at startup for a simple extension"
  );

  let loadedModules = Cu.loadedJSModules
    .concat(Cu.loadedESModules)
    .filter(url => url.startsWith("resource://gre/modules/Extension"));

  deepEqual(
    loadedModules.sort(),
    Array.from(STARTUP_MODULES).sort(),
    "No extra extension modules should be loaded at startup for a simple extension"
  );

  await extension.unload();
});
