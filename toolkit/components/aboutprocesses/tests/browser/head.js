/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

// A bunch of assumptions we make about the behavior of the parent process,
// and which we use as sanity checks. If Firefox evolves, we will need to
// update these values.
// Note that Test Verify can really stress the cpu durations.
const HARDCODED_ASSUMPTIONS_PROCESS = {
  minimalNumberOfThreads: 10,
  maximalNumberOfThreads: 1000,
  minimalCPUPercentage: 0,
  maximalCPUPercentage: 1000,
  minimalCPUTotalDurationMS: 10,
  maximalCPUTotalDurationMS: 10000000,
  minimalRAMBytesUsage: 1024 * 1024 /* 1 Megabyte */,
  maximalRAMBytesUsage: 1024 * 1024 * 1024 * 1024 * 1 /* 1 Tb */,
};

const HARDCODED_ASSUMPTIONS_THREAD = {
  minimalCPUPercentage: 0,
  maximalCPUPercentage: 100,
  minimalCPUTotalDurationMS: 0,
  maximalCPUTotalDurationMS: 10000000,
};

// How close we accept our rounding up/down.
const APPROX_FACTOR = 1.51;
const MS_PER_NS = 1000000;
const MEMORY_REGEXP = /([0-9.,]+)(TB|GB|MB|KB|B)( \(([-+]?)([0-9.,]+)(GB|MB|KB|B)\))?/;
//Example: "383.55MB (-12.5MB)"
const CPU_REGEXP = /(\~0%|idle|[0-9.,]+%|[?]) \(([0-9.,]+) ?(ns|µs|ms|s|m|h|d)\)/;
//Example: "13% (4,470ms)"

// Wait for `about:processes` to be updated.
async function promiseAboutProcessesUpdated({
  doc,
  tbody,
  force,
  tabAboutProcesses,
}) {
  let startTime = performance.now();
  let mutationPromise = new Promise(resolve => {
    let observer = new doc.ownerGlobal.MutationObserver(() => {
      info("Observed about:processes tbody childList change");
      observer.disconnect();
      resolve();
    });
    observer.observe(tbody, {
      childList: true,
      attributes: true,
      subtree: true,
    });
  });

  if (force) {
    await SpecialPowers.spawn(tabAboutProcesses.linkedBrowser, [], async () => {
      info("Forcing about:processes refresh");
      await content.Control.update(/* force = */ true);
    });
  }

  await mutationPromise;

  // Fluent will update the visible table content during the next
  // refresh driver tick, wait for it.
  await new Promise(doc.defaultView.requestAnimationFrame);

  ChromeUtils.addProfilerMarker(
    "promiseAboutProcessesUpdated",
    { startTime, category: "Test" },
    force ? "force" : undefined
  );
}

function promiseProcessDied({ childID }) {
  return new Promise(resolve => {
    let observer = properties => {
      properties.QueryInterface(Ci.nsIPropertyBag2);
      let subjectChildID = properties.get("childID");
      if (subjectChildID == childID) {
        Services.obs.removeObserver(observer, "ipc:content-shutdown");
        resolve();
      }
    };
    Services.obs.addObserver(observer, "ipc:content-shutdown");
  });
}

function isCloseEnough(value, expected) {
  if (value < 0 || expected < 0) {
    throw new Error(`Invalid isCloseEnough(${value}, ${expected})`);
  }
  if (Math.round(value) == Math.round(expected)) {
    return true;
  }
  if (expected == 0) {
    return false;
  }
  let ratio = value / expected;
  if (ratio <= APPROX_FACTOR && ratio >= 1 / APPROX_FACTOR) {
    return true;
  }
  return false;
}

function getMemoryMultiplier(unit, sign = "+") {
  let multiplier;
  switch (sign) {
    case "+":
      multiplier = 1;
      break;
    case "-":
      multiplier = -1;
      break;
    default:
      throw new Error("Invalid sign: " + sign);
  }
  switch (unit) {
    case "B":
      break;
    case "KB":
      multiplier *= 1024;
      break;
    case "MB":
      multiplier *= 1024 * 1024;
      break;
    case "GB":
      multiplier *= 1024 * 1024 * 1024;
      break;
    case "TB":
      multiplier *= 1024 * 1024 * 1024 * 1024;
      break;
    default:
      throw new Error("Invalid memory unit: " + unit);
  }
  return multiplier;
}

