// ----------------------------------------------------------------------------
// Test whether setting a new property in InstallTrigger then persists to other
// page loads
add_task(async function test() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.InstallTrigger.enabled", true],
      ["extensions.InstallTriggerImpl.enabled", true],
    ],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: TESTROOT + "enabled.html" },
    async function (browser) {
      await SpecialPowers.spawn(browser, [], () => {
        content.wrappedJSObject.InstallTrigger.enabled.k = function () {};
      });

      BrowserTestUtils.startLoadingURIString(
        browser,
        TESTROOT2 + "enabled.html"
      );
      await BrowserTestUtils.browserLoaded(browser);
      await SpecialPowers.spawn(browser, [], () => {
        is(
          content.wrappedJSObject.InstallTrigger.enabled.k,
          undefined,
          "Property should not be defined"
        );
      });
    }
  );
});
// ----------------------------------------------------------------------------
