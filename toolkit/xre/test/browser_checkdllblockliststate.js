// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

// Tests that the dll blocklist initializes correctly during test runs.
add_task(function* test() {
  yield BrowserTestUtils.withNewTab({gBrowser, url: "about:blank" }, function* (browser) {
    ok(Components.classes["@mozilla.org/xre/app-info;1"]
                 .getService(Ci.nsIXULRuntime)
                 .windowsDLLBlocklistStatus,
       "Windows dll blocklist status should be true, indicating it is " +
       "running properly. A failure in this test is considered a " +
       "release blocker.");
  });
});


