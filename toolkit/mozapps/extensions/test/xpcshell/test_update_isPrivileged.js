/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

ChromeUtils.defineESModuleGetters(this, {
  ExtensionParent: "resource://gre/modules/ExtensionParent.sys.mjs",
});

AddonTestUtils.usePrivilegedSignatures = id => id === "privileged@ext";

const EXTENSION_API_IMPL = `
this.extensionApiImpl = class extends ExtensionAPI {
  onStartup() {
    extensions.emit("test-ExtensionAPI-onStartup", {
      extensionId: this.extension.id,
      version: this.extension.manifest.version,
    });
  }
  static onUpdate(id, manifest) {
    extensions.emit("test-ExtensionAPI-onUpdate", {
      extensionId: id,
      version: manifest.version,
    });
  }
};`;

function setupTestExtensionAPI() {
  // The EXTENSION_API_IMPL script is going to be loaded in the main process,
  // where only safe loads are permitted. So we generate a resource:-URL, to
  // avoid the use of security.allow_parent_unrestricted_js_loads.
  let resProto = Services.io
    .getProtocolHandler("resource")
    .QueryInterface(Ci.nsIResProtocolHandler);
  resProto.setSubstitution(
    "extensionApiImplJs",
    Services.io.newURI(`data:,${encodeURIComponent(EXTENSION_API_IMPL)}`)
  );
  registerCleanupFunction(() => {
    resProto.setSubstitution("extensionApiImplJs", null);
  });

  const modules = {
    extensionApiImpl: {
      url: "resource://extensionApiImplJs",
      events: ["startup", "update"],
    },
  };

  Services.catMan.addCategoryEntry(
    "webextension-modules",
    "test-register-extensionApiImpl",
    `data:,${JSON.stringify(modules)}`,
    false,
    false
  );
}

async function runInstallAndUpdate({
  extensionId,
  expectPrivileged,
  installExtensionData,
}) {
  let extensionData = {
    useAddonManager: "permanent",
    manifest: {
      browser_specific_settings: { gecko: { id: extensionId } },
      version: "1.1",
    },
  };
  let events = [];
  function onUpdated(type, params) {
    params = { type, ...params };
    // resourceURI cannot be serialized for use with deepEqual.
    delete params.resourceURI;
    events.push(params);
  }
  function onExtensionAPI(type, params) {
    events.push({ type, ...params });
  }
  ExtensionParent.apiManager.on("update", onUpdated);
  ExtensionParent.apiManager.on("test-ExtensionAPI-onStartup", onExtensionAPI);
  ExtensionParent.apiManager.on("test-ExtensionAPI-onUpdate", onExtensionAPI);

  let { addon } = await installExtensionData(extensionData);
  equal(addon.isPrivileged, expectPrivileged, "Expected isPrivileged");

  extensionData.manifest.version = "2.22";
  extensionData.manifest.permissions = ["mozillaAddons"];
  // May warn about invalid permissions when the extension is not privileged.
  ExtensionTestUtils.failOnSchemaWarnings(false);
  let extension = await installExtensionData(extensionData);
  ExtensionTestUtils.failOnSchemaWarnings(true);
  await extension.unload();

  ExtensionParent.apiManager.off("update", onUpdated);
  ExtensionParent.apiManager.off("test-ExtensionAPI-onStartup", onExtensionAPI);
  ExtensionParent.apiManager.off("test-ExtensionAPI-onUpdate", onExtensionAPI);

  // Verify that we have (1) installed and (2) updated the extension.
  Assert.deepEqual(
    events,
    [
      { type: "test-ExtensionAPI-onStartup", extensionId, version: "1.1" },
      // The next two events show that ExtensionParent has run the onUpdate
      // handler, during which ExtensionData has supposedly been constructed.
      { type: "update", id: extensionId, isPrivileged: expectPrivileged },
      { type: "test-ExtensionAPI-onUpdate", extensionId, version: "2.22" },
      { type: "test-ExtensionAPI-onStartup", extensionId, version: "2.22" },
    ],
    "Expected startup and update events"
  );
}

add_task(async function setup() {
  setupTestExtensionAPI();
  await ExtensionTestUtils.startAddonManager();
});

// Tests that privileged extensions (e.g builtins) are always parsed with the
// correct isPrivileged flag.
add_task(async function test_install_and_update_builtin() {
  let { messages } = await promiseConsoleOutput(async () => {
    await runInstallAndUpdate({
      extensionId: "builtin@ext",
      expectPrivileged: true,
      async installExtensionData(extData) {
        return installBuiltinExtension(extData);
      },
    });
  });

  AddonTestUtils.checkMessages(messages, {
    expected: [{ message: /Addon with ID builtin@ext already installed,/ }],
    forbidden: [{ message: /Invalid extension permission: mozillaAddons/ }],
  });
});

add_task(async function test_install_and_update_regular_ext() {
  let { messages } = await promiseConsoleOutput(async () => {
    await runInstallAndUpdate({
      extensionId: "regular@ext",
      expectPrivileged: false,
      async installExtensionData(extData) {
        let extension = ExtensionTestUtils.loadExtension(extData);
        await extension.startup();
        return extension;
      },
    });
  });
  let errPattern =
    /Loading extension 'regular@ext': Reading manifest: Invalid extension permission: mozillaAddons/;
  let permissionWarnings = messages.filter(msg => errPattern.test(msg.message));
  // Expected number of warnings after triggering the update:
  // 1. Generated when the loaded by the Addons manager (ExtensionData).
  // 2. Generated when read again before ExtensionAPI.onUpdate (ExtensionData).
  // 3. Generated when the extension actually runs (Extension).
  equal(permissionWarnings.length, 3, "Expected number of permission warnings");
});

add_task(async function test_install_and_update_privileged_ext() {
  let { messages } = await promiseConsoleOutput(async () => {
    await runInstallAndUpdate({
      extensionId: "privileged@ext",
      expectPrivileged: true,
      async installExtensionData(extData) {
        let extension = ExtensionTestUtils.loadExtension(extData);
        await extension.startup();
        return extension;
      },
    });
  });
  AddonTestUtils.checkMessages(messages, {
    expected: [
      // First installation.
      { message: /Starting install of privileged@ext / },
      // Second installation (update).
      { message: /Starting install of privileged@ext / },
    ],
    forbidden: [{ message: /Invalid extension permission: mozillaAddons/ }],
  });
});
