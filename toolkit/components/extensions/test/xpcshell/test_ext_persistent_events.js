"use strict";

// Delay loading until createAppInfo is called and setup.
ChromeUtils.defineESModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
});

const { ExtensionAPI } = ExtensionCommon;

// The code in this class does not actually run in this test scope, it is
// serialized into a string which is later loaded by the WebExtensions
// framework in the same context as other extension APIs.  By writing it
// this way rather than as a big string constant we get lint coverage.
// But eslint doesn't understand that this code runs in a different context
// where the EventManager class is available so just tell it here:
/* global EventManager */
const API = class extends ExtensionAPI {
  static namespace = undefined;
  primeListener(event, fire, params, isInStartup) {
    if (isInStartup && event == "nonBlockingEvent") {
      return;
    }
    // eslint-disable-next-line no-undef
    let { eventName, throwError, ignoreListener } =
      this.constructor.testOptions || {};
    let { namespace } = this.constructor;

    if (eventName == event) {
      if (throwError) {
        throw new Error(throwError);
      }
      if (ignoreListener) {
        return;
      }
    }

    Services.obs.notifyObservers(
      { namespace, event, fire, params },
      "prime-event-listener"
    );

    const FIRE_TOPIC = `fire-${namespace}.${event}`;

    async function listener(subject, topic, data) {
      try {
        if (subject.wrappedJSObject.waitForBackground) {
          await fire.wakeup();
        }
        await fire.async(subject.wrappedJSObject.listenerArgs);
      } catch (err) {
        let errSubject = { namespace, event, errorMessage: err.toString() };
        Services.obs.notifyObservers(errSubject, "listener-callback-exception");
      }
    }
    Services.obs.addObserver(listener, FIRE_TOPIC);

    return {
      unregister() {
        Services.obs.notifyObservers(
          { namespace, event, params },
          "unregister-primed-listener"
        );
        Services.obs.removeObserver(listener, FIRE_TOPIC);
      },
      convert(_fire) {
        Services.obs.notifyObservers(
          { namespace, event, params },
          "convert-event-listener"
        );
        fire = _fire;
      },
    };
  }

  getAPI(context) {
    let self = this;
    let { namespace } = this.constructor;

    // TODO: split into their own test tasks the expected value to be set on
    // EventManager resetIdleOnEvent in the following cases:
    // - an EventManager instance in the parent process
    //   - for an event page
    //   - for a persistent background page
    //   - for an extension context that isn't a background context
    // - an EventManager instance in the child process
    //   (for the same 3 kinds of contexts)
    const EventManagerWithAssertions = class extends EventManager {
      constructor(...args) {
        super(...args);
        this.assertResetOnIdleOnEvent();
      }

      assertResetOnIdleOnEvent() {
        const expectResetIdleOnEventFalse =
          this.context.extension.persistentBackground;
        if (expectResetIdleOnEventFalse && this.resetIdleOnEvent) {
          const details = {
            eventManagerName: this.name,
            resetIdleOnEvent: this.resetIdleOnEvent,
            envType: this.context.envType,
            viewType: this.context.viewType,
            isBackgroundContext: this.context.isBackgroundContext,
            persistentBackground: this.context.extension.persistentBackground,
          };
          throw new Error(
            `EventManagerWithAssertions: resetIdleOnEvent should be forcefully set to false - ${JSON.stringify(
              details
            )}`
          );
        }
      }
    };
    return {
      [namespace]: {
        testOptions(options) {
          // We want to be able to test errors on startup.
          // We use a global here because we test restarting AOM,
          // which causes the instance of this class to be destroyed.
          // eslint-disable-next-line no-undef
          self.constructor.testOptions = options;
        },
        onEvent1: new EventManagerWithAssertions({
          context,
          module: namespace,
          event: "onEvent1",
          register: (fire, ...params) => {
            let data = { namespace, event: "onEvent1", params };
            Services.obs.notifyObservers(data, "register-event-listener");
            return () => {
              Services.obs.notifyObservers(data, "unregister-event-listener");
            };
          },
        }).api(),

        onEvent2: new EventManagerWithAssertions({
          context,
          module: namespace,
          event: "onEvent2",
          register: (fire, ...params) => {
            let data = { namespace, event: "onEvent2", params };
            Services.obs.notifyObservers(data, "register-event-listener");
            return () => {
              Services.obs.notifyObservers(data, "unregister-event-listener");
            };
          },
        }).api(),

        onEvent3: new EventManagerWithAssertions({
          context,
          module: namespace,
          event: "onEvent3",
          register: (fire, ...params) => {
            let data = { namespace, event: "onEvent3", params };
            Services.obs.notifyObservers(data, "register-event-listener");
            return () => {
              Services.obs.notifyObservers(data, "unregister-event-listener");
            };
          },
        }).api(),

        nonBlockingEvent: new EventManagerWithAssertions({
          context,
          module: namespace,
          event: "nonBlockingEvent",
          register: (fire, ...params) => {
            let data = { namespace, event: "nonBlockingEvent", params };
            Services.obs.notifyObservers(data, "register-event-listener");
            return () => {
              Services.obs.notifyObservers(data, "unregister-event-listener");
            };
          },
        }).api(),
      },
    };
  }
};

function makeModule(namespace, options = {}) {
  const SCHEMA = [
    {
      namespace,
      functions: [
        {
          name: "testOptions",
          type: "function",
          async: true,
          parameters: [
            {
              name: "options",
              type: "object",
              additionalProperties: {
                type: "any",
              },
            },
          ],
        },
      ],
      events: [
        {
          name: "onEvent1",
          type: "function",
          extraParameters: [{ type: "any", optional: true }],
        },
        {
          name: "onEvent2",
          type: "function",
          extraParameters: [{ type: "any", optional: true }],
        },
        {
          name: "onEvent3",
          type: "function",
          extraParameters: [
            { type: "object", optional: true, additionalProperties: true },
            { type: "any", optional: true },
          ],
        },
        {
          name: "nonBlockingEvent",
          type: "function",
          extraParameters: [{ type: "any", optional: true }],
        },
      ],
    },
  ];

  const API_SCRIPT = `
    this.${namespace} = ${API.toString()};
    this.${namespace}.namespace = "${namespace}";
  `;

  // MODULE_INFO for registerModules
  let { startupBlocking } = options;
  return {
    schema: `data:,${JSON.stringify(SCHEMA)}`,
    scopes: ["addon_parent"],
    paths: [[namespace]],
    startupBlocking,
    url: URL.createObjectURL(new Blob([API_SCRIPT])),
  };
}

