/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Return a web-based URL for a given file based on the testing directory.
 * @param {String} fileName
 *        file that caller wants its web-based url
 */
function GetTestWebBasedURL(fileName) {
  return (
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "http://example.org"
    ) + fileName
  );
}

const kNS_PER_MS = 1000000;

function printProcInfo(procInfo) {
  info(
    `  pid: ${procInfo.pid}, type = parent, cpu time = ${procInfo.cpuTime /
      kNS_PER_MS}ms`
  );
  for (let child of procInfo.children) {
    info(
      `  pid: ${child.pid}, type = ${child.type}, cpu time = ${child.cpuTime /
        kNS_PER_MS}ms`
    );
  }
}

// It would be nice to have an API to list all the statically defined labels of
// a labeled_counter. Hopefully bug 1672273 will help with this.
const kGleanProcessTypeLabels = [
  "parent.active",
  "parent.active.playing-audio",
  "parent.active.playing-video",
  "parent.inactive",
  "parent.inactive.playing-audio",
  "parent.inactive.playing-video",
  "prealloc",
  "privilegedabout",
  "rdd",
  "socket",
  "web.background",
  "web.background-perceivable",
  "web.foreground",
  "extension",
  "gpu",
  "gmplugin",
  "utility",
  "__other__",
];

async function getChildCpuTime(pid) {
  return (await ChromeUtils.requestProcInfo()).children.find(p => p.pid == pid)
    .cpuTime;
}

