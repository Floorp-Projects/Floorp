/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const ROOT_URL = "http://example.com/browser/widget/tests/browser";
const DUMMY_URL = ROOT_URL + "/dummy.html";
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const MAC = AppConstants.platform == "macosx";
const HAS_THREAD_NAMES =
  AppConstants.platform != "win" ||
  AppConstants.isPlatformAndVersionAtLeast("win", 10);
const isFissionEnabled = Services.prefs.getBoolPref("fission.autostart");

const SAMPLE_SIZE = 10;

add_task(async function test_proc_info() {
  console.log("YORIC", "Test starts");
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
    async function(browser) {
      let cpuThreads = 0;
      let cpuUser = 0;

      // We test `SAMPLE_SIZE` times to increase a tad the chance of encountering race conditions.
      for (let z = 0; z < SAMPLE_SIZE; z++) {
        let parentProc = await ChromeUtils.requestProcInfo();
        cpuUser += parentProc.cpuUser;

        Assert.equal(
          parentProc.type,
          "browser",
          "Parent proc type should be browser"
        );

        for (var x = 0; x < parentProc.threads.length; x++) {
          cpuThreads += parentProc.threads[x].cpuUser;
        }

        // Under Windows, thread names appeared with Windows 10.
        if (HAS_THREAD_NAMES) {
          Assert.ok(
            parentProc.threads.some(thread => thread.name),
            "At least one of the threads of the parent process is named"
          );
        }

        Assert.ok(
          parentProc.residentUniqueSize > 0,
          "Resident-unique-size was set"
        );
        Assert.ok(
          parentProc.residentUniqueSize <= parentProc.residentSetSize,
          `Resident-unique-size should be bounded by resident-set-size ${parentProc.residentUniqueSize} <= ${parentProc.residentSetSize}`
        );

        // While it's very unlikely that the parent will disappear while we're running
        // tests, some children can easily vanish. So we go twice through the list of
        // children. Once to test stuff that all process data respects the invariants
        // that don't care whether we have a race condition and once to test that at
        // least one well-known process that should not be able to vanish during
        // the test respects all the invariants.
        for (var i = 0; i < parentProc.children.length; i++) {
          let childProc = parentProc.children[i];
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

          for (var y = 0; y < childProc.threads.length; y++) {
            cpuThreads += childProc.threads[y].cpuUser;
          }
          cpuUser += childProc.cpuUser;
        }

        // We only check other properties on the `privilegedabout` subprocess, which
        // as of this writing is always active and available.
        var hasPrivilegedAbout = false;
        var numberOfAboutTabs = 0;
        for (i = 0; i < parentProc.children.length; i++) {
          let childProc = parentProc.children[i];
          if (childProc.type != "privilegedabout") {
            continue;
          }
          hasPrivilegedAbout = true;
          Assert.ok(
            childProc.residentUniqueSize > 0,
            "Resident-unique-size was set"
          );
          Assert.ok(
            childProc.residentUniqueSize <= childProc.residentSetSize,
            `Resident-unique-size should be bounded by resident-set-size ${childProc.residentUniqueSize} <= ${childProc.residentSetSize}`
          );

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
      // see https://bugzilla.mozilla.org/show_bug.cgi?id=1529023
      if (!MAC) {
        Assert.greater(cpuThreads, 0, "Got some cpu time in the threads");
      }
      Assert.greater(cpuUser, 0, "Got some cpu time");

      for (let tab of tabsAboutHome) {
        BrowserTestUtils.removeTab(tab);
      }
    }
  );
});
