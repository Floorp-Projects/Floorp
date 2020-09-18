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

add_task(async function testAboutProcesses() {
  info("Setting up about:processes");

  // Test twice, once without `showAllSubframes`, once with it.
  for (let showAllFrames of [false, true]) {
    Services.prefs.setBoolPref(
      "toolkit.aboutProcesses.showAllSubframes",
      showAllFrames
    );
    // Test twice, once without `showThreads`, once with it.
    for (let showThreads of [false, true]) {
      Services.prefs.setBoolPref(
        "toolkit.aboutProcesses.showThreads",
        showThreads
      );
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
      await SpecialPowers.spawn(tabHung.linkedBrowser, [], async () => {
        // Open an in-process iframe to test toolkit.aboutProcesses.showAllSubframes
        let frame = content.document.createElement("iframe");
        content.document.body.appendChild(frame);
      });

      info("Setting up fake process hang detector");
      let hungChildID = tabHung.linkedBrowser.frameLoader.childID;

      let doc = tabAboutProcesses.linkedBrowser.contentDocument;
      let tbody = doc.getElementById("process-tbody");

      // Keep informing about:processes that `tabHung` is hung.
      // Note: this is a background task, do not `await` it.
      let fakeProcessHangMonitor = async function() {
        for (let i = 0; i < 100; ++i) {
          if (!tabHung.linkedBrowser) {
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
          name: "hung",
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
        Assert.ok(row, `found a table row for ${finder.name}`);
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
          row.process.totalResidentUniqueSize,
          row.process.deltaResidentUniqueSize,
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
            numberOfThreads >=
              HARDCODED_ASSUMPTIONS_PROCESS.minimalNumberOfThreads
          );
          Assert.ok(
            numberOfThreads <=
              HARDCODED_ASSUMPTIONS_PROCESS.maximalNumberOfThreads
          );
          Assert.equal(numberOfThreads, row.process.threads.length);

          info("Testing that we can open the list of threads");
          let twisty = threads.row.getElementsByClassName("twisty")[0];
          twisty.click();
          let numberOfThreadsFound = 0;
          for (
            let threadRow = threads.row.nextSibling;
            threadRow && threadRow.classList.contains("thread");
            threadRow = threadRow.nextSibling
          ) {
            let children = threadRow.children;
            let cpuContent = children[2].textContent;
            let tidContent = document.l10n.getAttributes(
              children[0].children[0]
            ).args.tid;
            ++numberOfThreadsFound;

            info("Sanity checks: tid");
            let tid = Number.parseInt(tidContent);
            Assert.notEqual(tid, 0, "The tid should be set");
            Assert.equal(tid, threadRow.thread.tid, "Displayed tid is correct");

            info("Sanity checks: CPU (User and Kernel)");
            testCpu(
              cpuContent,
              threadRow.thread.totalCpu,
              threadRow.thread.slopeCpu,
              HARDCODED_ASSUMPTIONS_THREAD
            );
          }
          Assert.equal(
            numberOfThreads,
            numberOfThreadsFound,
            "Found as many threads as expected"
          );
        }

        // Testing subframes.
        let foundAtLeastOneInProcessSubframe = false;
        for (let row of doc.getElementsByClassName("window")) {
          let subframe = row.win;
          if (subframe.tab) {
            continue;
          }
          let url = document.l10n.getAttributes(row.children[0].children[0])
            .args.url;
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
      }

      info("Additional sanity check for all processes");
      for (let row of document.getElementsByClassName("process")) {
        let { pidContent, typeContent } = extractProcessDetails(row);
        Assert.equal(typeContent, row.process.type);
        Assert.equal(Number.parseInt(pidContent), row.process.pid);
      }
      BrowserTestUtils.removeTab(tabAboutProcesses);
      BrowserTestUtils.removeTab(tabHung);
    }
  }

  Services.prefs.clearUserPref("toolkit.aboutProcesses.showThreads");
});
