"use strict";

/**
 * Tests for PerformanceWatcher watching slow web pages.
 */

 /**
  * Simulate a slow webpage.
  */
function WebpageBurner() {
  CPUBurner.call(this, "http://example.com/browser/toolkit/components/perfmonitoring/tests/browser/browser_compartments.html?test=" + Math.random());
}
WebpageBurner.prototype = Object.create(CPUBurner.prototype);
WebpageBurner.prototype.promiseBurnContentCPU = function() {
  return promiseContentResponse(this._browser, "test-performance-watcher:burn-content-cpu", {});
};

function WebpageListener(windowId, accept) {
  info(`Creating WebpageListener for ${windowId}`);
  AlertListener.call(this, accept, {
    register: () => PerformanceWatcher.addPerformanceListener({windowId}, this.listener),
    unregister: () => PerformanceWatcher.removePerformanceListener({windowId}, this.listener)
  });
}
WebpageListener.prototype = Object.create(AlertListener.prototype);

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

add_task(function* test_open_window_then_watch_it() {
  let burner = new WebpageBurner();
  yield burner.promiseInitialized;
  yield burner.promiseBurnContentCPU();

  info(`Check that burning CPU triggers the real listener, but not the fake listener`);
  let realListener = new WebpageListener(burner.windowId, (group, details) => {
    info(`test: realListener for ${burner.tab.linkedBrowser.outerWindowID}: ${group}, ${details}\n`);
    Assert.equal(group.windowId, burner.windowId, "We should not receive data meant for another group");
    return details;
  }); // This listener should be triggered.

  info(`Creating fake burner`);
  let otherTab = gBrowser.addTab();
  yield BrowserTestUtils.browserLoaded(otherTab.linkedBrowser);
  info(`Check that burning CPU triggers the real listener, but not the fake listener`);
  let fakeListener = new WebpageListener(otherTab.linkedBrowser.outerWindowID, group => group.windowId == burner.windowId); // This listener should never be triggered.
  let universalListener = new WebpageListener(0, alerts => 
    alerts.find(alert => alert.source.windowId == burner.windowId)
  );

  // Waiting a little – listeners are buffered.
  yield new Promise(resolve => setTimeout(resolve, 100));

  yield burner.run("promiseBurnContentCPU", 20, realListener);
  Assert.ok(realListener.triggered, `1. The real listener was triggered`);
  Assert.ok(universalListener.triggered, `1. The universal listener was triggered`);
  Assert.ok(!fakeListener.triggered, `1. The fake listener was not triggered`);

  if (realListener.result) {
    Assert.ok(realListener.result.highestJank >= 300, `1. jank is at least 300ms (${realListener.result.highestJank}ms)`);
  }

  info(`Attempting to remove a performance listener incorrectly, check that this does not hurt our real listener`);
  Assert.throws(() => PerformanceWatcher.removePerformanceListener({addonId: addon.addonId}, () => {}));
  Assert.throws(() => PerformanceWatcher.removePerformanceListener({addonId: addon.addonId + "-unbound-id-" + Math.random()}, realListener.listener));

  // Waiting a little – listeners are buffered.
  yield new Promise(resolve => setTimeout(resolve, 100));
  yield burner.run("promiseBurnContentCPU", 20, realListener);
  // Waiting a little – listeners are buffered.
  yield new Promise(resolve => setTimeout(resolve, 100));

  Assert.ok(realListener.triggered, `2. The real listener was triggered`);
  Assert.ok(universalListener.triggered, `2. The universal listener was triggered`);
  Assert.ok(!fakeListener.triggered, `2. The fake listener was not triggered`);
  if (realListener.result) {
    Assert.ok(realListener.result.highestJank >= 300, `2. jank is at least 300ms (${realListener.jank}ms)`);
  }

  info(`Attempting to remove correctly, check if the listener is still triggered`);
  // Waiting a little – listeners are buffered.
  yield new Promise(resolve => setTimeout(resolve, 100));
  realListener.unregister();

  // Waiting a little – listeners are buffered.
  yield new Promise(resolve => setTimeout(resolve, 100));
  yield burner.run("promiseBurnContentCPU", 3, realListener);
  Assert.ok(!realListener.triggered, `3. After being unregistered, the real listener was not triggered`);
  Assert.ok(universalListener.triggered, `3. The universal listener is still triggered`);

  universalListener.unregister();

  // Waiting a little – listeners are buffered.
  yield new Promise(resolve => setTimeout(resolve, 100));
  yield burner.run("promiseBurnContentCPU", 3, realListener);
  Assert.ok(!universalListener.triggered, `4. After being unregistered, the universal listener is not triggered`);

  fakeListener.unregister();
  burner.dispose();
  gBrowser.removeTab(otherTab);
});
