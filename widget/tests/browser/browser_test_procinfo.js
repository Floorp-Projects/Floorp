/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const ROOT_URL = "http://example.com/browser/widget/tests/browser";
const DUMMY_URL = ROOT_URL + "/dummy.html";
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const MAC = AppConstants.platform == "macosx";
const isFissionEnabled = Services.prefs.getBoolPref("fission.autostart");

add_task(async function test_proc_info() {
  waitForExplicitFinish();
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: DUMMY_URL },
    async function(browser) {
      let cpuThreads = 0;
      let cpuUser = 0;
      for (let z = 0; z < 10; z++) {
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

        for (var i = 0; i < parentProc.children.length; i++) {
          let childProc = parentProc.children[i];
          Assert.notEqual(
            childProc.type,
            "browser",
            "Child proc type should not be browser"
          );
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
      }
      // see https://bugzilla.mozilla.org/show_bug.cgi?id=1529023
      if (!MAC) {
        Assert.greater(cpuThreads, 0, "Got some cpu time in the threads");
      }
      Assert.greater(cpuUser, 0, "Got some cpu time");
    }
  );
});
