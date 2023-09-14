function check_clear_visible(browser, aVisible) {
  return SpecialPowers.spawn(browser, [aVisible], function (aVisible) {
    const doc = content.document;
    let visible = false;
    const reportListSubmitted = doc.getElementById("reportListSubmitted");
    if (reportListSubmitted) {
      const style = doc.defaultView.getComputedStyle(reportListSubmitted);
      if (style.display !== "none" && style.visibility === "visible") {
        visible = true;
      }
    }
    Assert.equal(
      visible,
      aVisible,
      "clear submitted reports button is " + (aVisible ? "visible" : "hidden")
    );
  });
}

// each test here has a setup (run before loading about:crashes) and onload (run after about:crashes loads)
var _tests = [
  {
    setup: null,
    onload(browser) {
      return check_clear_visible(browser, false);
    },
  },
  {
    setup(crD) {
      return add_fake_crashes(crD, 1);
    },
    onload(browser) {
      return check_clear_visible(browser, true);
    },
  },
];

add_task(async function test() {
  let appD = make_fake_appdir();
  let crD = appD.clone();
  crD.append("Crash Reports");

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      for (let test of _tests) {
        // Run setup before loading about:crashes.
        if (test.setup) {
          await test.setup(crD);
        }

        BrowserTestUtils.startLoadingURIString(browser, "about:crashes");
        await BrowserTestUtils.browserLoaded(browser).then(() =>
          test.onload(browser)
        );
      }
    }
  );

  cleanup_fake_appdir();
});
