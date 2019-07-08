/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const ROOT_URL = "http://example.com/browser/widget/tests/browser";
const DUMMY_URL = ROOT_URL + "/dummy.html";
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const MAC = AppConstants.platform == "macosx";

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

        for (var x = 0; x < parentProc.threads.length; x++) {
          cpuThreads += parentProc.threads[x].cpuUser;
        }

        for (var i = 0; i < parentProc.children.length; i++) {
          let childProc = parentProc.children[i];
          for (var y = 0; y < childProc.threads.length; y++) {
            cpuThreads += childProc.threads[y].cpuUser;
          }
          cpuUser += childProc.cpuUser;
        }
      }
      // see https://bugzilla.mozilla.org/show_bug.cgi?id=1529023
      if (!MAC) {
        Assert.ok(cpuThreads > 0, "Got some cpu time in the threads");
      }
      Assert.ok(cpuUser > 0, "Got some cpu time");
    }
  );
});