function getTimeMultiplier(unit) {
  switch (unit) {
    case "ns":
      return 1 / (1000 * 1000);
    case "µs":
      return 1 / 1000;
    case "ms":
      return 1;
    case "s":
      return 1000;
    case "m":
      return 60000;
  }
  throw new Error("Invalid time unit: " + unit);
}
function testCpu(string, total, slope, assumptions) {
  info(`Testing CPU display ${string} vs total ${total}, slope ${slope}`);
  if (string == "(measuring)") {
    info("Still measuring");
    return;
  }
  let [, extractedPercentage, extractedTotal, extractedUnit] = CPU_REGEXP.exec(
    string
  );

  switch (extractedPercentage) {
    case "idle":
      Assert.equal(slope, 0, "Idle means exactly 0%");
      // Nothing else to do here.
      return;
    case "~0%":
      Assert.ok(slope > 0 && slope < 0.0001);
      break;
    case "?":
      Assert.ok(slope == null);
      // Nothing else to do here.
      return;
    default: {
      // `Number.parseFloat("99%")` returns `99`.
      let computedPercentage = Number.parseFloat(extractedPercentage);
      Assert.ok(
        isCloseEnough(computedPercentage, slope * 100),
        `The displayed approximation of the slope is reasonable: ${computedPercentage} vs ${slope *
          100}`
      );
      // Also, sanity checks.
      Assert.ok(
        computedPercentage / 100 >= assumptions.minimalCPUPercentage,
        `Not too little: ${computedPercentage / 100} >=? ${
          assumptions.minimalCPUPercentage
        } `
      );
      Assert.ok(
        computedPercentage / 100 <= assumptions.maximalCPUPercentage,
        `Not too much: ${computedPercentage / 100} <=? ${
          assumptions.maximalCPUPercentage
        } `
      );
      break;
    }
  }

  let totalMS = total / MS_PER_NS;
  let computedTotal =
    // We produce localized numbers, with "," as a thousands separator in en-US builds,
    // but `parseFloat` doesn't understand the ",", so we need to remove it
    // before parsing.
    Number.parseFloat(extractedTotal.replace(/,/g, "")) *
    getTimeMultiplier(extractedUnit);
  Assert.ok(
    isCloseEnough(computedTotal, totalMS),
    `The displayed approximation of the total duration is reasonable: ${computedTotal} vs ${totalMS}`
  );
  Assert.ok(
    totalMS <= assumptions.maximalCPUTotalDurationMS &&
      totalMS >= assumptions.minimalCPUTotalDurationMS,
    `The total number of MS is reasonable ${totalMS}: [${assumptions.minimalCPUTotalDurationMS}, ${assumptions.maximalCPUTotalDurationMS}]`
  );
}

