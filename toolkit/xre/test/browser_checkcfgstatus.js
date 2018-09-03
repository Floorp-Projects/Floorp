/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

/**
 * Returns the id of the crash minidump.
 *
 * @param subject (nsISupports)
 *        The subject passed through the ipc:content-shutdown
 *        observer notification when a content process crash has
 *        occurred.
 * @returns {String} The crash dump id.
 */
function getCrashDumpId(subject) {
  Assert.ok(subject instanceof Ci.nsIPropertyBag2,
            "Subject needs to be a nsIPropertyBag2 to clean up properly");

  return subject.getPropertyAsAString("dumpID");
}

// Calls a function that should crash when CFG is enabled
add_task(async function test_cfg_enabled() {
  // On debug builds, crashing tabs results in much thinking, which
  // slows down the test and results in intermittent test timeouts,
  // so we'll pump up the expected timeout for this test.
  requestLongerTimeout(2);

  if (!gMultiProcessBrowser) {
    Assert.ok(false, "This test should only be run in multi-process mode.");
    return;
  }

  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://example.com",
  }, async function(browser) {
    // First, sanity check...
    Assert.ok(browser.isRemoteBrowser,
              "This browser needs to be remote if this test is going to " +
              "work properly.");
    let contentProcessGone = TestUtils.topicObserved("ipc:content-shutdown");

    ContentTask.spawn(browser, null, function() {
      // Until 1342564 is fixed, we need to disable this call or it will cause False Positives
      privateNoteIntentionalCrash();

      ChromeUtils.import("resource://gre/modules/ctypes.jsm");
      let mozglue = ctypes.open("mozglue.dll");
      let CFG_DisabledOrCrash = mozglue.declare("CFG_DisabledOrCrash", ctypes.default_abi, ctypes.bool);
      CFG_DisabledOrCrash();
      // ^-- this line should have crashed us. If we get to the next line, no bueno

      Assert.ok(false, "This test should cause a crash when CFG is enabled. If it " +
           "does not, this false assertion will trigger. It means CFG is not enabled " +
           "and we have lost compiler hardening features.");
    });

    // If we don't crash within 5 seconds, give up.
    let timeout = new Promise(resolve => setTimeout(resolve, 5000, [null]));

    // We crash or timeout
    let [subject /* , data */] = await Promise.race([timeout, contentProcessGone]);

    if (!subject) {
      // We timed out, or otherwise didn't crash properly
      Assert.ok(false, "This test should cause a crash when CFG is enabled. We didn't " +
        "observe a crash. This specific assertion should be redundant to a false assertion " +
        "immediately prior to it. If it occurs alone, then something strange has occured " +
        "and CFG status and this test should be investigated.");
    } else {
      // We crashed properly, clean up...
      info("Content process is gone!");

      // If we don't clean up the minidump, the harness will complain.
      let dumpID = getCrashDumpId(subject);

      Assert.ok(dumpID == "", "There should NOT be a dumpID, but we have one: " + dumpID);
    }
  });
});
