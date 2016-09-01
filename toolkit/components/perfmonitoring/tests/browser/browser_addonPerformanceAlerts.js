"use strict";

/**
 * Tests for PerformanceWatcher watching slow addons.
 */

add_task(function* init() {
  // Get rid of buffering.
  let service = Cc["@mozilla.org/toolkit/performance-stats-service;1"].getService(
    Ci.nsIPerformanceStatsService);
  let oldDelay = service.jankAlertBufferingDelay;

  service.jankAlertBufferingDelay = 0 /* ms */;
  registerCleanupFunction(() => {
    info("Cleanup");
    service.jankAlertBufferingDelay = oldDelay;
  });
});

add_task(function* test_install_addon_then_watch_it() {
  for (let topic of ["burnCPU", "promiseBurnContentCPU", "promiseBurnCPOW"]) {
    info(`Starting subtest ${topic}`);
    info("Spawning fake add-on, making sure that the compartment is initialized");
    let addon = new AddonBurner();
    yield addon.promiseInitialized;
    addon.burnCPU();

    info(`Check that burning CPU triggers the real listener, but not the fake listener ${topic}`);
    let realListener = new AddonListener(addon.addonId, (group, details) => {
      if (group.addonId == addon.addonId) {
        return details.highestJank;
      }
      throw new Error(`I shouldn't have been called with addon ${group.addonId}`);
    });
    let fakeListener = new AddonListener(addon.addonId + "-fake-" + Math.random(), group => true); // This listener should never be triggered.
    let universalListener = new AddonListener("*", alerts => {
      info(`AddonListener: received alerts ${JSON.stringify(alerts)}`);
      let alert = alerts.find(({source}) => {
        return source.addonId == addon.addonId;
      });
      if (alert) {
        info(`AddonListener: I found an alert for ${addon.addonId}`);
        return alert.details.highestJank;
      }
      info(`AddonListener: I didn't find any alert for ${addon.addonId}`);
      return null;
    });

    // Waiting a little – listeners are buffered.
    yield new Promise(resolve => setTimeout(resolve, 100));
    yield addon.run(topic, 10, realListener);
    // Waiting a little – listeners are buffered.
    yield new Promise(resolve => setTimeout(resolve, 100));

    Assert.ok(realListener.triggered, `1. The real listener was triggered ${topic}`);
    Assert.ok(universalListener.triggered, `1. The universal listener was triggered ${topic}`);
    Assert.ok(!fakeListener.triggered, `1. The fake listener was not triggered ${topic}`);
    Assert.ok(realListener.result >= addon.jankThreshold, `1. jank is at least ${addon.jankThreshold/1000}ms (${realListener.result/1000}ms) ${topic}`);

    info(`Attempting to remove a performance listener incorrectly, check that this does not hurt our real listener ${topic}`);
    Assert.throws(() => PerformanceWatcher.removePerformanceListener({addonId: addon.addonId}, () => {}));
    Assert.throws(() => PerformanceWatcher.removePerformanceListener({addonId: addon.addonId + "-unbound-id-" + Math.random()}, realListener.listener));

    yield addon.run(topic, 10, realListener);
    // Waiting a little – listeners are buffered.
    yield new Promise(resolve => setTimeout(resolve, 300));

    Assert.ok(realListener.triggered, `2. The real listener was triggered ${topic}`);
    Assert.ok(universalListener.triggered, `2. The universal listener was triggered ${topic}`);
    Assert.ok(!fakeListener.triggered, `2. The fake listener was not triggered ${topic}`);
    Assert.ok(realListener.result >= 200000, `2. jank is at least 300ms (${realListener.result/1000}ms) ${topic}`);

    info(`Attempting to remove correctly, check if the listener is still triggered ${topic}`);
    realListener.unregister();
    yield addon.run(topic, 3, realListener);
    Assert.ok(!realListener.triggered, `3. After being unregistered, the real listener was not triggered ${topic}`);
    Assert.ok(universalListener.triggered, `3. The universal listener is still triggered ${topic}`);

    info("Unregistering universal listener");
    // Waiting a little – listeners are buffered.
    yield new Promise(resolve => setTimeout(resolve, 100));
    universalListener.unregister();
    // Waiting a little – listeners are buffered.
    yield new Promise(resolve => setTimeout(resolve, 100));
    yield addon.run(topic, 3, realListener);
    Assert.ok(!universalListener.triggered, `4. After being unregistered, the universal listener is not triggered ${topic}`);

    fakeListener.unregister();
    addon.dispose();
  }
});
