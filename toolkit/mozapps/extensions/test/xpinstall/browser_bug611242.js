// ----------------------------------------------------------------------------
// Test whether setting a new property in InstallTrigger then persists to other
// page loads
add_task(async function test() {
  await BrowserTestUtils.withNewTab({ gBrowser, url: TESTROOT + "enabled.html" }, async function(browser) {
    await ContentTask.spawn(browser, null, () => {
      content.wrappedJSObject.InstallTrigger.enabled.k = function() { };
    });

    BrowserTestUtils.loadURI(browser, TESTROOT2 + "enabled.html");
    await BrowserTestUtils.browserLoaded(browser);
    await ContentTask.spawn(browser, null, () => {
      is(content.wrappedJSObject.InstallTrigger.enabled.k, undefined, "Property should not be defined");
    });
  });
});
// ----------------------------------------------------------------------------
