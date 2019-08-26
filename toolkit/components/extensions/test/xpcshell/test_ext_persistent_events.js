"use strict";

const { ExtensionCommon } = ChromeUtils.import(
  "resource://gre/modules/ExtensionCommon.jsm"
);
const { ExtensionAPI } = ExtensionCommon;

const SCHEMA = [
  {
    namespace: "eventtest",
    events: [
      {
        name: "onEvent1",
        type: "function",
        extraParameters: [{ type: "any" }],
      },
      {
        name: "onEvent2",
        type: "function",
        extraParameters: [{ type: "any" }],
      },
    ],
  },
];

// The code in this class does not actually run in this test scope, it is
// serialized into a string which is later loaded by the WebExtensions
// framework in the same context as other extension APIs.  By writing it
// this way rather than as a big string constant we get lint coverage.
// But eslint doesn't understand that this code runs in a different context
// where the EventManager class is available so just tell it here:
/* global EventManager */
const API = class extends ExtensionAPI {
  primeListener(extension, event, fire, params) {
    Services.obs.notifyObservers({ event, params }, "prime-event-listener");

    const FIRE_TOPIC = `fire-${event}`;

    async function listener(subject, topic, data) {
      try {
        if (subject.wrappedJSObject.waitForBackground) {
          await fire.wakeup();
        }
        await fire.async(subject.wrappedJSObject.listenerArgs);
      } catch (err) {
        Services.obs.notifyObservers({ event }, "listener-callback-exception");
      }
    }
    Services.obs.addObserver(listener, FIRE_TOPIC);

    return {
      unregister() {
        Services.obs.notifyObservers(
          { event, params },
          "unregister-primed-listener"
        );
        Services.obs.removeObserver(listener, FIRE_TOPIC);
      },
      convert(_fire) {
        Services.obs.notifyObservers(
          { event, params },
          "convert-event-listener"
        );
        fire = _fire;
      },
    };
  }

  getAPI(context) {
    return {
      eventtest: {
        onEvent1: new EventManager({
          context,
          name: "test.event1",
          persistent: {
            module: "eventtest",
            event: "onEvent1",
          },
          register: (fire, ...params) => {
            let data = { event: "onEvent1", params };
            Services.obs.notifyObservers(data, "register-event-listener");
            return () => {
              Services.obs.notifyObservers(data, "unregister-event-listener");
            };
          },
        }).api(),

        onEvent2: new EventManager({
          context,
          name: "test.event1",
          persistent: {
            module: "eventtest",
            event: "onEvent2",
          },
          register: (fire, ...params) => {
            let data = { event: "onEvent2", params };
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

const API_SCRIPT = `this.eventtest = ${API.toString()}`;

const MODULE_INFO = {
  eventtest: {
    schema: `data:,${JSON.stringify(SCHEMA)}`,
    scopes: ["addon_parent"],
    paths: [["eventtest"]],
    url: URL.createObjectURL(new Blob([API_SCRIPT])),
  },
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
    results.push(subject.wrappedJSObject);
    if (results.length > count) {
      ok(false, `Got unexpected ${topic} event`);
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

add_task(async function() {
  Services.prefs.setBoolPref(
    "extensions.webextensions.background-delayed-startup",
    true
  );

  AddonTestUtils.init(global);
  AddonTestUtils.overrideCertDB();
  AddonTestUtils.createAppInfo(
    "xpcshell@tests.mozilla.org",
    "XPCShell",
    "1",
    "43"
  );

  await AddonTestUtils.promiseStartupManager();

  ExtensionParent.apiManager.registerModules(MODULE_INFO);

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
        browser.eventtest.onEvent1.addListener(listener1, "listener1");
      }
      if (register2) {
        browser.eventtest.onEvent1.addListener(listener2, "listener2");
        browser.eventtest.onEvent2.addListener(listener3, "listener3");
      }

      browser.test.onMessage.addListener(msg => {
        if (msg == "unregister2") {
          browser.eventtest.onEvent2.removeListener(listener3);
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
  let [info] = await Promise.all([
    promiseObservable("register-event-listener", 3),
    extension.startup(),
  ]);
  check(info, "register");

  await extension.awaitMessage("ready");

  // Check that the regular unregister process occurs when
  // the browser shuts down.
  [info] = await Promise.all([
    promiseObservable("unregister-event-listener", 3),
    new Promise(resolve => extension.extension.once("shutdown", resolve)),
    AddonTestUtils.promiseShutdownManager(),
  ]);
  check(info, "unregister");

  // Check that listeners are primed at the next browser startup.
  [info] = await Promise.all([
    promiseObservable("prime-event-listener", 3),
    AddonTestUtils.promiseStartupManager(),
  ]);
  check(info, "prime");

  // Check that primed listeners are converted to regular listeners
  // when the background page is started after browser startup.
  let p = promiseObservable("convert-event-listener", 3);
  Services.obs.notifyObservers(null, "sessionstore-windows-restored");
  info = await p;

  check(info, "convert");

  await extension.awaitMessage("ready");

  // Check that when the event is triggered, all the plumbing worked
  // correctly for the primed-then-converted listener.
  let listenerArgs = { test: "kaboom" };
  Services.obs.notifyObservers({ listenerArgs }, "fire-onEvent1");

  let details = await extension.awaitMessage("listener1");
  deepEqual(details, listenerArgs, "Listener 1 fired");
  details = await extension.awaitMessage("listener2");
  deepEqual(details, listenerArgs, "Listener 2 fired");

  // Check that the converted listener is properly unregistered at
  // browser shutdown.
  [info] = await Promise.all([
    promiseObservable("unregister-primed-listener", 3),
    AddonTestUtils.promiseShutdownManager(),
  ]);
  check(info, "unregister");

  // Start up again, listener should be primed
  [info] = await Promise.all([
    promiseObservable("prime-event-listener", 3),
    AddonTestUtils.promiseStartupManager(),
  ]);
  check(info, "prime");

  // Check that triggering the event before the listener has been converted
  // causes the background page to be loaded and the listener to be converted,
  // and the listener is invoked.
  p = promiseObservable("convert-event-listener", 3);
  listenerArgs.test = "startup event";
  Services.obs.notifyObservers({ listenerArgs }, "fire-onEvent2");
  info = await p;

  check(info, "convert");

  details = await extension.awaitMessage("listener3");
  deepEqual(details, listenerArgs, "Listener 3 fired for event during startup");

  await extension.awaitMessage("ready");

  // Check that the unregister process works when we manually remove
  // a listener.
  p = promiseObservable("unregister-primed-listener", 1);
  extension.sendMessage("unregister2");
  info = await p;
  check(info, "unregister", { listener1: false, listener2: false });

  // Check that we only get unregisters for the remaining events after
  // one listener has been removed.
  info = await promiseObservable("unregister-primed-listener", 2, () =>
    AddonTestUtils.promiseShutdownManager()
  );
  check(info, "unregister", { listener3: false });

  // Check that after restart, only listeners that were present at
  // the end of the last session are primed.
  info = await promiseObservable("prime-event-listener", 2, () =>
    AddonTestUtils.promiseStartupManager()
  );
  check(info, "prime", { listener3: false });

  // Check that if the background script does not re-register listeners,
  // the primed listeners are unregistered after the background page
  // starts up.
  p = promiseObservable("unregister-primed-listener", 1, () =>
    extension.awaitMessage("ready")
  );
  Services.obs.notifyObservers(null, "sessionstore-windows-restored");
  info = await p;
  check(info, "unregister", { listener1: false, listener3: false });

  // Just listener1 should be registered now, fire event1 to confirm.
  listenerArgs.test = "third time";
  Services.obs.notifyObservers({ listenerArgs }, "fire-onEvent1");
  details = await extension.awaitMessage("listener1");
  deepEqual(details, listenerArgs, "Listener 1 fired");

  // Tell the extension not to re-register listener1 on the next startup
  extension.sendMessage("unregister1");
  await extension.awaitMessage("unregistered");

  // Shut down, start up
  info = await promiseObservable("unregister-primed-listener", 1, () =>
    AddonTestUtils.promiseShutdownManager()
  );
  check(info, "unregister", { listener2: false, listener3: false });

  info = await promiseObservable("prime-event-listener", 1, () =>
    AddonTestUtils.promiseStartupManager()
  );
  check(info, "register", { listener2: false, listener3: false });

  // Check that firing event1 causes the listener fire callback to
  // reject.
  p = promiseObservable("listener-callback-exception", 1);
  Services.obs.notifyObservers(
    { listenerArgs, waitForBackground: true },
    "fire-onEvent1"
  );
  await p;
  ok(
    true,
    "Primed listener that was not re-registered received an error when event was triggered during startup"
  );

  await extension.awaitMessage("ready");

  await extension.unload();

  await AddonTestUtils.promiseShutdownManager();
});
