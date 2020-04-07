// Test ThrottleQueue initialization.
"use strict";

function init(tq, mean, max) {
  let threw = false;
  try {
    tq.init(mean, max);
  } catch (e) {
    threw = true;
  }
  return !threw;
}

function run_test() {
  let tq = Cc["@mozilla.org/network/throttlequeue;1"].createInstance(
    Ci.nsIInputChannelThrottleQueue
  );

  ok(!init(tq, 0, 50), "mean bytes cannot be 0");
  ok(!init(tq, 50, 0), "max bytes cannot be 0");
  ok(!init(tq, 0, 0), "mean and max bytes cannot be 0");
  ok(!init(tq, 70, 20), "max cannot be less than mean");

  ok(init(tq, 2, 2), "valid initialization");
}