// Two modules, primary test module is startupBlocking
const MODULE_INFO = {
  startupBlocking: makeModule("startupBlocking", { startupBlocking: true }),
  nonStartupBlocking: makeModule("nonStartupBlocking"),
};

const global = this;

// Wait for the given event (topic) to occur a specific number of times
// (count).  If fn is not supplied, the Promise returned from this function
// resolves as soon as that many instances of the event have been observed.
// If fn is supplied, this function also waits for the Promise that fn()
// returns to complete and ensures that the given event does not occur more
// than `count` times before then.  On success, resolves with an array
// of the subjects from each of the observed events.
async function promiseObservable(topic, count, fn = null) {
  let _countResolve;
  let results = [];
  function listener(subject, _topic, data) {
    const eventDetails = subject.wrappedJSObject;
    results.push(eventDetails);
    if (results.length > count) {
      ok(
        false,
        `Got unexpected ${topic} event with ${JSON.stringify(eventDetails)}`
      );
    } else if (results.length == count) {
      _countResolve();
    }
  }
  Services.obs.addObserver(listener, topic);

  try {
    await Promise.all([
      new Promise(resolve => {
        _countResolve = resolve;
      }),
      fn && fn(),
    ]);
  } finally {
    Services.obs.removeObserver(listener, topic);
  }

  return results;
}

function trackEvents(wrapper) {
  let events = new Map();
  for (let event of ["background-script-event", "start-background-script"]) {
    events.set(event, false);
    wrapper.extension.once(event, () => events.set(event, true));
  }
  return events;
}

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));

add_setup(async function setup() {
  // The blob:-URL registered above in MODULE_INFO gets loaded at
  // https://searchfox.org/mozilla-central/rev/0fec57c05d3996cc00c55a66f20dd5793a9bfb5d/toolkit/components/extensions/ExtensionCommon.jsm#1649
  Services.prefs.setBoolPref(
    "security.allow_parent_unrestricted_js_loads",
    true
  );
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("security.allow_parent_unrestricted_js_loads");
  });

  AddonTestUtils.init(global);
  AddonTestUtils.overrideCertDB();
  AddonTestUtils.createAppInfo(
    "xpcshell@tests.mozilla.org",
    "XPCShell",
    "42",
    "42"
  );

  ExtensionParent.apiManager.registerModules(MODULE_INFO);
});

