/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env mozilla/frame-script */

"use strict";

/**
 * Test that we see jank that takes place in a webpage,
 * and that jank from several iframes are actually charged
 * to the top window.
 */
Cu.import("resource://gre/modules/PerformanceStats.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://testing-common/ContentTask.jsm", this);


const URL = "http://example.com/browser/toolkit/components/perfmonitoring/tests/browser/browser_compartments.html?test=" + Math.random();
const PARENT_TITLE = `Main frame for test browser_compartments.js ${Math.random()}`;
const FRAME_TITLE = `Subframe for test browser_compartments.js ${Math.random()}`;

const PARENT_PID = Services.appinfo.processID;

// This function is injected as source as a frameScript
function frameScript() {
  try {
    "use strict";

    const { utils: Cu, classes: Cc, interfaces: Ci } = Components;
    Cu.import("resource://gre/modules/PerformanceStats.jsm");
    Cu.import("resource://gre/modules/Services.jsm");

    // Make sure that the stopwatch is now active.
    let monitor = PerformanceStats.getMonitor(["jank", "cpow", "ticks", "compartments"]);

    addMessageListener("compartments-test:getStatistics", () => {
      try {
        monitor.promiseSnapshot().then(snapshot => {
          sendAsyncMessage("compartments-test:getStatistics", {snapshot, pid: Services.appinfo.processID});
        });
      } catch (ex) {
        Cu.reportError("Error in content (getStatistics): " + ex);
        Cu.reportError(ex.stack);
      }
    });

    addMessageListener("compartments-test:setTitles", titles => {
      try {
        content.document.title = titles.data.parent;
        for (let i = 0; i < content.frames.length; ++i) {
          content.frames[i].postMessage({title: titles.data.frames}, "*");
        }
        console.log("content", "Done setting titles", content.document.title);
        sendAsyncMessage("compartments-test:setTitles");
      } catch (ex) {
        Cu.reportError("Error in content (setTitles): " + ex);
        Cu.reportError(ex.stack);
      }
    });
  } catch (ex) {
    Cu.reportError("Error in content (setup): " + ex);
    Cu.reportError(ex.stack);
  }
}

// A variant of `Assert` that doesn't spam the logs
// in case of success.
var SilentAssert = {
  equal(a, b, msg) {
    if (a == b) {
      return;
    }
    Assert.equal(a, b, msg);
  },
  notEqual(a, b, msg) {
    if (a != b) {
      return;
    }
    Assert.notEqual(a, b, msg);
  },
  ok(a, msg) {
    if (a) {
      return;
    }
    Assert.ok(a, msg);
  },
  leq(a, b, msg) {
    this.ok(a <= b, `${msg}: ${a} <= ${b}`);
  }
};

var isShuttingDown = false;
function monotinicity_tester(source, testName) {
  // In the background, check invariants:
  // - numeric data can only ever increase;
  // - the name, isSystem of a component never changes;
  // - the name, isSystem of the process data;
  // - there is at most one component with `name`;
  // - types, etc.
  let previous = {
    processData: null,
    componentsMap: new Map(),
  };

  let sanityCheck = function(prev, next) {
    if (prev == null) {
      return;
    }
    for (let k of ["groupId", "isSystem"]) {
      SilentAssert.equal(prev[k], next[k], `Sanity check (${testName}): ${k} hasn't changed (${prev.name}).`);
    }
    for (let [probe, k] of [
      ["jank", "totalUserTime"],
      ["jank", "totalSystemTime"],
      ["cpow", "totalCPOWTime"],
      ["ticks", "ticks"]
    ]) {
      SilentAssert.equal(typeof next[probe][k], "number", `Sanity check (${testName}): ${k} is a number.`);
      SilentAssert.leq(prev[probe][k], next[probe][k], `Sanity check (${testName}): ${k} is monotonic.`);
      SilentAssert.leq(0, next[probe][k], `Sanity check (${testName}): ${k} is >= 0.`)
    }
    SilentAssert.equal(prev.jank.durations.length, next.jank.durations.length,
                       `Sanity check (${testName}): Jank durations should be equal`);
    for (let i = 0; i < next.jank.durations.length; ++i) {
      SilentAssert.ok(typeof next.jank.durations[i] == "number" && next.jank.durations[i] >= 0,
        `Sanity check (${testName}): durations[${i}] is a non-negative number.`);
      SilentAssert.leq(prev.jank.durations[i], next.jank.durations[i],
        `Sanity check (${testName}): durations[${i}] is monotonic.`);
    }
    for (let i = 0; i < next.jank.durations.length - 1; ++i) {
      SilentAssert.leq(next.jank.durations[i + 1], next.jank.durations[i],
        `Sanity check (${testName}): durations[${i}] >= durations[${i + 1}].`)
    }
  };
  let iteration = 0;
  let frameCheck = async function() {
    if (isShuttingDown) {
      window.clearInterval(interval);
      return;
    }
    let name = `${testName}: ${iteration++}`;
    let result = await source();
    if (!result) {
      // This can happen at the end of the test when we attempt
      // to communicate too late with the content process.
      window.clearInterval(interval);
      return;
    }
    let {pid, snapshot} = result;

    // Sanity check on the process data.
    sanityCheck(previous.processData, snapshot.processData);
    SilentAssert.equal(snapshot.processData.isSystem, true, "Should be system");
    SilentAssert.equal(snapshot.processData.name, "<process>", "Should have '<process>' name");
    SilentAssert.equal(snapshot.processData.processId, pid, "Process id should match");
    previous.procesData = snapshot.processData;

    // Sanity check on components data.
    let map = new Map();
    for (let item of snapshot.componentsData) {
      let isCorrectPid = (item.processId == pid && !item.isChildProcess)
        || (item.processId != pid && item.isChildProcess);
      SilentAssert.ok(isCorrectPid, `Pid check (${name}): the item comes from the right process`);

      let key = item.groupId;
      if (map.has(key)) {
        let old = map.get(key);
        Assert.ok(false, `Component ${key} has already been seen. Latest: ${item.name}, previous: ${old.name}`);
      }
      map.set(key, item);
    }
    for (let item of snapshot.componentsData) {
      if (!item.parentId) {
        continue;
      }
      let parent = map.get(item.parentId);
      SilentAssert.ok(parent, `The parent exists ${item.parentId}`);

      for (let [probe, k] of [
        ["jank", "totalUserTime"],
        ["jank", "totalSystemTime"],
        ["cpow", "totalCPOWTime"]
      ]) {
        // Note that we cannot expect components data to be always smaller
        // than parent data, as `getrusage` & co are not monotonic.
        SilentAssert.leq(item[probe][k], 2 * parent[probe][k],
          `Sanity check (${testName}): ${k} of component is not impossibly larger than that of parent`);
      }
    }
    for (let [key, item] of map) {
      sanityCheck(previous.componentsMap.get(key), item);
      previous.componentsMap.set(key, item);
    }
  };
  let interval = window.setInterval(frameCheck, 300);
  registerCleanupFunction(() => {
    window.clearInterval(interval);
  });
}

