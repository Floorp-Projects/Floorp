/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
});

AddonTestUtils.init(this);
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);
AddonTestUtils.overrideCertDB();

add_task(async function setup() {
  ok(
    WebExtensionPolicy.useRemoteWebExtensions,
    "Expect remote-webextensions mode enabled"
  );
  ok(
    WebExtensionPolicy.backgroundServiceWorkerEnabled,
    "Expect remote-webextensions mode enabled"
  );

  await AddonTestUtils.promiseStartupManager();

  Services.prefs.setBoolPref("dom.serviceWorkers.testing.enabled", true);

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("dom.serviceWorkers.testing.enabled");
    Services.prefs.clearUserPref("dom.serviceWorkers.idle_timeout");
  });
});

add_task(
  async function test_fail_spawn_extension_worker_for_disabled_extension() {
    const extension = ExtensionTestUtils.loadExtension({
      useAddonManager: "temporary",
      manifest: {
        version: "1.0",
        background: {
          service_worker: "sw.js",
        },
        browser_specific_settings: { gecko: { id: "test-bg-sw@mochi.test" } },
      },
      files: {
        "page.html": "<!DOCTYPE html><body></body>",
        "sw.js": "dump('Background ServiceWorker - executed\\n');",
      },
    });

    const testWorkerWatcher = new TestWorkerWatcher();
    let watcher = await testWorkerWatcher.watchExtensionServiceWorker(
      extension
    );

    await extension.startup();

    info("Wait for the background service worker to be spawned");

    ok(
      await watcher.promiseWorkerSpawned,
      "The extension service worker has been spawned as expected"
    );

    info("Wait for the background service worker to be terminated");
    ok(
      await watcher.terminate(),
      "The extension service worker has been terminated as expected"
    );

    const swReg = testWorkerWatcher.getRegistration(extension);
    ok(swReg, "Got a service worker registration");
    ok(swReg?.activeWorker, "Got an active worker");

    info("Spawn the active worker by attaching the debugger");

    watcher = await testWorkerWatcher.watchExtensionServiceWorker(extension);

    swReg.activeWorker.attachDebugger();
    info(
      "Wait for the background service worker to be spawned after attaching the debugger"
    );
    ok(
      await watcher.promiseWorkerSpawned,
      "The extension service worker has been spawned as expected"
    );

    swReg.activeWorker.detachDebugger();
    info(
      "Wait for the background service worker to be terminated after detaching the debugger"
    );
    ok(
      await watcher.terminate(),
      "The extension service worker has been terminated as expected"
    );

    info(
      "Disabling the addon policy, and then double-check that the worker can't be spawned"
    );
    const policy = WebExtensionPolicy.getByID(extension.id);
    policy.active = false;

    await Assert.throws(
      () => swReg.activeWorker.attachDebugger(),
      /InvalidStateError/,
      "Got the expected extension when trying to spawn a worker for a disabled addon"
    );

    info(
      "Enabling the addon policy and double-check the worker is spawned successfully"
    );
    policy.active = true;

    watcher = await testWorkerWatcher.watchExtensionServiceWorker(extension);

    swReg.activeWorker.attachDebugger();
    info(
      "Wait for the background service worker to be spawned after attaching the debugger"
    );
    ok(
      await watcher.promiseWorkerSpawned,
      "The extension service worker has been spawned as expected"
    );

    swReg.activeWorker.detachDebugger();
    info(
      "Wait for the background service worker to be terminated after detaching the debugger"
    );
    ok(
      await watcher.terminate(),
      "The extension service worker has been terminated as expected"
    );

    await testWorkerWatcher.destroy();
    await extension.unload();
  }
);