add_task(async function test_persistent_events() {
  await AddonTestUtils.promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    background() {
      let register1 = true,
        register2 = true;
      if (localStorage.getItem("skip1")) {
        register1 = false;
      }
      if (localStorage.getItem("skip2")) {
        register2 = false;
      }

      let listener1 = arg => browser.test.sendMessage("listener1", arg);
      let listener2 = arg => browser.test.sendMessage("listener2", arg);
      let listener3 = arg => browser.test.sendMessage("listener3", arg);

      if (register1) {
        browser.startupBlocking.onEvent1.addListener(listener1, "listener1");
      }
      if (register2) {
        browser.startupBlocking.onEvent1.addListener(listener2, "listener2");
        browser.startupBlocking.onEvent2.addListener(listener3, "listener3");
      }

      browser.test.onMessage.addListener(msg => {
        if (msg == "unregister2") {
          browser.startupBlocking.onEvent2.removeListener(listener3);
          localStorage.setItem("skip2", true);
        } else if (msg == "unregister1") {
          localStorage.setItem("skip1", true);
          browser.test.sendMessage("unregistered");
        }
      });

      browser.test.sendMessage("ready");
    },
  });

  function check(
    info,
    what,
    { listener1 = true, listener2 = true, listener3 = true } = {}
  ) {
    let count = (listener1 ? 1 : 0) + (listener2 ? 1 : 0) + (listener3 ? 1 : 0);
    equal(info.length, count, `Got ${count} ${what} events`);

    let i = 0;
    if (listener1) {
      equal(info[i].event, "onEvent1", `Got ${what} on event1 for listener 1`);
      deepEqual(
        info[i].params,
        ["listener1"],
        `Got event1 ${what} args for listener 1`
      );
      ++i;
    }

    if (listener2) {
      equal(info[i].event, "onEvent1", `Got ${what} on event1 for listener 2`);
      deepEqual(
        info[i].params,
        ["listener2"],
        `Got event1 ${what} args for listener 2`
      );
      ++i;
    }

    if (listener3) {
      equal(info[i].event, "onEvent2", `Got ${what} on event2 for listener 3`);
      deepEqual(
        info[i].params,
        ["listener3"],
        `Got event2 ${what} args for listener 3`
      );
      ++i;
    }
  }

  // Check that the regular event registration process occurs when
  // the extension is installed.
  let [observed] = await Promise.all([
    promiseObservable("register-event-listener", 3),
    extension.startup(),
  ]);
  check(observed, "register");

  await extension.awaitMessage("ready");

  // Check that the regular unregister process occurs when
  // the browser shuts down.
  [observed] = await Promise.all([
    promiseObservable("unregister-event-listener", 3),
    new Promise(resolve => extension.extension.once("shutdown", resolve)),
    AddonTestUtils.promiseShutdownManager(),
  ]);
  check(observed, "unregister");

  // Check that listeners are primed at the next browser startup.
  [observed] = await Promise.all([
    promiseObservable("prime-event-listener", 3),
    AddonTestUtils.promiseStartupManager(),
  ]);
  check(observed, "prime");

  assertPersistentListeners(extension, "startupBlocking", "onEvent1", {
    primed: true,
    primedListenersCount: 2,
  });

  assertPersistentListeners(extension, "startupBlocking", "onEvent2", {
    primed: true,
    primedListenersCount: 1,
  });

  // Check that primed listeners are converted to regular listeners
  // when the background page is started after browser startup.
  let p = promiseObservable("convert-event-listener", 3);
  AddonTestUtils.notifyLateStartup();
  observed = await p;

  check(observed, "convert");

  await extension.awaitMessage("ready");

  // Check that when the event is triggered, all the plumbing worked
  // correctly for the primed-then-converted listener.
  let listenerArgs = { test: "kaboom" };
  Services.obs.notifyObservers(
    { listenerArgs },
    "fire-startupBlocking.onEvent1"
  );

  let details = await extension.awaitMessage("listener1");
  deepEqual(details, listenerArgs, "Listener 1 fired");
  details = await extension.awaitMessage("listener2");
  deepEqual(details, listenerArgs, "Listener 2 fired");

  // Check that the converted listener is properly unregistered at
  // browser shutdown.
  [observed] = await Promise.all([
    promiseObservable("unregister-primed-listener", 3),
    AddonTestUtils.promiseShutdownManager(),
  ]);
  check(observed, "unregister");

  // Start up again, listener should be primed
  [observed] = await Promise.all([
    promiseObservable("prime-event-listener", 3),
    AddonTestUtils.promiseStartupManager(),
  ]);
  check(observed, "prime");

  // Check that triggering the event before the listener has been converted
  // causes the background page to be loaded and the listener to be converted,
  // and the listener is invoked.
  p = promiseObservable("convert-event-listener", 3);
  listenerArgs.test = "startup event";
  Services.obs.notifyObservers(
    { listenerArgs },
    "fire-startupBlocking.onEvent2"
  );
  observed = await p;

  check(observed, "convert");

  details = await extension.awaitMessage("listener3");
  deepEqual(details, listenerArgs, "Listener 3 fired for event during startup");

  await extension.awaitMessage("ready");

  // Check that triggering onEvent1 emits calls to both listener1 and listener2
  // (See Bug 1795801).
  [observed] = await Promise.all([
    promiseObservable("unregister-primed-listener", 3),
    AddonTestUtils.promiseShutdownManager(),
  ]);
  check(observed, "unregister");
  [observed] = await Promise.all([
    promiseObservable("prime-event-listener", 3),
    AddonTestUtils.promiseStartupManager(),
  ]);
  check(observed, "prime");
  assertPersistentListeners(extension, "startupBlocking", "onEvent1", {
    primed: true,
    primedListenersCount: 2,
  });
  assertPersistentListeners(extension, "startupBlocking", "onEvent2", {
    primed: true,
    primedListenersCount: 1,
  });

  p = promiseObservable("convert-event-listener", 3);
  listenerArgs.test = "startup event";
  Services.obs.notifyObservers(
    { listenerArgs },
    "fire-startupBlocking.onEvent1"
  );
  observed = await p;

  check(observed, "convert");

  const [detailsListener1Call, detailsListener2Call] = await Promise.all([
    extension.awaitMessage("listener1"),
    extension.awaitMessage("listener2"),
  ]);
  deepEqual(
    detailsListener1Call,
    listenerArgs,
    "Listener 1 fired for event during startup"
  );
  deepEqual(
    detailsListener2Call,
    listenerArgs,
    "Listener 2 fired for event during startup"
  );

  await extension.awaitMessage("ready");

  // Check that the unregister process works when we manually remove
  // a listener.
  p = promiseObservable("unregister-primed-listener", 1);
  extension.sendMessage("unregister2");
  observed = await p;
  check(observed, "unregister", { listener1: false, listener2: false });

  // Check that we only get unregisters for the remaining events after
  // one listener has been removed.
  observed = await promiseObservable("unregister-primed-listener", 2, () =>
    AddonTestUtils.promiseShutdownManager()
  );
  check(observed, "unregister", { listener3: false });

  // Check that after restart, only listeners that were present at
  // the end of the last session are primed.
  observed = await promiseObservable("prime-event-listener", 2, () =>
    AddonTestUtils.promiseStartupManager()
  );
  check(observed, "prime", { listener3: false });

  // Check that if the background script does not re-register listeners,
  // the primed listeners are unregistered after the background page
  // starts up.
  p = promiseObservable("unregister-primed-listener", 1, () =>
    extension.awaitMessage("ready")
  );

  AddonTestUtils.notifyLateStartup();
  observed = await p;
  check(observed, "unregister", { listener1: false, listener3: false });

  // Just listener1 should be registered now, fire event1 to confirm.
  listenerArgs.test = "third time";
  Services.obs.notifyObservers(
    { listenerArgs },
    "fire-startupBlocking.onEvent1"
  );
  details = await extension.awaitMessage("listener1");
  deepEqual(details, listenerArgs, "Listener 1 fired");

  // Tell the extension not to re-register listener1 on the next startup
  extension.sendMessage("unregister1");
  await extension.awaitMessage("unregistered");

  // Shut down, start up
  observed = await promiseObservable("unregister-primed-listener", 1, () =>
    AddonTestUtils.promiseShutdownManager()
  );
  check(observed, "unregister", { listener2: false, listener3: false });

  observed = await promiseObservable("prime-event-listener", 1, () =>
    AddonTestUtils.promiseStartupManager()
  );
  check(observed, "register", { listener2: false, listener3: false });

  // Check that firing event1 causes the listener fire callback to
  // reject.
  p = promiseObservable("listener-callback-exception", 1);
  Services.obs.notifyObservers(
    { listenerArgs, waitForBackground: true },
    "fire-startupBlocking.onEvent1"
  );
  equal(
    (await p)[0].errorMessage,
    "Error: primed listener startupBlocking.onEvent1 not re-registered",
    "Primed listener that was not re-registered received an error when event was triggered during startup"
  );

  await extension.awaitMessage("ready");

  await extension.unload();

  await AddonTestUtils.promiseShutdownManager();
});