add_task(async () => {
  // Temporarily open a new foreground tab to make the current tab a background
  // tab, and burn some CPU time in it while it's in the background.
  const kBusyWaitForMs = 50;
  let cpuTimeSpentOnBackgroundTab;
  let firstBrowser = gBrowser.selectedTab.linkedBrowser;
  let processPriorityChangedPromise = BrowserTestUtils.contentTopicObserved(
    firstBrowser.browsingContext,
    "ipc:process-priority-changed"
  );
  await BrowserTestUtils.withNewTab(
    GetTestWebBasedURL("dummy.html"),
    async () => {
      await processPriorityChangedPromise;
      // We can't be sure that a busy loop lasting for a specific duration
      // will use the same amount of CPU time, as that would require a core
      // to be fully available for our busy loop, which is unlikely on single
      // core hardware.
      // To be able to have a predictable amount of CPU time used, we need to
      // check using ChromeUtils.requestProcInfo how much CPU time has actually
      // been spent.
      let pid = firstBrowser.frameLoader.remoteTab.osPid;
      let initalCpuTime = await getChildCpuTime(pid);
      let afterCpuTime;
      do {
        await SpecialPowers.spawn(
          firstBrowser,
          [kBusyWaitForMs],
          async kBusyWaitForMs => {
            let startTime = Date.now();
            while (Date.now() - startTime < 10) {
              // Burn CPU time...
            }
          }
        );
        afterCpuTime = await getChildCpuTime(pid);
      } while (afterCpuTime - initalCpuTime < kBusyWaitForMs * kNS_PER_MS);
      cpuTimeSpentOnBackgroundTab = Math.floor(
        (afterCpuTime - initalCpuTime) / kNS_PER_MS
      );
    }
  );

  let beforeProcInfo = await ChromeUtils.requestProcInfo();
  await Services.fog.testFlushAllChildren();

  let cpuTimeByType = {},
    gpuTimeByType = {};
  for (let label of kGleanProcessTypeLabels) {
    cpuTimeByType[label] = Glean.power.cpuTimePerProcessTypeMs[
      label
    ].testGetValue();
    gpuTimeByType[label] = Glean.power.gpuTimePerProcessTypeMs[
      label
    ].testGetValue();
  }
  let totalCpuTime = Glean.power.totalCpuTimeMs.testGetValue();
  let totalGpuTime = Glean.power.totalGpuTimeMs.testGetValue();

  let afterProcInfo = await ChromeUtils.requestProcInfo();

  info("CPU time from ProcInfo before calling testFlushAllChildren:");
  printProcInfo(beforeProcInfo);

  info("CPU time for each label:");
  let totalCpuTimeByType = 0;
  for (let label of kGleanProcessTypeLabels) {
    totalCpuTimeByType += cpuTimeByType[label] ?? 0;
    info(`  ${label} = ${cpuTimeByType[label]}`);
  }

  info("CPU time from ProcInfo after calling testFlushAllChildren:");
  printProcInfo(afterProcInfo);

  Assert.equal(
    totalCpuTimeByType,
    totalCpuTime,
    "The sum of CPU time used by all process types should match totalCpuTimeMs"
  );

  // In infra the parent process time will be reported as parent.inactive,
  // but when running the test locally the user might move the mouse a bit.
  let parentTime =
    (cpuTimeByType["parent.active"] || 0) +
    (cpuTimeByType["parent.inactive"] || 0);
  Assert.greaterOrEqual(
    parentTime,
    Math.floor(beforeProcInfo.cpuTime / kNS_PER_MS),
    "reported parent cpu time should be at least what the first requestProcInfo returned"
  );
  Assert.lessOrEqual(
    parentTime,
    Math.ceil(afterProcInfo.cpuTime / kNS_PER_MS),
    "reported parent cpu time should be at most what the second requestProcInfo returned"
  );

  kGleanProcessTypeLabels
    .filter(label => label.startsWith("parent.") && label.includes(".playing-"))
    .forEach(label => {
      Assert.strictEqual(
        cpuTimeByType[label],
        undefined,
        `no media was played so the CPU time for ${label} should be undefined`
      );
    });

  if (beforeProcInfo.children.some(p => p.type == "preallocated")) {
    Assert.greaterOrEqual(
      cpuTimeByType.prealloc,
      beforeProcInfo.children.reduce(
        (time, p) =>
          time +
          (p.type == "preallocated" ? Math.floor(p.cpuTime / kNS_PER_MS) : 0),
        0
      ),
      "reported cpu time for preallocated content processes should be at least the sum of what the first requestProcInfo returned."
    );
    // We can't compare with the values returned by the second requestProcInfo
    // call because one preallocated content processes has been turned into
    // a normal content process when we opened a tab.
  } else {
    info(
      "no preallocated content process existed when we started our test, but some might have existed before"
    );
  }

  if (beforeProcInfo.children.some(p => p.type == "privilegedabout")) {
    Assert.greaterOrEqual(
      cpuTimeByType.privilegedabout,
      1,
      "we used some CPU time in a foreground tab, but don't know how much as the process might have started as preallocated"
    );
  }

  for (let label of [
    "rdd",
    "socket",
    "extension",
    "gpu",
    "gmplugin",
    "utility",
  ]) {
    if (!kGleanProcessTypeLabels.includes(label)) {
      Assert.ok(
        false,
        `coding error in the test, ${label} isn't part of ${kGleanProcessTypeLabels.join(
          ", "
        )}`
      );
    }
    if (beforeProcInfo.children.some(p => p.type == label)) {
      Assert.greaterOrEqual(
        cpuTimeByType[label],
        Math.floor(
          beforeProcInfo.children.find(p => p.type == label).cpuTime /
            kNS_PER_MS
        ),
        "reported cpu time for " +
          label +
          " process should be at least what the first requestProcInfo returned."
      );
      Assert.lessOrEqual(
        cpuTimeByType[label],
        Math.ceil(
          afterProcInfo.children.find(p => p.type == label).cpuTime / kNS_PER_MS
        ),
        "reported cpu time for " +
          label +
          " process should be at most what the second requestProcInfo returned."
      );
    } else {
      info(
        "no " +
          label +
          " process existed when we started our test, but some might have existed before"
      );
    }
  }

  Assert.greaterOrEqual(
    cpuTimeByType["web.background"],
    cpuTimeSpentOnBackgroundTab,
    "web.background should be at least the time we spent."
  );

  Assert.greaterOrEqual(
    cpuTimeByType["web.foreground"],
    1,
    "we used some CPU time in a foreground tab, but don't know how much"
  );

  // We only have web.background-perceivable CPU time if a muted video was
  // played in a background tab.
  Assert.strictEqual(
    cpuTimeByType["web.background-perceivable"],
    undefined,
    "CPU time should only be recorded in the web.background-perceivable label"
  );

  // __other__ should be undefined, if it is not, we have a missing label in the metrics.yaml file.
  Assert.strictEqual(
    cpuTimeByType.__other__,
    undefined,
    "no CPU time should be recorded in the __other__ label"
  );

  info("GPU time for each label:");
  let totalGpuTimeByType = undefined;
  for (let label of kGleanProcessTypeLabels) {
    if (gpuTimeByType[label] !== undefined) {
      totalGpuTimeByType = (totalGpuTimeByType || 0) + gpuTimeByType[label];
    }
    info(`  ${label} = ${gpuTimeByType[label]}`);
  }

  Assert.equal(
    totalGpuTimeByType,
    totalGpuTime,
    "The sum of GPU time used by all process types should match totalGpuTimeMs"
  );

  // __other__ should be undefined, if it is not, we have a missing label in the metrics.yaml file.
  Assert.strictEqual(
    gpuTimeByType.__other__,
    undefined,
    "no GPU time should be recorded in the __other__ label"
  );
});
