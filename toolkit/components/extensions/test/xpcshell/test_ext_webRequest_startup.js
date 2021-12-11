"use strict";

// Delay loading until createAppInfo is called and setup.
ChromeUtils.defineModuleGetter(
  this,
  "AddonManager",
  "resource://gre/modules/AddonManager.jsm"
);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

// The app and platform version here should be >= of the version set in the extensions.webExtensionsMinPlatformVersion preference,
// otherwise test_persistent_listener_after_staged_update will fail because no compatible updates will be found.
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

let {
  promiseRestartManager,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));

let scopes = AddonManager.SCOPE_PROFILE | AddonManager.SCOPE_APPLICATION;
Services.prefs.setIntPref("extensions.enabledScopes", scopes);

Services.prefs.setBoolPref(
  "extensions.webextensions.background-delayed-startup",
  true
);

function trackEvents(wrapper) {
  let events = new Map();
  for (let event of ["background-page-event", "start-background-page"]) {
    events.set(event, false);
    wrapper.extension.once(event, () => events.set(event, true));
  }
  return events;
}

async function testPersistentRequestStartup(extension, events, expect) {
  equal(
    events.get("background-page-event"),
    expect.background,
    "Should have gotten a background page event"
  );
  equal(
    events.get("start-background-page"),
    false,
    "Background page should not be started"
  );

  Services.obs.notifyObservers(null, "browser-delayed-startup-finished");
  await ExtensionParent.browserPaintedPromise;

  equal(
    events.get("start-background-page"),
    expect.delayedStart,
    "Should have gotten start-background-page event"
  );

  if (expect.request) {
    await extension.awaitMessage("got-request");
    ok(true, "Background page loaded and received webRequest event");
  }
}

// Test that a non-blocking listener during startup does not immediately
// start the background page, but the event is queued until the background
// page is started.
add_task(async function test_1() {
  await promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      permissions: ["webRequest", "http://example.com/"],
    },

    background() {
      browser.webRequest.onBeforeRequest.addListener(
        details => {
          browser.test.sendMessage("got-request");
        },
        { urls: ["http://example.com/data/file_sample.html"] }
      );
    },
  });

  await extension.startup();

  await promiseRestartManager();
  await extension.awaitStartup();

  let events = trackEvents(extension);

  await ExtensionTestUtils.fetch(
    "http://example.com/",
    "http://example.com/data/file_sample.html"
  );

  await testPersistentRequestStartup(extension, events, {
    background: true,
    delayedStart: true,
    request: true,
  });

  await extension.unload();

  await promiseShutdownManager();
});

// Tests that filters are handled properly: if we have a blocking listener
// with a filter, a request that does not match the filter does not get
// suspended and does not start the background page.
add_task(async function test_2() {
  await promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      permissions: [
        "webRequest",
        "webRequestBlocking",
        "http://test1.example.com/",
      ],
    },

    background() {
      browser.webRequest.onBeforeRequest.addListener(
        details => {
          browser.test.fail("Listener should not have been called");
        },
        { urls: ["http://test1.example.com/*"] },
        ["blocking"]
      );

      browser.test.sendMessage("ready");
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  await promiseRestartManager();
  await extension.awaitStartup();

  let events = trackEvents(extension);

  await ExtensionTestUtils.fetch(
    "http://example.com/",
    "http://example.com/data/file_sample.html"
  );

  await testPersistentRequestStartup(extension, events, {
    background: false,
    delayedStart: false,
    request: false,
  });

  Services.obs.notifyObservers(null, "sessionstore-windows-restored");
  await extension.awaitMessage("ready");

  await extension.unload();
  await promiseShutdownManager();
});