// This test checks whether primed listeners are correctly unregistered when
// a background page load is interrupted. In particular, it verifies that the
// fire.wakeup() and fire.async() promises settle eventually.
add_task(async function test_shutdown_before_background_loaded() {
  await AddonTestUtils.promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    background() {
      let listener = arg => browser.test.sendMessage("triggered", arg);
      browser.startupBlocking.onEvent1.addListener(listener, "triggered");
      browser.test.sendMessage("bg_started");
    },
  });
  await Promise.all([
    promiseObservable("register-event-listener", 1),
    extension.startup(),
  ]);
  await extension.awaitMessage("bg_started");

  await Promise.all([
    promiseObservable("unregister-event-listener", 1),
    new Promise(resolve => extension.extension.once("shutdown", resolve)),
    AddonTestUtils.promiseShutdownManager(),
  ]);

  let primeListenerPromise = promiseObservable("prime-event-listener", 1);
  let fire;
  let fireWakeupBeforeBgFail;
  let fireAsyncBeforeBgFail;

  let bgAbortedPromise = new Promise(resolve => {
    let Management = ExtensionParent.apiManager;
    Management.once("extension-browser-inserted", (eventName, browser) => {
      browser.fixupAndLoadURIString = async () => {
        // The fire.wakeup/fire.async promises created while loading the
        // background page should settle when the page fails to load.
        fire = (await primeListenerPromise)[0].fire;
        fireWakeupBeforeBgFail = fire.wakeup();
        fireAsyncBeforeBgFail = fire.async();

        extension.extension.once("background-script-aborted", resolve);
        info("Forcing the background load to fail");
        browser.remove();
      };
    });
  });

  let unregisterPromise = promiseObservable("unregister-primed-listener", 1);

  await Promise.all([
    primeListenerPromise,
    AddonTestUtils.promiseStartupManager(),
  ]);
  await bgAbortedPromise;
  info("Loaded extension and aborted load of background page");

  await unregisterPromise;
  info("Primed listener has been unregistered");

  await fireWakeupBeforeBgFail;
  info("fire.wakeup() before background load failure should settle");

  await Assert.rejects(
    fireAsyncBeforeBgFail,
    /Error: listener not re-registered/,
    "fire.async before background load failure should be rejected"
  );

  await fire.wakeup();
  info("fire.wakeup() after background load failure should settle");

  await Assert.rejects(
    fire.async(),
    /Error: primed listener startupBlocking.onEvent1 not re-registered/,
    "fire.async after background load failure should be rejected"
  );

  info(
    "Expect fire.wakeup call after load failure to restart the background page"
  );
  await extension.awaitMessage("bg_started");

  await AddonTestUtils.promiseShutdownManager();

  // End of the abnormal shutdown test. Now restart the extension to verify
  // that the persistent listeners have not been unregistered.

  // Suppress background page start until an explicit notification.
  await Promise.all([
    promiseObservable("prime-event-listener", 1),
    AddonTestUtils.promiseStartupManager({ earlyStartup: false }),
  ]);
  info("Triggering persistent event to force the background page to start");
  Services.obs.notifyObservers(
    { listenerArgs: 123 },
    "fire-startupBlocking.onEvent1"
  );
  AddonTestUtils.notifyEarlyStartup();
  await extension.awaitMessage("bg_started");
  equal(await extension.awaitMessage("triggered"), 123, "triggered event");

  await Promise.all([
    promiseObservable("unregister-primed-listener", 1),
    AddonTestUtils.promiseShutdownManager(),
  ]);

  // And lastly, verify that a primed listener is correctly removed when the
  // extension unloads normally before the delayed background page can load.
  await Promise.all([
    promiseObservable("prime-event-listener", 1),
    AddonTestUtils.promiseStartupManager({ earlyStartup: false }),
  ]);

  info("Unloading extension before background page has loaded");
  await Promise.all([
    promiseObservable("unregister-primed-listener", 1),
    extension.unload(),
  ]);

  await AddonTestUtils.promiseShutdownManager();
});

// This test checks whether primed listeners are correctly primed to
// restart the background once the background has been shutdown or
// put to sleep.
add_task(async function test_background_restarted() {
  await AddonTestUtils.promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    background() {
      let listener = arg => browser.test.sendMessage("triggered", arg);
      browser.startupBlocking.onEvent1.addListener(listener, "triggered");
      browser.test.sendMessage("bg_started");
    },
  });
  await Promise.all([
    promiseObservable("register-event-listener", 1),
    extension.startup(),
  ]);
  await extension.awaitMessage("bg_started");
  assertPersistentListeners(extension, "startupBlocking", "onEvent1", {
    primed: false,
  });

  // Shutdown the background page
  await Promise.all([
    promiseObservable("unregister-event-listener", 1),
    extension.terminateBackground(),
  ]);
  // When sleeping the background, its events should become persisted
  assertPersistentListeners(extension, "startupBlocking", "onEvent1", {
    primed: true,
  });

  info("Triggering persistent event to force the background page to start");
  Services.obs.notifyObservers(
    { listenerArgs: 123 },
    "fire-startupBlocking.onEvent1"
  );
  await extension.awaitMessage("bg_started");
  equal(await extension.awaitMessage("triggered"), 123, "triggered event");

  await extension.unload();
  await AddonTestUtils.promiseShutdownManager();
});

// This test checks whether primed listeners are correctly primed to
// restart the background once the background has been shutdown or
// put to sleep.
add_task(
  { pref_set: [["extensions.eventPages.enabled", true]] },
  async function test_eventpage_startup() {
    await AddonTestUtils.promiseStartupManager();

    let extension = ExtensionTestUtils.loadExtension({
      useAddonManager: "permanent",
      manifest: {
        browser_specific_settings: { gecko: { id: "eventpage@test" } },
        background: { persistent: false },
      },
      background() {
        let listener = arg => browser.test.sendMessage("triggered", arg);
        browser.startupBlocking.onEvent1.addListener(listener, "triggered");
        let listenerNs = arg => browser.test.sendMessage("triggered-et2", arg);
        browser.nonStartupBlocking.onEvent1.addListener(
          listenerNs,
          "triggered-et2"
        );
        browser.test.onMessage.addListener(() => {
          let listener = arg => browser.test.sendMessage("triggered2", arg);
          browser.startupBlocking.onEvent2.addListener(listener, "triggered2");
          browser.test.sendMessage("async-registered-listener");
        });
        browser.test.sendMessage("bg_started");
      },
    });
    await Promise.all([
      promiseObservable("register-event-listener", 2),
      extension.startup(),
    ]);
    await extension.awaitMessage("bg_started");
    extension.sendMessage("async-register-listener");
    await extension.awaitMessage("async-registered-listener");

    async function testAfterRestart() {
      assertPersistentListeners(extension, "startupBlocking", "onEvent1", {
        primed: true,
      });
      // async registration should not be primed or persisted
      assertPersistentListeners(extension, "startupBlocking", "onEvent2", {
        primed: false,
        persisted: false,
      });

      let events = trackEvents(extension);
      ok(
        !events.get("background-script-event"),
        "Should not have received a background script event"
      );
      ok(
        !events.get("start-background-script"),
        "Background script should not be started"
      );

      info("Triggering persistent event to force the background page to start");
      let converted = promiseObservable("convert-event-listener", 1);
      Services.obs.notifyObservers(
        { listenerArgs: 123 },
        "fire-startupBlocking.onEvent1"
      );
      await extension.awaitMessage("bg_started");
      await converted;
      equal(await extension.awaitMessage("triggered"), 123, "triggered event");
      ok(
        events.get("background-script-event"),
        "Should have received a background script event"
      );
      ok(
        events.get("start-background-script"),
        "Background script should be started"
      );
    }

    // Shutdown the background page
    await Promise.all([
      promiseObservable("unregister-event-listener", 3),
      new Promise(resolve => extension.extension.once("shutdown", resolve)),
      AddonTestUtils.promiseShutdownManager(),
    ]);
    await AddonTestUtils.promiseStartupManager({ lateStartup: false });
    await extension.awaitStartup();
    assertPersistentListeners(extension, "nonStartupBlocking", "onEvent1", {
      primed: false,
      persisted: true,
    });
    await testAfterRestart();

    extension.sendMessage("async-register-listener");
    await extension.awaitMessage("async-registered-listener");

    // We sleep twice to ensure startup and shutdown work correctly
    info("test event listener registration during termination");
    let registrationEvents = Promise.all([
      promiseObservable("unregister-event-listener", 2),
      promiseObservable("unregister-primed-listener", 1),
      promiseObservable("prime-event-listener", 2),
    ]);
    await extension.terminateBackground();
    await registrationEvents;

    assertPersistentListeners(extension, "nonStartupBlocking", "onEvent1", {
      primed: true,
      persisted: true,
    });

    // Ensure onEvent2 does not fire, testAfterRestart will fail otherwise.
    Services.obs.notifyObservers(
      { listenerArgs: 123 },
      "fire-startupBlocking.onEvent2"
    );
    await testAfterRestart();

    registrationEvents = Promise.all([
      promiseObservable("unregister-primed-listener", 2),
      promiseObservable("prime-event-listener", 2),
    ]);
    await extension.terminateBackground();
    await registrationEvents;
    await testAfterRestart();

    await extension.unload();
    await AddonTestUtils.promiseShutdownManager();
  }
);

