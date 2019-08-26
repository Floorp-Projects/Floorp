/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { MockRegistrar } = ChromeUtils.import(
  "resource://testing-common/MockRegistrar.jsm"
);

let idleService = {
  _observers: new Set(),
  _activity: {
    addCalls: [],
    removeCalls: [],
    observerFires: [],
  },
  _reset: function() {
    this._observers.clear();
    this._activity.addCalls = [];
    this._activity.removeCalls = [];
    this._activity.observerFires = [];
  },
  _fireObservers: function(state) {
    for (let observer of this._observers.values()) {
      observer.observe(observer, state, null);
      this._activity.observerFires.push(state);
    }
  },
  QueryInterface: ChromeUtils.generateQI([Ci.nsIIdleService]),
  idleTime: 19999,
  addIdleObserver: function(observer, time) {
    this._observers.add(observer);
    this._activity.addCalls.push(time);
  },
  removeIdleObserver: function(observer, time) {
    this._observers.delete(observer);
    this._activity.removeCalls.push(time);
  },
};

function checkActivity(expectedActivity) {
  let { expectedAdd, expectedRemove, expectedFires } = expectedActivity;
  let { addCalls, removeCalls, observerFires } = idleService._activity;
  equal(
    expectedAdd.length,
    addCalls.length,
    "idleService.addIdleObserver was called the expected number of times"
  );
  equal(
    expectedRemove.length,
    removeCalls.length,
    "idleService.removeIdleObserver was called the expected number of times"
  );
  equal(
    expectedFires.length,
    observerFires.length,
    "idle observer was fired the expected number of times"
  );
  deepEqual(
    addCalls,
    expectedAdd,
    "expected interval passed to idleService.addIdleObserver"
  );
  deepEqual(
    removeCalls,
    expectedRemove,
    "expected interval passed to idleService.removeIdleObserver"
  );
  deepEqual(
    observerFires,
    expectedFires,
    "expected topic passed to idle observer"
  );
}

add_task(async function setup() {
  let fakeIdleService = MockRegistrar.register(
    "@mozilla.org/widget/idleservice;1",
    idleService
  );
  registerCleanupFunction(() => {
    MockRegistrar.unregister(fakeIdleService);
  });
});

add_task(async function testQueryStateActive() {
  function background() {
    browser.idle.queryState(20).then(
      status => {
        browser.test.assertEq("active", status, "Idle status is active");
        browser.test.notifyPass("idle");
      },
      err => {
        browser.test.fail(`Error: ${err} :: ${err.stack}`);
        browser.test.notifyFail("idle");
      }
    );
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["idle"],
    },
  });

  await extension.startup();
  await extension.awaitFinish("idle");
  await extension.unload();
});

add_task(async function testQueryStateIdle() {
  function background() {
    browser.idle.queryState(15).then(
      status => {
        browser.test.assertEq("idle", status, "Idle status is idle");
        browser.test.notifyPass("idle");
      },
      err => {
        browser.test.fail(`Error: ${err} :: ${err.stack}`);
        browser.test.notifyFail("idle");
      }
    );
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["idle"],
    },
  });

  await extension.startup();
  await extension.awaitFinish("idle");
  await extension.unload();
});

add_task(async function testOnlySetDetectionInterval() {
  function background() {
    browser.idle.setDetectionInterval(99);
    browser.test.sendMessage("detectionIntervalSet");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["idle"],
    },
  });

  idleService._reset();
  await extension.startup();
  await extension.awaitMessage("detectionIntervalSet");
  idleService._fireObservers("idle");
  checkActivity({ expectedAdd: [], expectedRemove: [], expectedFires: [] });
  await extension.unload();
});

add_task(async function testSetDetectionIntervalBeforeAddingListener() {
  function background() {
    browser.idle.setDetectionInterval(99);
    browser.idle.onStateChanged.addListener(newState => {
      browser.test.assertEq(
        "idle",
        newState,
        "listener fired with the expected state"
      );
      browser.test.sendMessage("listenerFired");
    });
    browser.test.sendMessage("listenerAdded");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["idle"],
    },
  });

  idleService._reset();
  await extension.startup();
  await extension.awaitMessage("listenerAdded");
  idleService._fireObservers("idle");
  await extension.awaitMessage("listenerFired");
  checkActivity({
    expectedAdd: [99],
    expectedRemove: [],
    expectedFires: ["idle"],
  });
  // Defer unloading the extension so the asynchronous event listener
  // reply finishes.
  await new Promise(resolve => setTimeout(resolve, 0));
  await extension.unload();
});

add_task(async function testSetDetectionIntervalAfterAddingListener() {
  function background() {
    browser.idle.onStateChanged.addListener(newState => {
      browser.test.assertEq(
        "idle",
        newState,
        "listener fired with the expected state"
      );
      browser.test.sendMessage("listenerFired");
    });
    browser.idle.setDetectionInterval(99);
    browser.test.sendMessage("detectionIntervalSet");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["idle"],
    },
  });

  idleService._reset();
  await extension.startup();
  await extension.awaitMessage("detectionIntervalSet");
  idleService._fireObservers("idle");
  await extension.awaitMessage("listenerFired");
  checkActivity({
    expectedAdd: [60, 99],
    expectedRemove: [60],
    expectedFires: ["idle"],
  });

  // Defer unloading the extension so the asynchronous event listener
  // reply finishes.
  await new Promise(resolve => setTimeout(resolve, 0));
  await extension.unload();
});

add_task(async function testOnlyAddingListener() {
  function background() {
    browser.idle.onStateChanged.addListener(newState => {
      browser.test.assertEq(
        "active",
        newState,
        "listener fired with the expected state"
      );
      browser.test.sendMessage("listenerFired");
    });
    browser.test.sendMessage("listenerAdded");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["idle"],
    },
  });

  idleService._reset();
  await extension.startup();
  await extension.awaitMessage("listenerAdded");
  idleService._fireObservers("active");
  await extension.awaitMessage("listenerFired");
  // check that "idle-daily" topic does not cause a listener to fire
  idleService._fireObservers("idle-daily");
  checkActivity({
    expectedAdd: [60],
    expectedRemove: [],
    expectedFires: ["active", "idle-daily"],
  });

  // Defer unloading the extension so the asynchronous event listener
  // reply finishes.
  await new Promise(resolve => setTimeout(resolve, 0));
  await extension.unload();
});