// Tests that moving permission to optional retains permission and that the
// persistent listeners are used as expected.
add_task(async function test_persistent_listener_after_sideload_upgrade() {
  let id = "permission-sideload-upgrade@test";
  let extensionData = {
    useAddonManager: "permanent",
    manifest: {
      version: "1.0",
      applications: { gecko: { id } },
      permissions: ["webRequest", "http://example.com/"],
    },

    background() {
      browser.webRequest.onBeforeRequest.addListener(
        details => {
          browser.test.sendMessage("got-request");
        },
        { urls: ["http://example.com/data/file_sample.html"] }
      );
    },
  };
  let xpi = AddonTestUtils.createTempWebExtensionFile(extensionData);

  let extension = ExtensionTestUtils.expectExtension(id);
  await AddonTestUtils.manuallyInstall(xpi);
  await promiseStartupManager();
  await extension.awaitStartup();

  await ExtensionTestUtils.fetch(
    "http://example.com/",
    "http://example.com/data/file_sample.html"
  );
  await extension.awaitMessage("got-request");

  await promiseShutdownManager();

  // Prepare a sideload update for the extension.
  extensionData.manifest.version = "2.0";
  extensionData.manifest.permissions = ["http://example.com/"];
  extensionData.manifest.optional_permissions = ["webRequest"];
  xpi = AddonTestUtils.createTempWebExtensionFile(extensionData);
  await AddonTestUtils.manuallyInstall(xpi);

  ExtensionParent._resetStartupPromises();
  await promiseStartupManager();
  await extension.awaitStartup();
  let events = trackEvents(extension);

  // Verify webRequest permission.
  let policy = WebExtensionPolicy.getByID(id);
  ok(policy.hasPermission("webRequest"), "addon webRequest permission added");

  await ExtensionTestUtils.fetch(
    "http://example.com/",
    "http://example.com/data/file_sample.html"
  );

  await testPersistentRequestStartup(extension, events, {
    background: true,
    delayedStart: true,
    request: true,
  });

  await extension.unload();
  await promiseShutdownManager();
});

// Utility to install builtin addon
async function installBuiltinExtension(extensionData) {
  let xpi = await AddonTestUtils.createTempWebExtensionFile(extensionData);

  // The built-in location requires a resource: URL that maps to a
  // jar: or file: URL.  This would typically be something bundled
  // into omni.ja but for testing we just use a temp file.
  let base = Services.io.newURI(`jar:file:${xpi.path}!/`);
  let resProto = Services.io
    .getProtocolHandler("resource")
    .QueryInterface(Ci.nsIResProtocolHandler);
  resProto.setSubstitution("ext-test", base);
  return AddonManager.installBuiltinAddon("resource://ext-test/");
}

function promisePostponeInstall(install) {
  return new Promise((resolve, reject) => {
    let listener = {
      onInstallFailed: () => {
        install.removeListener(listener);
        reject(new Error("extension installation should not have failed"));
      },
      onInstallEnded: () => {
        install.removeListener(listener);
        reject(
          new Error(
            `extension installation should not have ended for ${install.addon.id}`
          )
        );
      },
      onInstallPostponed: () => {
        install.removeListener(listener);
        resolve();
      },
    };

    install.addListener(listener);
    install.install();
  });
}