// This test verifies primeListener behavior for errors or ignored listeners.
add_task(async function test_background_primeListener_errors() {
  await AddonTestUtils.promiseStartupManager();

  // The internal APIs to shutdown the background work with any
  // background, and in the shutdown case, events will be persisted
  // and primed for a restart.
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    background() {
      // Listen for options being set so a restart will have them.
      browser.test.onMessage.addListener(async (message, options) => {
        if (message == "set-options") {
          await browser.startupBlocking.testOptions(options);
          browser.test.sendMessage("set-options:done");
        }
      });
      let listener = arg => browser.test.sendMessage("triggered", arg);
      browser.startupBlocking.onEvent1.addListener(listener, "triggered");
      let listener2 = arg => browser.test.sendMessage("triggered", arg);
      browser.startupBlocking.onEvent2.addListener(listener2, "triggered");
      browser.test.sendMessage("bg_started");
    },
  });
  await Promise.all([
    promiseObservable("register-event-listener", 1),
    extension.startup(),
  ]);
  await extension.awaitMessage("bg_started");
  assertPersistentListeners(extension, "startupBlocking", "onEvent1", {
    primed: false,
  });

  // If an event is removed from an api, a permission is removed,
  // or some other option prevents priming, ensure that
  // primelistener works correctly.
  // In this scenario we are testing that an event is not renewed
  // on startup because the API does not re-prime it.  The result
  // is that the event is also not persisted.  However the other
  // events that are renewed should still be primed and persisted.
  extension.sendMessage("set-options", {
    eventName: "onEvent1",
    ignoreListener: true,
  });
  await extension.awaitMessage("set-options:done");

  // Shutdown the background page
  await Promise.all([
    promiseObservable("unregister-event-listener", 2),
    extension.terminateBackground(),
  ]);
  // startupBlocking.onEvent1 was not re-primed and should not be persisted, but
  // onEvent2 should still be primed and persisted.
  assertPersistentListeners(extension, "startupBlocking", "onEvent1", {
    primed: false,
    persisted: false,
  });
  assertPersistentListeners(extension, "startupBlocking", "onEvent2", {
    primed: true,
  });

  info("Triggering persistent event to force the background page to start");
  Services.obs.notifyObservers(
    { listenerArgs: 123 },
    "fire-startupBlocking.onEvent2"
  );
  await extension.awaitMessage("bg_started");
  equal(await extension.awaitMessage("triggered"), 123, "triggered event");

  // On restart, test an exception, it should not be re-primed.
  extension.sendMessage("set-options", {
    eventName: "onEvent1",
    throwError: "error",
  });
  await extension.awaitMessage("set-options:done");

  // Shutdown the background page
  await Promise.all([
    promiseObservable("unregister-event-listener", 1),
    extension.terminateBackground(),
  ]);
  // startupBlocking.onEvent1 failed and should not be persisted
  assertPersistentListeners(extension, "startupBlocking", "onEvent1", {
    primed: false,
    persisted: false,
  });

  info("Triggering event to verify background starts after prior error");
  Services.obs.notifyObservers(
    { listenerArgs: 123 },
    "fire-startupBlocking.onEvent2"
  );
  await extension.awaitMessage("bg_started");
  equal(await extension.awaitMessage("triggered"), 123, "triggered event");

  info("reset options for next test");
  extension.sendMessage("set-options", {});
  await extension.awaitMessage("set-options:done");

  // Test errors on app restart
  info("Test errors during app startup");
  await AddonTestUtils.promiseRestartManager();
  await extension.awaitStartup();
  await extension.awaitMessage("bg_started");

  info("restart AOM and verify primed listener");
  await AddonTestUtils.promiseRestartManager({ earlyStartup: false });
  await extension.awaitStartup();
  assertPersistentListeners(extension, "startupBlocking", "onEvent1", {
    primed: true,
    persisted: true,
  });
  AddonTestUtils.notifyEarlyStartup();

  Services.obs.notifyObservers(
    { listenerArgs: 123 },
    "fire-startupBlocking.onEvent1"
  );
  await extension.awaitMessage("bg_started");
  equal(await extension.awaitMessage("triggered"), 123, "triggered event");

  // Test that an exception happening during priming clears the
  // event from being persisted when restarting the browser, and that
  // the background correctly starts.
  info("test exception during primeListener on startup");
  extension.sendMessage("set-options", {
    eventName: "onEvent1",
    throwError: "error",
  });
  await extension.awaitMessage("set-options:done");

  await AddonTestUtils.promiseRestartManager({ earlyStartup: false });
  await extension.awaitStartup();
  AddonTestUtils.notifyEarlyStartup();

  // At this point, the exception results in the persisted entry
  // being cleared.
  assertPersistentListeners(extension, "startupBlocking", "onEvent1", {
    primed: false,
    persisted: false,
  });

  AddonTestUtils.notifyLateStartup();

  await extension.awaitMessage("bg_started");

  // The background added the listener back during top level execution,
  // verify it is in the persisted list.
  assertPersistentListeners(extension, "startupBlocking", "onEvent1", {
    primed: false,
    persisted: true,
  });

  // reset options
  extension.sendMessage("set-options", {});
  await extension.awaitMessage("set-options:done");
  await extension.unload();
  await AddonTestUtils.promiseShutdownManager();
});

