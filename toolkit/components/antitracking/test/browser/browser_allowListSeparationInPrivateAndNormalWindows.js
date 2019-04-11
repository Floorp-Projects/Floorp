// This test works by setting up an exception for the private window allow list
// manually, and it then expects to see some blocking notifications (note the
// document.cookie setter in the blocking callback.)
// If the exception lists aren't handled separately, we'd get confused and put
// the pages loaded under this test in the allow list, which would result in
// the test not passing because no blocking notifications would be observed.

// Testing the reverse case would also be interesting, but unfortunately there
// isn't a super easy way to do that with our antitracking test framework since
// private windows wouldn't send any blocking notifications as they don't have
// storage access in the first place.

/* import-globals-from antitracking_head.js */

"use strict";
add_task(async _ => {
  let uri = Services.io.newURI("https://example.net");
  Services.perms.add(uri, "trackingprotection-pb",
                     Services.perms.ALLOW_ACTION);

  registerCleanupFunction(_ => {
    Services.perms.removeAll();
  });
});

AntiTracking.runTest("Test that we don't honour a private allow list exception in a normal window",
  // Blocking callback
  async _ => {
    document.cookie = "name=value";
  },

  // Non blocking callback
  async _ => {
    // Nothing to do here.
  },

  // Cleanup callback
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  },
  null, // no extra prefs
  false, // run the window.open() test
  false, // run the user interaction test
  Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER, // expect blocking notifications
  false); // run in a normal window