// Tests that moving permission to optional retains permission and that the
// persistent listeners are used as expected.
add_task(
  async function test_persistent_listener_after_builtin_location_upgrade() {
    let id = "permission-builtin-upgrade@test";
    let extensionData = {
      useAddonManager: "permanent",
      manifest: {
        version: "1.0",
        applications: { gecko: { id } },
        permissions: ["webRequest", "http://example.com/"],
      },

      async background() {
        browser.runtime.onUpdateAvailable.addListener(() => {
          browser.test.sendMessage("postponed");
        });

        browser.webRequest.onBeforeRequest.addListener(
          details => {
            browser.test.sendMessage("got-request");
          },
          { urls: ["http://example.com/data/file_sample.html"] }
        );
      },
    };
    await promiseStartupManager();
    // If we use an extension wrapper via ExtensionTestUtils.expectExtension
    // it will continue to handle messages even after the update, resulting
    // in errors when it receives additional messages without any awaitMessage.
    let promiseExtension = AddonTestUtils.promiseWebExtensionStartup(id);
    await installBuiltinExtension(extensionData);
    let extv1 = await promiseExtension;

    // Prepare an update for the extension.
    extensionData.manifest.version = "2.0";
    let xpi = AddonTestUtils.createTempWebExtensionFile(extensionData);
    let install = await AddonManager.getInstallForFile(xpi);

    // Install the update and wait for the onUpdateAvailable event to complete.
    let promiseUpdate = new Promise(resolve =>
      extv1.once("test-message", (kind, msg) => {
        if (msg == "postponed") {
          resolve();
        }
      })
    );
    await Promise.all([promisePostponeInstall(install), promiseUpdate]);
    await promiseShutdownManager();

    // restarting allows upgrade to proceed
    ExtensionParent._resetStartupPromises();
    let extension = ExtensionTestUtils.expectExtension(id);
    await promiseStartupManager();
    await extension.awaitStartup();
    let events = trackEvents(extension);

    await ExtensionTestUtils.fetch(
      "http://example.com/",
      "http://example.com/data/file_sample.html"
    );

    await testPersistentRequestStartup(extension, events, {
      background: true,
      delayedStart: true,
      request: true,
    });

    await extension.unload();

    // remove the builtin addon which will have restarted now.
    let addon = await AddonManager.getAddonByID(id);
    await addon.uninstall();

    await promiseShutdownManager();
  }
);

// Tests that moving permission to optional during a staged upgrade retains permission
// and that the persistent listeners are used as expected.
add_task(async function test_persistent_listener_after_staged_upgrade() {
  AddonManager.checkUpdateSecurity = false;
  let id = "persistent-staged-upgrade@test";

  // register an update file.
  AddonTestUtils.registerJSON(server, "/test_update.json", {
    addons: {
      "persistent-staged-upgrade@test": {
        updates: [
          {
            version: "2.0",
            update_link:
              "http://example.com/addons/test_settings_staged_restart.xpi",
          },
        ],
      },
    },
  });

  let extensionData = {
    useAddonManager: "permanent",
    manifest: {
      version: "2.0",
      applications: {
        gecko: { id, update_url: `http://example.com/test_update.json` },
      },
      permissions: ["http://example.com/"],
      optional_permissions: ["webRequest"],
    },

    background() {
      browser.webRequest.onBeforeRequest.addListener(
        details => {
          browser.test.sendMessage("got-request");
        },
        { urls: ["http://example.com/data/file_sample.html"] }
      );
      // Force a staged updated.
      browser.runtime.onUpdateAvailable.addListener(async details => {
        if (details && details.version) {
          // This should be the version of the pending update.
          browser.test.assertEq("2.0", details.version, "correct version");
          browser.test.sendMessage("delay");
        }
      });
    },
  };

  // Prepare the update first.
  server.registerFile(
    `/addons/test_settings_staged_restart.xpi`,
    AddonTestUtils.createTempWebExtensionFile(extensionData)
  );

  // Prepare the extension that will be updated.
  extensionData.manifest.version = "1.0";
  extensionData.manifest.permissions = ["webRequest", "http://example.com/"];
  delete extensionData.manifest.optional_permissions;

  await promiseStartupManager();
  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

  await ExtensionTestUtils.fetch(
    "http://example.com/",
    "http://example.com/data/file_sample.html"
  );
  await extension.awaitMessage("got-request");
  ok(true, "Initial version received webRequest event");

  let addon = await AddonManager.getAddonByID(id);
  Assert.equal(addon.version, "1.0", "1.0 is loaded");

  let update = await AddonTestUtils.promiseFindAddonUpdates(addon);
  let install = update.updateAvailable;
  Assert.ok(install, `install is available ${update.error}`);

  await AddonTestUtils.promiseCompleteAllInstalls([install]);

  Assert.equal(
    install.state,
    AddonManager.STATE_POSTPONED,
    "update is staged for install"
  );
  await extension.awaitMessage("delay");

  await promiseShutdownManager();

  // restarting allows upgrade to proceed
  ExtensionParent._resetStartupPromises();
  await promiseStartupManager();
  await extension.awaitStartup();
  let events = trackEvents(extension);

  // Verify webRequest permission.
  let policy = WebExtensionPolicy.getByID(id);
  ok(policy.hasPermission("webRequest"), "addon webRequest permission added");

  await ExtensionTestUtils.fetch(
    "http://example.com/",
    "http://example.com/data/file_sample.html"
  );

  await testPersistentRequestStartup(extension, events, {
    background: true,
    delayedStart: true,
    request: true,
  });

  await extension.unload();
  await promiseShutdownManager();
  AddonManager.checkUpdateSecurity = true;
});

