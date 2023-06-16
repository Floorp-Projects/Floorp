"use strict";

// Delay loading until createAppInfo is called and setup.
ChromeUtils.defineESModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
});

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

let { promiseShutdownManager, promiseStartupManager, promiseRestartManager } =
  AddonTestUtils;

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));

let scopes = AddonManager.SCOPE_PROFILE | AddonManager.SCOPE_APPLICATION;
Services.prefs.setIntPref("extensions.enabledScopes", scopes);

function trackEvents(wrapper) {
  let events = new Map();
  for (let event of ["background-script-event", "start-background-script"]) {
    events.set(event, false);
    wrapper.extension.once(event, () => events.set(event, true));
  }
  return events;
}

/**
 * That that we get the expected events
 *
 * @param {Extension} extension
 * @param {Map} events
 * @param {object} expect
 * @param {boolean} expect.background   delayed startup event expected
 * @param {boolean} expect.started      background has already started
 * @param {boolean} expect.delayedStart startup is delayed, notify start and
 *                                      expect the starting event
 * @param {boolean} expect.request      wait for the request event
 */
async function testPersistentRequestStartup(extension, events, expect = {}) {
  equal(
    events.get("background-script-event"),
    !!expect.background,
    "Should have gotten a background script event"
  );
  equal(
    events.get("start-background-script"),
    !!expect.started,
    "Background script should be started"
  );

  if (!expect.started) {
    AddonTestUtils.notifyEarlyStartup();
    await ExtensionParent.browserPaintedPromise;

    equal(
      events.get("start-background-script"),
      !!expect.delayedStart,
      "Should have gotten start-background-script event"
    );
  }

  if (expect.request) {
    await extension.awaitMessage("got-request");
    ok(true, "Background page loaded and received webRequest event");
  }
}

// Test that a non-blocking listener does not start the background on
// startup, but that it does work after startup.
add_task(async function test_nonblocking() {
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
      browser.test.sendMessage("ready");
    },
  });

  // First install runs background immediately, this sets persistent listeners
  await extension.startup();
  await extension.awaitMessage("ready");

  // Restart to get APP_STARTUP, the background should not start
  await promiseRestartManager({ lateStartup: false });
  await extension.awaitStartup();
  assertPersistentListeners(extension, "webRequest", "onBeforeRequest", {
    primed: false,
  });

  // Test an early startup event
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

  AddonTestUtils.notifyLateStartup();
  await extension.awaitMessage("ready");
  assertPersistentListeners(extension, "webRequest", "onBeforeRequest", {
    primed: false,
  });

  // Test an event after startup
  await ExtensionTestUtils.fetch(
    "http://example.com/",
    "http://example.com/data/file_sample.html"
  );

  await testPersistentRequestStartup(extension, events, {
    background: false,
    started: true,
    request: true,
  });

  await extension.unload();

  await promiseShutdownManager();
});

// Test that a non-blocking listener does not start the background on
// startup, but that it does work after startup.
add_task(async function test_eventpage_nonblocking() {
  Services.prefs.setBoolPref("extensions.eventPages.enabled", true);
  await promiseStartupManager();

  let id = "event-nonblocking@test";
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      browser_specific_settings: { gecko: { id } },
      permissions: ["webRequest", "http://example.com/"],
      background: { persistent: false },
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

  // First install runs background immediately, this sets persistent listeners
  await extension.startup();

  // Restart to get APP_STARTUP, the background should not start
  await promiseRestartManager({ lateStartup: false });
  await extension.awaitStartup();
  assertPersistentListeners(extension, "webRequest", "onBeforeRequest", {
    primed: false,
  });

  // Test an early startup event
  let events = trackEvents(extension);

  await ExtensionTestUtils.fetch(
    "http://example.com/",
    "http://example.com/data/file_sample.html"
  );

  await testPersistentRequestStartup(extension, events);

  await AddonTestUtils.notifyLateStartup();
  // After late startup, event page listeners should be primed.
  assertPersistentListeners(extension, "webRequest", "onBeforeRequest", {
    primed: true,
  });

  // We should not have seen any events yet.
  await testPersistentRequestStartup(extension, events);

  // Test an event after startup
  await ExtensionTestUtils.fetch(
    "http://example.com/",
    "http://example.com/data/file_sample.html"
  );

  // Now the event page should be started and we'll see the request.
  await testPersistentRequestStartup(extension, events, {
    background: true,
    started: true,
    request: true,
  });

  await extension.unload();

  await promiseShutdownManager();
  Services.prefs.setBoolPref("extensions.eventPages.enabled", false);
});

