/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

// A bunch of assumptions we make about the behavior of the parent process,
// and which we use as sanity checks. If Firefox evolves, we will need to
// update these values.
const HARDCODED_ASSUMPTIONS_PROCESS = {
  minimalNumberOfThreads: 10,
  maximalNumberOfThreads: 1000,
  minimalCPUPercentage: 0.0001,
  maximalCPUPercentage: 1000,
  minimalCPUTotalDurationMS: 100,
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
const APPROX_FACTOR = 1.1;
const MS_PER_NS = 1000000;
const MEMORY_REGEXP = /([0-9.,]+)(TB|GB|MB|KB|B)( \(([-+]?)([0-9.,]+)(GB|MB|KB|B)\))?/;
//Example: "383.55MB (-12.5MB)"
const CPU_REGEXP = /(\~0%|idle|[0-9.,]+%|[?]) \(([0-9.,]+) ?(ms)\)/;
//Example: "13% (4,470ms)"

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
  if (unit == "ms") {
    return 1;
  }
  throw new Error("Invalid time unit: " + unit);
}
function testCpu(string, total, slope, assumptions) {
  info(`Testing CPU display ${string} vs total ${total}, slope ${slope}`);
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
    Assert.ok(delta == 0);
    return;
  }
  let deltaTotalNumber = Number.parseFloat(extractedDeltaTotal);
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
    extractedDeltaTotal;
  Assert.equal(
    computedDelta >= 0,
    delta >= 0,
    `Delta has the right sign: ${computedDelta} vs ${delta}`
  );
  Assert.ok(
    isCloseEnough(Math.abs(computedDelta), Math.abs(delta)),
    `The displayed approximation of the delta amount of memory is reasonable: ${computedDelta} vs ${delta}`
  );
}

add_task(async function testAboutProcesses() {
  info("Setting up about:processes");

  // The tab we're testing.
  let tabAboutProcesses = (gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    "about:processes"
  ));
  await BrowserTestUtils.browserLoaded(tabAboutProcesses.linkedBrowser);

  info("Setting up example.com");
  // Another tab that we'll pretend is hung.
  let tabHung = BrowserTestUtils.addTab(gBrowser, "http://example.com");
  await BrowserTestUtils.browserLoaded(tabHung.linkedBrowser);

  info("Setting up fake process hang detector");
  let hungChildID = tabHung.linkedBrowser.frameLoader.childID;

  let doc = tabAboutProcesses.linkedBrowser.contentDocument;
  let tbody = doc.getElementById("process-tbody");

  // Keep informing about:processes that `tabHung` is hung.
  // Note: this is a background task, do not `await` it.
  let isProcessHangDetected = false;
  let fakeProcessHangMonitor = async function() {
    for (let i = 0; i < 100; ++i) {
      if (isProcessHangDetected || !tabHung.linkedBrowser) {
        // Let's stop spamming as soon as we can.
        return;
      }
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      await new Promise(resolve => setTimeout(resolve, 300));
      Services.obs.notifyObservers(
        {
          childID: hungChildID,
          hangType: Ci.nsIHangReport.PLUGIN_HANG,
          pluginName: "Fake plug-in",
          QueryInterface: ChromeUtils.generateQI(["nsIHangReport"]),
        },
        "process-hang-report"
      );
    }
  };
  fakeProcessHangMonitor();

  info("Waiting for the first update of about:processes");
  // Wait until the table has first been populated.
  await TestUtils.waitForCondition(() => tbody.childElementCount);

  info("Waiting for the second update of about:processes");
  // And wait for another update using a mutation observer, to give our newly created test tab some time
  // to burn some CPU.
  await new Promise(resolve => {
    let observer = new doc.ownerGlobal.MutationObserver(() => {
      observer.disconnect();
      resolve();
    });
    observer.observe(tbody, { childList: true });
  });

  info("Looking at the contents of about:processes");
  // Find the row for the browser process.
  let row = tbody.firstChild;
  while (row && row.children[1].textContent != "browser") {
    row = row.nextSibling;
  }

  Assert.ok(row, "found a table row for the browser");
  let children = row.children;
  let pidContent = children[0].textContent;
  let memoryResidentContent = children[2].textContent;
  let cpuContent = children[3].textContent;
  let numberOfThreadsContent = children[4].textContent;

  info("Sanity checks: pid");
  let pid = Number.parseInt(pidContent);
  Assert.ok(pid > 0);
  Assert.equal(pid, row.process.pid);

  info("Sanity checks: memory resident");
  testMemory(
    memoryResidentContent,
    row.process.totalResidentSize,
    row.process.deltaResidentSize,
    HARDCODED_ASSUMPTIONS_PROCESS
  );

  info("Sanity checks: CPU (Total)");
  testCpu(
    cpuContent,
    row.process.totalCpu,
    row.process.slopeCpu,
    HARDCODED_ASSUMPTIONS_PROCESS
  );

  info("Sanity checks: number of threads");
  let numberOfThreads = Number.parseInt(numberOfThreadsContent);
  Assert.ok(
    numberOfThreads >= HARDCODED_ASSUMPTIONS_PROCESS.minimalNumberOfThreads
  );
  Assert.ok(
    numberOfThreads <= HARDCODED_ASSUMPTIONS_PROCESS.maximalNumberOfThreads
  );

  info("Testing that we can open the list of threads");
  let twisty = row.getElementsByClassName("twisty")[0];
  EventUtils.synthesizeMouseAtCenter(
    twisty,
    {},
    tabAboutProcesses.linkedBrowser.contentWindow
  );

  let numberOfThreadsFound = 0;
  for (
    let threadRow = row.nextSibling;
    threadRow && threadRow.classList.contains("thread");
    threadRow = threadRow.nextSibling
  ) {
    let children = threadRow.children;
    let tidContent = children[0].textContent;
    let cpuContent = children[3].textContent;
    ++numberOfThreadsFound;

    info("Sanity checks: tid");
    let tid = Number.parseInt(tidContent);
    Assert.notEqual(tid, 0, "The tid should be set");
    Assert.equal(tid, threadRow.thread.tid);

    info("Sanity checks: CPU (User and Kernel)");
    testCpu(
      cpuContent,
      threadRow.thread.totalCpu,
      threadRow.thread.slopeCpu,
      HARDCODED_ASSUMPTIONS_THREAD
    );
  }
  Assert.equal(numberOfThreads, numberOfThreadsFound);

  info("Ensuring that the hung process is marked as hung");
  let isOneNonHungProcessDetected = false;
  for (let row of tbody.getElementsByClassName("process")) {
    if (row.classList.contains("hung")) {
      if (row.process.childID == hungChildID) {
        isProcessHangDetected = true;
      }
    } else {
      isOneNonHungProcessDetected = true;
    }
    if (isProcessHangDetected && isOneNonHungProcessDetected) {
      break;
    }
  }

  Assert.ok(isProcessHangDetected, "We have found our hung process");
  Assert.ok(
    isOneNonHungProcessDetected,
    "We have found at least one non-hung process"
  );

  BrowserTestUtils.removeTab(tabAboutProcesses);
  BrowserTestUtils.removeTab(tabHung);
});
