"use strict";

/**
 * Tests for PerformanceWatcher watching slow addons.
 */

add_task(function* test_watch_addon_then_install_it() {
  for (let topic of ["burnCPU", "promiseBurnContentCPU", "promiseBurnCPOW"]) {
    let addonId = "addon:test_watch_addons_before_installing" + Math.random();
    let realListener = new AddonListener(addonId, (group, details) => {
      if (group.addonId == addonId) {
        return details.highestJank;
      }
      throw new Error(`I shouldn't have been called with addon ${group.addonId}`);
    });

    info("Now install the add-on, *after* having installed the listener");
    let addon = new AddonBurner(addonId);

    Assert.ok((yield addon.run(topic, 10, realListener)), `5. The real listener was triggered ${topic}`);
    Assert.ok(realListener.result >= 200000, `5. jank is at least 200ms (${realListener.result}µs) ${topic}`);
    realListener.unregister();
    addon.dispose();
  }
});
