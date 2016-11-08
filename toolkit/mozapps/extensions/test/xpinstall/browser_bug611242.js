// ----------------------------------------------------------------------------
// Test whether setting a new property in InstallTrigger then persists to other
// page loads
add_task(function* test() {
  yield BrowserTestUtils.withNewTab({ gBrowser, url: TESTROOT + "enabled.html" }, function* (browser) {
    yield ContentTask.spawn(browser, null, () => {
      content.wrappedJSObject.InstallTrigger.enabled.k = function() { };
    });

    BrowserTestUtils.loadURI(browser, TESTROOT2 + "enabled.html");
    yield BrowserTestUtils.browserLoaded(browser);
    yield ContentTask.spawn(browser, null, () => {
      is(content.wrappedJSObject.InstallTrigger.enabled.k, undefined, "Property should not be defined");
    });
  });
});
// ----------------------------------------------------------------------------