add_task(async function test() {
  let monitor = PerformanceStats.getMonitor(["jank", "cpow", "ticks"]);

  info("Extracting initial state");
  let stats0 = await monitor.promiseSnapshot();
  Assert.notEqual(stats0.componentsData.length, 0, "There is more than one component");
  Assert.ok(!stats0.componentsData.find(stat => stat.name.indexOf(URL) != -1),
    "The url doesn't appear yet");

  let newTab = BrowserTestUtils.addTab(gBrowser);
  let browser = newTab.linkedBrowser;
  // Setup monitoring in the tab
  info("Setting up monitoring in the tab");
  await ContentTask.spawn(newTab.linkedBrowser, null, frameScript);

  info("Opening URL");
  newTab.linkedBrowser.loadURI(URL);

  if (Services.sysinfo.getPropertyAsAString("name") == "Windows_NT") {
    info("Deactivating sanity checks under Windows (bug 1151240)");
  } else {
    info("Setting up sanity checks");
    monotinicity_tester(() => monitor.promiseSnapshot().then(snapshot => ({snapshot, pid: PARENT_PID})), "parent process");
    monotinicity_tester(() => promiseContentResponseOrNull(browser, "compartments-test:getStatistics", null), "content process" );
  }

  let skipTotalUserTime = hasLowPrecision();


  while (true) {
    await new Promise(resolve => setTimeout(resolve, 100));

    // We may have race conditions with DOM loading.
    // Don't waste too much brainpower here, let's just ask
    // repeatedly for the title to be changed, until this works.
    info("Setting titles");
    await promiseContentResponse(browser, "compartments-test:setTitles", {
      parent: PARENT_TITLE,
      frames: FRAME_TITLE
    });
    info("Titles set");

    let {snapshot: stats} = (await promiseContentResponse(browser, "compartments-test:getStatistics", null));

    // Attach titles to components.
    let titles = [];
    let map = new Map();
    let windows = Services.wm.getEnumerator("navigator:browser");
    while (windows.hasMoreElements()) {
      let window = windows.getNext();
      let tabbrowser = window.gBrowser;
      for (let browser of tabbrowser.browsers) {
        let id = browser.outerWindowID; // May be `null` if the browser isn't loaded yet
        if (id != null) {
          map.set(id, browser);
        }
      }
    }
    for (let stat of stats.componentsData) {
      if (!stat.windowId) {
        continue;
      }
      let browser = map.get(stat.windowId);
      if (!browser) {
        continue;
      }
      let title = browser.contentTitle;
      if (title) {
        stat.title = title;
        titles.push(title);
      }
    }

    // While the webpage consists in three compartments, we should see only
    // one `PerformanceData` in `componentsData`. Its `name` is undefined
    // (could be either the main frame or one of its subframes), but its
    // `title` should be the title of the main frame.
    info(`Searching for frame title '${FRAME_TITLE}' in ${JSON.stringify(titles)} (I hope not to find it)`);
    Assert.ok(!titles.includes(FRAME_TITLE), "Searching by title, the frames don't show up in the list of components");

    info(`Searching for window title '${PARENT_TITLE}' in ${JSON.stringify(titles)} (I hope to find it)`);
    let parent = stats.componentsData.find(x => x.title == PARENT_TITLE);
    if (!parent) {
      info("Searching by title, we didn't find the main frame");
      continue;
    }
    info("Found the main frame");

    if (skipTotalUserTime) {
      info("Not looking for total user time on this platform, we're done");
      break;
    } else if (parent.jank.totalUserTime > 1000) {
      info("Enough CPU time detected, we're done");
      break;
    } else {
      info(`Not enough CPU time detected: ${parent.jank.totalUserTime}`);
    }
  }
  isShuttingDown = true;

  // Cleanup
  gBrowser.removeTab(newTab, {skipPermitUnload: true});
});