// Tests that filters are handled properly: if we have a blocking listener
// with a filter, a request that does not match the filter does not get
// suspended and does not start the background page.
add_task(async function test_persistent_blocking() {
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
    },
  });

  await extension.startup();
  assertPersistentListeners(extension, "webRequest", "onBeforeRequest", {
    primed: false,
  });

  await promiseRestartManager({ lateStartup: false });
  await extension.awaitStartup();
  assertPersistentListeners(extension, "webRequest", "onBeforeRequest", {
    primed: true,
  });

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

  AddonTestUtils.notifyLateStartup();

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
      browser_specific_settings: { gecko: { id } },
      permissions: ["webRequest", "webRequestBlocking", "http://example.com/"],
    },

    background() {
      browser.webRequest.onBeforeRequest.addListener(
        details => {
          browser.test.sendMessage("got-request");
        },
        { urls: ["http://example.com/data/file_sample.html"] },
        ["blocking"]
      );
    },
  };
  let xpi = AddonTestUtils.createTempWebExtensionFile(extensionData);

  let extension = ExtensionTestUtils.expectExtension(id);
  await AddonTestUtils.manuallyInstall(xpi);
  await promiseStartupManager();
  await extension.awaitStartup();
  // Sideload install does not prime listeners
  assertPersistentListeners(extension, "webRequest", "onBeforeRequest", {
    primed: false,
  });

  await ExtensionTestUtils.fetch(
    "http://example.com/",
    "http://example.com/data/file_sample.html"
  );
  await extension.awaitMessage("got-request");

  await promiseShutdownManager();

  // Prepare a sideload update for the extension.
  extensionData.manifest.version = "2.0";
  extensionData.manifest.permissions = ["http://example.com/"];
  extensionData.manifest.optional_permissions = [
    "webRequest",
    "webRequestBlocking",
  ];
  xpi = AddonTestUtils.createTempWebExtensionFile(extensionData);
  await AddonTestUtils.manuallyInstall(xpi);

  await promiseStartupManager();
  await extension.awaitStartup();
  // Upgrades start the background when the extension is loaded, so
  // primed listeners are cleared already and background events are
  // already completed.
  assertPersistentListeners(extension, "webRequest", "onBeforeRequest", {
    primed: false,
    persisted: true,
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
        browser_specific_settings: { gecko: { id } },
        permissions: [
          "webRequest",
          "webRequestBlocking",
          "http://example.com/",
        ],
      },

      async background() {
        browser.runtime.onUpdateAvailable.addListener(() => {
          browser.test.sendMessage("postponed");
        });

        browser.webRequest.onBeforeRequest.addListener(
          details => {
            browser.test.sendMessage("got-request");
          },
          { urls: ["http://example.com/data/file_sample.html"] },
          ["blocking"]
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
    assertPersistentListeners(
      { extension: extv1 },
      "webRequest",
      "onBeforeRequest",
      {
        primed: false,
      }
    );

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
    let extension = ExtensionTestUtils.expectExtension(id);
    await promiseStartupManager();
    await extension.awaitStartup();
    // Upgrades start the background when the extension is loaded, so
    // primed listeners are cleared already and background events are
    // already completed.
    assertPersistentListeners(extension, "webRequest", "onBeforeRequest", {
      primed: false,
      persisted: true,
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
      browser_specific_settings: {
        gecko: { id, update_url: `http://example.com/test_update.json` },
      },
      permissions: ["http://example.com/"],
      optional_permissions: ["webRequest", "webRequestBlocking"],
    },

    background() {
      browser.webRequest.onBeforeRequest.addListener(
        details => {
          browser.test.sendMessage("got-request");
        },
        { urls: ["http://example.com/data/file_sample.html"] },
        ["blocking"]
      );
      browser.webRequest.onSendHeaders.addListener(
        details => {
          browser.test.sendMessage("got-sendheaders");
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
  extensionData.manifest.permissions = [
    "webRequest",
    "webRequestBlocking",
    "http://example.com/",
  ];
  delete extensionData.manifest.optional_permissions;
  extensionData.background = function () {
    browser.webRequest.onBeforeRequest.addListener(
      details => {
        browser.test.sendMessage("got-request");
      },
      { urls: ["http://example.com/data/file_sample.html"] },
      ["blocking"]
    );
    browser.webRequest.onBeforeSendHeaders.addListener(
      details => {
        browser.test.sendMessage("got-beforesendheaders");
      },
      { urls: ["http://example.com/data/file_sample.html"] }
    );
    browser.webRequest.onSendHeaders.addListener(
      details => {
        browser.test.sendMessage("got-sendheaders");
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
  };

  await promiseStartupManager();
  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();
  assertPersistentListeners(extension, "webRequest", "onBeforeRequest", {
    primed: false,
  });
  assertPersistentListeners(extension, "webRequest", "onBeforeSendHeaders", {
    primed: false,
  });
  assertPersistentListeners(extension, "webRequest", "onSendHeaders", {
    primed: false,
  });

  await ExtensionTestUtils.fetch(
    "http://example.com/",
    "http://example.com/data/file_sample.html"
  );
  await extension.awaitMessage("got-request");
  await extension.awaitMessage("got-beforesendheaders");
  await extension.awaitMessage("got-sendheaders");
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
  await extension.awaitStartup();

  // Upgrades start the background when the extension is loaded, so
  // primed listeners are cleared already and background events are
  // already completed.
  assertPersistentListeners(extension, "webRequest", "onBeforeRequest", {
    primed: false,
    persisted: true,
  });
  // this was removed in the upgrade background, should not be persisted.
  assertPersistentListeners(extension, "webRequest", "onBeforeSendHeaders", {
    primed: false,
    persisted: false,
  });
  assertPersistentListeners(extension, "webRequest", "onSendHeaders", {
    primed: false,
    persisted: true,
  });

  await extension.unload();
  await promiseShutdownManager();
  AddonManager.checkUpdateSecurity = true;
});

// Tests that removing the permission releases the persistent listener.
add_task(async function test_persistent_listener_after_permission_removal() {
  AddonManager.checkUpdateSecurity = false;
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
      browser_specific_settings: {
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
      browser_specific_settings: {
        gecko: { id, update_url: `http://example.com/test_remove.json` },
      },
      permissions: ["webRequest", "webRequestBlocking", "http://example.com/"],
    },

    background() {
      browser.webRequest.onBeforeRequest.addListener(
        details => {
          browser.test.sendMessage("got-request");
        },
        { urls: ["http://example.com/data/file_sample.html"] },
        ["blocking"]
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
  await promiseStartupManager({ lateStartup: false });
  await extension.awaitStartup();
  await extension.awaitMessage("loaded");

  // Upgrades start the background when the extension is loaded, so
  // primed listeners are cleared already and background events are
  // already completed.
  assertPersistentListeners(extension, "webRequest", "onBeforeRequest", {
    primed: false,
    persisted: false,
  });

  await extension.unload();
  await promiseShutdownManager();
  AddonManager.checkUpdateSecurity = true;
});
