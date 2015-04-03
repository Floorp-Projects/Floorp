"use strict";

const {utils: Cu, interfaces: Ci, classes: Cc} = Components;

Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/PerformanceStats.jsm", this);

function run_test() {
  run_next_test();
}

let promiseStatistics = Task.async(function*(name) {
  yield Promise.resolve(); // Make sure that we wait until
  // statistics have been updated.
  let snapshot = PerformanceStats.getSnapshot();
  do_print("Statistics: " + name);
  do_print(JSON.stringify(snapshot.processData, null, "\t"));
  do_print(JSON.stringify(snapshot.componentsData, null, "\t"));
  return snapshot;
});

let promiseSetMonitoring = Task.async(function*(to) {
  Cc["@mozilla.org/toolkit/performance-stats-service;1"].
    getService(Ci.nsIPerformanceStatsService).
    isStopwatchActive = to;
  yield Promise.resolve();
});

function getBuiltinStatistics(snapshot) {
  let stats = snapshot.componentsData.find(stats =>
    stats.isSystem && !stats.addonId
  );
  return stats;
}

function burnCPU(ms) {
  do_print("Burning CPU");
  let counter = 0;
  let ignored = [];
  let start = Date.now();
  while (Date.now() - start < ms) {
    ignored.push(0);
    ignored.shift();
    ++counter;
  }
  do_print("Burning CPU over, after " + counter + " iterations");
}

function ensureEquals(snap1, snap2, name) {
  Assert.equal(
    JSON.stringify(snap1.processData),
    JSON.stringify(snap2.processData),
    "Same process data: " + name);
  let stats1 = snap1.componentsData.sort((a, b) => a.name <= b.name);
  let stats2 = snap2.componentsData.sort((a, b) => a.name <= b.name);
  Assert.equal(
    JSON.stringify(stats1),
    JSON.stringify(stats2),
    "Same components data: " + name
  );
}

function hasLowPrecision() {
  let [sysName, sysVersion] = [Services.sysinfo.getPropertyAsAString("name"), Services.sysinfo.getPropertyAsDouble("version")];
  do_print(`Running ${sysName} version ${sysVersion}`);

  if (sysName == "Windows_NT" && sysVersion < 6) {
    do_print("Running old Windows, need to deactivate tests due to bad precision.");
    return true;
  }
  if (sysName == "Linux" && sysVersion <= 2.6) {
    do_print("Running old Linux, need to deactivate tests due to bad precision.");
    return true;
  }
  do_print("This platform has good precision.")
  return false;
}

add_task(function* test_measure() {
  let skipPrecisionTests = hasLowPrecision();

  do_print("Burn CPU without the stopwatch");
  yield promiseSetMonitoring(false);
  let stats0 = yield promiseStatistics("Initial state");
  burnCPU(300);
  let stats1 = yield promiseStatistics("Initial state + burn, without stopwatch");

  do_print("Burn CPU with the stopwatch");
  yield promiseSetMonitoring(true);
  burnCPU(300);
  let stats2 = yield promiseStatistics("Second burn, with stopwatch");

  do_print("Burn CPU without the stopwatch again")
  yield promiseSetMonitoring(false);
  let stats3 = yield promiseStatistics("Before third burn, without stopwatch");
  burnCPU(300);
  let stats4 = yield promiseStatistics("After third burn, without stopwatch");

  ensureEquals(stats0, stats1, "Initial state vs. Initial state + burn, without stopwatch");
  let process1 = stats1.processData;
  let process2 = stats2.processData;
  let process3 = stats3.processData;
  let process4 = stats4.processData;
  if (skipPrecisionTests) {
    do_print("Skipping totalUserTime check under Windows XP, as timer is not always updated by the OS.")
  } else {
    Assert.ok(process2.totalUserTime - process1.totalUserTime >= 10000, `At least 10ms counted for process time (${process2.totalUserTime - process1.totalUserTime})`);
  }
  Assert.equal(process2.totalCPOWTime, process1.totalCPOWTime, "We haven't used any CPOW time during the first burn");
  Assert.equal(process4.totalUserTime, process3.totalUserTime, "After deactivating the stopwatch, we didn't count any time");
  Assert.equal(process4.totalCPOWTime, process3.totalCPOWTime, "After deactivating the stopwatch, we didn't count any CPOW time");

  let builtin1 = getBuiltinStatistics(stats1) || { totalUserTime: 0, totalCPOWTime: 0 };
  let builtin2 = getBuiltinStatistics(stats2);
  let builtin3 = getBuiltinStatistics(stats3);
  let builtin4 = getBuiltinStatistics(stats4);
  Assert.notEqual(builtin2, null, "Found the statistics for built-ins 2");
  Assert.notEqual(builtin3, null, "Found the statistics for built-ins 3");
  Assert.notEqual(builtin4, null, "Found the statistics for built-ins 4");

  if (skipPrecisionTests) {
    do_print("Skipping totalUserTime check under Windows XP, as timer is not always updated by the OS.")
  } else {
    Assert.ok(builtin2.totalUserTime - builtin1.totalUserTime >= 10000, `At least 10ms counted for built-in statistics (${builtin2.totalUserTime - builtin1.totalUserTime})`);
  }
  Assert.equal(builtin2.totalCPOWTime, builtin1.totalCPOWTime, "We haven't used any CPOW time during the first burn for the built-in");
  Assert.equal(builtin2.totalCPOWTime, builtin1.totalCPOWTime, "No CPOW for built-in statistics");
  Assert.equal(builtin4.totalUserTime, builtin3.totalUserTime, "After deactivating the stopwatch, we didn't count any time for the built-in");
  Assert.equal(builtin4.totalCPOWTime, builtin3.totalCPOWTime, "After deactivating the stopwatch, we didn't count any CPOW time for the built-in");
});
