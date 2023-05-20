/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const LABELS_STARTUP_CACHE_REQUESTS = {
  HitMemory: 0,
  HitDisk: 1,
  Miss: 2,
};

add_task(async function () {
  // Turn off tab preloading to avoid issues with RemoteController.js
  await SpecialPowers.pushPrefEnv({
    set: [["browser.newtab.preload", false]],
  });

  Services.obs.notifyObservers(null, "startupcache-invalidate");
  Services.telemetry.getSnapshotForHistograms("main", true);
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let snapshot = Services.telemetry.getSnapshotForHistograms("main", true);
  function histValue(label) {
    return snapshot.parent.STARTUP_CACHE_REQUESTS.values[label] || 0;
  }
  Assert.equal(histValue(LABELS_STARTUP_CACHE_REQUESTS.HitMemory), 0);
  Assert.equal(histValue(LABELS_STARTUP_CACHE_REQUESTS.HitDisk), 0);
  Assert.notEqual(histValue(LABELS_STARTUP_CACHE_REQUESTS.Miss), 0);
  await BrowserTestUtils.closeWindow(newWin);

  newWin = await BrowserTestUtils.openNewBrowserWindow();
  snapshot = Services.telemetry.getSnapshotForHistograms("main", true);
  Assert.notEqual(histValue(LABELS_STARTUP_CACHE_REQUESTS.HitMemory), 0);
  Assert.equal(histValue(LABELS_STARTUP_CACHE_REQUESTS.HitDisk), 0);

  // Here and below, 50 is just a nice round number that should be well over
  // what we should observe under normal circumstances, and well under what
  // we should see if something is seriously wrong. It won't catch everything,
  // but it's better than nothing, and better than a test that cries wolf.
  Assert.less(histValue(LABELS_STARTUP_CACHE_REQUESTS.Miss), 50);
  await BrowserTestUtils.closeWindow(newWin);

  Services.obs.notifyObservers(null, "startupcache-invalidate", "memoryOnly");
  newWin = await BrowserTestUtils.openNewBrowserWindow();
  snapshot = Services.telemetry.getSnapshotForHistograms("main", true);
  Assert.less(histValue(LABELS_STARTUP_CACHE_REQUESTS.HitMemory), 50);
  Assert.notEqual(histValue(LABELS_STARTUP_CACHE_REQUESTS.HitDisk), 0);
  // Some misses can come through - just ensure it's a small number
  Assert.less(histValue(LABELS_STARTUP_CACHE_REQUESTS.Miss), 50);
  await BrowserTestUtils.closeWindow(newWin);
});