add_task(async function test_serviceworker_lifecycle_events() {
  async function assertLifecycleEvents({ extension, expected, message }) {
    const getLifecycleEvents = async () => {
      const { active } = await this.content.navigator.serviceWorker.ready;
      const { port1, port2 } = new content.MessageChannel();

      return new Promise(resolve => {
        port1.onmessage = msg => resolve(msg.data.lifecycleEvents);
        active.postMessage("test", [port2]);
      });
    };
    const page = await ExtensionTestUtils.loadContentPage(
      extension.extension.baseURI.resolve("page.html"),
      { extension }
    );
    Assert.deepEqual(
      await page.spawn([], getLifecycleEvents),
      expected,
      `Got the expected lifecycle events on ${message}`
    );
    await page.close();
  }

  const swm = Cc["@mozilla.org/serviceworkers/manager;1"].getService(
    Ci.nsIServiceWorkerManager
  );

  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      version: "1.0",
      background: {
        service_worker: "sw.js",
      },
      browser_specific_settings: { gecko: { id: "test-bg-sw@mochi.test" } },
    },
    files: {
      "page.html": "<!DOCTYPE html><body></body>",
      "sw.js": `
        dump('Background ServiceWorker - executed\\n');

        const lifecycleEvents = [];
        self.oninstall = () => {
          dump('Background ServiceWorker - oninstall\\n');
          lifecycleEvents.push("install");
        };
        self.onactivate = () => {
          dump('Background ServiceWorker - onactivate\\n');
          lifecycleEvents.push("activate");
        };
        self.onmessage = (evt) => {
          dump('Background ServiceWorker - onmessage\\n');
          evt.ports[0].postMessage({ lifecycleEvents });
          dump('Background ServiceWorker - postMessage\\n');
        };
      `,
    },
  });

  const testWorkerWatcher = new TestWorkerWatcher();
  let watcher = await testWorkerWatcher.watchExtensionServiceWorker(extension);

  await extension.startup();

  await assertLifecycleEvents({
    extension,
    expected: ["install", "activate"],
    message: "initial worker registration",
  });

  const file = Services.dirsvc.get("ProfD", Ci.nsIFile);
  file.append("serviceworker.txt");
  await TestUtils.waitForCondition(
    () => file.exists(),
    "Wait for service worker registrations to have been dumped on disk"
  );

  const managerShutdownCompleted = AddonTestUtils.promiseShutdownManager();

  const firstSwReg = swm.getRegistrationByPrincipal(
    extension.extension.principal,
    extension.extension.principal.spec
  );
  // Force the worker shutdown (in normal condition the worker would have been
  // terminated as part of the entire application shutting down).
  firstSwReg.forceShutdown();

  info(
    "Wait for the background service worker to be terminated while the app is shutting down"
  );
  ok(
    await watcher.promiseWorkerTerminated,
    "The extension service worker has been terminated as expected"
  );
  await managerShutdownCompleted;

  Assert.equal(
    firstSwReg,
    swm.getRegistrationByPrincipal(
      extension.extension.principal,
      extension.extension.principal.spec
    ),
    "Expect the service worker to not be unregistered on application shutdown"
  );

  info("Restart AddonManager (mocking Browser instance restart)");
  // Start the addon manager with `earlyStartup: false` to keep the background service worker
  // from being started right away:
  //
  // - the call to `swm.reloadRegistrationForTest()` that follows is making sure that
  //   the previously registered service worker is in the same state it would be when
  //   the entire browser is restarted.
  //
  // - if the background service worker is being spawned again by the time we call
  //   `swm.reloadRegistrationForTest()`, ServiceWorkerUpdateJob would fail and trigger
  //   an `mState == State::Started` diagnostic assertion from ServiceWorkerJob::Finish
  //   and the xpcshell test will fail for the crash triggered by the assertion.
  await AddonTestUtils.promiseStartupManager({ lateStartup: false });
  await extension.awaitStartup();

  info(
    "Force reload ServiceWorkerManager registrations (mocking a Browser instance restart)"
  );
  swm.reloadRegistrationsForTest();

  info(
    "trigger delayed call to nsIServiceWorkerManager.registerForAddonPrincipal"
  );
  // complete the startup notifications, then start the background
  AddonTestUtils.notifyLateStartup();
  extension.extension.emit("start-background-script");

  info("Force activate the extension worker");
  const newSwReg = swm.getRegistrationByPrincipal(
    extension.extension.principal,
    extension.extension.principal.spec
  );

  Assert.notEqual(
    newSwReg,
    firstSwReg,
    "Expect the service worker registration to have been recreated"
  );

  await assertLifecycleEvents({
    extension,
    expected: [],
    message: "on previous registration loaded",
  });

  const { principal } = extension.extension;
  const addon = await AddonManager.getAddonByID(extension.id);
  await addon.disable();

  ok(
    await watcher.promiseWorkerTerminated,
    "The extension service worker has been terminated as expected"
  );

  Assert.throws(
    () => swm.getRegistrationByPrincipal(principal, principal.spec),
    /NS_ERROR_FAILURE/,
    "Expect the service worker to have been unregistered on addon disabled"
  );

  await addon.enable();
  await assertLifecycleEvents({
    extension,
    expected: ["install", "activate"],
    message: "on disabled addon re-enabled",
  });

  await testWorkerWatcher.destroy();
  await extension.unload();
});
