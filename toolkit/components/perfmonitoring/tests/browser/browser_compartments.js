/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/PerformanceStats.jsm", this);
Cu.import("resource://testing-common/ContentTask.jsm", this);

const URL = "http://example.com/browser/toolkit/components/perfmonitoring/tests/browser/browser_compartments.html?test=" + Math.random();

// This function is injected as source as a frameScript
function frameScript() {
  "use strict";

  const { utils: Cu, classes: Cc, interfaces: Ci } = Components;
  Cu.import("resource://gre/modules/PerformanceStats.jsm");

  let performanceStatsService =
    Cc["@mozilla.org/toolkit/performance-stats-service;1"].
    getService(Ci.nsIPerformanceStatsService);

  // Make sure that the stopwatch is now active.
  performanceStatsService.isStopwatchActive = true;

  addMessageListener("compartments-test:getStatistics", () => {
    try {
      sendAsyncMessage("compartments-test:getStatistics", PerformanceStats.getSnapshot());
    } catch (ex) {
      Cu.reportError("Error in content: " + ex);
      Cu.reportError(ex.stack);
    }
  });
}

// A variant of `Assert` that doesn't spam the logs
// in case of success.
let SilentAssert = {
  equal: function(a, b, msg) {
    if (a == b) {
      return;
    }
    Assert.equal(a, b, msg);
  },
  notEqual: function(a, b, msg) {
    if (a != b) {
      return;
    }
    Assert.notEqual(a, b, msg);
  },
  ok: function(a, msg) {
    if (a) {
      return;
    }
    Assert.ok(a, msg);
  },
  leq: function(a, b, msg) {
    this.ok(a <= b, `${msg}: ${a} <= ${b}`);
  }
};


function monotinicity_tester(source, testName) {
  // In the background, check invariants:
  // - numeric data can only ever increase;
  // - the name, addonId, isSystem of a component never changes;
  // - the name, addonId, isSystem of the process data;
  // - there is at most one component with a combination of `name` and `addonId`;
  // - types, etc.
  let previous = {
    processData: null,
    componentsMap: new Map(),
  };

  let sanityCheck = function(prev, next) {
    info(`Sanity check: ${JSON.stringify(next, null, "\t")}`);
    if (prev == null) {
      return;
    }
    for (let k of ["name", "addonId", "isSystem"]) {
      SilentAssert.equal(prev[k], next[k], `Sanity check (${testName}): ${k} hasn't changed.`);
    }
    for (let k of ["totalUserTime", "totalSystemTime", "totalCPOWTime", "ticks"]) {
      SilentAssert.equal(typeof next[k], "number", `Sanity check (${testName}): ${k} is a number.`);
      SilentAssert.leq(prev[k], next[k], `Sanity check (${testName}): ${k} is monotonic.`);
      SilentAssert.leq(0, next[k], `Sanity check (${testName}): ${k} is >= 0.`)
    }
    SilentAssert.equal(prev.durations.length, next.durations.length);
    for (let i = 0; i < next.durations.length; ++i) {
      SilentAssert.ok(typeof next.durations[i] == "number" && next.durations[i] >= 0,
        `Sanity check (${testName}): durations[${i}] is a non-negative number.`);
      SilentAssert.leq(prev.durations[i], next.durations[i],
        `Sanity check (${testName}): durations[${i}] is monotonic.`)
    }
    for (let i = 0; i < next.durations.length - 1; ++i) {
      SilentAssert.leq(next.durations[i + 1], next.durations[i],
        `Sanity check (${testName}): durations[${i}] >= durations[${i + 1}].`)
    }
  };
  let iteration = 0;
  let frameCheck = Task.async(function*() {
    let name = `${testName}: ${iteration++}`;
    let snapshot = yield source();
    if (!snapshot) {
      // This can happen at the end of the test when we attempt
      // to communicate too late with the content process.
      window.clearInterval(interval);
      return;
    }

    // Sanity check on the process data.
    sanityCheck(previous.processData, snapshot.processData);
    SilentAssert.equal(snapshot.processData.isSystem, true);
    SilentAssert.equal(snapshot.processData.name, "<process>");
    SilentAssert.equal(snapshot.processData.addonId, "");
    previous.procesData = snapshot.processData;

    // Sanity check on components data.
    let set = new Set();
    let keys = [];
    for (let item of snapshot.componentsData) {
      let key = `{name: ${item.name}, addonId: ${item.addonId}, isSystem: ${item.isSystem}}`;
      keys.push(key);
      set.add(key);
      sanityCheck(previous.componentsMap.get(key), item);
      previous.componentsMap.set(key, item);

      for (let k of ["totalUserTime", "totalSystemTime", "totalCPOWTime"]) {
        SilentAssert.leq(item[k], snapshot.processData[k],
          `Sanity check (${testName}): component has a lower ${k} than process`);
      }
    }
    info(`Deactivating deduplication check (Bug 1150045)`);
    if (false) {
      SilentAssert.equal(set.size, snapshot.componentsData.length);
    }
  });
  let interval = window.setInterval(frameCheck, 300);
  registerCleanupFunction(() => {
    window.clearInterval(interval);
  });
}

add_task(function* test() {
  info("Extracting initial state");
  let stats0 = PerformanceStats.getSnapshot();
  Assert.notEqual(stats0.componentsData.length, 0, "There is more than one component");
  Assert.ok(!stats0.componentsData.find(stat => stat.name.indexOf(URL) != -1),
    "The url doesn't appear yet");

  let newTab = gBrowser.addTab();
  let browser = newTab.linkedBrowser;
  // Setup monitoring in the tab
  info("Setting up monitoring in the tab");
  yield ContentTask.spawn(newTab.linkedBrowser, null, frameScript);

  info("Opening URL");
  newTab.linkedBrowser.loadURI(URL);

  info("Setting up monotonicity testing");
  monotinicity_tester(() => PerformanceStats.getSnapshot(), "parent process");
  monotinicity_tester(() => promiseContentResponseOrNull(browser, "compartments-test:getStatistics", null), "content process" );

  let skipTotalUserTime = hasLowPrecision();

  while (true) {
    let stats = (yield promiseContentResponse(browser, "compartments-test:getStatistics", null));
    let found = stats.componentsData.find(stat => {
      return (stat.name.indexOf(URL) != -1)
      && (skipTotalUserTime || stat.totalUserTime > 1000)
    });
    if (found) {
      info(`Expected totalUserTime > 1000, got ${found.totalUserTime}`);
      break;
    }
    yield new Promise(resolve => setTimeout(resolve, 100));
  }

  // Cleanup
  gBrowser.removeTab(newTab);
});