// Tests that removing the permission releases the persistent listener.
add_task(async function test_persistent_listener_after_permission_removal() {
  let id = "persistent-staged-remove@test";

  // register an update file.
  AddonTestUtils.registerJSON(server, "/test_remove.json", {
    addons: {
      "persistent-staged-remove@test": {
        updates: [
          {
            version: "2.0",
            update_link:
              "http://example.com/addons/test_settings_staged_remove.xpi",
          },
        ],
      },
    },
  });

  let extensionData = {
    useAddonManager: "permanent",
    manifest: {
      version: "2.0",
      applications: {
        gecko: { id, update_url: `http://example.com/test_remove.json` },
      },
      permissions: ["tabs", "http://example.com/"],
    },

    background() {
      browser.test.sendMessage("loaded");
    },
  };

  // Prepare the update first.
  server.registerFile(
    `/addons/test_settings_staged_remove.xpi`,
    AddonTestUtils.createTempWebExtensionFile(extensionData)
  );

  await promiseStartupManager();
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      version: "1.0",
      applications: {
        gecko: { id, update_url: `http://example.com/test_remove.json` },
      },
      permissions: ["webRequest", "http://example.com/"],
    },

    background() {
      browser.webRequest.onBeforeRequest.addListener(
        details => {
          browser.test.sendMessage("got-request");
        },
        { urls: ["http://example.com/data/file_sample.html"] }
      );
      // Force a staged updated.
      browser.runtime.onUpdateAvailable.addListener(async details => {
        if (details && details.version) {
          // This should be the version of the pending update.
          browser.test.assertEq("2.0", details.version, "correct version");
          browser.test.sendMessage("delay");
        }
      });
    },
  });

  await extension.startup();

  await ExtensionTestUtils.fetch(
    "http://example.com/",
    "http://example.com/data/file_sample.html"
  );
  await extension.awaitMessage("got-request");
  ok(true, "Initial version received webRequest event");

  let addon = await AddonManager.getAddonByID(id);
  Assert.equal(addon.version, "1.0", "1.0 is loaded");

  let update = await AddonTestUtils.promiseFindAddonUpdates(addon);
  let install = update.updateAvailable;
  Assert.ok(install, `install is available ${update.error}`);

  await AddonTestUtils.promiseCompleteAllInstalls([install]);

  Assert.equal(
    install.state,
    AddonManager.STATE_POSTPONED,
    "update is staged for install"
  );
  await extension.awaitMessage("delay");

  await promiseShutdownManager();

  // restarting allows upgrade to proceed
  await promiseStartupManager();
  let events = trackEvents(extension);
  await extension.awaitStartup();

  // Verify webRequest permission.
  let policy = WebExtensionPolicy.getByID(id);
  ok(
    !policy.hasPermission("webRequest"),
    "addon webRequest permission removed"
  );

  await ExtensionTestUtils.fetch(
    "http://example.com/",
    "http://example.com/data/file_sample.html"
  );

  await testPersistentRequestStartup(extension, events, {
    background: false,
    delayedStart: false,
    request: false,
  });

  Services.obs.notifyObservers(null, "sessionstore-windows-restored");

  await extension.awaitMessage("loaded");
  ok(true, "Background page loaded");

  await extension.unload();
  await promiseShutdownManager();
});
