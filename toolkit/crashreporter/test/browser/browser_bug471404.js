function check_clear_visible(browser, aVisible) {
  return ContentTask.spawn(browser, aVisible, function(aVisible) {
    let doc = content.document;
    let visible = false;
    let button = doc.getElementById("clear-reports");
    if (button) {
      let style = doc.defaultView.getComputedStyle(button, "");
      if (style.display != "none" &&
          style.visibility == "visible")
        visible = true;
    }
    Assert.equal(visible, aVisible,
      "clear reports button is " + (aVisible ? "visible" : "hidden"));
  });
}

// each test here has a setup (run before loading about:crashes) and onload (run after about:crashes loads)
var _tests = [{setup: null, onload: function(browser) { return check_clear_visible(browser, false); }},
              {setup: function(crD) { return add_fake_crashes(crD, 1); },
               onload: function(browser) { return check_clear_visible(browser, true); }}
              ];

add_task(function* test() {
  let appD = make_fake_appdir();
  let crD = appD.clone();
  crD.append("Crash Reports");

  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" }, function* (browser) {
    for (let test of _tests) {
      // Run setup before loading about:crashes.
      if (test.setup) {
        yield test.setup(crD);
      }

      BrowserTestUtils.loadURI(browser, "about:crashes");
      yield BrowserTestUtils.browserLoaded(browser).then(() => test.onload(browser));
    }
  });

  cleanup_fake_appdir();
});