add_task(async function test_non_background_context_listener_not_persisted() {
  await AddonTestUtils.promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    background() {
      let listener = arg => browser.test.sendMessage("triggered", arg);
      browser.startupBlocking.onEvent1.addListener(listener, "triggered");
      browser.test.sendMessage(
        "bg_started",
        browser.runtime.getURL("extpage.html")
      );
    },
    files: {
      "extpage.html": `<script src="extpage.js"></script>`,
      "extpage.js": function () {
        let listener = arg =>
          browser.test.sendMessage("extpage-triggered", arg);
        browser.startupBlocking.onEvent2.addListener(
          listener,
          "extpage-triggered"
        );
        // Send a message to signal the extpage has registered the listener,
        // after calling an async method and wait it to be resolved to make sure
        // the addListener call to have been handled in the parent process by
        // the time we will assert the persisted listeners.
        browser.runtime.getPlatformInfo().then(() => {
          browser.test.sendMessage("extpage_started");
        });
      },
    },
  });

  await extension.startup();
  const extpage_url = await extension.awaitMessage("bg_started");

  assertPersistentListeners(extension, "startupBlocking", "onEvent1", {
    persisted: true,
    primed: false,
  });

  assertPersistentListeners(extension, "startupBlocking", "onEvent2", {
    persisted: false,
  });

  const page = await ExtensionTestUtils.loadContentPage(extpage_url);
  await extension.awaitMessage("extpage_started");

  // Expect the onEvent2 listener subscribed by the extpage to not be persisted.
  assertPersistentListeners(extension, "startupBlocking", "onEvent2", {
    persisted: false,
  });

  await page.close();
  await extension.unload();
  await AddonTestUtils.promiseShutdownManager();
});

// Test support for event page tests
const background = async function () {
  let listener2 = () =>
    browser.test.sendMessage("triggered:non-startupblocking");
  browser.startupBlocking.onEvent1.addListener(() => {});
  browser.startupBlocking.nonBlockingEvent.addListener(() => {});
  browser.nonStartupBlocking.onEvent2.addListener(listener2);
  browser.test.sendMessage("bg_started");
};

const background_update = async function () {
  browser.startupBlocking.onEvent1.addListener(() => {});
  browser.nonStartupBlocking.onEvent2.addListener(() => {});
  browser.test.sendMessage("updated_bg_started");
};

function testPersistentListeners(extension, expect) {
  for (let [ns, event, persisted, primed] of expect) {
    assertPersistentListeners(extension, ns, event, {
      persisted,
      primed,
    });
  }
}

add_task(
  { pref_set: [["extensions.eventPages.enabled", true]] },
  async function test_startupblocking_behavior() {
    await AddonTestUtils.promiseStartupManager();

    let extension = ExtensionTestUtils.loadExtension({
      useAddonManager: "permanent",
      manifest: {
        background: { persistent: false },
      },
      background,
    });

    await extension.startup();
    await extension.awaitMessage("bg_started");

    // All are persisted on startup
    testPersistentListeners(extension, [
      ["startupBlocking", "onEvent1", true, false],
      ["startupBlocking", "nonBlockingEvent", true, false],
      ["nonStartupBlocking", "onEvent2", true, false],
    ]);

    info("Test after mocked browser restart");
    await Promise.all([
      new Promise(resolve => extension.extension.once("shutdown", resolve)),
      AddonTestUtils.promiseShutdownManager(),
    ]);
    await AddonTestUtils.promiseStartupManager({ lateStartup: false });
    await extension.awaitStartup();

    testPersistentListeners(extension, [
      // Startup blocking event is expected to be persisted and primed.
      ["startupBlocking", "onEvent1", true, true],
      // A non-startup-blocking event shouldn't be primed yet.
      ["startupBlocking", "nonBlockingEvent", true, false],
      // Non "Startup blocking" event is expected to be persisted but not primed yet.
      ["nonStartupBlocking", "onEvent2", true, false],
    ]);

    // Complete the browser startup and fire the startup blocking event
    // to let the backgrund script to run.
    AddonTestUtils.notifyLateStartup();
    Services.obs.notifyObservers({}, "fire-startupBlocking.onEvent1");
    await extension.awaitMessage("bg_started");

    info("Test after terminate background script");
    await extension.terminateBackground();

    // After the background is terminated, all are persisted and primed.
    testPersistentListeners(extension, [
      ["startupBlocking", "onEvent1", true, true],
      ["startupBlocking", "nonBlockingEvent", true, true],
      ["nonStartupBlocking", "onEvent2", true, true],
    ]);

    info("Notify event for the non-startupBlocking API event");
    Services.obs.notifyObservers({}, "fire-nonStartupBlocking.onEvent2");
    await extension.awaitMessage("bg_started");
    await extension.awaitMessage("triggered:non-startupblocking");

    await extension.unload();
    await AddonTestUtils.promiseShutdownManager();
  }
);

add_task(
  { pref_set: [["extensions.eventPages.enabled", true]] },
  async function test_startupblocking_behavior_upgrade() {
    let id = "persistent-upgrade@test";
    await AddonTestUtils.promiseStartupManager();

    let extensionData = {
      useAddonManager: "permanent",
      manifest: {
        version: "1.0",
        browser_specific_settings: {
          gecko: { id },
        },
        background: { persistent: false },
      },
      background,
    };
    let extension = ExtensionTestUtils.loadExtension(extensionData);
    await extension.startup();
    await extension.awaitMessage("bg_started");

    // All are persisted on startup
    testPersistentListeners(extension, [
      ["startupBlocking", "onEvent1", true, false],
      ["startupBlocking", "nonBlockingEvent", true, false],
      ["nonStartupBlocking", "onEvent2", true, false],
    ]);

    // Prepare the extension that will be updated.
    extensionData.manifest.version = "2.0";
    extensionData.background = background_update;

    info("Test after a upgrade");
    await extension.upgrade(extensionData);
    // upgrade should start the background
    await extension.awaitMessage("updated_bg_started");

    // Nothing should be primed at this point after the background
    // has started.  We look specifically for nonBlockingEvent to
    // no longer be a part of the persisted listeners.
    testPersistentListeners(extension, [
      ["startupBlocking", "onEvent1", true, false],
      ["startupBlocking", "nonBlockingEvent", false, false],
      ["nonStartupBlocking", "onEvent2", true, false],
    ]);

    await extension.unload();
    await AddonTestUtils.promiseShutdownManager();
  }
);