function testMemory(string, total, delta, assumptions) {
  Assert.ok(
    true,
    `Testing memory display ${string} vs total ${total}, delta ${delta}`
  );
  let extracted = MEMORY_REGEXP.exec(string);
  Assert.notEqual(
    extracted,
    null,
    `Can we parse ${string} with ${MEMORY_REGEXP}?`
  );
  let [
    ,
    extractedTotal,
    extractedUnit,
    ,
    extractedDeltaSign,
    extractedDeltaTotal,
    extractedDeltaUnit,
  ] = extracted;
  let extractedTotalNumber = Number.parseFloat(extractedTotal);
  Assert.ok(
    extractedTotalNumber > 0,
    `Unitless total memory use is greater than 0: ${extractedTotal}`
  );
  if (extractedUnit != "GB") {
    Assert.ok(
      extractedTotalNumber < 1024,
      `Unitless total memory use is less than 1024: ${extractedTotal}`
    );
  }

  // Now check that the conversion was meaningful.
  let computedTotal = getMemoryMultiplier(extractedUnit) * extractedTotalNumber;
  Assert.ok(
    isCloseEnough(computedTotal, total),
    `The displayed approximation of the total amount of memory is reasonable: ${computedTotal} vs ${total}`
  );
  if (!AppConstants.ASAN) {
    // ASAN plays tricks with RAM (e.g. allocates the entirety of virtual memory),
    // which makes this test unrealistic.
    Assert.ok(
      assumptions.minimalRAMBytesUsage <= computedTotal &&
        computedTotal <= assumptions.maximalRAMBytesUsage,
      `The total amount amount of memory is reasonable: ${computedTotal} in [${assumptions.minimalRAMBytesUsage}, ${assumptions.maximalRAMBytesUsage}]`
    );
  }

  if (extractedDeltaSign == null) {
    Assert.equal(delta || 0, 0);
    return;
  }
  let deltaTotalNumber = Number.parseFloat(
    // Remove the thousands separator that breaks parseFloat.
    extractedDeltaTotal.replace(/,/g, "")
  );
  Assert.ok(
    deltaTotalNumber > 0 && deltaTotalNumber < 1024,
    `Unitless delta memory use is in (0, 1024): ${extractedDeltaTotal}`
  );
  Assert.ok(
    ["B", "KB", "MB"].includes(extractedDeltaUnit),
    `Delta unit is reasonable: ${extractedDeltaUnit}`
  );

  // Now check that the conversion was meaningful.
  // Let's just check that the number displayed is within 10% of `delta`.
  let computedDelta =
    getMemoryMultiplier(extractedDeltaUnit, extractedDeltaSign) *
    deltaTotalNumber;
  Assert.equal(
    computedDelta >= 0,
    delta >= 0,
    `Delta has the right sign: ${computedDelta} vs ${delta}`
  );
}

function extractProcessDetails(row) {
  let children = row.children;
  let memoryResidentContent = children[1].textContent;
  let cpuContent = children[2].textContent;
  let fluentArgs = document.l10n.getAttributes(children[0].children[0]).args;
  let process = {
    memoryResidentContent,
    cpuContent,
    pidContent: fluentArgs.pid,
    typeContent: fluentArgs.type,
    threads: null,
  };
  let threadDetailsRow = row.nextSibling;
  while (threadDetailsRow) {
    if (threadDetailsRow.classList.contains("thread-summary")) {
      break;
    }
    threadDetailsRow = threadDetailsRow.nextSibling;
  }
  if (!threadDetailsRow) {
    return process;
  }
  process.threads = {
    row: threadDetailsRow,
    numberContent: document.l10n.getAttributes(
      threadDetailsRow.children[0].children[1]
    ).args.number,
  };
  return process;
}

function findTabRowByName(doc, name) {
  for (let row of doc.getElementsByClassName("name")) {
    if (!row.parentNode.classList.contains("window")) {
      continue;
    }
    let foundName = document.l10n.getAttributes(row.children[0]).args.name;
    if (foundName != name) {
      continue;
    }
    return row.parentNode;
  }
  return null;
}

function findProcessRowByOrigin(doc, origin) {
  for (let row of doc.getElementsByClassName("process")) {
    if (row.process.origin == origin) {
      return row;
    }
  }
  return null;
}

async function setupTabWithOriginAndTitle(origin, title) {
  let tab = BrowserTestUtils.addTab(gBrowser, origin, { skipAnimation: true });
  tab.testTitle = title;
  tab.testOrigin = origin;
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await SpecialPowers.spawn(tab.linkedBrowser, [title], async title => {
    content.document.title = title;
  });
  return tab;
}

