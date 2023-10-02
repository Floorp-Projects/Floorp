/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const DUMMY_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "http://example.com"
  ) + "/dummy.html";

const isFissionEnabled = SpecialPowers.useRemoteSubframes;

const SAMPLE_SIZE = 10;
const NS_PER_MS = 1000000;

function checkProcessCpuTime(proc) {
  Assert.greater(proc.cpuTime, 0, "Got some cpu time");

  let cpuThreads = 0;
  for (let thread of proc.threads) {
    cpuThreads += Math.floor(thread.cpuTime / NS_PER_MS);
  }
  Assert.greater(cpuThreads, 0, "Got some cpu time in the threads");
  let processCpuTime = Math.ceil(proc.cpuTime / NS_PER_MS);
  if (AppConstants.platform == "win" && processCpuTime < cpuThreads) {
    // On Windows, our test jobs likely run in VMs without constant TSC,
    // so we might have low precision CPU time measurements.
    const MAX_DISCREPENCY = 100;
    Assert.ok(
      cpuThreads - processCpuTime < MAX_DISCREPENCY,
      `on Windows, we accept a discrepency of up to ${MAX_DISCREPENCY}ms between the process CPU time and the sum of its threads' CPU time, process CPU time: ${processCpuTime}, sum of thread CPU time: ${cpuThreads}`
    );
  } else {
    Assert.greaterOrEqual(
      processCpuTime,
      cpuThreads,
      "The total CPU time of the process should be at least the sum of the CPU time spent by the still alive threads"
    );
  }
}

add_task(async function test_proc_info() {
  // Open a few `about:home` tabs, they'll end up in `privilegedabout`.
  let tabsAboutHome = [];
  for (let i = 0; i < 5; ++i) {
    let tab = BrowserTestUtils.addTab(gBrowser, "about:home");
    tabsAboutHome.push(tab);
    gBrowser.selectedTab = tab;
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  }

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: DUMMY_URL },
    async function (browser) {
      // We test `SAMPLE_SIZE` times to increase a tad the chance of encountering race conditions.
      for (let z = 0; z < SAMPLE_SIZE; z++) {
        let parentProc = await ChromeUtils.requestProcInfo();

        Assert.equal(
          parentProc.type,
          "browser",
          "Parent proc type should be browser"
        );

        checkProcessCpuTime(parentProc);

        Assert.ok(
          parentProc.threads.some(thread => thread.name),
          "At least one of the threads of the parent process is named"
        );

        Assert.ok(parentProc.memory > 0, "Memory was set");

        // While it's very unlikely that the parent will disappear while we're running
        // tests, some children can easily vanish. So we go twice through the list of
        // children. Once to test stuff that all process data respects the invariants
        // that don't care whether we have a race condition and once to test that at
        // least one well-known process that should not be able to vanish during
        // the test respects all the invariants.
        for (let childProc of parentProc.children) {
          Assert.notEqual(
            childProc.type,
            "browser",
            "Child proc type should not be browser"
          );

          // We set the `childID` for child processes that have a `ContentParent`/`ContentChild`
          // actor hierarchy.
          if (childProc.type.startsWith("web")) {
            Assert.notEqual(
              childProc.childID,
              0,
              "Child proc should have been set"
            );
          }
          Assert.notEqual(
            childProc.type,
            "unknown",
            "Child proc type should be known"
          );
          if (childProc.type == "webIsolated") {
            Assert.notEqual(
              childProc.origin || "",
              "",
              "Child process should have an origin"
            );
          }

          checkProcessCpuTime(childProc);
        }

        // We only check other properties on the `privilegedabout` subprocess, which
        // as of this writing is always active and available.
        var hasPrivilegedAbout = false;
        var numberOfAboutTabs = 0;
        for (let childProc of parentProc.children) {
          if (childProc.type != "privilegedabout") {
            continue;
          }
          hasPrivilegedAbout = true;
          Assert.ok(childProc.memory > 0, "Memory was set");

          for (var win of childProc.windows) {
            if (win.documentURI.spec != "about:home") {
              // We're only interested in about:home for this test.
              continue;
            }
            numberOfAboutTabs++;
            Assert.ok(
              win.outerWindowId > 0,
              `ContentParentID should be > 0 ${win.outerWindowId}`
            );
            if (win.documentTitle) {
              // Unfortunately, we sometimes reach this point before the document is fully loaded, so
              // `win.documentTitle` may still be empty.
              Assert.equal(win.documentTitle, "New Tab");
            }
          }
          Assert.ok(
            numberOfAboutTabs >= tabsAboutHome.length,
            "We have found at least as many about:home tabs as we opened"
          );

          // Once we have verified the privileged about process, bailout.
          break;
        }

        Assert.ok(
          hasPrivilegedAbout,
          "We have found the privileged about process"
        );
      }

      for (let tab of tabsAboutHome) {
        BrowserTestUtils.removeTab(tab);
      }
    }
  );
});
