/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "AddonManager",
  "resource://gre/modules/AddonManager.jsm"
);

const { EventEmitter } = ExtensionCommon;

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
  // Ensure that the profile-after-change message has been notified,
  // so that ServiceWokerRegistrar is going to be initialized.
  Services.obs.notifyObservers(
    null,
    "profile-after-change",
    "force-serviceworkerrestart-init"
  );

  // Make sure background-delayed-startup is set to true (in some builds,
  // in particular Thunderbird, it is set to false) otherwise the extension
  // service worker will be started before the test cases can properly
  // mock the behavior expected on browser startup by calling the
  // nsIServiceWorkerManager.reloadRegistrationsForTest (and then the test task
  // test_serviceworker_lifecycle_events will fail because the worker will
  // refuse to be spawned while the extension is still disabled).
  Services.prefs.setBoolPref(
    "extensions.webextensions.background-delayed-startup",
    true
  );

  Services.prefs.setBoolPref("dom.serviceWorkers.testing.enabled", true);

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(
      "extensions.webextensions.background-delayed-startup"
    );
    Services.prefs.clearUserPref("dom.serviceWorkers.testing.enabled");
    Services.prefs.clearUserPref("dom.serviceWorkers.idle_timeout");
  });
});

// A test utility class used in the test case to watch for a given extension
// service worker being spawned and terminated (using the same kind of Firefox DevTools
// internals that about:debugging is using to watch the workers activity).
class TestWorkerWatcher extends EventEmitter {
  JS_ACTOR_NAME = "TestWorkerWatcher";

  constructor() {
    super();
    this.extensionProcess = null;
    this.extensionProcessActor = null;
    this.registerProcessActor();
    this.getAndWatchExtensionProcess();
    // Observer child process creation and shutdown if the extension
    // are meant to run in a child process.
    Services.obs.addObserver(this, "ipc:content-created");
    Services.obs.addObserver(this, "ipc:content-shutdown");
  }

  async destroy() {
    await this.stopWatchingWorkers();
    ChromeUtils.unregisterProcessActor(this.JS_ACTOR_NAME);
  }

  getRegistration(extension) {
    const swm = Cc["@mozilla.org/serviceworkers/manager;1"].getService(
      Ci.nsIServiceWorkerManager
    );

    return swm.getRegistrationByPrincipal(
      extension.extension.principal,
      extension.extension.principal.spec
    );
  }

  watchExtensionServiceWorker(extension) {
    // These events are emitted by TestWatchExtensionWorkersParent.
    const promiseWorkerSpawned = this.waitForEvent("worker-spawned", extension);
    const promiseWorkerTerminated = this.waitForEvent(
      "worker-terminated",
      extension
    );

    // Terminate the worker sooner by settng the idle_timeout to 0,
    // then clear the pref as soon as the worker has been terminated.
    const terminate = () => {
      promiseWorkerTerminated.then(() => {
        Services.prefs.clearUserPref("dom.serviceWorkers.idle_timeout");
      });
      Services.prefs.setIntPref("dom.serviceWorkers.idle_timeout", 0);
      const swReg = this.getRegistration(extension);
      // If the active worker is already active, we have to make sure the new value
      // set on the idle_timeout pref is picked up by ServiceWorkerPrivate::ResetIdleTimeout.
      swReg.activeWorker?.attachDebugger();
      swReg.activeWorker?.detachDebugger();
      return promiseWorkerTerminated;
    };

    return {
      promiseWorkerSpawned,
      promiseWorkerTerminated,
      terminate,
    };
  }

  // Methods only used internally.

  waitForEvent(event, extension) {
    return new Promise(resolve => {
      const listener = (_eventName, data) => {
        if (!data.workerUrl.startsWith(extension.extension?.principal.spec)) {
          return;
        }
        this.off(event, listener);
        resolve(data);
      };

      this.on(event, listener);
    });
  }

  registerProcessActor() {
    const { JS_ACTOR_NAME } = this;
    const getModuleURI = relPath =>
      Services.io.newFileURI(do_get_file(relPath)).spec;
    ChromeUtils.registerProcessActor(JS_ACTOR_NAME, {
      parent: {
        moduleURI: getModuleURI(`data/${JS_ACTOR_NAME}Parent.jsm`),
      },
      child: {
        moduleURI: getModuleURI(`data/${JS_ACTOR_NAME}Child.jsm`),
      },
    });
  }

  startWatchingWorkers() {
    if (!this.extensionProcessActor) {
      return;
    }
    this.extensionProcessActor.eventEmitter = this;
    return this.extensionProcessActor.sendQuery("Test:StartWatchingWorkers");
  }

  stopWatchingWorkers() {
    if (!this.extensionProcessActor) {
      return;
    }
    this.extensionProcessActor.eventEmitter = null;
    return this.extensionProcessActor.sendQuery("Test:StopWatchingWorkers");
  }

  getAndWatchExtensionProcess() {
    const extensionProcess = ChromeUtils.getAllDOMProcesses().find(p => {
      return p.remoteType === "extension";
    });
    if (extensionProcess !== this.extensionProcess) {
      this.extensionProcess = extensionProcess;
      this.extensionProcessActor = extensionProcess
        ? extensionProcess.getActor(this.JS_ACTOR_NAME)
        : null;
      this.startWatchingWorkers();
    }
  }

  observe(subject, topic, childIDString) {
    // Keep the watched process and related test child process actor updated
    // when a process is created or destroyed.
    this.getAndWatchExtensionProcess();
  }
}

add_task(
  async function test_fail_spawn_extension_worker_for_disabled_extension() {
    const extension = ExtensionTestUtils.loadExtension({
      useAddonManager: "temporary",
      manifest: {
        version: "1.0",
        background: {
          service_worker: "sw.js",
        },
        applications: { gecko: { id: "test-bg-sw@mochi.test" } },
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
      applications: { gecko: { id: "test-bg-sw@mochi.test" } },
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
  ExtensionParent._resetStartupPromises();
  await AddonTestUtils.promiseStartupManager();
  await extension.awaitStartup();

  info(
    "Force reload ServiceWorkerManager registrations (mocking a Browser instance restart)"
  );
  swm.reloadRegistrationsForTest();

  info(
    "trigger delayed call to nsIServiceWorkerManager.registerForAddonPrincipal"
  );
  extension.extension.emit("start-background-page");

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