async function testAboutProcessesWithConfig({ showAllFrames, showThreads }) {
  const isFission = Services.prefs.getBoolPref("fission.autostart");
  Services.prefs.setBoolPref(
    "toolkit.aboutProcesses.showAllSubframes",
    showAllFrames
  );
  Services.prefs.setBoolPref("toolkit.aboutProcesses.showThreads", showThreads);

  // Install a test extension to also cover processes and sub-frames related to the
  // extension process.
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: { gecko: { id: "test-aboutprocesses@mochi.test" } },
    },
    background() {
      // Creates an about:blank iframe in the extension process to make sure that
      // Bug 1665099 doesn't regress.
      document.body.appendChild(document.createElement("iframe"));

      this.browser.test.sendMessage("bg-page-loaded");
    },
  });

  await extension.startup();
  await extension.awaitMessage("bg-page-loaded");

  // Setup tabs asynchronously.

  // The about:processes tab.
  info("Setting up about:processes");
  let promiseTabAboutProcesses = BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:processes",
    waitForLoad: true,
  });

  info("Setting up example.com");
  // Another tab that we'll pretend is hung.
  let promiseTabHung = (async function() {
    let tab = BrowserTestUtils.addTab(gBrowser, "http://example.com", {
      skipAnimation: true,
    });
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
      // Open an in-process iframe to test toolkit.aboutProcesses.showAllSubframes
      let frame = content.document.createElement("iframe");
      content.document.body.appendChild(frame);
    });
    return tab;
  })();

  info("Setting up tabs we intend to close");

  // The two following tabs share the same domain.
  // We use them to check that closing one doesn't close the other.
  let promiseTabCloseSeparately1 = setupTabWithOriginAndTitle(
    "http://example.org",
    "Close me 1 (separately)"
  );
  let promiseTabCloseSeparately2 = setupTabWithOriginAndTitle(
    "http://example.org",
    "Close me 2 (separately)"
  );

  // The two following tabs share the same domain.
  // We use them to check that closing the process kills them both.
  let promiseTabCloseProcess1 = setupTabWithOriginAndTitle(
    "http://example.net",
    "Close me 1 (process)"
  );

  let promiseTabCloseProcess2 = setupTabWithOriginAndTitle(
    "http://example.net",
    "Close me 2 (process)"
  );

  // The two following tabs share the same domain.
  // We use them to check that closing the process kills them both.
  let promiseTabCloseTogether1 = setupTabWithOriginAndTitle(
    "https://example.org",
    "Close me 1 (together)"
  );

  let promiseTabCloseTogether2 = setupTabWithOriginAndTitle(
    "https://example.org",
    "Close me 2 (together)"
  );

  // Wait for initialization to finish.
  let tabAboutProcesses = await promiseTabAboutProcesses;
  let tabHung = await promiseTabHung;
  let tabCloseSeparately1 = await promiseTabCloseSeparately1;
  let tabCloseSeparately2 = await promiseTabCloseSeparately2;
  let tabCloseProcess1 = await promiseTabCloseProcess1;
  let tabCloseProcess2 = await promiseTabCloseProcess2;
  let tabCloseTogether1 = await promiseTabCloseTogether1;
  let tabCloseTogether2 = await promiseTabCloseTogether2;

  let doc = tabAboutProcesses.linkedBrowser.contentDocument;
  let tbody = doc.getElementById("process-tbody");
  Assert.ok(doc);
  Assert.ok(tbody);

  info("Setting up fake process hang detector");
  let hungChildID = tabHung.linkedBrowser.frameLoader.childID;

  // Keep informing about:processes that `tabHung` is hung.
  // Note: this is a background task, do not `await` it.
  let fakeProcessHangMonitor = async function() {
    for (let i = 0; i < 100; ++i) {
      if (!tabHung.linkedBrowser) {
        // Let's stop spamming as soon as we can.
        return;
      }

      Services.obs.notifyObservers(
        {
          childID: hungChildID,
          hangType: Ci.nsIHangReport.PLUGIN_HANG,
          pluginName: "Fake plug-in",
          QueryInterface: ChromeUtils.generateQI(["nsIHangReport"]),
        },
        "process-hang-report"
      );

      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      await new Promise(resolve => setTimeout(resolve, 300));
    }
  };
  fakeProcessHangMonitor();

  // about:processes will take a little time to appear and be populated.
  await promiseAboutProcessesUpdated({ doc, tbody, tabAboutProcesses });
  Assert.ok(tbody.childElementCount, "The table should be populated");
  Assert.ok(
    !!tbody.getElementsByClassName("hung").length,
    "The hung process should appear"
  );

  info("Looking at the contents of about:processes");
  let processesToBeFound = [
    // The browser process.
    {
      name: "browser",
      type: ["browser"],
      predicate: row => row.process.type == "browser",
    },
    // The hung process.
    {
      name: "hung",
      type: ["web", "webIsolated"],
      predicate: row =>
        row.classList.contains("hung") && row.classList.contains("process"),
    },
    // Any non-hung process
    {
      name: "non-hung",
      type: ["web", "webIsolated"],
      predicate: row =>
        !row.classList.contains("hung") &&
        row.classList.contains("process") &&
        row.process.type == "web",
    },
  ];
  for (let finder of processesToBeFound) {
    info(`Running sanity tests on ${finder.name}`);
    let row = tbody.firstChild;
    while (row) {
      if (finder.predicate(row)) {
        break;
      }
      row = row.nextSibling;
    }
    Assert.ok(!!row, `found a table row for ${finder.name}`);
    let {
      memoryResidentContent,
      cpuContent,
      pidContent,
      typeContent,
      threads,
    } = extractProcessDetails(row);

    info("Sanity checks: type");
    Assert.ok(
      finder.type.includes(typeContent),
      `Type ${typeContent} should be one of ${finder.type}`
    );

    info("Sanity checks: pid");
    let pid = Number.parseInt(pidContent);
    Assert.ok(pid > 0, `Checking pid ${pidContent}`);
    Assert.equal(pid, row.process.pid);

    info("Sanity checks: memory resident");
    testMemory(
      memoryResidentContent,
      row.process.totalRamSize,
      row.process.deltaRamSize,
      HARDCODED_ASSUMPTIONS_PROCESS
    );

    info("Sanity checks: CPU (Total)");
    testCpu(
      cpuContent,
      row.process.totalCpu,
      row.process.slopeCpu,
      HARDCODED_ASSUMPTIONS_PROCESS
    );

    // Testing threads.
    if (!showThreads) {
      info("In this mode, we shouldn't display any threads");
      Assert.equal(
        threads,
        null,
        "In hidden threads mode, we shouldn't have any thread summary"
      );
    } else {
      info("Sanity checks: number of threads");
      let numberOfThreads = Number.parseInt(threads.numberContent);
      Assert.ok(
        numberOfThreads >= HARDCODED_ASSUMPTIONS_PROCESS.minimalNumberOfThreads
      );
      Assert.ok(
        numberOfThreads <= HARDCODED_ASSUMPTIONS_PROCESS.maximalNumberOfThreads
      );
      Assert.equal(
        numberOfThreads,
        row.process.threads.length,
        "The number we display should be the number of threads"
      );

      info("Testing that we can open the list of threads");
      let twisty = threads.row.getElementsByClassName("twisty")[0];
      twisty.click();

      // Since `twisty.click()` is partially async, we need to wait
      // until all the threads are properly displayed.
      await promiseAboutProcessesUpdated({ doc, tbody, tabAboutProcesses });
      let numberOfThreadsFound = 0;
      for (
        let threadRow = threads.row.nextSibling;
        threadRow && threadRow.classList.contains("thread");
        threadRow = threadRow.nextSibling
      ) {
        numberOfThreadsFound++;
      }
      Assert.equal(
        numberOfThreadsFound,
        numberOfThreads,
        `We should see ${numberOfThreads} threads, found ${numberOfThreadsFound}`
      );
      for (
        let threadRow = threads.row.nextSibling;
        threadRow && threadRow.classList.contains("thread");
        threadRow = threadRow.nextSibling
      ) {
        Assert.ok(
          threadRow.children.length >= 3 && threadRow.children[1].textContent,
          "The thread row should be populated"
        );
        let children = threadRow.children;
        let cpuContent = children[1].textContent;
        let tidContent = document.l10n.getAttributes(children[0].children[0])
          .args.tid;

        // Sanity checks: tid
        let tid = Number.parseInt(tidContent);
        Assert.notEqual(tid, 0, "The tid should be set");
        Assert.equal(tid, threadRow.thread.tid, "Displayed tid is correct");

        // Sanity checks: CPU (per thread)
        testCpu(
          cpuContent,
          threadRow.thread.totalCpu,
          threadRow.thread.slopeCpu,
          HARDCODED_ASSUMPTIONS_THREAD
        );
      }
    }
  }

  // Testing subframes.
  info("Testing subframes");
  let foundAtLeastOneInProcessSubframe = false;
  for (let row of doc.getElementsByClassName("window")) {
    let subframe = row.win;
    if (subframe.tab) {
      continue;
    }
    let url = document.l10n.getAttributes(row.children[0].children[0]).args.url;
    Assert.equal(url, subframe.documentURI.spec);
    if (!subframe.isProcessRoot) {
      foundAtLeastOneInProcessSubframe = true;
    }
  }
  if (showAllFrames) {
    Assert.ok(
      foundAtLeastOneInProcessSubframe,
      "Found at least one about:blank in-process subframe"
    );
  } else {
    Assert.ok(
      !foundAtLeastOneInProcessSubframe,
      "We shouldn't have any about:blank in-process subframe"
    );
  }

  await promiseAboutProcessesUpdated({
    doc,
    tbody,
    force: true,
    tabAboutProcesses,
  });

  info("Double-clicking on a tab");
  let whenTabSwitchedToWeb = BrowserTestUtils.switchTab(gBrowser, () => {
    // We pass a function to use `BrowserTestUtils.switchTab` not in its
    // role as a tab switcher but rather in its role as a function that
    // waits until something else has switched the tab.
    // We'll actually cause tab switching below, by doucle-clicking
    // in `about:processes`.
  });
  await SpecialPowers.spawn(tabAboutProcesses.linkedBrowser, [], async () => {
    // Locate and double-click on the representation of `tabHung`.
    let tbody = content.document.getElementById("process-tbody");
    for (let row of tbody.getElementsByClassName("tab")) {
      if (row.parentNode.win.documentURI.spec != "http://example.com/") {
        continue;
      }
      // Simulate double-click.
      let evt = new content.window.MouseEvent("dblclick", {
        bubbles: true,
        cancelable: true,
        view: content.window,
      });
      row.dispatchEvent(evt);
      return;
    }
    Assert.ok(false, "We should have found the hung tab");
  });

  info("Waiting for tab switch");
  await whenTabSwitchedToWeb;
  Assert.equal(
    gBrowser.selectedTab.linkedBrowser.currentURI.spec,
    tabHung.linkedBrowser.currentURI.spec,
    "We should have focused the hung tab"
  );

  await BrowserTestUtils.switchTab(gBrowser, tabAboutProcesses);

  info("Double-clicking on the extensions process");
  let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, "about:addons");
  await SpecialPowers.spawn(tabAboutProcesses.linkedBrowser, [], async () => {
    let extensionsRow = content.document.getElementsByClassName(
      "extensions"
    )[0];
    Assert.ok(!!extensionsRow, "We should have found the extensions process");
    let evt = new content.window.MouseEvent("dblclick", {
      bubbles: true,
      cancelable: true,
      view: content.window,
    });
    extensionsRow.dispatchEvent(evt);
  });
  info("Waiting for about:addons to open");
  await tabPromise;
  Assert.equal(
    gBrowser.selectedTab.linkedBrowser.currentURI.spec,
    "about:addons",
    "We should now see the addon tab"
  );
  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  info("Testing tab closing");

  // A list of processes we have killed and for which we're waiting
  // death confirmation. Only used in Fission.
  let waitForProcessesToDisappear = [];
  await promiseAboutProcessesUpdated({
    doc,
    tbody,
    force: true,
    tabAboutProcesses,
  });
  if (isFission) {
    // Before closing, all our origins should be present
    for (let origin of [
      "http://example.com", // tabHung
      "http://example.net", // tabCloseProcess*
      "http://example.org", // tabCloseSeparately*
      "https://example.org", // tabCloseTogether*
    ]) {
      Assert.ok(
        findProcessRowByOrigin(doc, origin),
        `There is a process for origin ${origin}`
      );
    }

    // These origins will disappear.
    for (let origin of [
      "http://example.net", // tabCloseProcess*
      "https://example.org", // tabCloseTogether*
    ]) {
      let row = findProcessRowByOrigin(doc, origin);
      let childID = row.process.childID;
      waitForProcessesToDisappear.push(promiseProcessDied({ childID }));
    }
  }

  // Close a few tabs.
  for (let tab of [tabCloseSeparately1, tabCloseTogether1, tabCloseTogether2]) {
    info("Closing a tab through about:processes");
    let found = findTabRowByName(doc, tab.linkedBrowser.contentTitle);
    Assert.ok(
      found,
      `We should have found tab ${tab.linkedBrowser.contentTitle} to close it`
    );
    let closeIcons = found.getElementsByClassName("close-icon");
    Assert.equal(
      closeIcons.length,
      1,
      "This tab should have exactly one close icon"
    );
    closeIcons[0].click();
    Assert.ok(
      found.classList.contains("killing"),
      "We should have marked the row as dying"
    );
  }

  //...and a process, if we're in Fission.
  if (isFission) {
    info("Closing an entire process through about:processes");
    let found = findProcessRowByOrigin(doc, "http://example.net");
    let closeIcons = found.getElementsByClassName("close-icon");
    Assert.equal(
      closeIcons.length,
      1,
      "This process should have exactly one close icon"
    );
    closeIcons[0].click();
    Assert.ok(
      found.classList.contains("killing"),
      "We should have marked the row as dying"
    );
  }

  // Give Firefox a little time to close the tabs and update about:processes.
  // This might take two updates as we're racing between collecting data and
  // processes actually being killed.
  await promiseAboutProcessesUpdated({
    doc,
    tbody,
    force: true,
    tabAboutProcesses,
  });

  // The tabs we have closed directly or indirectly should now be (closed or crashed) and invisible in about:processes.
  for (let { origin, tab } of [
    { origin: "http://example.org", tab: tabCloseSeparately1 },
    { origin: "https://example.org", tab: tabCloseTogether1 },
    { origin: "https://example.org", tab: tabCloseTogether2 },
    ...(isFission
      ? [
          { origin: "http://example.net", tab: tabCloseProcess1 },
          { origin: "http://example.net", tab: tabCloseProcess2 },
        ]
      : []),
  ]) {
    // Tab shouldn't show up anymore in about:processes
    Assert.ok(
      !findTabRowByName(doc, origin),
      `Tab for ${origin} shouldn't show up anymore in about:processes`
    );
    // ...and should be unloaded.
    Assert.ok(
      !tab.getAttribute("linkedPanel"),
      `The tab should now be unloaded (${tab.testOrigin} - ${tab.testTitle})`
    );
  }

  // On the other hand, tabs we haven't closed should still be open and visible in about:processes.
  Assert.ok(
    tabCloseSeparately2.linkedBrowser,
    "Killing one tab in the domain should not have closed the other tab"
  );
  let foundtabCloseSeparately2 = findTabRowByName(
    doc,
    tabCloseSeparately2.linkedBrowser.contentTitle
  );
  Assert.ok(
    foundtabCloseSeparately2,
    "The second tab is still visible in about:processes"
  );

  if (isFission) {
    // After closing, we must have closed some of our origins.
    for (let origin of [
      "http://example.com", // tabHung
      "http://example.org", // tabCloseSeparately*
    ]) {
      Assert.ok(
        findProcessRowByOrigin(doc, origin),
        `There should still be a process row for origin ${origin}`
      );
    }

    info("Waiting for processes to die");
    await Promise.all(waitForProcessesToDisappear);

    info("Waiting for about:processes to be updated");
    await promiseAboutProcessesUpdated({
      doc,
      tbody,
      force: true,
      tabAboutProcesses,
    });

    for (let origin of [
      "http://example.net", // tabCloseProcess*
      "https://example.org", // tabCloseTogether*
    ]) {
      Assert.ok(
        !findProcessRowByOrigin(doc, origin),
        `Process ${origin} should disappear from about:processes`
      );
    }
  }

  info("Additional sanity check for all processes");
  for (let row of document.getElementsByClassName("process")) {
    let { pidContent, typeContent } = extractProcessDetails(row);
    Assert.equal(typeContent, row.process.type);
    Assert.equal(Number.parseInt(pidContent), row.process.pid);
  }
  BrowserTestUtils.removeTab(tabAboutProcesses);
  BrowserTestUtils.removeTab(tabHung);
  BrowserTestUtils.removeTab(tabCloseSeparately2);

  // We still need to remove these tabs.
  // We killed the process, but we don't want to leave zombie tabs lying around.
  BrowserTestUtils.removeTab(tabCloseProcess1);
  BrowserTestUtils.removeTab(tabCloseProcess2);

  Services.prefs.clearUserPref("toolkit.aboutProcesses.showAllSubframes");
  Services.prefs.clearUserPref("toolkit.aboutProcesses.showThreads");

  await extension.unload();
}
