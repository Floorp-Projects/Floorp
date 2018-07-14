/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* exported setLowDiskMode */

// Small test helper to trigger the QuotaExceededError (it is basically the same
// test helper defined in "dom/indexedDB/test/unit/test_lowDiskSpace.js", but it doesn't
// use SpecialPowers).
let lowDiskMode = false;
function setLowDiskMode(val) {
  let data = val ? "full" : "free";

  if (val == lowDiskMode) {
    info("Low disk mode is: " + data);
  } else {
    info("Changing low disk mode to: " + data);
    Services.obs.notifyObservers(null, "disk-space-watcher", data);
    lowDiskMode = val;
  }
}
