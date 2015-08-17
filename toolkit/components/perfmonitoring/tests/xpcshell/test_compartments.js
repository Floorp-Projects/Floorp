"use strict";

const {utils: Cu, interfaces: Ci, classes: Cc} = Components;

Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/PerformanceStats.jsm", this);

function run_test() {
  run_next_test();
}

let promiseStatistics = Task.async(function*(name) {
  yield new Promise(resolve => do_execute_soon(resolve));
  // Make sure that we wait until statistics have been updated.
  let service = Cc["@mozilla.org/toolkit/performance-stats-service;1"].
    getService(Ci.nsIPerformanceStatsService);
  let snapshot = service.getSnapshot();
  let componentsData = [];
  let componentsEnum = snapshot.getComponentsData().enumerate();
  while (componentsEnum.hasMoreElements()) {
    let data = componentsEnum.getNext().QueryInterface(Ci.nsIPerformanceStats);
    let normalized = JSON.parse(JSON.stringify(data));
    componentsData.push(data);
  }
  yield new Promise(resolve => do_execute_soon(resolve));
  return {
    processData: JSON.parse(JSON.stringify(snapshot.getProcessData())),
    componentsData
  };
});

let promiseSetMonitoring = Task.async(function*(to) {
  let service = Cc["@mozilla.org/toolkit/performance-stats-service;1"].
    getService(Ci.nsIPerformanceStatsService);
  service.isMonitoringJank = to;
  service.isMonitoringCPOW = to;
  yield new Promise(resolve => do_execute_soon(resolve));
});

let promiseSetPerCompartment = Task.async(function*(to) {
  let service = Cc["@mozilla.org/toolkit/performance-stats-service;1"].
    getService(Ci.nsIPerformanceStatsService);
  service.isMonitoringPerCompartment = to;
  yield new Promise(resolve => do_execute_soon(resolve));
});

function getBuiltinStatistics(name, snapshot) {
  let stats = snapshot.componentsData.find(stats =>
    stats.isSystem && !stats.addonId
  );
  do_print(`Built-in statistics for ${name} were ${stats?"":"not "}found`);
  do_print(JSON.stringify(snapshot.componentsData, null, "\t"));
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
  do_print(`Burning CPU over, after ${counter} iterations and ${Date.now() - start} milliseconds.`);
}

function ensureEquals(snap1, snap2, name) {
  for (let k of Object.keys(snap1.processData)) {
    if (k == "ticks") {
      // Ticks monitoring cannot be deactivated
      continue;
    }
    Assert.equal(snap1.processData[k], snap2.processData[k], `Same process data value ${k} (${name})`)
  }
  let stats1 = snap1.componentsData.sort((a, b) => a.name <= b.name);
  let stats2 = snap2.componentsData.sort((a, b) => a.name <= b.name);
  Assert.equal(stats1.length, stats2.length, `Same number of components (${name})`);
  for (let i = 0; i < stats1.length; ++i) {
    for (let k of Object.keys(stats1[i])) {
      if (k == "ticks") {
        // Ticks monitoring cannot be deactivated
        continue;
      }
      Assert.equal(stats1[i][k], stats1[i][k], `Same component data value ${i} ${k} (${name})`)
    }
  }
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
  yield promiseSetPerCompartment(false);

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
    do_print(JSON.stringify(process2));
    Assert.ok(process2.totalUserTime - process1.totalUserTime >= 10000, `At least 10ms counted for process time (${process2.totalUserTime - process1.totalUserTime})`);
  }
  Assert.equal(process2.totalCPOWTime, process1.totalCPOWTime, "We haven't used any CPOW time during the first burn");
  Assert.equal(process4.totalUserTime, process3.totalUserTime, "After deactivating the stopwatch, we didn't count any time");
  Assert.equal(process4.totalCPOWTime, process3.totalCPOWTime, "After deactivating the stopwatch, we didn't count any CPOW time");

  let builtin1 = getBuiltinStatistics("Built-ins 1", stats1) || { totalUserTime: 0, totalCPOWTime: 0 };
  let builtin2 = getBuiltinStatistics("Built-ins 2", stats2);
  let builtin3 = getBuiltinStatistics("Built-ins 3", stats3);
  let builtin4 = getBuiltinStatistics("Built-ins 4", stats4);
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

  // Ideally, we should be able to look for test_compartments.js, but
  // it doesn't have its own compartment.
  for (let stats of [stats1, stats2, stats3, stats4]) {
    Assert.ok(!stats.componentsData.find(x => x.name.includes("Task.jsm")), "At this stage, Task.jsm doesn't show up in the components data");
  }
  yield promiseSetMonitoring(true);
  yield promiseSetPerCompartment(true);
  burnCPU(300);
  let stats5 = yield promiseStatistics("With per-compartment monitoring");
  Assert.ok(stats5.componentsData.find(x => x.name.indexOf("Task.jsm") != -1), "With per-compartment monitoring, Task.jsm shows up");
});