add_task(
  { pref_set: [["extensions.eventPages.enabled", true]] },
  async function test_startupblocking_behavior_staged_upgrade() {
    AddonManager.checkUpdateSecurity = false;
    let id = "persistent-staged-upgrade@test";

    // register an update file.
    AddonTestUtils.registerJSON(server, "/test_update.json", {
      addons: {
        [id]: {
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
        background: { persistent: false },
      },
      background: background_update,
    };

    // Prepare the update first.
    server.registerFile(
      `/addons/test_settings_staged_restart.xpi`,
      AddonTestUtils.createTempWebExtensionFile(extensionData)
    );

    // Prepare the extension that will be updated.
    extensionData.manifest.version = "1.0";
    extensionData.background = async function () {
      // we're testing persistence, not operation, so no action in listeners.
      browser.startupBlocking.onEvent1.addListener(() => {});
      // nonBlockingEvent will be removed on upgrade
      browser.startupBlocking.nonBlockingEvent.addListener(() => {});
      browser.nonStartupBlocking.onEvent2.addListener(() => {});

      // Force a staged updated.
      browser.runtime.onUpdateAvailable.addListener(async details => {
        // This should be the version of the pending update.
        browser.test.assertEq("2.0", details.version, "correct version");
        browser.test.sendMessage("delay");
      });

      browser.test.sendMessage("bg_started");
    };

    await AddonTestUtils.promiseStartupManager();
    let extension = ExtensionTestUtils.loadExtension(extensionData);

    await extension.startup();
    await extension.awaitMessage("bg_started");

    // All are persisted but not primed on startup
    testPersistentListeners(extension, [
      ["startupBlocking", "onEvent1", true, false],
      ["startupBlocking", "nonBlockingEvent", true, false],
      ["nonStartupBlocking", "onEvent2", true, false],
    ]);

    info("Test after a staged update");
    // first, deal with getting and staging an upgrade
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

    await AddonTestUtils.promiseShutdownManager();

    // restarting allows upgrade to proceed
    await AddonTestUtils.promiseStartupManager();
    // upgrade should always start the background
    await extension.awaitMessage("updated_bg_started");

    // Since this is an upgraded addon, the background will have started
    // and we no longer have primed listeners.  Check only the persisted
    // values, and that nonBlockingEvent is not persisted.
    testPersistentListeners(extension, [
      ["startupBlocking", "onEvent1", true, false],
      ["startupBlocking", "nonBlockingEvent", false, false],
      ["nonStartupBlocking", "onEvent2", true, false],
    ]);

    await extension.unload();
    await AddonTestUtils.promiseShutdownManager();
  }
);

// Regression test for Bug 1795801:
// - verifies that multiple listeners sharing the same event and set of extra
//   params are being stored in the startupData and then all primed on the next
//   startup
// - verifies behaviors expected when startupData stored from an older
//   Firefox version (one that didn't include Bug 1795801 changes) is
//   loaded from a new Firefox version
// - a small smoke test to also verify the behaviors when startupData stored
//   by a newer version is being loaded by an older one (where Bug 1795801
//   changes have not been introduced yet).
add_task(async function test_migrate_startupData_to_new_format() {
  await AddonTestUtils.promiseStartupManager();

  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      background: { persistent: false },
    },
    background() {
      const eventParams = [
        { fromCustomParam1: "value1" },
        ["fromCustomParam2"],
      ];
      const otherEventParams = [
        { fromCustomParam1: "value2" },
        ["fromCustomParam2Other"],
      ];
      browser.nonStartupBlocking.onEvent3.addListener(function listener1(arg) {
        browser.test.log("listener1 called on nonStartupBlocking.onEvent3");
        browser.test.sendMessage("listener1", arg);
      }, ...eventParams);
      browser.nonStartupBlocking.onEvent3.addListener(function listener2(arg) {
        browser.test.log("listener2 called on nonStartupBlocking.onEvent3");
        browser.test.sendMessage("listener2", arg);
      }, ...eventParams);
      browser.nonStartupBlocking.onEvent3.addListener(function listener3(arg) {
        browser.test.log("listener3 called on nonStartupBlocking.onEvent3");
        browser.test.sendMessage("listener3", arg);
      }, ...otherEventParams);
      browser.test.sendMessage("ready");
    },
  });

  // Data expected to be stored in the extension startupData with the new
  // format and old format.
  const STARTUP_DATA = {
    newPersistentListenersFormat: {
      nonStartupBlocking: {
        onEvent3: [
          // 2 listeners registered with the same set of extra params
          [{ fromCustomParam1: "value1" }, ["fromCustomParam2"]],
          [{ fromCustomParam1: "value1" }, ["fromCustomParam2"]],
          // 1 listener registered with different set of extra params
          [{ fromCustomParam1: "value2" }, ["fromCustomParam2Other"]],
        ],
      },
    },
    oldPersistentListenersFormat: {
      nonStartupBlocking: {
        onEvent3: [
          [{ fromCustomParam1: "value1" }, ["fromCustomParam2"]],
          [{ fromCustomParam1: "value2" }, ["fromCustomParam2Other"]],
        ],
      },
    },
  };

  function getXPIStatesFilePath() {
    let { path } = ChromeUtils.import(
      "resource://gre/modules/addons/XPIProvider.jsm"
    ).XPIInternal.XPIStates._jsonFile;
    ok(
      typeof path === "string" && !!path.length,
      `Found XPIStates file path: ${path}`
    );
    return path;
  }

  async function tamperStartupData(testExtensionWrapper) {
    const { startupData } = testExtensionWrapper.extension;
    Assert.deepEqual(
      startupData.persistentListeners,
      STARTUP_DATA.newPersistentListenersFormat,
      "Got data stored from extension.startupData.persistentListeners"
    );

    startupData.persistentListeners = STARTUP_DATA.oldPersistentListenersFormat;

    // Force the data to be stored on disk (by requesting AddonTestUtils to flush
    // the XPIStates after having tampered them to make sure they are in the
    // format we expect from older Firefox versions).
    testExtensionWrapper.extension.saveStartupData();
    await AddonTestUtils.loadAddonsList(/* flush */ true);
    const { XPIInternal } = ChromeUtils.import(
      "resource://gre/modules/addons/XPIProvider.jsm"
    );
    XPIInternal.XPIStates.save();
    await XPIInternal.XPIStates._jsonFile._save();
    return getXPIStatesFilePath();
  }

  async function assertDiskStoredPersistentListeners(
    extensionId,
    xpiStatesPath,
    expectedData
  ) {
    const xpiStatesData = await IOUtils.readJSON(xpiStatesPath, {
      decompress: true,
    });
    const startupData =
      xpiStatesData["app-profile"]?.addons[extensionId]?.startupData;
    ok(startupData, `Found startupData for test extension ${extensionId}`);
    Assert.deepEqual(
      startupData.persistentListeners,
      expectedData,
      "Got the expected tampered addon startupData stored on disk"
    );
  }

  await extension.startup();

  await extension.awaitMessage("ready");
  assertPersistentListeners(extension, "nonStartupBlocking", "onEvent3", {
    persisted: true,
  });

  info(
    "Manually tampering startupData.persistentListeners to match the format older Firefox format"
  );
  const xpiStatesFilePath = await tamperStartupData(extension);
  await AddonTestUtils.promiseShutdownManager();
  await assertDiskStoredPersistentListeners(
    extension.id,
    xpiStatesFilePath,
    STARTUP_DATA.oldPersistentListenersFormat
  );

  info(
    "Confirm that the expected listeners have been primed and the startupData migrated to the new format"
  );

  {
    await AddonTestUtils.promiseStartupManager();
    await extension.awaitStartup;

    assertPersistentListeners(extension, "nonStartupBlocking", "onEvent3", {
      primed: true,
      // Old format of startupData.persistentListeners did not have a listenersCount
      // property and so only two primed listeners are expected on the first startup
      // after the addon startupData have been tampered to match the format expected
      // by an older Firefox version.
      primedListenersCount: 2,
    });

    const promiseListenersConverted = promiseObservable(
      "convert-event-listener",
      2
    );
    Services.obs.notifyObservers(
      { listenerArgs: "test-startup" },
      "fire-nonStartupBlocking.onEvent3"
    );
    await promiseListenersConverted;

    deepEqual(
      await extension.awaitMessage("listener1"),
      "test-startup",
      "Listener1 fired for event during startup"
    );

    deepEqual(
      await extension.awaitMessage("listener3"),
      "test-startup",
      "Listener3 fired for event during startup"
    );

    await extension.awaitMessage("ready");

    Assert.deepEqual(
      extension.extension.startupData.persistentListeners,
      STARTUP_DATA.newPersistentListenersFormat,
      "Got startupData.persistentListeners migrated to the new format"
    );
  }

  info(
    "Confirm that the startupData written on disk have been migrated to the new format"
  );

  await AddonTestUtils.promiseShutdownManager();
  await assertDiskStoredPersistentListeners(
    extension.id,
    xpiStatesFilePath,
    STARTUP_DATA.newPersistentListenersFormat
  );

  info(
    "Verify that both listeners are called after migrating to the new format"
  );
  {
    await AddonTestUtils.promiseStartupManager();
    await extension.awaitStartup;

    assertPersistentListeners(extension, "nonStartupBlocking", "onEvent3", {
      primed: true,
      primedListenersCount: 3,
    });

    const promiseListenersConverted = promiseObservable(
      "convert-event-listener",
      2
    );
    Services.obs.notifyObservers(
      { listenerArgs: "test-startup" },
      "fire-nonStartupBlocking.onEvent3"
    );
    await promiseListenersConverted;

    // Now we expect both the listeners to have been called.
    deepEqual(
      await extension.awaitMessage("listener1"),
      "test-startup",
      "Listener1 fired for event during startup"
    );

    deepEqual(
      await extension.awaitMessage("listener2"),
      "test-startup",
      "Listener2 fired for event during startup"
    );

    deepEqual(
      await extension.awaitMessage("listener3"),
      "test-startup",
      "Listener3 fired for event during startup"
    );

    await extension.awaitMessage("ready");
  }

  await extension.unload();
  await AddonTestUtils.promiseShutdownManager();

  // The additional assertions below are meant to provide a smoke test covering
  // the behavior we would expect if an older Firefox versions (one that would
  // expect the old format) is loading persistentListeners from startupData
  // using the new format.
  info("Verify backward compatibility with old format");

  const { ExtensionUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionUtils.sys.mjs"
  );
  const { DefaultMap } = ExtensionUtils;
  const loadedListeners = new DefaultMap(() => new DefaultMap(() => new Map()));

  // Logic from older Firefox versions expecting the old format
  // (https://searchfox.org/mozilla-central/rev/cd2121e7d8/toolkit/components/extensions/ExtensionCommon.jsm#2360-2371)
  let found = false;
  for (let [module, entry] of Object.entries(
    STARTUP_DATA.newPersistentListenersFormat
  )) {
    for (let [event, paramlists] of Object.entries(entry)) {
      for (let paramlist of paramlists) {
        let key = uneval(paramlist);
        loadedListeners.get(module).get(event).set(key, { params: paramlist });
        found = true;
      }
    }
  }

  Assert.ok(
    found,
    "Expect persistentListeners to have been found from the old loading logic"
  );

  // We expect the older Firefox version to don't choke on loading
  // the new format, a primed listener is still expected to be
  // found because the old Firefox version will be overriding a single
  // entry in the inmemory Map with the multiple entries from the
  // ondisk format listing the same extra params for multiple listeners,
  // Bug 1795801 would still be hit, but no other change in behavior is
  // expected to be hit with the old logic.
  Assert.ok(
    loadedListeners
      .get("nonStartupBlocking")
      .get("onEvent3")
      .has(uneval([{ fromCustomParam1: "value1" }, ["fromCustomParam2"]])),
    "Expect the listener params key to be found in older Firefox versions"
  );
});

add_task(
  { pref_set: [["extensions.eventPages.enabled", true]] },
  async function test_resetOnIdleOnEvent_false_on_other_extpages() {
    await AddonTestUtils.promiseStartupManager();

    const extension = ExtensionTestUtils.loadExtension({
      useAddonManager: "temporary",
      files: {
        "extpage.html": `<!DOCTYPE html><script src="extpage.js"></script>`,
        "extpage.js": function () {
          // We expect this to throw if the EventManagerWithAssertions constructor
          // throws when asserting that resetIdleOnEvent was forcefully set to
          // false for a non-event page context.
          browser.nonStartupBlocking.onEvent2.addListener(() => {});
          browser.test.sendMessage("extpage:loaded");
        },
      },
    });

    await extension.startup();

    const awaitRegisteredEventListener = promiseObservable(
      "register-event-listener",
      1
    );
    const page = await ExtensionTestUtils.loadContentPage(
      `moz-extension://${extension.uuid}/extpage.html`
    );

    info("Wait for the extension page script to complete");

    await Promise.all([
      extension.awaitMessage("extpage:loaded"),
      awaitRegisteredEventListener,
    ]);
    await page.close();

    await extension.unload();
    await AddonTestUtils.promiseShutdownManager();
  }
);
